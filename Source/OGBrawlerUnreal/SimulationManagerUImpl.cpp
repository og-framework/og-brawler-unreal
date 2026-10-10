// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationManagerUImpl-rationale.md · docs/SimulationManagerUImpl-guards.md

#include "SimulationManagerUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/SimulationComparison.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "GameFramework/PlayerController.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"
#include "OGSimulationUnreal/SimulationInputRelay.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosScene.h"
#include "Runtime/Engine/Public/Physics/NetworkPhysicsComponent.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"
#include "Runtime/Experimental/Chaos/Public/RewindData.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackGuardSimulation.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "OGBrawler/OGBrawlerLog.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"
#if !OG_PHYSICS_BACKEND_CHAOS
#include "Async/UniqueLock.h"
#include "OGSimulationJolt/JoltLayerTable.h"
#include "OGSimulationJolt/JoltStaticWorldBuilder.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#endif

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGatePolicy.h"
#include "OGSimulation/OGAssert.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGSim);
DEFINE_LOG_CATEGORY(LogOGSimTick);
DEFINE_LOG_CATEGORY(LogOGMgmt);
DEFINE_LOG_CATEGORY(LogOGNet);
DEFINE_LOG_CATEGORY(LogOG);
DEFINE_LOG_CATEGORY(LogOGRelayProbe);
DEFINE_LOG_CATEGORY(LogOGDivergenceProbe);
DEFINE_LOG_CATEGORY(LogOGResimProbe);
DEFINE_LOG_CATEGORY(LogOGLatencyBudget);
DEFINE_LOG_CATEGORY(LogOGChaosDilation);
DEFINE_LOG_CATEGORY(LogOGSimHost);
DEFINE_LOG_CATEGORY(LogOGBrawler);

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

namespace
{
#if OG_PHYSICS_BACKEND_CHAOS
	template <typename QueryAdapterOptional>
	void emplaceBrawlerQueryAdapter(QueryAdapterOptional& queryAdapter, UWorld* world)
	{
		queryAdapter.emplace(world, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
			{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
			{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
		});
	}
#else
	TAutoConsoleVariable<int32> CVarSimRunInline(
		TEXT("og.Sim.RunInline"),
		0,
		TEXT("Jolt configuration only. 1 = a client steps inline on the game thread instead of on a worker task. ")
		TEXT("Read once, at the simulation manager's BeginPlay. The authority always steps inline."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderInterpolationDelaySteps(
		TEXT("og.Render.InterpolationDelaySteps"),
		2.f,
		TEXT("Jolt configuration only. How many physics steps behind the host time the characters are rendered, ")
		TEXT("interpolated between the two published steps that bracket that time by their step deadlines. 2 matches ")
		TEXT("Chaos's p.AsyncInterpolationMultiplier. Clamped to [0, 2]: the render snapshot channel keeps four steps."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarRenderNewestSnapshot(
		TEXT("og.Render.NewestSnapshot"),
		0,
		TEXT("Jolt configuration only, for comparison. 1 = render the newest published step, with no interpolation and ")
		TEXT("no delay."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarRenderTraceFrames(
		TEXT("og.Render.TraceFrames"),
		0,
		TEXT("Jolt configuration, non-shipping. 1 = log every frame's rendered capsule position per character, with the ")
		TEXT("render time and the bracketing steps ([SimHost.RenderTrace] on LogOGSimHost, Log)."),
		ECVF_Default);
#endif

	const std::vector<UEStaticCategoryChannel>& brawlerCategoryChannels()
	{
		static const std::vector<UEStaticCategoryChannel> channels{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 },
			{ collisionCategory::world,        ECollisionChannel::ECC_WorldStatic       },
			{ collisionCategory::character,    ECollisionChannel::ECC_GameTraceChannel6 }
		};
		return channels;
	}

	uint32 brawlerMappedCategories()
	{
		uint32 mapped = 0u;
		for (const UEStaticCategoryChannel& channel : brawlerCategoryChannels())
			mapped |= 1u << channel.category;
		return mapped;
	}

	void logJoltToSimHost(const char* message)
	{
		const FString line(UTF8_TO_TCHAR(message));
		if (line.StartsWith(TEXT("[Warning]")))
		{
			UE_LOG(LogOGSimHost, Warning, TEXT("%s"), *line);
		}
		else
		{
			UE_LOG(LogOGSimHost, Log, TEXT("%s"), *line);
		}
	}

	using BrawlerPhysicsComposite =
		std::remove_cvref_t<decltype(std::declval<const SimulatableBrawler&>().getPhysicsComposite())>;

	std::vector<SlotBodyTemplate> brawlerSlotTemplate()
	{
		std::vector<SlotBodyTemplate> slotTemplate;
		const BrawlerPhysicsComposite composite{};
		composite.forEach([&slotTemplate](const auto& declaration)
		{
			using D = std::decay_t<decltype(declaration)>;
			slotTemplate.push_back(SlotBodyTemplate{ D::descriptor(), static_cast<uint8_t>(slotTemplate.size()) });
		});
		return slotTemplate;
	}

	constexpr uint8 kJoltAuthorityRoleBit = 1u;
	constexpr uint8 kJoltClientRoleBit    = 2u;

	thread_local uint8 t_joltWorldMutexHeldRoles = 0u;
	thread_local uint8 t_joltStepRunningRoles    = 0u;

	thread_local uint64 t_joltShadowQueryAccesses        = 0u;
	thread_local uint64 t_joltStepQueryGameThreadAccesses = 0u;

	constexpr uint32 kShadowTempAllocatorBytes = 64u * 1024u;

	template <uint8 RoleBit>
	bool joltWorldAccessFromStepOrWorldMutex()
	{
		return ((t_joltStepRunningRoles | t_joltWorldMutexHeldRoles) & RoleBit) != 0u;
	}

	template <uint8 RoleBit>
	bool joltWorkerClientStepWorldAccess()
	{
#if !UE_BUILD_SHIPPING
		if ((t_joltWorldMutexHeldRoles & RoleBit) == 0u && IsInGameThread())
			++t_joltStepQueryGameThreadAccesses;
#endif
		return joltWorldAccessFromStepOrWorldMutex<RoleBit>();
	}

	bool joltShadowAccessOnGameThread()
	{
#if !UE_BUILD_SHIPPING
		++t_joltShadowQueryAccesses;
#endif
		return IsInGameThread();
	}

	template <uint8 RoleBit>
	bool joltWorldAccessFromStepWorldMutexOrInlineGameThread()
	{
		return joltWorldAccessFromStepOrWorldMutex<RoleBit>() || IsInGameThread();
	}

	JoltWorldAccessCheckFn joltWorldAccessCheckFor(uint8 roleBit, bool stepsInline)
	{
		if (roleBit == kJoltAuthorityRoleBit)
		{
			return stepsInline ? &joltWorldAccessFromStepWorldMutexOrInlineGameThread<kJoltAuthorityRoleBit>
			                   : &joltWorldAccessFromStepOrWorldMutex<kJoltAuthorityRoleBit>;
		}
		return stepsInline ? &joltWorldAccessFromStepWorldMutexOrInlineGameThread<kJoltClientRoleBit>
		                   : &joltWorkerClientStepWorldAccess<kJoltClientRoleBit>;
	}

	UE::FMutex& checkedNotYetHeld(UE::FMutex& worldMutex, uint8 roleBit)
	{
		checkf(roleBit == kJoltAuthorityRoleBit || roleBit == kJoltClientRoleBit,
			TEXT("The Jolt world mutex (J2) was taken before the stepping mode chose this manager's role bit."));
		checkf((t_joltWorldMutexHeldRoles & roleBit) == 0u,
			TEXT("This manager's Jolt world mutex (J2) is already held on this thread. UE::FMutex is not recursive: a ")
			TEXT("second lock deadlocks. Take J2 once, around the whole GT world write or the whole step (UImpl G-81)."));
		return worldMutex;
	}

	class JoltWorldLockScopeUImpl
	{
	public:
		JoltWorldLockScopeUImpl(UE::FMutex& worldMutex, uint8 roleBit)
			: m_lock(checkedNotYetHeld(worldMutex, roleBit))
			, m_roleBit(roleBit)
		{
			t_joltWorldMutexHeldRoles |= m_roleBit;
		}

		~JoltWorldLockScopeUImpl()
		{
			t_joltWorldMutexHeldRoles &= static_cast<uint8>(~m_roleBit);
		}

		JoltWorldLockScopeUImpl(const JoltWorldLockScopeUImpl&) = delete;
		JoltWorldLockScopeUImpl& operator=(const JoltWorldLockScopeUImpl&) = delete;

	private:
		UE::TUniqueLock<UE::FMutex> m_lock;
		JoltWorldAccessScope        m_access;
		uint8                       m_roleBit;
	};

	class JoltStepScopeUImpl
	{
	public:
		explicit JoltStepScopeUImpl(uint8 roleBit)
			: m_roleBit(roleBit)
		{
			checkf((t_joltStepRunningRoles & m_roleBit) == 0u,
				TEXT("A Jolt step of this manager is already running on this thread: steps never nest."));
			t_joltStepRunningRoles |= m_roleBit;
		}

		~JoltStepScopeUImpl()
		{
			t_joltStepRunningRoles &= static_cast<uint8>(~m_roleBit);
		}

		JoltStepScopeUImpl(const JoltStepScopeUImpl&) = delete;
		JoltStepScopeUImpl& operator=(const JoltStepScopeUImpl&) = delete;

	private:
		uint8 m_roleBit;
	};

	template <typename Composite>
	struct physicsDeclarationCountOf;

	template <typename... Declarations>
	struct physicsDeclarationCountOf<SimulationComposite<Declarations...>>
		: std::integral_constant<size_t, sizeof...(Declarations)>
	{
	};

	constexpr double kRenderMaxInterpolationDelaySteps = 2.0;
	constexpr float  kRenderParkSnapCm = -0.5f * brawlerProjectileSimulation::kParkZ;

#if !UE_BUILD_SHIPPING
	const TCHAR* stepKindText(StepKind kind)
	{
		switch (kind)
		{
		case StepKind::Normal:     return TEXT("Normal");
		case StepKind::Stall:      return TEXT("Stall");
		case StepKind::Skip:       return TEXT("Skip");
		case StepKind::HardResync: return TEXT("HardResync");
		}
		return TEXT("?");
	}

	constexpr double kRenderApplyToleranceCm  = 1.0e-6;
	constexpr double kShadowPoseToleranceCm   = 1.0e-3;
	constexpr double kShadowPoseToleranceRad  = 1.0e-4;
	constexpr uint32 kRenderApplyWindowFrames = 600u;
	constexpr uint32 kShadowWindowFrames      = 600u;
#endif
#endif
	void bindCorrectionFieldDiffGate()
	{
		// ⛔G-50  docs/SimulationManagerUImpl-guards.md
		correctionFieldDiff::setEnabledPredicate(
			[]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
	}

#if OG_PHYSICS_BACKEND_CHAOS
	void writeRestoredBodyState(Chaos::FRigidBodyHandle_Internal& ptApi, const PhysicsBodyState& restored,
	                            bool wireCarriesRotationAndSpin)
	{
		ptApi.SetX(uglm::toFVector(restored.position));
		ptApi.SetV(uglm::toFVector(restored.linearVelocity));
		// ⛔G-77  docs/SimulationManagerUImpl-guards.md
		if (wireCarriesRotationAndSpin)
		{
			ptApi.SetR(uglm::toFQuat(restored.rotation));
			ptApi.SetW(uglm::toFVector(restored.angularVelocity));
		}
	}

	const TCHAR* pushProbeResimTypeText(Chaos::EResimType type)
	{
		switch (type)
		{
		case Chaos::EResimType::FullResim:       return TEXT("FullResim");
		case Chaos::EResimType::ResimAsFollower: return TEXT("ResimAsFollower");
		default:                                 return TEXT("?");
		}
	}

	const TCHAR* pushProbeObjectStateText(Chaos::EObjectStateType state)
	{
		switch (state)
		{
		case Chaos::EObjectStateType::Static:    return TEXT("Static");
		case Chaos::EObjectStateType::Kinematic: return TEXT("Kinematic");
		case Chaos::EObjectStateType::Dynamic:   return TEXT("Dynamic");
		case Chaos::EObjectStateType::Sleeping:  return TEXT("Sleeping");
		default:                                 return TEXT("?");
		}
	}

	struct LivePushProbeRead
	{
		PhysicsBodyState startOfStep;
		glm::vec3        solvedP{0.f};
	};

	LivePushProbeRead readLiveBodyForPushProbe(Chaos::FSingleParticlePhysicsProxy& proxy)
	{
		LivePushProbeRead read;
		auto* ptApi = proxy.GetPhysicsThreadAPI();
		const Chaos::FConstGenericParticleHandle particle(proxy.GetHandle_LowLevel());
		// ⛔G-52  docs/SimulationManagerUImpl-guards.md
		read.startOfStep.position        = uglm::toGLMVec3(particle->GetX());
		read.startOfStep.rotation        = uglm::toGLMQuat(FQuat(particle->GetR()));
		read.startOfStep.linearVelocity  = uglm::toGLMVec3(ptApi->GetV());
		read.startOfStep.angularVelocity = uglm::toGLMVec3(ptApi->GetW());
		read.solvedP                     = uglm::toGLMVec3(particle->GetP());
		return read;
	}

	float pushProbeVectorDelta(const glm::vec3& a, const glm::vec3& b)
	{
		const float dx = std::abs(a.x - b.x);
		const float dy = std::abs(a.y - b.y);
		const float dz = std::abs(a.z - b.z);
		float worst = dx;
		if (dy > worst) worst = dy;
		if (dz > worst) worst = dz;
		return worst;
	}

	float pushProbeRotationDelta(const glm::quat& a, const glm::quat& b)
	{
		return std::abs(std::abs(glm::dot(a, b)) - 1.f);
	}

	// ⛔G-53  docs/SimulationManagerUImpl-guards.md
	bool pushProbeFieldsSimilar(const PhysicsBodyState& a, const PhysicsBodyState& b,
	                            bool wireCarriesRotationAndSpin)
	{
		return isSimilarToField(a.position,       b.position)
			&& isSimilarToField(a.linearVelocity, b.linearVelocity)
			&& (!wireCarriesRotationAndSpin
				|| (isSimilarToField(a.rotation,        b.rotation)
					&& isSimilarToField(a.angularVelocity, b.angularVelocity)));
	}

	float pushProbeWorstDelta(const PhysicsBodyState& a, const PhysicsBodyState& b,
	                          bool wireCarriesRotationAndSpin)
	{
		float worst = pushProbeVectorDelta(a.position, b.position);
		const float lin = pushProbeVectorDelta(a.linearVelocity, b.linearVelocity);
		if (lin > worst) worst = lin;
		if (wireCarriesRotationAndSpin)
		{
			const float rot = pushProbeRotationDelta(a.rotation,        b.rotation);
			const float ang = pushProbeVectorDelta  (a.angularVelocity, b.angularVelocity);
			if (rot > worst) worst = rot;
			if (ang > worst) worst = ang;
		}
		return worst;
	}

	struct PushProbeTally
	{
		int32 bodies             = 0;
		int32 compared           = 0;
		int32 unresolved         = 0;
		int32 inert              = 0;
		int32 nonInert           = 0;
		int32 nonInertMatched    = 0;
		int32 nonInertMismatched = 0;
		int32 movedAtPush        = 0;
	};

	// ⛔G-54  docs/SimulationManagerUImpl-guards.md
	const TCHAR* pushProbeVerdictText(const PushProbeTally& tally)
	{
		if (tally.nonInert == 0)
			return TEXT("VACUOUS");
		return tally.nonInertMismatched == 0 ? TEXT("ALL_MATCH") : TEXT("MISMATCH");
	}

	void logPushProbeBody(
		PushProbeTally&                       tally,
		const Chaos::FGeometryParticleHandle& handle,
		const FRewindPushProbeStashedBody&    stashed,
		const LivePushProbeRead&              live,
		int32                                 pushFrame,
		int32                                 readFrame,
		uint32_t                              simTick)
	{
		const PhysicsBodyState& now    = live.startOfStep;
		const PhysicsBodyState& pushed = stashed.pushed;
		const PhysicsBodyState& before = stashed.beforePush;
		const bool              wire   = stashed.wireCarriesRotationAndSpin;

		const bool inert = pushProbeFieldsSimilar(before, pushed, wire);
		const bool match = pushProbeFieldsSimilar(now,    pushed, wire);

		++tally.compared;
		if (stashed.movedAtPush)
			++tally.movedAtPush;
		if (inert)
		{
			++tally.inert;
		}
		else
		{
			++tally.nonInert;
			if (match)
				++tally.nonInertMatched;
			else
				++tally.nonInertMismatched;
		}

		UE_LOG(LogOGResimProbe, Verbose,
			TEXT("[ResimProbe.PushTarget] pushFrame=%d readFrame=%d simTick=%u id=%u body=%hs bodyId=%u ")
			TEXT("inert=%d match=%d moved=%d cmp=%hs resimType=%s objState=%s eps=%.6f ")
			TEXT("dBefore=%.6f dBeforePos=%.6f dBeforeRot=%.6f dBeforeLin=%.6f dBeforeAng=%.6f ")
			TEXT("dPos=%.6f dRot=%.6f dLin=%.6f dAng=%.6f dXP=%.6f ")
			TEXT("pushedPos=(%.4f,%.4f,%.4f) livePos=(%.4f,%.4f,%.4f) ")
			TEXT("pushedRot=(%.4f,%.4f,%.4f,%.4f) liveRot=(%.4f,%.4f,%.4f,%.4f) ")
			TEXT("pushedLin=(%.4f,%.4f,%.4f) liveLin=(%.4f,%.4f,%.4f) ")
			TEXT("pushedAng=(%.4f,%.4f,%.4f) liveAng=(%.4f,%.4f,%.4f) ")
			TEXT("beforePos=(%.4f,%.4f,%.4f) beforeLin=(%.4f,%.4f,%.4f)"),
			pushFrame, readFrame, simTick, stashed.simulatableId,
			stashed.bodyName != nullptr ? stashed.bodyName : "?",
			static_cast<uint32>(stashed.bodyId.value),
			inert ? 1 : 0, match ? 1 : 0, stashed.movedAtPush ? 1 : 0,
			wire ? "pos,rot,lin,ang" : "pos,lin",
			pushProbeResimTypeText(handle.ResimType()),
			pushProbeObjectStateText(handle.ObjectState()),
			kDefaultSimilarityEpsilon,
			pushProbeWorstDelta   (before, pushed, wire),
			pushProbeVectorDelta  (before.position,        pushed.position),
			pushProbeRotationDelta(before.rotation,        pushed.rotation),
			pushProbeVectorDelta  (before.linearVelocity,  pushed.linearVelocity),
			pushProbeVectorDelta  (before.angularVelocity, pushed.angularVelocity),
			pushProbeVectorDelta  (now.position,        pushed.position),
			pushProbeRotationDelta(now.rotation,        pushed.rotation),
			pushProbeVectorDelta  (now.linearVelocity,  pushed.linearVelocity),
			pushProbeVectorDelta  (now.angularVelocity, pushed.angularVelocity),
			pushProbeVectorDelta  (now.position,        live.solvedP),
			pushed.position.x, pushed.position.y, pushed.position.z,
			now.position.x,    now.position.y,    now.position.z,
			pushed.rotation.x, pushed.rotation.y, pushed.rotation.z, pushed.rotation.w,
			now.rotation.x,    now.rotation.y,    now.rotation.z,    now.rotation.w,
			pushed.linearVelocity.x,  pushed.linearVelocity.y,  pushed.linearVelocity.z,
			now.linearVelocity.x,     now.linearVelocity.y,     now.linearVelocity.z,
			pushed.angularVelocity.x, pushed.angularVelocity.y, pushed.angularVelocity.z,
			now.angularVelocity.x,    now.angularVelocity.y,    now.angularVelocity.z,
			before.position.x,       before.position.y,       before.position.z,
			before.linearVelocity.x, before.linearVelocity.y, before.linearVelocity.z);
	}
#endif

	enum class OGLogRoute : uint8
	{
		SimTick, ResimProbe, Sim, Net, RelayProbe, DivergenceProbe, Mgmt
	};

	struct OGLogRouteEntry
	{
		const TCHAR* prefix;
		OGLogRoute   route;
	};

	constexpr OGLogRouteEntry kOGLogRoutes[] = {
		{ TEXT("[Resim.Input]"),                     OGLogRoute::SimTick },
		{ TEXT("[ResimProbe"),                       OGLogRoute::ResimProbe },
		{ TEXT("[TimeResync."),                      OGLogRoute::Sim },
		{ TEXT("[Resim."),                           OGLogRoute::Sim },
		{ TEXT("[ResimCheck.Divergence]"),           OGLogRoute::Sim },
		{ TEXT("[ResimCheck.PrepareRestore]"),       OGLogRoute::Sim },
		{ TEXT("[ResimCheck.Check]"),                OGLogRoute::SimTick },
		{ TEXT("[ResimCheck.IsSimilar]"),            OGLogRoute::SimTick },
		{ TEXT("[ResimCheck.TriggerRewind]"),        OGLogRoute::SimTick },
		{ TEXT("[AuthoritySimulation]"),             OGLogRoute::SimTick },
		{ TEXT("[ClientPrediction]"),                OGLogRoute::SimTick },
		{ TEXT("[PredictionSimulation]"),            OGLogRoute::SimTick },
		{ TEXT("[PostPrediction]"),                  OGLogRoute::SimTick },
		{ TEXT("[CollectInput]"),                    OGLogRoute::SimTick },
		{ TEXT("[ServerReceive]"),                   OGLogRoute::Net },
		{ TEXT("[ReceiveLocalInput]"),               OGLogRoute::Net },
		{ TEXT("[SendCorrectionStateToClients]"),    OGLogRoute::Net },
		{ TEXT("[SendRemoteInputToClients]"),        OGLogRoute::Net },
		{ TEXT("[SendLocalInputToServer]"),          OGLogRoute::Net },
		{ TEXT("[ReceiveCorrectionState]"),          OGLogRoute::Net },
		{ TEXT("[ReceiveCorrectionInput]"),          OGLogRoute::Net },
		{ TEXT("[InjectCorrectionState]"),           OGLogRoute::Net },
		{ TEXT("[InjectCorrectionInput]"),           OGLogRoute::Net },
		{ TEXT("[DrainOutOfOrder]"),                 OGLogRoute::Net },
		{ TEXT("[InputGap]"),                        OGLogRoute::Net },
		{ TEXT("[InputDrop]"),                       OGLogRoute::Net },
		{ TEXT("[DelayShift]"),                      OGLogRoute::Net },
		{ TEXT("[InputStats]"),                      OGLogRoute::Net },
		{ TEXT("[Park]"),                            OGLogRoute::Net },
		{ TEXT("[Release]"),                         OGLogRoute::Net },
		{ TEXT("[InputDomain]"),                     OGLogRoute::Net },
		{ TEXT("[RelaySkip]"),                       OGLogRoute::Net },
		{ TEXT("[RelayProbe"),                       OGLogRoute::RelayProbe },
		{ TEXT("[DivergenceProbe"),                  OGLogRoute::DivergenceProbe },
		{ TEXT("SimulationManager:"),                OGLogRoute::Mgmt },
		{ TEXT("tryRegister:"),                      OGLogRoute::Mgmt },
		{ TEXT("NewFramework:"),                     OGLogRoute::Mgmt }
	};

	constexpr TCHAR ogLogRouteFoldCase(TCHAR c)
	{
		return (c >= TEXT('A') && c <= TEXT('Z')) ? static_cast<TCHAR>(c - TEXT('A') + TEXT('a')) : c;
	}

	constexpr bool ogLogRouteStartsWith(const TCHAR* text, const TCHAR* prefix)
	{
		for (; *prefix != 0; ++text, ++prefix)
		{
			if (*text == 0 || ogLogRouteFoldCase(*text) != ogLogRouteFoldCase(*prefix))
				return false;
		}
		return true;
	}

	constexpr bool ogLogRoutesAreAllReachable()
	{
		for (std::size_t later = 0; later < std::size(kOGLogRoutes); ++later)
		{
			for (std::size_t earlier = 0; earlier < later; ++earlier)
			{
				if (ogLogRouteStartsWith(kOGLogRoutes[later].prefix, kOGLogRoutes[earlier].prefix))
					return false;
			}
		}
		return true;
	}

	static_assert(ogLogRoutesAreAllReachable(),
		"RouteOGMessage: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier "
		"route always wins and the later one can never be taken. FString::StartsWith ignores "
		"case and so does this check. Was the ORDER IS LOAD-BEARING fence: [Resim.Input] must be "
		"matched ahead of the [Resim. catch-all, and widening that catch-all to [Resim would "
		"swallow every [ResimCheck. line.");

	void RouteOGMessage(const char* msg)
	{
		FString fmsg(msg);

		ELogVerbosity::Type severity = ELogVerbosity::Log;
		FString body = fmsg;
		if (fmsg.StartsWith(TEXT("[Verbose]")))
		{
			severity = ELogVerbosity::Verbose;
			body = fmsg.RightChop(9);
		}
		else if (fmsg.StartsWith(TEXT("[Warning]")))
		{
			severity = ELogVerbosity::Warning;
			body = fmsg.RightChop(9);
		}

#define EMIT_OG(cat) \
		do { \
			switch (severity) { \
				case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *fmsg); break; \
				case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *fmsg); break; \
				default:                     UE_LOG(cat, Log,     TEXT("%s"), *fmsg); break; \
			} \
		} while (0)

		for (const OGLogRouteEntry& entry : kOGLogRoutes)
		{
			if (!body.StartsWith(entry.prefix))
				continue;

			switch (entry.route)
			{
			case OGLogRoute::SimTick:          EMIT_OG(LogOGSimTick); break;
			case OGLogRoute::ResimProbe:       EMIT_OG(LogOGResimProbe); break;
			case OGLogRoute::Sim:              EMIT_OG(LogOGSim); break;
			case OGLogRoute::Net:              EMIT_OG(LogOGNet); break;
			case OGLogRoute::RelayProbe:       EMIT_OG(LogOGRelayProbe); break;
			case OGLogRoute::DivergenceProbe:  EMIT_OG(LogOGDivergenceProbe); break;
			case OGLogRoute::Mgmt:             EMIT_OG(LogOGMgmt); break;
			}
			return;
		}

		EMIT_OG(LogOG);

#undef EMIT_OG
	}

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarTestGameThreadHitchMs(
		TEXT("og.Sim.TestGameThreadHitchMs"),
		0,
		TEXT("Test only (non-shipping): when above 0, the next OnPostPhysicsStep sleeps the game thread ")
		TEXT("this many milliseconds, once, and resets this variable to 0."),
		ECVF_Default);
#endif

}

OGSIM_OPTIMIZE_OFF

#if OG_PHYSICS_BACKEND_CHAOS
void FSimulationState2::Copy(const FSimulationState2& Value)
{
	bIsValid = Value.bIsValid;
	DeltaTime = Value.DeltaTime;
}

FName FSimulationManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FSimulationManagerAsyncCallback");
	return StaticName;
}

void FSimulationManagerAsyncCallback::OnPreSimulate_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting(); 
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	// ⛔G-51  docs/SimulationManagerUImpl-guards.md
	if (m_pushProbeStashFrame != INDEX_NONE)
	{
		if (isFirstResimulationFrame)
			readPushProbeVerdict_Internal();
		else
			discardPushProbeStash(TEXT("notResetting"));
	}

	if (!isResimulating)
	{
		const unsigned int chaosTick = solver.GetCurrentFrame();
		const unsigned int simulationTick = [&input]() {
			if (!input->m_manager->runsPrediction())
				return input->m_manager->getServerClock().getTick();
			else
				return input->m_manager->getClientClock().getPredictionTick();
		}();
		input->m_manager->editChaosTickMapper().update((int32_t)chaosTick, (int32_t)simulationTick);
	}

	const double stepStartSeconds = FPlatformTime::Seconds();
	input->m_manager->onGameSimulation(updateInfo);

	if (!isResimulating)
		input->m_manager->stampLatencyAfterStep_Internal(stepStartSeconds);
}

void FSimulationManagerAsyncCallback::OnPostSolve_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();
	if (input == nullptr || input->m_manager == nullptr)
		return;

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting();
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	input->m_manager->onPostGameSimulation(updateInfo);

	if (!isResimulating)
		input->m_manager->stampLatencyStepEnd_Internal();
}

void FSimulationManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{

}

void FSimulationManagerAsyncCallback::ProcessInputs_External(int32 PhysicsStep)
{

}

int32 FSimulationManagerAsyncCallback::TriggerRewindIfNeeded_Internal(int32 LastCompletedStep)
{

	ASimulationManagerUImpl* manager = m_manager;
	if (manager == nullptr)
	{
		UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d manager=null rewind=0"),
			LastCompletedStep);
		return INDEX_NONE;
	}

	if (!manager->runsPrediction())
		return INDEX_NONE;

	unsigned int correctionTick = manager->onCheckIsSimilar();
	const bool willRewind = correctionTick != std::numeric_limits<unsigned int>::max() && correctionTick != 0u;
	if (!willRewind)
	{
		UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u rewind=0"),
			LastCompletedStep, correctionTick);
		return INDEX_NONE;
	}

	const int32 unrealTickDifferenceAdjustedTick = manager->getChaosTickMapper().toChaosTick(static_cast<int32_t>(correctionTick));
	UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u chaosTick=%d rewind=1"),
		LastCompletedStep, correctionTick, unrealTickDifferenceAdjustedTick);

	manager->noteResimRequest(correctionTick, LastCompletedStep, unrealTickDifferenceAdjustedTick);

	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.Request] requestedSimTick=%u requestedChaosFrame=%d lastCompletedStep=%d mapperOffset=%d"),
		correctionTick, unrealTickDifferenceAdjustedTick, LastCompletedStep,
		manager->getChaosTickMapper().toChaosTick(0));

	return unrealTickDifferenceAdjustedTick;
}

void FSimulationManagerAsyncCallback::ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input)
{

}

void FSimulationManagerAsyncCallback::FirstPreResimStep_Internal(int32 PhysicsStep)
{
	if (m_manager == nullptr || !m_manager->runsPrediction())
		return;

	// ⛔G-55  docs/SimulationManagerUImpl-guards.md
	m_manager->noteResimGrant(PhysicsStep);

	const uint32_t simTick = static_cast<uint32_t>(
		m_manager->getChaosTickMapper().toSimulationTick(static_cast<int32_t>(PhysicsStep)));
	m_manager->prepareResimulation(PhysicsStep, simTick);

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();

	const bool pushProbeActive = UE_LOG_ACTIVE(LogOGResimProbe, Verbose);

	if (m_pushProbeStashFrame != INDEX_NONE)
		discardPushProbeStash(TEXT("newPushBeforeRead"));

	if (pushProbeActive)
	{
		m_pushProbeStash.Reset();
		m_pushProbeStashFrame      = PhysicsStep;
		m_pushProbeStashSimTick    = simTick;
		m_pushProbeStashBodies     = 0;
		m_pushProbeStashUnresolved = 0;
	}

	auto pushBodyState = [&](BodyId bodyId, const PhysicsBodyState& bs,
	                         const char* bodyName, bool wireCarriesRotationAndSpin,
	                         unsigned int simulatableId)
	{
		if (pushProbeActive)
			++m_pushProbeStashBodies;

		Chaos::FSingleParticlePhysicsProxy* proxy =
			solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
		if (proxy == nullptr)
		{
			if (pushProbeActive)
				++m_pushProbeStashUnresolved;
			return;
		}
		Chaos::FRigidBodyHandle_Internal* ptApi = proxy->GetPhysicsThreadAPI();
		if (ptApi == nullptr)
		{
			if (pushProbeActive)
				++m_pushProbeStashUnresolved;
			return;
		}

		LivePushProbeRead beforePush;
		if (pushProbeActive)
			beforePush = readLiveBodyForPushProbe(*proxy);

		// ⛔G-76  docs/SimulationManagerUImpl-guards.md
		writeRestoredBodyState(*ptApi, bs, wireCarriesRotationAndSpin);

		if (pushProbeActive)
		{
			const LivePushProbeRead afterPush = readLiveBodyForPushProbe(*proxy);
			FRewindPushProbeStashedBody stashed;
			stashed.pushed                     = bs;
			stashed.beforePush                 = beforePush.startOfStep;
			stashed.bodyId                     = bodyId;
			stashed.simulatableId              = simulatableId;
			stashed.bodyName                   = bodyName;
			stashed.wireCarriesRotationAndSpin = wireCarriesRotationAndSpin;
			stashed.movedAtPush = !(
				   isSimilarToField(afterPush.startOfStep.position,        beforePush.startOfStep.position)
				&& isSimilarToField(afterPush.startOfStep.rotation,        beforePush.startOfStep.rotation)
				&& isSimilarToField(afterPush.startOfStep.linearVelocity,  beforePush.startOfStep.linearVelocity)
				&& isSimilarToField(afterPush.startOfStep.angularVelocity, beforePush.startOfStep.angularVelocity));
			m_pushProbeStash.Add(stashed);
		}
	};

	m_manager->editStorage().forEachSimulatable(
		[&](unsigned int id, SimulatableBrawler& simulatable)
		{
			const simulatableBrawler::State& state = simulatable.getAllState().getState();
			simulatable.getPhysicsComposite().forEach([&](const auto& decl) {
				using D = std::decay_t<decltype(decl)>;
				using S = typename D::StateType;
				using BodyStateT = std::remove_cvref_t<
					decltype(D::bodyStateOf(state.template get<S>()))>;
				pushBodyState(decl.bindings.ownBodyId,
				              D::bodyStateOf(state.template get<S>()),
				              D::name,
				              std::is_same_v<BodyStateT, PhysicsBodyState>,
				              id);
			});
		});

}

void FSimulationManagerAsyncCallback::readPushProbeVerdict_Internal()
{
	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	const int32 readFrame = static_cast<int32>(solver.GetCurrentFrame());

	PushProbeTally tally;
	tally.bodies     = m_pushProbeStashBodies;
	tally.unresolved = m_pushProbeStashUnresolved;

	for (const FRewindPushProbeStashedBody& stashed : m_pushProbeStash)
	{
		Chaos::FSingleParticlePhysicsProxy* proxy =
			solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(stashed.bodyId.value)});
		Chaos::FGeometryParticleHandle* handle =
			proxy != nullptr ? proxy->GetHandle_LowLevel() : nullptr;
		if (handle == nullptr)
		{
			++tally.unresolved;
			continue;
		}

		const LivePushProbeRead live = readLiveBodyForPushProbe(*proxy);
		logPushProbeBody(tally, *handle, stashed, live,
		                 m_pushProbeStashFrame, readFrame, m_pushProbeStashSimTick);
	}

	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.PushVerdict] pushFrame=%d readFrame=%d simTick=%u verdict=%s ")
		TEXT("bodies=%d compared=%d inert=%d nonInert=%d nonInertMatched=%d ")
		TEXT("nonInertMismatched=%d unresolved=%d moved=%d eps=%.6f reason=-"),
		m_pushProbeStashFrame, readFrame, m_pushProbeStashSimTick,
		pushProbeVerdictText(tally),
		tally.bodies, tally.compared, tally.inert, tally.nonInert, tally.nonInertMatched,
		tally.nonInertMismatched, tally.unresolved, tally.movedAtPush,
		kDefaultSimilarityEpsilon);

	m_pushProbeStash.Reset();
	m_pushProbeStashFrame = INDEX_NONE;
}

void FSimulationManagerAsyncCallback::discardPushProbeStash(const TCHAR* reason)
{
	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.PushVerdict] pushFrame=%d readFrame=-1 simTick=%u verdict=UNREAD ")
		TEXT("bodies=%d compared=0 inert=0 nonInert=0 nonInertMatched=0 ")
		TEXT("nonInertMismatched=0 unresolved=%d moved=0 eps=%.6f reason=%s"),
		m_pushProbeStashFrame, m_pushProbeStashSimTick,
		m_pushProbeStashBodies, m_pushProbeStashUnresolved,
		kDefaultSimilarityEpsilon, reason);

	m_pushProbeStash.Reset();
	m_pushProbeStashFrame = INDEX_NONE;
}
#endif

ASimulationManagerUImpl* ASimulationManagerUImpl::s_instances[] = {nullptr, nullptr};

ASimulationManagerUImpl::ASimulationManagerUImpl()
{
    bReplicates = false;
}

ASimulationManagerUImpl::~ASimulationManagerUImpl()
{
	// ⛔G-79  docs/SimulationManagerUImpl-guards.md
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(true);
		ISimulationConnectionRelayListener::unregisterInstance(true);
		ISimulationInputRelayListener::unregisterInstance(true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(false);
		ISimulationConnectionRelayListener::unregisterInstance(false);
		ISimulationInputRelayListener::unregisterInstance(false);
	}
}

void ASimulationManagerUImpl::seedRingoutSpawnPointsFromLevel(UWorld& world)
{
	checkf(!m_ringoutSpawnPointsSeeded,
		TEXT("seedRingoutSpawnPointsFromLevel: the ring-out spawn table has already been ")
		TEXT("seeded. It is a ONE-TIME init, and a second pass would merge the level over its ")
		TEXT("own previous answer rather than over the authored defaults."));

	checkf(!m_manager.has_value() && !m_integrationLayer.has_value(),
		TEXT("seedRingoutSpawnPointsFromLevel: the manager or integration layer already ")
		TEXT("exists, so a simulation tick may already have read StaticData::spawnPoints. ")
		TEXT("This seed must run BEFORE the first integrate - move it back above the role ")
		TEXT("branches in BeginPlay rather than relaxing this check."));

	std::vector<brawlerRingout::LevelSpawnPoint> placements;
	for (TActorIterator<APlayerStart> it(&world); it; ++it)
	{
		const APlayerStart* playerStart = *it;
		if (playerStart == nullptr)
			continue;

		brawlerRingout::LevelSpawnPoint placement;
		placement.name     = TCHAR_TO_UTF8(*playerStart->GetName());
		placement.position = uglm::toGLMVec3(playerStart->GetActorLocation());
		placements.push_back(std::move(placement));
	}

	const std::array<glm::vec3, brawlerRingout::kMaxSpawnPoints> authoredFallback =
		m_staticData.m_ringoutStaticData.spawnPoints;

	m_staticData.m_ringoutStaticData.spawnPoints =
		brawlerRingout::spawnPointsFromLevelPlacements(placements, authoredFallback);

	m_ringoutSpawnPointsSeeded = true;

	UE_LOG(LogOGMgmt, Warning,
		TEXT("[Ringout.spawnPoints] seeded from the level: %d APlayerStart actor(s) found, ")
		TEXT("%d slot(s) filled, %d left authored (netMode=%d)"),
		static_cast<int32>(placements.size()),
		FMath::Min(static_cast<int32>(placements.size()),
			static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(brawlerRingout::kMaxSpawnPoints)
			- FMath::Min(static_cast<int32>(placements.size()),
				static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(GetNetMode()));

	for (uint32 slot = 0u; slot < brawlerRingout::kMaxSpawnPoints; ++slot)
	{
		const glm::vec3& point = m_staticData.m_ringoutStaticData.spawnPoints[slot];
		UE_LOG(LogOGMgmt, Warning,
			TEXT("[Ringout.spawnPoints]   slot %u -> (%.2f, %.2f, %.2f) [%s]"),
			slot, point.x, point.y, point.z,
			(slot < placements.size()) ? TEXT("level") : TEXT("authored"));
	}
}

void ASimulationManagerUImpl::BeginPlay()
{
	Super::BeginPlay();

	UWorld* uWorld = GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	const float engineGravityZ = uWorld->GetGravityZ();
	const float simGravityZ    = m_staticData.m_movementStaticData.gravity;
	checkf(FMath::Abs(simGravityZ - engineGravityZ) < 1e-3f,
		TEXT("SimulationManagerUImpl: the movement simulation's gravity (%f cm/s^2) disagrees with ")
		TEXT("the engine's (%f cm/s^2). The sim's value is read from UPhysicsSettings::DefaultGravityZ ")
		TEXT("at construction; this world overrides gravity in its WorldSettings. Either clear that ")
		TEXT("override or teach the movement StaticData about per-level gravity."),
		simGravityZ, engineGravityZ);

	seedRingoutSpawnPointsFromLevel(*uWorld);

	const float dt = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
	checkf(FMath::Abs(dt * static_cast<float>(UEBrawlerNetConfig::tickFrequencyHz) - 1.f) < 1e-4f,
		TEXT("SimulationManagerUImpl: UPhysicsSettings::AsyncFixedTimeStepSize (%f s) is not one tick of ")
		TEXT("UEBrawlerNetConfig::tickFrequencyHz (%d Hz). The manager's step dt and the tick rate the ")
		TEXT("per-connection templates are compiled for must agree. Was design D13 (task 18)."),
		dt, UEBrawlerNetConfig::tickFrequencyHz);

#if OG_PHYSICS_BACKEND_CHAOS
	FPhysScene* physScene = uWorld->GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	checkf(static_cast<float>(solver->GetAsyncDeltaTime()) == dt,
		TEXT("SimulationManagerUImpl: the Chaos solver's async dt (%f s) differs from ")
		TEXT("UPhysicsSettings::AsyncFixedTimeStepSize (%f s), which the manager now steps with. Was ")
		TEXT("design D13: the two are the same number."),
		static_cast<float>(solver->GetAsyncDeltaTime()), dt);
#endif

	const ENetMode worldNetMode = GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);

	createLatencyBudget(worldIsAuthority);
	m_latencyPostTickFlushHandle =
		uWorld->OnPostTickFlush().AddUObject(this, &ASimulationManagerUImpl::onLatencyPostTickFlush);

	int32 configuredRelayDelayFloorTicks = -1;
	// ⛔G-57  docs/SimulationManagerUImpl-guards.md
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniFloorTicks = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GEngineIni))
		{
			configuredRelayDelayFloorTicks = iniFloorTicks;
		}
	}

	// ⛔G-56  docs/SimulationManagerUImpl-guards.md

	int32 configuredCorrectionRotationK = -1;
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniK = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GEngineIni))
		{
			configuredCorrectionRotationK = iniK;
		}
	}

	bool    hasIniResimTriggerPolicy = false;
	FString configuredResimTriggerPolicy;
	// ⛔G-58  docs/SimulationManagerUImpl-guards.md
	if (GConfig != nullptr)
	{
		FString iniPolicy;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GEngineIni))
		{
			hasIniResimTriggerPolicy = true;
			configuredResimTriggerPolicy = iniPolicy.TrimStartAndEnd();
		}
	}

#if !OG_PHYSICS_BACKEND_CHAOS
	decideJoltSteppingMode(worldIsAuthority);
	buildJoltWorld(*uWorld, worldIsAuthority);
#endif

	if (worldIsAuthority)
	{
		if (s_instances[0] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[0] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/true, this);
		auto pctmloggerServer = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogServer = [](const char* msg) {
			FString fmsg(msg);
			if (fmsg.StartsWith(TEXT("[Verbose]")))
			{
				UE_LOG(LogOGBrawler, Verbose, TEXT("%s"), *fmsg);
			}
			else if (fmsg.StartsWith(TEXT("[Warning]")))
			{
				UE_LOG(LogOGBrawler, Warning, TEXT("%s"), *fmsg);
			}
			else
			{
				UE_LOG(LogOGBrawler, Log, TEXT("%s"), *fmsg);
			}
		};
#if OG_PHYSICS_BACKEND_CHAOS
		Chaos::FPBDRigidsSolver& rigidsSolverS = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverS);
		m_physReaderAdapter.emplace(rigidsSolverS);
		emplaceBrawlerQueryAdapter(m_queryAdapter, uWorld);
#endif
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(false, dt, ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

		// ⛔G-59  docs/SimulationManagerUImpl-guards.md
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		if (configuredRelayDelayFloorTicks >= 0)
		{
			const int32 clampedFloor = clampRelayDelayFloorTicks(
				configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
			if (clampedFloor != configuredRelayDelayFloorTicks)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"),
					configuredRelayDelayFloorTicks, clampedFloor);
			}
			m_manager->setRelayDelayFloorTicks(clampedFloor);
			UE_LOG(LogOGNet, Log,
				TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), clampedFloor);

			logRelayDelayFloorAdvisory(clampedFloor);
		}

		if (configuredCorrectionRotationK != -1)
		{
			const int32 clampedK = correctionRotation::clampK(configuredCorrectionRotationK);
			if (clampedK != configuredCorrectionRotationK)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"),
					configuredCorrectionRotationK, clampedK);
			}
			m_manager->setCorrectionRotationK(clampedK);
		}

		const int32 sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
		UE_LOG(LogOGNet, Warning,
			TEXT("[StateRotation] session K = %d (%s)"),
			sessionCorrectionRotationK,
			configuredCorrectionRotationK != -1 ? TEXT("ini override") : TEXT("compiled default"));

		const int32 sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->setRelayDelayFloorTicks((uint8)sessionRelayDelayFloorTicks);
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[RelayDelayFloor] no timing relay at manager BeginPlay — session floor %d NOT published"),
				sessionRelayDelayFloorTicks);
		}

		// ⛔G-61  docs/SimulationManagerUImpl-guards.md
		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/true, this);
			ISimulationInputRelayListener::registerInstance(/*isAuthority=*/true, this);

		m_receptionCoordinator.emplace(m_manager->getTimeConfig());
		m_receptionCoordinator->setLogger(std::function<void(const char*)>(pctmloggerServer));
		simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogServer));
		bindCorrectionFieldDiffGate();
	}
	else
	{
		if (s_instances[1] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[1] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/false, this);
		auto pctmlogger = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogClient = [](const char* msg) {
			FString fmsg(msg);
			if (fmsg.StartsWith(TEXT("[Verbose]")))
			{
				UE_LOG(LogOGBrawler, Verbose, TEXT("%s"), *fmsg);
			}
			else if (fmsg.StartsWith(TEXT("[Warning]")))
			{
				UE_LOG(LogOGBrawler, Warning, TEXT("%s"), *fmsg);
			}
			else
			{
				UE_LOG(LogOGBrawler, Log, TEXT("%s"), *fmsg);
			}
		};
#if OG_PHYSICS_BACKEND_CHAOS
		Chaos::FPBDRigidsSolver& rigidsSolverC = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverC);
		m_physReaderAdapter.emplace(rigidsSolverC);
		emplaceBrawlerQueryAdapter(m_queryAdapter, uWorld);
#endif
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(/*usePrediction=*/true, dt, ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

		// ⛔G-60  docs/SimulationManagerUImpl-guards.md
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();

		// ⛔G-62  docs/SimulationManagerUImpl-guards.md
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);
		ISimulationInputRelayListener::registerInstance(/*isAuthority=*/false, this);
		if (ASimulationConnectionRelay* connectionRelay =
				ASimulationConnectionRelay::findLocalClientRelay(uWorld))
		{
			connectionRelay->replayLatchedTier();
		}

		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->replayLatchedRelayDelayFloor();
		}
		simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogClient));
		bindCorrectionFieldDiffGate();

#if !NO_LOGGING
		UE_LOG(LogOGResimProbe, Warning,
			TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
			ToString(LogOGResimProbe.GetVerbosity()),
			LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
			static_cast<uint32>(kResimGateProbeWindowSamples));
#endif
	}

	// ⛔G-63  docs/SimulationManagerUImpl-guards.md
	{
		if (hasIniResimTriggerPolicy)
		{
			if (configuredResimTriggerPolicy.Equals(TEXT("OnDisagreement"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
			}
			else if (configuredResimTriggerPolicy.Equals(TEXT("FrontierExact"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
			}
			else
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
					TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"),
					*configuredResimTriggerPolicy);
				hasIniResimTriggerPolicy = false;
			}
		}

		const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
		const bool policyIsOnDisagreement =
			sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
		const FString depthPolicyText = policyIsOnDisagreement
			? FString::Printf(TEXT("skip beyond %d ticks"), sessionTimeConfig.rollbackWindowTicks)
			: FString(TEXT("off (inert under FrontierExact)"));
		UE_LOG(LogOGNet, Warning,
			TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
			policyIsOnDisagreement ? TEXT("OnDisagreement") : TEXT("FrontierExact"),
			hasIniResimTriggerPolicy ? TEXT("ini override") : TEXT("compiled default"),
			*depthPolicyText);
	}

	checkf(m_ringoutSpawnPointsSeeded,
		TEXT("SimulationManagerUImpl::BeginPlay: the ring-out spawn table was not seeded on this peer ")
		TEXT("(netMode=%d). Every peer reads StaticData::spawnPoints - a client predicts its own ")
		TEXT("respawn - so seedRingoutSpawnPointsFromLevel must not be role-gated. Was the NOT ")
		TEXT("ROLE-GATED fence at the seed call."),
		static_cast<int32>(worldNetMode));

	{
		const std::string_view fingerprint = buildIdentityUImpl::backendFingerprint();
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Backend] backend=%hs fingerprint=%.*hs"),
			physicsBackendUImpl::kBackendName, static_cast<int32>(fingerprint.size()), fingerprint.data());
	}

#if OG_PHYSICS_BACKEND_CHAOS
	m_hysScenePostTickCallbackHandle = physScene->OnPhysScenePostTick.AddWeakLambda(
		this, [this](FChaosScene*) { OnPostPhysicsStep(); });

	m_asyncCallback = solver->CreateAndRegisterSimCallbackObject_External<FSimulationManagerAsyncCallback>();
	m_asyncCallback->setManager(this);

	FNetworkPhysicsCallback* solverCallback = static_cast<FNetworkPhysicsCallback*>(solver->GetRewindCallback());
	if (solverCallback == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	m_injectInputsExternalCallbackHandle = solverCallback->InjectInputsExternal.AddUObject(this, &ASimulationManagerUImpl::InjectInputs_External);
#else
	startJoltStepping(*uWorld, dt, worldIsAuthority);
#endif
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

void ASimulationManagerUImpl::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
#if !OG_PHYSICS_BACKEND_CHAOS
	m_frameHost.unregisterTickFunctions();
	// ⛔G-80  docs/SimulationManagerUImpl-guards.md
	m_frameHost.waitForOutstandingSteps();
#endif

	// ⛔G-64  docs/SimulationManagerUImpl-guards.md
	m_delayedInputComponentsById.clear();
	m_receptionCoordinator.reset();

	m_replicatedTierConsumer.reset();

	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/false);
	}

	if (UWorld* World = GetWorld())
	{
		if (m_latencyPostTickFlushHandle.IsValid())
		{
			World->OnPostTickFlush().Remove(m_latencyPostTickFlushHandle);
			m_latencyPostTickFlushHandle.Reset();
		}

#if OG_PHYSICS_BACKEND_CHAOS
		if (FPhysScene* PhysScene = World->GetPhysicsScene())
		{
			if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
			{
				if (m_injectInputsExternalCallbackHandle.IsValid())
				{
					if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
					{
						SolverCallback->InjectInputsExternal.Remove(m_injectInputsExternalCallbackHandle);
					}
				}

				if (m_hysScenePostTickCallbackHandle.IsValid())
				{
					PhysScene->OnPhysScenePostTick.Remove(m_hysScenePostTickCallbackHandle);
				}

				if (m_asyncCallback)
				{
					Solver->UnregisterAndFreeSimCallbackObject_External(m_asyncCallback);
				}
			}
		}
#endif
	}

	Super::EndPlay(EndPlayReason);

}

void ASimulationManagerUImpl::onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
{
    // ⛔G-65  docs/SimulationManagerUImpl-guards.md
    const bool hadAnyTier =
        m_replicatedTierConsumer.has_value() && m_replicatedTierConsumer->hasReceivedTier();

    applyReplicatedConnectionTier(newTier);

    applyTierTransitionStall(oldTier, newTier, hadAnyTier);
}

void ASimulationManagerUImpl::onConnectionTierReplayed(uint8_t tier)
{
    applyReplicatedConnectionTier(tier);
}

void ASimulationManagerUImpl::applyReplicatedConnectionTier(uint8 tier)
{
    if (!m_replicatedTierConsumer.has_value())
        return;

    m_replicatedTierConsumer->onReplicatedTierReceived((int32)tier);
    recomputeAndPublishEffectiveInputDelay();
}

void ASimulationManagerUImpl::onInputRelayHostReady(ASimulationInputRelay& host)
{
    AActor* ownerActor = host.GetOwner();
    if (ownerActor == nullptr)
        return;

    AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(ownerActor);
    if (character == nullptr)
        return;

    if (USimmableUpdateComponent* component =
            character->FindComponentByClass<USimmableUpdateComponent>())
    {
        component->attachInputRelayHost(&host);
    }
}

void ASimulationManagerUImpl::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/true);
}

void ASimulationManagerUImpl::onRelayDelayFloorReplayed(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/false);
}

void ASimulationManagerUImpl::applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease)
{
    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 clampedFloor = clampRelayDelayFloorTicks((int32)floorTicks, cfg);
    if (clampedFloor != (int32)floorTicks)
    {
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
            (unsigned int)floorTicks, clampedFloor);
    }

    m_manager->setRelayDelayFloorTicks(clampedFloor);

    logRelayDelayFloorAdvisory(clampedFloor);

    const int32 deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

    if (!payForIncrease || deltaDelayTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor = %d ticks, effective delay delta=%d, no stall"),
            clampedFloor, deltaDelayTicks);
        return;
    }

    requestInputDelayIncreaseStall(deltaDelayTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[RelayDelayFloor] floor = %d ticks, UPWARD, requesting %d-tick prediction stall"),
        clampedFloor, deltaDelayTicks);
}

void ASimulationManagerUImpl::logRelayDelayFloorAdvisory(int32 floorTicks)
{
    if (!m_manager.has_value())
        return;

    // ⛔G-66  docs/SimulationManagerUImpl-guards.md
    switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
    {
    case RelayDelayFloorAdvisory::BelowHiccupBaseline:
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) — the one value that is neither"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::UniformDFairnessActive:
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active; tiers and LAN override are inert"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::None:
        break;
    }
}

int32 ASimulationManagerUImpl::recomputeAndPublishEffectiveInputDelay()
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

    const int32 delayTicks = (int32)m_replicatedTierConsumer->effectiveInputDelayTicks();
    const int32 deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

    m_lastPublishedEffectiveInputDelayTicks = delayTicks;
    publishClientEffectiveInputDelayTicks(delayTicks);

    UE_LOG(LogOGNet, Log,
        TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
        delayTicks,
        (int)m_replicatedTierConsumer->currentTierIndex(),
        m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
        m_manager.has_value() ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

    return deltaDelayTicks;
}

void ASimulationManagerUImpl::applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier)
{
    if (oldTier == newTier)
        return;

    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 stallTicks = (int32)shouldStallForTierTransition(
        (int32)oldTier, (int32)newTier, hadAnyTier, cfg);

    if (stallTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[ConnectionTier] tier %u -> %u hadAnyTier=%d, no stall"),
            (unsigned int)oldTier, (unsigned int)newTier, hadAnyTier ? 1 : 0);
        return;
    }

    requestInputDelayIncreaseStall(stallTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[ConnectionTier] tier %u -> %u UPWARD, requesting %d-tick prediction stall"),
        (unsigned int)oldTier, (unsigned int)newTier, stallTicks);
}

ASimulationTimingRelay* ASimulationManagerUImpl::findTimingRelay()
{
    if (m_timingRelay) return m_timingRelay;
    for (TActorIterator<ASimulationTimingRelay> it(GetWorld()); it; ++it)
    {
        m_timingRelay = *it;
        UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: timing relay found"));
        return m_timingRelay;
    }
    return nullptr;
}

FSmallSimulationStateSyncBuffer& ASimulationManagerUImpl::getSyncedTimingBuffer()
{
    ASimulationTimingRelay* relay = findTimingRelay();
    checkf(relay != nullptr, TEXT("SimulationManagerUImpl::getSyncedTimingBuffer: relay not found"));
    return relay->editBuffer();
}

void ASimulationManagerUImpl::onPostSimulationGameThread()
{
    m_manager->onPostSimulationGameThread();
}

namespace
{
    void pushRingoutScoresToCharacters(
        const brawlerRingout::ScoreSystem& scoreSystem,
        const std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>& routes)
    {
        for (const auto& entry : routes)
        {
            const unsigned int id = entry.first;

            USimmableUpdateComponent* component = entry.second.Get();
            if (component == nullptr)
                continue;

            AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(component->GetOwner());
            if (character == nullptr)
                continue;

            checkf(scoreSystem.hasScoreEntry(id),
                   TEXT("Ring-out score push: id=%u is in the authority route table but has no ")
                   TEXT("ScoreSystem roster entry. The game-thread read of the score table is ")
                   TEXT("safe only because the roster is seeded by onCharacterRegistered before ")
                   TEXT("tryRegister returns Ready - an unseeded id lets postIntegrate's ")
                   TEXT("operator[] INSERT, and rehash, on the physics thread under this walk."),
                   id);

            character->SetAuthoritativeRingoutScore(
                static_cast<int32>(scoreSystem.scoreOf(id)));
        }
    }
}

void ASimulationManagerUImpl::OnPostPhysicsStep()
{
#if !UE_BUILD_SHIPPING
	if (const int32 hitchMs = CVarTestGameThreadHitchMs.GetValueOnGameThread(); hitchMs > 0)
	{
		CVarTestGameThreadHitchMs->Set(0, ECVF_SetByConsole);
		FPlatformProcess::Sleep(static_cast<float>(hitchMs) / 1000.f);
	}
#endif

#if !OG_PHYSICS_BACKEND_CHAOS
	syncRenderPoses_GameThread();
#endif

	onPostSimulationGameThread();

	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}

#if OG_PHYSICS_BACKEND_CHAOS
	updateVisualizationAll(m_storage);
#else
	updateVisualizationAtRenderPoses_GameThread();
#endif

	if (m_manager.has_value() && m_systemsExec.has_value() && !m_manager->runsPrediction())
	{
		pushRingoutScoresToCharacters(m_systemsExec->get<brawlerRingout::ScoreSystem>(),
			m_delayedInputComponentsById);
	}

	tickLatencyBudget_GameThread();
}

physicsBackendUImpl::VizQuery& ASimulationManagerUImpl::editVizQuery()
{
#if OG_PHYSICS_BACKEND_CHAOS
	return m_queryAdapter.value();
#else
	if (m_joltStepsInline)
	{
#if !UE_BUILD_SHIPPING
		++m_shadowWindow.vizQueryStep;
#endif
		return m_queryAdapter.value();
	}
#if !UE_BUILD_SHIPPING
	++m_shadowWindow.vizQueryShadow;
#endif
	return m_shadowQuery.value();
#endif
}

const physicsBackendUImpl::VizReader& ASimulationManagerUImpl::getVizReader() const
{
#if OG_PHYSICS_BACKEND_CHAOS
	return m_physReaderAdapter.value();
#else
	if (m_joltStepsInline)
	{
#if !UE_BUILD_SHIPPING
		++m_shadowWindow.vizReaderStep;
#endif
		return m_physReaderAdapter.value();
	}
#if !UE_BUILD_SHIPPING
	++m_shadowWindow.vizReaderShadow;
#endif
	return m_shadowReader.value();
#endif
}

QueryVolumeId ASimulationManagerUImpl::registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
{
#if OG_PHYSICS_BACKEND_CHAOS
	FCollisionQueryParams queryParams;
	queryParams.bTraceComplex = false;
	queryParams.AddIgnoredActor(&owner);
	return m_queryAdapter->registerVolume(descriptor, queryParams, FActorInstanceHandle(&owner));
#else
	const uint32 slot = acquireJoltSlot(owner);
	if (!m_joltStepsInline)
		return m_shadowQuery->registerVolume(descriptor, joltSlotRootBodyId(*m_shadowWorld, slot));

	return m_queryAdapter->registerVolume(descriptor, joltSlotRootBodyId(*m_joltWorld, slot));
#endif
}

#if !OG_PHYSICS_BACKEND_CHAOS
void ASimulationManagerUImpl::decideJoltSteppingMode(bool worldIsAuthority)
{
	m_joltStepsInline = worldIsAuthority || CVarSimRunInline.GetValueOnGameThread() != 0;
	m_joltRoleBit     = worldIsAuthority ? kJoltAuthorityRoleBit : kJoltClientRoleBit;

	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Mode] role=%s stepping=%s viz=%s"),
		worldIsAuthority ? TEXT("Authority") : TEXT("Client"),
		m_joltStepsInline ? TEXT("inline") : TEXT("worker"),
		m_joltStepsInline ? TEXT("stepWorld") : TEXT("shadowWorld"));
}

void ASimulationManagerUImpl::buildJoltWorld(UWorld& world, bool worldIsAuthority)
{
	m_joltRuntime.emplace(&logJoltToSimHost);

	const UEStaticImportResult import = UEStaticGeometryImporter(brawlerCategoryChannels()).importWorld(world);
	UEStaticGeometryImporter::logReport(import.report, worldIsAuthority ? TEXT("authority") : TEXT("client"));

	StaticWorldDescription queryableStatics;
	StaticWorldDescription physicsOnlyStatics;
	for (std::size_t index = 0; index < import.description.shapes.size(); ++index)
	{
		StaticWorldDescription& target = import.physicsOnly[index] != 0u ? physicsOnlyStatics : queryableStatics;
		target.shapes.push_back(import.description.shapes[index]);
	}

	m_joltSlotTemplate = brawlerSlotTemplate();
	const auto rootTemplate = std::find_if(m_joltSlotTemplate.begin(), m_joltSlotTemplate.end(),
		[](const SlotBodyTemplate& body) { return body.descriptor.body.isRoot; });
	checkf(rootTemplate != m_joltSlotTemplate.end()
			&& std::none_of(rootTemplate + 1, m_joltSlotTemplate.end(),
				[](const SlotBodyTemplate& body) { return body.descriptor.body.isRoot; }),
		TEXT("buildJoltWorld: the brawler's physics composite must declare exactly one isRoot body (the capsule the ")
		TEXT("render apply moves)."));
	m_joltRootDeclarationIndex = static_cast<uint8>(rootTemplate - m_joltSlotTemplate.begin());

	JoltWorldConfig config;
	config.simulatableSlots = kMaxSimulatableSlots;
	config.slotTemplate     = m_joltSlotTemplate;
	config.staticLayers     = staticLayerKeysOf(queryableStatics);
	const std::vector<JoltLayerKey> physicsOnlyLayers = staticLayerKeysOf(physicsOnlyStatics);
	config.staticLayers.insert(config.staticLayers.end(), physicsOnlyLayers.begin(), physicsOnlyLayers.end());
	config.ringDepthTicks   = worldIsAuthority ? 0u : static_cast<uint32_t>(TimeConfig{}.rollbackWindowHardCap) + 2u;
	config.gravityCmPerS2   = glm::vec3(0.f, 0.f, m_staticData.m_movementStaticData.gravity);

	std::vector<JPH::BodyID> stepStaticBodies;
	{
		JoltWorldLockScopeUImpl worldLock(m_joltWorldMutex, m_joltRoleBit);

		m_joltWorld.emplace(m_joltRuntime->runtime, config, &logJoltToSimHost);

		JoltStaticWorldBuilder queryableBuilder(*m_joltWorld, &logJoltToSimHost);
		queryableBuilder.build(queryableStatics);
		JoltStaticWorldBuilder physicsOnlyBuilder(*m_joltWorld, &logJoltToSimHost);
		physicsOnlyBuilder.build(physicsOnlyStatics);

		m_physAdapter.emplace(*m_joltWorld);
		m_physReaderAdapter.emplace(*m_joltWorld, m_physAdapter->bindTable());
		m_queryAdapter.emplace(*m_joltWorld, &logJoltToSimHost, JoltSpatialQueryConfig{
			brawlerMappedCategories(),
			joltWorldAccessCheckFor(m_joltRoleBit, m_joltStepsInline) });
		m_queryAdapter->excludeStaticBodiesFromQueries(physicsOnlyBuilder.stats().bodies);

		stepStaticBodies = queryableBuilder.stats().bodies;
		stepStaticBodies.insert(stepStaticBodies.end(), physicsOnlyBuilder.stats().bodies.begin(),
			physicsOnlyBuilder.stats().bodies.end());

		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.World] slots=%u bodiesPerSlot=%u ringDepthTicks=%u statics=%d physicsOnlyStatics=%d staticBodies=%d physicsOnlyBodies=%d"),
			m_joltWorld->simulatableSlots(), m_joltWorld->bodiesPerSlot(), config.ringDepthTicks,
			static_cast<int32>(queryableStatics.shapes.size()), static_cast<int32>(physicsOnlyStatics.shapes.size()),
			static_cast<int32>(queryableBuilder.stats().bodies.size()),
			static_cast<int32>(physicsOnlyBuilder.stats().bodies.size()));
	}

	if (!m_joltStepsInline)
		buildShadowWorld(config, queryableStatics, physicsOnlyStatics, stepStaticBodies);
}

void ASimulationManagerUImpl::buildShadowWorld(const JoltWorldConfig& stepConfig,
	const StaticWorldDescription& queryableStatics, const StaticWorldDescription& physicsOnlyStatics,
	const std::vector<JPH::BodyID>& stepStaticBodies)
{
	checkf(m_joltWorld->ring().slotBytes() == JoltWorldConfig{}.ringSlotBytes,
		TEXT("buildShadowWorld: the step world's ring slots hold %u bytes, but the shadow hand-over's pooled slots ")
		TEXT("are pre-reserved to JoltWorldConfig's default %u. Keep the two equal, or a hand-over allocates."),
		m_joltWorld->ring().slotBytes(), JoltWorldConfig{}.ringSlotBytes);

	JoltWorldConfig shadowConfig = stepConfig;
	shadowConfig.ringDepthTicks     = 0u;
	shadowConfig.tempAllocatorBytes = kShadowTempAllocatorBytes;

	// ⛔G-82  docs/SimulationManagerUImpl-guards.md
	m_shadowWorld.emplace(m_joltRuntime->runtime, shadowConfig, &logJoltToSimHost);

	JoltStaticWorldBuilder queryableBuilder(*m_shadowWorld, &logJoltToSimHost);
	queryableBuilder.build(queryableStatics);
	JoltStaticWorldBuilder physicsOnlyBuilder(*m_shadowWorld, &logJoltToSimHost);
	physicsOnlyBuilder.build(physicsOnlyStatics);

	m_shadowBodyAdapter.emplace(*m_shadowWorld);
	m_shadowReader.emplace(*m_shadowWorld, m_shadowBodyAdapter->bindTable());
	m_shadowQuery.emplace(*m_shadowWorld, &logJoltToSimHost, JoltSpatialQueryConfig{
		brawlerMappedCategories(),
		&joltShadowAccessOnGameThread });
	m_shadowQuery->excludeStaticBodiesFromQueries(physicsOnlyBuilder.stats().bodies);

	std::vector<JPH::BodyID> shadowStaticBodies = queryableBuilder.stats().bodies;
	shadowStaticBodies.insert(shadowStaticBodies.end(), physicsOnlyBuilder.stats().bodies.begin(),
		physicsOnlyBuilder.stats().bodies.end());

	const uint32 stepBodies =
		m_joltWorld->simulatableSlots() * m_joltWorld->bodiesPerSlot() + static_cast<uint32>(stepStaticBodies.size());
	const uint32 shadowBodies =
		m_shadowWorld->simulatableSlots() * m_shadowWorld->bodiesPerSlot() + static_cast<uint32>(shadowStaticBodies.size());
	const bool sameStatics = shadowStaticBodies == stepStaticBodies;
	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Shadow] built bodies step=%u shadow=%u sameStatics=%d slots=%u bodiesPerSlot=%u ringDepthTicks=%u tempAllocatorBytes=%u"),
		stepBodies, shadowBodies, sameStatics ? 1 : 0, m_shadowWorld->simulatableSlots(), m_shadowWorld->bodiesPerSlot(),
		shadowConfig.ringDepthTicks, shadowConfig.tempAllocatorBytes);
#if !UE_BUILD_SHIPPING
	checkf(stepBodies == shadowBodies && sameStatics,
		TEXT("buildShadowWorld: the shadow world holds %u bodies and the step world %u (same static bodies: %d). The ")
		TEXT("shadow must be built by the same calls, in the same order, from the same configuration except the ring ")
		TEXT("depth and the temp allocator (UImpl G-82); its restore checks only the slot bodies, so a missing or extra ")
		TEXT("static would go unnoticed there."),
		shadowBodies, stepBodies, sameStatics ? 1 : 0);
#endif
}

uint32 ASimulationManagerUImpl::acquireJoltSlot(const AActor& owner)
{
	if (const auto found = m_joltSlotByOwner.find(&owner); found != m_joltSlotByOwner.end())
		return found->second;

	std::array<bool, kMaxSimulatableSlots> taken{};
	for (const auto& entry : m_joltSlotByOwner)
		taken[entry.second] = true;

	const auto freeSlot = std::find(taken.begin(), taken.end(), false);
	checkf(freeSlot != taken.end(),
		TEXT("acquireJoltSlot: all %u Jolt slots are taken; '%s' cannot get one. The Jolt world has a fixed ")
		TEXT("body set of kMaxSimulatableSlots characters (design D7, task 10)."),
		kMaxSimulatableSlots, *owner.GetName());

	const uint32 slot = static_cast<uint32>(freeSlot - taken.begin());
	m_joltSlotByOwner.emplace(&owner, slot);
	return slot;
}

void ASimulationManagerUImpl::releaseJoltSlot(const AActor& owner)
{
	const auto found = m_joltSlotByOwner.find(&owner);
	if (found == m_joltSlotByOwner.end())
		return;

	const uint32 slot = found->second;
	m_frameHost.pushOccupancy_GameThread(slot, false);
	m_physAdapter->bindTable().releaseSlot(slot);
	m_joltSlotByOwner.erase(found);

	UE_LOG(LogOGSimHost, Log, TEXT("[SimHost.Bind] release slot=%u boundBodies=%u slotsInUse=%d"),
		slot, boundJoltBodyCount(), static_cast<int32>(m_joltSlotByOwner.size()));

	if (m_shadowWorld.has_value())
		releaseShadowSlot(slot);
}

BodyId ASimulationManagerUImpl::joltSlotRootBodyId(const JoltWorld& world, uint32 slot) const
{
	for (uint32 templateIndex = 0; templateIndex < m_joltSlotTemplate.size(); ++templateIndex)
	{
		if (m_joltSlotTemplate[templateIndex].descriptor.body.isRoot)
			return JoltPhysicsBodyAdapter::bodyIdOf(world.slotBodyId(slot, templateIndex));
	}
	checkf(false, TEXT("joltSlotRootBodyId: the slot template has no isRoot body"));
	return BodyId{};
}

uint32 ASimulationManagerUImpl::boundJoltBodyCount() const
{
	uint32 bound = 0u;
	for (uint32 slot = 0; slot < m_joltWorld->simulatableSlots(); ++slot)
	{
		for (uint32 templateIndex = 0; templateIndex < m_joltWorld->bodiesPerSlot(); ++templateIndex)
		{
			if (m_physAdapter->bindTable().isBound(
					JoltPhysicsBodyAdapter::bodyIdOf(m_joltWorld->slotBodyId(slot, templateIndex))))
				++bound;
		}
	}
	return bound;
}

void ASimulationManagerUImpl::startJoltStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	checkf(world.PersistentLevel != nullptr, TEXT("startJoltStepping: the world has no persistent level."));

	{
		JoltWorldLockScopeUImpl worldLock(m_joltWorldMutex, m_joltRoleBit);
		m_stepHooks.emplace(m_frameHost);
		m_stepDriver.emplace(*m_manager, *m_joltWorld, *m_integrationLayer, *m_stepHooks,
			StepDriverConfig{ dt, ResimPolicy::OnRequest, 0u });
	}

	m_frameHost.begin(*this, *world.PersistentLevel,
		SimulationFrameHostUImpl::Config{ static_cast<double>(dt), worldIsAuthority, m_joltStepsInline });
}

void ASimulationManagerUImpl::runJoltStep_Step(uint64 physicsStep, double stepDeadlineSeconds)
{
	checkf(m_frameHost.isRunningBatchOnThisThread(),
		TEXT("runJoltStep_Step: called outside the frame host's step batch. It is the step driver's only door to ")
		TEXT("runTick: the frame host's batch is the one caller, so the steps stay serialized in the scheduler's ")
		TEXT("order and each step's deadline and offset belong to the batch the game thread dispatched."));
	checkf(!m_joltStepsInline || IsInGameThread(),
		TEXT("runJoltStep_Step: an inline step ran off the game thread. In the inline mode (the authority, and a client ")
		TEXT("under og.Sim.RunInline) the game thread owns the step driver: the authority's input release delivers into ")
		TEXT("UObjects, and the inline access predicates accept the game thread."));
	JoltWorldLockScopeUImpl worldLock(m_joltWorldMutex, m_joltRoleBit);
	JoltStepScopeUImpl      stepScope(m_joltRoleBit);
	m_stepHooks->currentStepDeadline = stepDeadlineSeconds;
	m_stepDriver->runTick(physicsStep);
}

bool ASimulationManagerUImpl::isJoltWorldMutexHeldOnThisThread() const
{
	return (t_joltWorldMutexHeldRoles & m_joltRoleBit) != 0u;
}

bool ASimulationManagerUImpl::isJoltStepRunningOnThisThread() const
{
	return (t_joltStepRunningRoles & m_joltRoleBit) != 0u;
}

void ASimulationManagerUImpl::publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds)
{
	static_assert(RenderSnapshot::kMaxBodies
			== kMaxSimulatableSlots * physicsDeclarationCountOf<BrawlerPhysicsComposite>::value,
		"RenderSnapshot holds one body per declaration of the brawler's physics composite in every slot: "
		"kMaxSimulatableSlots x 6. A declaration added to or removed from SimulatableBrawler's physics composite must "
		"change the alias's 6 with it, or fillRenderSnapshot drops the excess bodies. Was design D section 8's "
		"'MaxBodies = kMaxSimulatableSlots (8) x 6 = 48'.");
	static_assert(std::is_trivially_copyable_v<RenderSnapshot>,
		"RenderSnapshot must stay a flat value with no heap-owning member: the J5 channel's four slots are built with "
		"the manager and filled in place on every step, which allocates nothing only while the snapshot owns no heap "
		"storage. Was task 55's 'no allocation on the publish path after warm-up'.");

	RenderSnapshot* const snapshot = m_renderSnapshots.beginWrite();
	checkf(snapshot != nullptr,
		TEXT("publishRenderSnapshot_Step: the render snapshot channel (J5) has no free slot. The game thread holds at ")
		TEXT("most one snapshot (the newest, for one apply), and a 4-slot channel keeps a free slot for up to two."));
	if (snapshot == nullptr)
		return;

	fillRenderSnapshot(*snapshot, m_storage, outcome, stepDeadlineSeconds);
	m_renderSnapshots.commit();
}

bool ASimulationManagerUImpl::buildRenderPoses_GameThread()
{
	using RenderChannel = decltype(m_renderSnapshots);
	static_assert(kRenderMaxInterpolationDelaySteps <= static_cast<double>(RenderChannel::kCapacity - 2),
		"og.Render.InterpolationDelaySteps is clamped to what the J5 channel can bracket: the step before the render time is "
		"at most floor(delay) + 1 snapshots behind the newest, so a delay of N - 2 steps is the most an N-slot channel "
		"reaches. Raise the channel's N with the clamp (task 19).");

	RenderInterpolationParams params;
	params.newestOnly = CVarRenderNewestSnapshot.GetValueOnGameThread() != 0;
	params.snapDistanceCm = kRenderParkSnapCm;
	const double delaySteps = FMath::Clamp(static_cast<double>(CVarRenderInterpolationDelaySteps.GetValueOnGameThread()),
		0.0, kRenderMaxInterpolationDelaySteps);
	params.renderTimeSeconds =
		m_frameHost.hostNowSeconds_GameThread() - delaySteps * m_frameHost.stepIntervalSeconds_GameThread();

	RenderSnapshot* nextCopy = nullptr;
#if !UE_BUILD_SHIPPING
	if (CVarRenderTraceFrames.GetValueOnGameThread() != 0)
		nextCopy = &m_renderTraceNext;
#endif
	if (!interpolateRenderPoses(m_renderSnapshots, params, m_renderPoses, m_renderFrame, nextCopy))
		return false;

	m_renderPosesValid = true;
	return true;
}

void ASimulationManagerUImpl::applyRenderSnapshot_GameThread()
{
#if !UE_BUILD_SHIPPING
	++m_renderApplyWindow.frames;
	checkRenderTargetsHeld_GameThread();
#endif

	if (buildRenderPoses_GameThread())
	{
		for (auto& [id, target] : m_renderTargetsById)
		{
			AOGBrawlerUECharacter* const character = target.character.Get();
			const RenderBody* const body = findRenderBody(m_renderPoses, id, m_joltRootDeclarationIndex);
			if (character == nullptr || body == nullptr)
				continue;

			const FVector location = uglm::toFVector(body->positionCm);
			const FQuat rotation = body->hasRotation != 0u ? uglm::toFQuat(body->rotation) : character->GetActorQuat();
#if !UE_BUILD_SHIPPING
			const FVector before = character->GetActorLocation();
#endif
			// ⛔G-83  docs/SimulationManagerUImpl-guards.md
			character->SetActorLocationAndRotation(location, rotation, /*bSweep*/ false, /*OutSweepHitResult*/ nullptr,
				ETeleportType::TeleportPhysics);

#if !UE_BUILD_SHIPPING
			if (target.applied)
			{
				const double displacementCm = FVector::Dist(target.appliedCm, character->GetActorLocation());
				m_renderApplyWindow.pathCm += displacementCm;
				m_renderApplyWindow.frameDisplacementCm.push_back(static_cast<float>(displacementCm));
			}
			else
			{
				UE_LOG(LogOGSimHost, Log,
					TEXT("[SimHost.RenderApply] first role=%s id=%u from=(%.3f,%.3f,%.3f) to=(%.3f,%.3f,%.3f) physicsStep=%llu tick=%u"),
					runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, before.X, before.Y, before.Z,
					location.X, location.Y, location.Z, static_cast<unsigned long long>(m_renderPoses.physicsStep),
					static_cast<uint32>(m_renderPoses.tick));
			}
#endif
			target.appliedCm = character->GetActorLocation();
			target.applied   = true;
		}

#if !UE_BUILD_SHIPPING
		checkRenderApply_GameThread();
		traceRenderFrame_GameThread();
#endif
	}

#if !UE_BUILD_SHIPPING
	if (m_renderApplyWindow.frames >= kRenderApplyWindowFrames)
		logRenderApplyWindow_GameThread();
#endif
}

void ASimulationManagerUImpl::syncRenderPoses_GameThread()
{
	if (m_shadowWorld.has_value())
		restoreShadowWorld_GameThread();
	applyRenderSnapshot_GameThread();
	overwriteShadowPoses_GameThread();
}

void ASimulationManagerUImpl::updateVisualizationAtRenderPoses_GameThread()
{
	updateVisualizationAll(m_storage);
	if (!m_renderPosesValid)
		return;

	m_storage.forEachSimulatable<SimulatableBrawler>([this](unsigned int id, SimulatableBrawler& simulatable)
	{
		auto& vizState = simulatable.editVizState().editState();
		uint8 declarationIndex = 0;
		simulatable.getPhysicsComposite().forEach([&](const auto& declaration)
		{
			using D = std::decay_t<decltype(declaration)>;
			using S = typename D::StateType;
			const uint8 index = declarationIndex++;
			const RenderBody* const pose = findRenderBody(m_renderPoses, id, index);
			if (pose == nullptr || index == m_joltRootDeclarationIndex)
				return;

			auto& bodyState = D::bodyStateOf(vizState.template edit<S>());
			bodyState.position = pose->positionCm;
			if constexpr (std::is_same_v<std::remove_cvref_t<decltype(bodyState)>, PhysicsBodyState>)
				bodyState.rotation = pose->rotation;
		});
	});
}

void ASimulationManagerUImpl::overwriteShadowPoses_GameThread()
{
	if (!m_shadowWorld.has_value() || !m_renderPosesValid)
		return;
	checkf(m_shadowRestoredFrame == GFrameCounter,
		TEXT("overwriteShadowPoses_GameThread: the shadow world was not restored this frame before the render poses were ")
		TEXT("written into it. The restore loads the newest saved tick into every slot body, so a restore after this ")
		TEXT("overwrite would leave the visualizations querying the newest tick instead of the rendered pose."));

	const JoltBodyBindTable& shadowBindTable = m_shadowBodyAdapter->bindTable();
	for (const auto& [id, target] : m_renderTargetsById)
	{
		const AOGBrawlerUECharacter* const character = target.character.Get();
		if (character == nullptr)
			continue;
		const auto slot = m_joltSlotByOwner.find(character);
		if (slot == m_joltSlotByOwner.end())
			continue;

		for (uint32 declarationIndex = 0u; declarationIndex < m_shadowWorld->bodiesPerSlot(); ++declarationIndex)
		{
			const RenderBody* const pose = findRenderBody(m_renderPoses, id, static_cast<uint8>(declarationIndex));
			if (pose == nullptr)
				continue;
			const BodyId body = JoltPhysicsBodyAdapter::bodyIdOf(m_shadowWorld->slotBodyId(slot->second, declarationIndex));
			if (!shadowBindTable.isBound(body))
				continue;

			glm::mat4 transform = pose->hasRotation != 0u ? glm::mat4_cast(pose->rotation)
			                                              : m_shadowBodyAdapter->getBodyTransform(body);
			transform[3] = glm::vec4(pose->positionCm, 1.f);
			m_shadowBodyAdapter->setBodyTransform(body, transform);

#if !UE_BUILD_SHIPPING
			RenderApplyWindowUImpl& window = m_renderApplyWindow;
			const glm::mat4 queried = m_shadowReader->getBodyTransform(body);
			const double errorCm = glm::distance(glm::vec3(queried[3]), pose->positionCm);
			double angleRad = 0.0;
			if (pose->hasRotation != 0u)
			{
				const glm::dquat queriedRotation(glm::normalize(glm::quat_cast(glm::mat3(queried))));
				const glm::dquat relative = glm::conjugate(glm::dquat(pose->rotation)) * queriedRotation;
				angleRad = 2.0 * FMath::Atan2(glm::length(glm::dvec3(relative.x, relative.y, relative.z)), FMath::Abs(relative.w));
			}
			++window.shadowBodies;
			window.shadowExact += errorCm == 0.0 ? 1u : 0u;
			window.shadowMaxErrorCm  = FMath::Max(window.shadowMaxErrorCm, errorCm);
			window.shadowMaxAngleRad = FMath::Max(window.shadowMaxAngleRad, angleRad);
			if ((errorCm > kShadowPoseToleranceCm || angleRad > kShadowPoseToleranceRad) && window.shadowOutOfTolerance++ == 0u)
			{
				UE_LOG(LogOGSimHost, Warning,
					TEXT("[SimHost.RenderSync] SHADOW id=%u declaration=%u queried=(%.6f,%.6f,%.6f) rendered=(%.6f,%.6f,%.6f) ")
					TEXT("errorCm=%.9f angleRad=%.9f: the shadow world the visualizations query is not at the rendered pose"),
					id, declarationIndex, queried[3].x, queried[3].y, queried[3].z, pose->positionCm.x, pose->positionCm.y,
					pose->positionCm.z, errorCm, angleRad);
			}
#endif
		}
	}
}

#if !UE_BUILD_SHIPPING
void ASimulationManagerUImpl::checkRenderTargetsHeld_GameThread()
{
	for (const auto& [id, target] : m_renderTargetsById)
	{
		const AOGBrawlerUECharacter* const character = target.character.Get();
		if (!target.applied || character == nullptr)
			continue;

		const FVector planePoint = character->GetCapsuleComponent()->GetComponentTransform().GetTranslation();
		const double driftCm = FVector::Dist(planePoint, target.appliedCm);
		++m_renderApplyWindow.held;
		m_renderApplyWindow.maxHeldDriftCm = FMath::Max(m_renderApplyWindow.maxHeldDriftCm, driftCm);
		if (driftCm > kRenderApplyToleranceCm && m_renderApplyWindow.moved++ == 0u)
		{
			UE_LOG(LogOGSimHost, Warning,
				TEXT("[SimHost.RenderApply] MOVED role=%s id=%u capsule=(%.6f,%.6f,%.6f) applied=(%.6f,%.6f,%.6f) driftCm=%.9f: ")
				TEXT("something moved the capsule between the last render apply and this one"),
				runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, planePoint.X, planePoint.Y, planePoint.Z,
				target.appliedCm.X, target.appliedCm.Y, target.appliedCm.Z, driftCm);
		}
	}
}

void ASimulationManagerUImpl::checkRenderApply_GameThread()
{
	RenderApplyWindowUImpl& window = m_renderApplyWindow;
	const RenderInterpolationFrame& bracket = m_renderFrame;
	if (window.appliedFrames++ == 0u)
		window.firstAppliedStep = m_renderPoses.physicsStep;
	window.lastAppliedStep = m_renderPoses.physicsStep;
	window.interpolated += bracket.hasPrev ? 1u : 0u;
	window.newestMode   += bracket.newestOnly ? 1u : 0u;
	window.atNewest     += bracket.atNewest ? 1u : 0u;
	window.beyondOldest += bracket.beyondOldest ? 1u : 0u;
	window.snaps        += bracket.snaps;
	window.maxNextBack   = FMath::Max(window.maxNextBack, bracket.nextBack);

	for (const auto& [id, target] : m_renderTargetsById)
	{
		const AOGBrawlerUECharacter* const character = target.character.Get();
		const RenderBody* const body = findRenderBody(m_renderPoses, id, m_joltRootDeclarationIndex);
		if (character == nullptr || body == nullptr)
		{
			++window.missing;
			continue;
		}

		const FVector planePoint = character->GetCapsuleComponent()->GetComponentTransform().GetTranslation();
		const FVector pose = uglm::toFVector(body->positionCm);
		const double errorCm = FVector::Dist(planePoint, pose);
		++window.applied;
		window.exact += planePoint == pose ? 1u : 0u;
		window.maxErrorCm = FMath::Max(window.maxErrorCm, errorCm);
		if (!planePoint.Equals(pose, UE_KINDA_SMALL_NUMBER) && window.outOfTolerance++ == 0u)
		{
			UE_LOG(LogOGSimHost, Warning,
				TEXT("[SimHost.RenderApply] MISMATCH role=%s id=%u capsule=(%.6f,%.6f,%.6f) rendered=(%.6f,%.6f,%.6f) errorCm=%.9f physicsStep=%llu"),
				runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, planePoint.X, planePoint.Y, planePoint.Z,
				pose.X, pose.Y, pose.Z, errorCm, static_cast<unsigned long long>(m_renderPoses.physicsStep));
		}
	}
}

void ASimulationManagerUImpl::traceRenderFrame_GameThread() const
{
	if (CVarRenderTraceFrames.GetValueOnGameThread() == 0)
		return;

	const RenderInterpolationFrame& bracket = m_renderFrame;
	for (const auto& [id, target] : m_renderTargetsById)
	{
		const RenderBody* const nextBody = findRenderBody(m_renderTraceNext, id, m_joltRootDeclarationIndex);
		if (!target.applied || nextBody == nullptr)
			continue;
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.RenderTrace] role=%s frame=%llu id=%u pos=(%.4f,%.4f,%.4f) nextPos=(%.4f,%.4f,%.4f) host=%.6f ")
			TEXT("render=%.6f mode=%s prev=%llu/%s next=%llu/%s back=%u alpha=%.4f newest=%llu"),
			runsPrediction() ? TEXT("Client") : TEXT("Authority"), static_cast<unsigned long long>(GFrameCounter), id,
			target.appliedCm.X, target.appliedCm.Y, target.appliedCm.Z,
			nextBody->positionCm.x, nextBody->positionCm.y, nextBody->positionCm.z,
			m_frameHost.hostNowSeconds_GameThread(), bracket.renderTimeSeconds,
			bracket.newestOnly ? TEXT("newest") : TEXT("interp"),
			static_cast<unsigned long long>(bracket.prevStep), bracket.hasPrev ? stepKindText(bracket.prevKind) : TEXT("-"),
			static_cast<unsigned long long>(bracket.nextStep), stepKindText(bracket.nextKind), bracket.nextBack,
			bracket.alpha, static_cast<unsigned long long>(bracket.newestStep));
	}
}

void ASimulationManagerUImpl::logRenderApplyWindow_GameThread()
{
	RenderApplyWindowUImpl& window = m_renderApplyWindow;
	FString displacement = TEXT("-");
	if (!window.frameDisplacementCm.empty())
	{
		std::vector<float>& values = window.frameDisplacementCm;
		std::sort(values.begin(), values.end());
		const uint32 n = static_cast<uint32>(values.size());
		displacement = FString::Printf(TEXT("%.4f/%.4f/%.4f"), values[latencyBudget::nearestRankIndex(n, 500u)],
			values[latencyBudget::nearestRankIndex(n, 990u)], values.back());
	}

	UE_LOG(LogOGSimHost, Log,
		TEXT("[SimHost.RenderApply] role=%s frames=%u appliedFrames=%u characters=%d applied=%u missing=%u exact=%u ")
		TEXT("outOfTolerance=%u maxErrorCm=%.9f held=%u moved=%u maxHeldDriftCm=%.9f pathCm=%.3f steps=[%llu,%llu] drops=%llu ")
		TEXT("mode=%s delaySteps=%.2f interpolated=%u atNewest=%u beyondOldest=%u snaps=%u maxNextBack=%u dispCm=%s"),
		runsPrediction() ? TEXT("Client") : TEXT("Authority"), window.frames, window.appliedFrames,
		static_cast<int32>(m_renderTargetsById.size()), window.applied, window.missing, window.exact,
		window.outOfTolerance, window.maxErrorCm, window.held, window.moved, window.maxHeldDriftCm, window.pathCm,
		static_cast<unsigned long long>(window.firstAppliedStep), static_cast<unsigned long long>(window.lastAppliedStep),
		static_cast<unsigned long long>(m_renderSnapshots.drops()),
		window.newestMode > 0u ? TEXT("newest") : TEXT("interp"),
		FMath::Clamp(static_cast<double>(CVarRenderInterpolationDelaySteps.GetValueOnGameThread()), 0.0,
			kRenderMaxInterpolationDelaySteps),
		window.interpolated, window.atNewest, window.beyondOldest, window.snaps, window.maxNextBack, *displacement);

	if (m_shadowWorld.has_value())
	{
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.RenderSync] role=Client frames=%u shadowBodies=%u shadowExact=%u shadowOutOfTolerance=%u ")
			TEXT("shadowMaxErrorCm=%.6f shadowMaxAngleRad=%.6f"),
			window.frames, window.shadowBodies, window.shadowExact, window.shadowOutOfTolerance, window.shadowMaxErrorCm,
			window.shadowMaxAngleRad);
	}

	std::vector<float> samples = std::move(window.frameDisplacementCm);
	samples.clear();
	window = RenderApplyWindowUImpl{};
	window.frameDisplacementCm = std::move(samples);
}
#endif

ASimulationManagerUImpl::ShadowStateSlotUImpl::ShadowStateSlotUImpl()
{
	state.bytes.assign(JoltWorldConfig{}.ringSlotBytes, 0u);
	state.bodyIds.reserve(kMaxSimulatableSlots * physicsDeclarationCountOf<BrawlerPhysicsComposite>::value);
}

void ASimulationManagerUImpl::bindShadowSlot(uint32 slot, unsigned int id, const SimulatableBrawler& simulatable)
{
	JoltPhysicsFactory factory(*m_shadowBodyAdapter, m_joltSlotTemplate, slot, id, JoltPhysicsFactoryOptions{},
		[this](BodyId body, uint32_t shapeIndex, std::optional<BodyId> rootBodyId)
		{
			return m_shadowQuery->registerShape(body, shapeIndex, rootBodyId);
		});

	bool sameResults = true;
	simulatable.getPhysicsComposite().forEach([&](const auto& decl)
	{
		using D = std::decay_t<decltype(decl)>;
		const JoltPhysicsFactory::PhysicalObjectResult result = factory.createPhysicalObject(D::descriptor(), D::name);
		sameResults = sameResults && result.bodyId == decl.bindings.ownBodyId && result.shapeIds == decl.bindings.shapeIds
			&& factory.parentBodyId() == decl.bindings.parentBodyId;
	});

	compareShadowBindTables_GameThread(TEXT("bind"), slot, id, sameResults);
}

void ASimulationManagerUImpl::releaseShadowSlot(uint32 slot)
{
	m_shadowBodyAdapter->bindTable().releaseSlot(slot);
	compareShadowBindTables_GameThread(TEXT("release"), slot, 0u, true);
}

void ASimulationManagerUImpl::compareShadowBindTables_GameThread(const TCHAR* after, uint32 slot, unsigned int id,
	bool sameResults)
{
#if !UE_BUILD_SHIPPING
	const JoltBodyBindTable& stepTable   = m_physAdapter->bindTable();
	const JoltBodyBindTable& shadowTable = m_shadowBodyAdapter->bindTable();
	const bool sameShape = stepTable.simulatableSlots() == shadowTable.simulatableSlots()
		&& stepTable.bodiesPerSlot() == shadowTable.bodiesPerSlot();
	const uint32 slots = FMath::Min(stepTable.simulatableSlots(), shadowTable.simulatableSlots());
	const uint32 bodiesPerSlot = FMath::Min(stepTable.bodiesPerSlot(), shadowTable.bodiesPerSlot());

	uint32 bound = 0u;
	uint32 differing = 0u;
	for (uint32 slotIndex = 0u; slotIndex < slots; ++slotIndex)
	{
		for (uint32 templateIndex = 0u; templateIndex < bodiesPerSlot; ++templateIndex)
		{
			const BodyId stepBody   = JoltPhysicsBodyAdapter::bodyIdOf(m_joltWorld->slotBodyId(slotIndex, templateIndex));
			const BodyId shadowBody = JoltPhysicsBodyAdapter::bodyIdOf(m_shadowWorld->slotBodyId(slotIndex, templateIndex));
			const std::optional<JoltBodyBinding> stepBinding   = stepTable.bindingOf(stepBody);
			const std::optional<JoltBodyBinding> shadowBinding = shadowTable.bindingOf(shadowBody);
			bound += stepBinding.has_value() ? 1u : 0u;
			const bool same = stepBody == shadowBody && stepBinding == shadowBinding
				&& (!stepBinding.has_value()
					|| stepTable.lockedRotationInertiaOf(stepBody) == shadowTable.lockedRotationInertiaOf(shadowBody));
			differing += same ? 0u : 1u;
		}
	}

	const bool equal = sameShape && differing == 0u && sameResults;
	++m_shadowBindCompares;
	if (equal)
	{
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.Shadow] bindCompare after=%s id=%u slot=%u equal=1 boundBodies=%u compares=%u mismatches=%u"),
			after, id, slot, bound, m_shadowBindCompares, m_shadowBindMismatches);
	}
	else
	{
		++m_shadowBindMismatches;
		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.Shadow] bindCompare after=%s id=%u slot=%u equal=0 boundBodies=%u differing=%u sameShape=%d ")
			TEXT("sameResults=%d compares=%u mismatches=%u: the shadow world's bind table differs from the step world's"),
			after, id, slot, bound, differing, sameShape ? 1 : 0, sameResults ? 1 : 0, m_shadowBindCompares,
			m_shadowBindMismatches);
	}
#endif
}

void ASimulationManagerUImpl::publishShadowSlot_Step(const TickOutcome& outcome)
{
	if (!m_shadowWorld.has_value() || !stepAllocatesFrontierSlot(outcome.kind))
		return;

	const JoltStateSlot* const saved = m_joltWorld->ring().find(outcome.tick);
	if (saved == nullptr)
	{
		m_shadowSlotsMissing.fetch_add(1u, std::memory_order_relaxed);
		return;
	}

	ShadowStateSlotUImpl* const target = m_shadowSlots.beginWrite();
	checkf(target != nullptr,
		TEXT("publishShadowSlot_Step: the shadow hand-over channel has no free slot. The game thread holds at most one ")
		TEXT("slot (the newest, for one restore), and a 3-slot channel keeps a free slot for one."));
	if (target == nullptr)
		return;

	JoltStateSlot& state = target->state;
	checkf(saved->byteCount <= state.bytes.size(),
		TEXT("publishShadowSlot_Step: a saved tick holds %u bytes but the pooled hand-over slot only %d."),
		saved->byteCount, static_cast<int32>(state.bytes.size()));
	std::copy_n(saved->bytes.begin(), saved->byteCount, state.bytes.begin());
	state.byteCount = saved->byteCount;
	state.bodyIds.assign(saved->bodyIds.begin(), saved->bodyIds.end());
	state.sidecar   = saved->sidecar;
	state.stateHash = saved->stateHash;
	state.tick      = saved->tick;
	state.valid     = saved->valid;

	target->sequence = m_shadowSlotsPublished.load(std::memory_order_relaxed) + 1u;
	m_shadowSlotsPublished.store(target->sequence, std::memory_order_relaxed);
	m_shadowSlots.commit();
}

void ASimulationManagerUImpl::restoreShadowWorld_GameThread()
{
	ShadowWindowUImpl& window = m_shadowWindow;
	++window.frames;
	m_shadowRestoredFrame = GFrameCounter;

	if (const ShadowStateSlotUImpl* const newest = m_shadowSlots.peekNewest(0))
	{
		if (newest->sequence > m_shadowRestoredSequence)
		{
			const double startSeconds = FPlatformTime::Seconds();
			const bool restored = m_shadowWorld->restoreFromSnapshot(newest->state);
			const double restoreSeconds = FPlatformTime::Seconds() - startSeconds;
			const double restoreMs = restoreSeconds * 1000.0;
			m_shadowRestoredSequence = newest->sequence;
			m_frameHost.noteShadowRestore_GameThread(restoreSeconds, !restored);
			if (restored)
			{
				++window.restores;
				window.lastRestoredTick = static_cast<uint32>(newest->state.tick);
				window.maxRestoreMs = FMath::Max(window.maxRestoreMs, restoreMs);
#if !UE_BUILD_SHIPPING
				window.occupiedSlots = static_cast<uint32>(newest->state.sidecar.occupancy.occupied.count());
				if (m_shadowWorld->liveStateHash() == newest->state.stateHash)
				{
					++window.hashEqual;
				}
				else if (window.hashDiffer++ == 0u)
				{
					UE_LOG(LogOGSimHost, Warning,
						TEXT("[SimHost.Shadow] HASH role=Client tick=%u: the restored shadow world's state hash differs from ")
						TEXT("the step world's saved tick"),
						static_cast<uint32>(newest->state.tick));
				}
#endif
			}
			else
			{
				++window.refused;
			}
		}
		else
		{
			++window.unchanged;
		}
		m_shadowSlots.release(newest);
	}
	else
	{
		++window.unchanged;
	}

#if !UE_BUILD_SHIPPING
	if (window.frames >= kShadowWindowFrames)
		logShadowWindow_GameThread();
#endif
}

void ASimulationManagerUImpl::logShadowWindow_GameThread()
{
#if !UE_BUILD_SHIPPING
	const ShadowWindowUImpl& window = m_shadowWindow;
	const uint64 published = m_shadowSlotsPublished.load(std::memory_order_relaxed);
	const uint64 missing   = m_shadowSlotsMissing.load(std::memory_order_relaxed);
	UE_LOG(LogOGSimHost, Log,
		TEXT("[SimHost.Shadow] role=Client frames=%u restores=%u unchanged=%u refused=%u published=%llu missing=%llu ")
		TEXT("hashEqual=%u hashDiffer=%u occupiedSlots=%u lastTick=%u maxRestoreMs=%.4f vizQuery=[shadow:%llu step:%llu] ")
		TEXT("vizReader=[shadow:%llu step:%llu] shadowQueryAccesses=%llu stepQueryGameThreadAccesses=%llu bindCompares=%u ")
		TEXT("bindMismatches=%u drops=%llu"),
		window.frames, window.restores, window.unchanged, window.refused,
		static_cast<unsigned long long>(published - window.publishedAtStart),
		static_cast<unsigned long long>(missing - window.missingAtStart),
		window.hashEqual, window.hashDiffer, window.occupiedSlots,
		window.lastRestoredTick, window.maxRestoreMs,
		static_cast<unsigned long long>(window.vizQueryShadow), static_cast<unsigned long long>(window.vizQueryStep),
		static_cast<unsigned long long>(window.vizReaderShadow), static_cast<unsigned long long>(window.vizReaderStep),
		static_cast<unsigned long long>(t_joltShadowQueryAccesses - window.shadowQueryAccessesAtStart),
		static_cast<unsigned long long>(t_joltStepQueryGameThreadAccesses - window.stepQueryGameThreadAccessesAtStart),
		m_shadowBindCompares, m_shadowBindMismatches, static_cast<unsigned long long>(m_shadowSlots.drops()));

	ShadowWindowUImpl next;
	next.publishedAtStart                   = published;
	next.missingAtStart                     = missing;
	next.shadowQueryAccessesAtStart         = t_joltShadowQueryAccesses;
	next.stepQueryGameThreadAccessesAtStart = t_joltStepQueryGameThreadAccesses;
	m_shadowWindow = next;
#endif
}
#endif

SimCharacterId ASimulationManagerUImpl::allocateSimCharacterId()
{
    checkf(GetNetMode() != NM_Client,
           TEXT("allocateSimCharacterId called on a client world's manager. Only the authority assigns ")
           TEXT("simulation character ids; a client learns each one through the pawn's replicated ")
           TEXT("SimCharacterIdValue."));

    const SimCharacterId simId = m_simCharacterIds.allocate();
    // ⛔G-78  docs/SimulationManagerUImpl-guards.md
    if (simId == SimCharacterId::None)
    {
        UE_LOG(LogOGMgmt, Error,
            TEXT("allocateSimCharacterId: registration REFUSED. This authority has already assigned all %u ")
            TEXT("simulation character ids and never reuses one, so this character does not register. ")
            TEXT("Widening SimCharacterId or reusing ids is a user ruling (R2), not a fix to make here."),
            static_cast<unsigned int>(SimCharacterIdAllocator::kLastAssignable));
        OG_CHECK(false, "allocateSimCharacterId: all SimCharacterIds are spent; registration refused (R2)");
        return SimCharacterId::None;
    }

    UE_LOG(LogOGMgmt, Log, TEXT("allocateSimCharacterId: assigned id=%u"), toStorageKey(simId));
    return simId;
}

TryRegisterStatus ASimulationManagerUImpl::tryRegister(
    SimCharacterId simId,
    SimulatableBrawler simulatable,
    USimmableUpdateComponent& owner,
    BrawlerInputProviderFn inputProvider,
    bool isAuthority)
{
    checkf(simId != SimCharacterId::None,
           TEXT("tryRegister called with SimCharacterId::None; the component must wait until its ")
           TEXT("pawn's id is assigned (authority) or replicated (client)."));
    const unsigned int id = toStorageKey(simId);

    auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
    {
        PendingRegistration record;
        record.simulatable.emplace(std::move(simulatable));
        record.inputProvider = std::move(inputProvider);
        record.isAuthority   = isAuthority;
        auto inserted = m_pendingRegistrations.emplace(id, std::move(record));
        it = inserted.first;
    }

    PendingRegistration& record = it->second;

    if (!record.bodiesCreated)
    {
        AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(owner.GetOwner());
        checkf(character != nullptr,
               TEXT("USimmableUpdateComponent must be attached to an AOGBrawlerUECharacter — the ")
               TEXT("first-call body pass reads that class's own root capsule"));
#if OG_PHYSICS_BACKEND_CHAOS
        FBodyInstanceAsyncPhysicsTickHandle parentHandle =
            character->GetCapsuleComponent()->GetBodyInstanceAsyncPhysicsTickHandle();
        const BodyId parentBodyId = m_physAdapter->getBodyId(parentHandle);

        AActor* ownerActor = owner.GetOwner();
        UPrimitiveComponent* attachParent = character->GetCapsuleComponent();
        const simulatableBrawler::StaticData& staticData = owner.getStaticData();

        ChaosPhysicsFactory factory(*m_physAdapter, *m_queryAdapter, ownerActor, attachParent);

        record.simulatable->editPhysicsComposite().forEach([&](auto& decl)
        {
            using D = std::decay_t<decltype(decl)>;
            const auto& subStaticData = D::staticDataOf(staticData);
            auto r = factory.createPhysicalObject(D::descriptor(), D::name);
            decl.bindings.ownBodyId        = r.bodyId;
            decl.bindings.parentBodyId     = parentBodyId;
            decl.bindings.attachmentOffset = D::attachmentOffset(subStaticData);
            decl.bindings.shapeIds         = std::move(r.shapeIds);
            FCollisionQueryParams qp;
            qp.AddIgnoredActor(ownerActor);
            for (const auto& volDesc : D::queryVolumes(subStaticData))
                decl.bindings.queryVolumeIds.push_back(
                    m_queryAdapter->registerVolume(volDesc, qp, FActorInstanceHandle(ownerActor)));
        });
#else
        {
            const UCapsuleComponent* capsule = character->GetCapsuleComponent();
            const CapsuleGeometry* descriptorCapsule = std::get_if<CapsuleGeometry>(
                &brawlerMovementSimulation::PhysicsDeclaration::descriptor().shapes.front().geometry);
            checkf(descriptorCapsule != nullptr
                       && FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleRadius(), descriptorCapsule->radius)
                       && FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleHalfHeight(), descriptorCapsule->halfHeight),
                   TEXT("tryRegister: the pawn's capsule (radius %f, half-height %f) disagrees with the movement ")
                   TEXT("descriptor's CapsuleGeometry (radius %f, half-height %f), id=%u. The Jolt slot body is built ")
                   TEXT("from the descriptor, while the UE capsule is the aim plane and the camera anchor: change both ")
                   TEXT("together. Was the Chaos factory's adopt-root check (design D14; OGBrawlerUECharacter G-01)."),
                   capsule->GetUnscaledCapsuleRadius(), capsule->GetUnscaledCapsuleHalfHeight(),
                   descriptorCapsule != nullptr ? descriptorCapsule->radius : 0.f,
                   descriptorCapsule != nullptr ? descriptorCapsule->halfHeight : 0.f, id);
        }

        AActor* ownerActor = owner.GetOwner();
        const simulatableBrawler::StaticData& staticData = owner.getStaticData();
        const uint32 slot = acquireJoltSlot(*ownerActor);
        BodyId parentBodyId;
        {
            const double worldMutexWaitStartSeconds = FPlatformTime::Seconds();
            // ⛔G-81  docs/SimulationManagerUImpl-guards.md
            JoltWorldLockScopeUImpl worldLock(m_joltWorldMutex, m_joltRoleBit);
            m_frameHost.noteWorldMutexWait_GameThread(FPlatformTime::Seconds() - worldMutexWaitStartSeconds);

            JoltPhysicsFactory factory(*m_physAdapter, m_joltSlotTemplate, slot, id, JoltPhysicsFactoryOptions{},
                [this](BodyId body, uint32_t shapeIndex, std::optional<BodyId> rootBodyId)
                {
                    return m_queryAdapter->registerShape(body, shapeIndex, rootBodyId);
                });
            parentBodyId = factory.parentBodyId();

            record.simulatable->editPhysicsComposite().forEach([&](auto& decl)
            {
                using D = std::decay_t<decltype(decl)>;
                const auto& subStaticData = D::staticDataOf(staticData);
                auto r = factory.createPhysicalObject(D::descriptor(), D::name);
                decl.bindings.ownBodyId        = r.bodyId;
                decl.bindings.parentBodyId     = parentBodyId;
                decl.bindings.attachmentOffset = D::attachmentOffset(subStaticData);
                decl.bindings.shapeIds         = std::move(r.shapeIds);
                for (const auto& volDesc : D::queryVolumes(subStaticData))
                    decl.bindings.queryVolumeIds.push_back(m_queryAdapter->registerVolume(volDesc, parentBodyId));
            });
        }

        UE_LOG(LogOGSimHost, Log, TEXT("[SimHost.Bind] bind id=%u slot=%u boundBodies=%u slotsInUse=%d"),
            id, slot, boundJoltBodyCount(), static_cast<int32>(m_joltSlotByOwner.size()));

        if (m_shadowWorld.has_value())
            bindShadowSlot(slot, id, *record.simulatable);
#endif

#if DO_CHECK
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif

        const BodyId movementOwnBodyId =
            record.simulatable->getPhysicsComposite()
                .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;

        checkf(movementOwnBodyId == parentBodyId,
               TEXT("tryRegister: the movement declaration's own body (%u) is not the pawn's root ")
               TEXT("capsule (%u), id=%u. Either CharacterBindings is being stamped before the physics ")
               TEXT("fold populated bindings.ownBodyId, or the factory created a body instead of ")
               TEXT("adopting the capsule. Was the stamp-after-the-fold and isRoot fences."),
               movementOwnBodyId.value, parentBodyId.value, id);

        record.simulatable->setCharacterBindings({ /*.capsuleBodyId =*/ movementOwnBodyId });

        auto& movementIC = record.simulatable->editAllState().editState()
            .edit<brawlerMovementSimulation::InitialConditions>();
        movementIC.teleportPending = 1u;
        movementIC.teleportPos     =
            uglm::toGLMVec3(character->GetCapsuleComponent()->GetComponentLocation());

        if (record.isAuthority)
        {
            checkf(record.isAuthority == (GetNetMode() != NM_Client),
                   TEXT("tryRegister: isAuthority disagrees with the world-level net mode; the ")
                   TEXT("spawn-slot table must be populated on the authority world only"));

            auto& ringoutIC = record.simulatable->editAllState().editState()
                .edit<brawlerRingout::InitialConditions>();

            // ⛔G-67  docs/SimulationManagerUImpl-guards.md
            ringoutIC.spawnSlot = m_spawnSlots.acquire(id);

            UE_LOG(LogOGMgmt, Log,
                TEXT("tryRegister: ring-out spawnSlot=%u assigned to id=%u"),
                ringoutIC.spawnSlot, id);
        }

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    const BodyId movementBodyId =
        record.simulatable->getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
    bool allResolvable = m_physAdapter->isBodyResolvable(movementBodyId);
    if (allResolvable)
    {
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
                allResolvable = false;
        });
    }

    if (!allResolvable)
        return TryRegisterStatus::Pending;

    if (record.isAuthority)
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/wrapInputProviderWithLatencyStamp(
                id, owner, std::move(record.inputProvider)));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    // ⛔G-68  docs/SimulationManagerUImpl-guards.md
    if (record.isAuthority)
    {
        m_authorityRegisteredIds.insert(id);
        const int32 registered = static_cast<int32>(m_authorityRegisteredIds.size());
        if (registered > kPreDietCharacterCap)
        {
            UE_LOG(LogOGNet, Warning,
                TEXT("[PreDietCap] character %d exceeds pre-diet cap %d — input-loss margins "
                     "unsafe (T44/T38 §16); land item 40"),
                registered, kPreDietCharacterCap);
        }
    }

    // ⛔G-69  docs/SimulationManagerUImpl-guards.md
    m_manager->notifyCharacterRegistered(id);

#if !OG_PHYSICS_BACKEND_CHAOS
    {
        const auto slot = m_joltSlotByOwner.find(owner.GetOwner());
        checkf(slot != m_joltSlotByOwner.end(),
               TEXT("tryRegister: id=%u is Ready but its owner holds no Jolt slot; the first pass binds one."), id);
        m_frameHost.pushOccupancy_GameThread(slot->second, true);
        AOGBrawlerUECharacter* const character = Cast<AOGBrawlerUECharacter>(owner.GetOwner());
        m_renderTargetsById[id] = RenderTargetUImpl{ character };
        if (character != nullptr)
        {
            UCapsuleComponent* const capsule = character->GetCapsuleComponent();
            capsule->SetSimulatePhysics(false);

            TArray<UPrimitiveComponent*> primitives;
            character->GetComponents<UPrimitiveComponent>(primitives);
            FString simulating;
            for (const UPrimitiveComponent* primitive : primitives)
            {
                if (primitive->IsSimulatingPhysics())
                    simulating += (simulating.IsEmpty() ? TEXT("") : TEXT(",")) + primitive->GetName();
            }
            UE_LOG(LogOGSimHost, Log,
                TEXT("[SimHost.RenderTarget] role=%s id=%u capsuleSimulatesPhysics=%d simulatingComponents=[%s]"),
                runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, capsule->IsSimulatingPhysics() ? 1 : 0,
                *simulating);
        }
    }
#endif

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

void ASimulationManagerUImpl::noteDelayedInputComponent(
    SimCharacterId simId, USimmableUpdateComponent& component)
{
    m_delayedInputComponentsById[toStorageKey(simId)] = &component;
}

static_assert(
    RemoteInputDeliverySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputDeliverySink so "
    "ServerReceptionCoordinator can drive remote-input delivery through it");

void ASimulationManagerUImpl::deliverRemoteInput(
    unsigned int id, uint32 captureTick, const simulatableBrawler::PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->deliverDelayedRemoteInput(captureTick, input);
}

static_assert(
    RemoteInputRelaySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputRelaySink so "
    "ServerReceptionCoordinator can drive the input relay through it");

void ASimulationManagerUImpl::relayRemoteInput(
    unsigned int id, uint32 captureTick, uint8 dA,
    const simulatableBrawler::PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->stageRelayedInput(captureTick, dA, input);

    {
        RelayWriteWindowSummary w;
        if (m_relayWriteProbe.noteWrite(
                id, static_cast<uint64>(GFrameCounter), captureTick, w))
        {
            char line[256];

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u runs=%u writes=%u observableWrites=%u "
                "captureSpan=%u receivedX1000=%u observableX1000=%u deliverableX1000=%u "
                "replaceLatestObservableX1000=%u",
                w.ownerId, w.runs, w.writes, w.observableWrites, w.captureSpan,
                w.receivedX1000, w.observableX1000, w.deliverableX1000,
                w.replaceLatestObservableX1000);
            RouteOGMessage(line);

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u writesPerFrame p50=%u p99=%u%s "
                "max=%u emptyFrames=%u nonConsecutive=%u missedCaptureTicks=%u "
                "captures=[%u,%u] discont=%u",
                w.ownerId, w.p50, w.p99, w.p99Saturated ? "+" : "", w.maxRun,
                w.emptyFrames, w.nonConsecutiveWrites, w.missedCaptureTicks,
                w.firstCaptureTick, w.lastCaptureTick, w.discontinuities);
            RouteOGMessage(line);
        }
    }
}

void ASimulationManagerUImpl::onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps)
{
    static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples,
        "PROBE A's window must stay the same length as ResimGateProbe's, so one [ResimProbe.Frame] "
        "window lines up against the surrounding [ResimProbe.Gate] windows without a resim-tick "
        "count on the line. Was the COMPARABLE WITH ResimGateProbe caution in PROBE A's banner.");
    {
        FrameHealthWindowSummary frameSummary;
        const bool frameWindowClosed = m_frameHealthProbe.noteFrame(
            static_cast<uint64>(GFrameCounter),
            static_cast<uint32>(firstUpcomingSimTick),
            static_cast<uint32>(numSteps),
            static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0),
            frameSummary);

        if (frameWindowClosed)
        {
            const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
            char line[256];

            if (isClient)
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
            else
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
        }
    }

    // ⛔G-70  docs/SimulationManagerUImpl-guards.md
    if (!m_receptionCoordinator.has_value())
        return;

    if (const UWorld* world = GetWorld())
    {
        if (const UNetDriver* netDriver = world->GetNetDriver())
        {
            const uint32 tickRateHz =
                static_cast<uint32>(FMath::Max(1, netDriver->GetNetServerMaxTickRate()));
            const uint64 nowMicros =
                static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0);

            for (UNetConnection* conn : netDriver->ClientConnections)
            {
                if (conn == nullptr)
                    continue;

                {
                    static bool s_loggedPacketBudget = false;
                    if (!s_loggedPacketBudget)
                    {
                        s_loggedPacketBudget = true;
                        const int32 usableBits = conn->GetMaxSingleBunchSizeBits();
                        UE_LOG(LogOGNet, Warning,
                            TEXT("[PacketBudget] usableSingleBunchBytes = %d, handlerBits = %d "
                                 "(maxPacket=%d, usableBits=%d)"),
                            // ∴D-50  docs/SimulationManagerUImpl-rationale.md
                            (usableBits / 32) * 4, conn->MaxPacketHandlerBits,
                            conn->MaxPacket, usableBits);
                    }
                }

                ConnectionBudgetWindowSummary budget;
                // ⛔G-72  docs/SimulationManagerUImpl-guards.md
                const bool budgetWindowClosed = m_connectionBudgetProbe.noteSample(
                    static_cast<uint32>(conn->GetUniqueID()),
                    conn->CurrentNetSpeed,
                    conn->QueuedBits,
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalBytes)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPackets)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPacketsLost)),
                    tickRateHz, nowMicros, budget);

                if (!budgetWindowClosed)
                    continue;

                char line[256];

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u frames=%u elapsedMs=%u "
                    "netSpeedBps=%d allowanceBytesPerTick=%u outBytes=%u "
                    "bytesPerTick=%u occupancyPctX10=%u",
                    budget.connectionId, budget.samples, budget.elapsedMs,
                    budget.netSpeedBps, budget.allowanceBytesPerTick,
                    budget.outBytes, budget.bytesPerSample, budget.occupancyPctX10);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u queuedBits min=%d max=%d "
                    "mean=%d notReadyFrames=%u outPackets=%u bytesPerPacket=%u lost=%u",
                    budget.connectionId, budget.queuedBitsMin, budget.queuedBitsMax,
                    budget.queuedBitsMean, budget.notReadySamples,
                    budget.outPackets, budget.bytesPerPacket, budget.outPacketsLost);
                RouteOGMessage(line);
            }
        }
    }
}

void ASimulationManagerUImpl::releaseDelayedInputsForStep(int32 firstTick, int32 numSteps)
{
    if (!m_receptionCoordinator.has_value())
        return;

    UE_LOG(LogOGNet, Log, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    auto deliver = [this](unsigned int id, uint32 captureTick,
                          const simulatableBrawler::PlayerInput& input) -> bool
    {
        const auto it = m_delayedInputComponentsById.find(id);
        if (it == m_delayedInputComponentsById.end())
            return false;

        if (it->second.Get() == nullptr)
        {
            m_delayedInputComponentsById.erase(it);
            return false;
        }

        deliverRemoteInput(id, captureTick, input);
        return true;
    };

    m_receptionCoordinator->releaseDelayedInputs<SimulatableBrawler>(
        firstTick, numSteps, deliver);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    SimCharacterId simId, USimmableUpdateComponent& owner, bool isAuthority)
{
    const unsigned int id = toStorageKey(simId);

    // ⛔G-73  docs/SimulationManagerUImpl-guards.md
    if (m_storage.has<SimulatableBrawler>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

    unregisterSimulatable<SimulatableBrawler>(
        m_storage, m_reconciliation, m_inputResolution, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

    if (m_receptionCoordinator.has_value())
    {
        m_receptionCoordinator->forgetOwner(id);
    }
    m_delayedInputComponentsById.erase(id);

    m_relayWriteProbe.forgetOwner(id);

    m_authorityRegisteredIds.erase(id);

    // ⛔G-74  docs/SimulationManagerUImpl-guards.md
    m_spawnSlots.release(id);

    m_inputHistory.forgetCharacter(id);

    if (m_latencyProbe)
        m_latencyProbe->forgetLane(id);
    m_latencyLastHandedCorrectionTick.erase(id);

#if !OG_PHYSICS_BACKEND_CHAOS
    m_renderTargetsById.erase(id);
    if (const AActor* ownerActor = owner.GetOwner())
        releaseJoltSlot(*ownerActor);
#endif

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

#if OG_PHYSICS_BACKEND_CHAOS
void ASimulationManagerUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = GetWorld();
	asyncInput->m_manager = this;

	// ⛔G-71  docs/SimulationManagerUImpl-guards.md
	const int32 firstUpcomingSimTick =
		static_cast<int32>(m_chaosTickMapper.toSimulationTick(static_cast<int32_t>(PhysicsStep))) + 1;

	onFrameStepsDue_GameThread(firstUpcomingSimTick, NumSteps);

	if (m_receptionCoordinator.has_value())
	{
		releaseDelayedInputsForStep(firstUpcomingSimTick, NumSteps);
		m_receptionCoordinator->reapConnections(firstUpcomingSimTick);
	}
}
#endif

void ASimulationManagerUImpl::createLatencyBudget(bool isAuthority)
{
	m_latencyProbe = std::make_unique<latencyBudget::LatencyBudgetProbe>(
		isAuthority ? latencyBudget::kServerHops : latencyBudget::kClientHops);
	m_latencyMailbox      = std::make_unique<LatencyMailbox>();
	m_latencyClockMailbox = std::make_unique<LatencyClockMailbox>();
}

BrawlerInputProviderFn ASimulationManagerUImpl::wrapInputProviderWithLatencyStamp(
	unsigned int id, const USimmableUpdateComponent& owner, BrawlerInputProviderFn inputProvider)
{
	if (!inputProvider)
		return inputProvider;

	const UOGBrawlerInputCollectionComponent* inputCollection = owner.getOwnerInputCollection();
	BrawlerInputProviderFn wrapped =
		[this, id, inputCollection, inner = std::move(inputProvider)](
			const SimulationTimeStep& step,
			const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache)
		{
			simulatableBrawler::PlayerInput input = inner(step, localInputCache);
			const double capturedSeconds = FPlatformTime::Seconds();
			const uint32 tick = step.getTick();
			const double sampledSeconds =
				inputCollection != nullptr ? inputCollection->getInputSampledSeconds() : 0.0;
			if (sampledSeconds > 0.0)
			{
				postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, tick, latencyBudget::Point::InputSampled, sampledSeconds));
			}
			postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
				id, tick, latencyBudget::Point::Captured, capturedSeconds));
			return input;
		};

	checkf(static_cast<bool>(wrapped),
		TEXT("wrapInputProviderWithLatencyStamp: a present provider must stay present. Provider ")
		TEXT("presence is the core's local-vs-remote identity test, so the latency wrapper may ")
		TEXT("never turn an empty provider into a callable or a callable into an empty one."));
	return wrapped;
}

void ASimulationManagerUImpl::stampLatency(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point)
{
	checkf(IsInGameThread(),
		TEXT("ASimulationManagerUImpl::stampLatency is the GAME-THREAD door to the latency probe; ")
		TEXT("the physics thread posts through postLatencyEvent_Internal's mailbox instead."));
	if (m_latencyProbe)
		m_latencyProbe->stamp(stream, lane, tick, point, FPlatformTime::Seconds());
}

void ASimulationManagerUImpl::noteLatencyWireSample(double roundTripSeconds)
{
	checkf(IsInGameThread(),
		TEXT("ASimulationManagerUImpl::noteLatencyWireSample is game-thread only: the probe has one owner."));
	if (m_latencyProbe)
		m_latencyProbe->addSample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5);
}

void ASimulationManagerUImpl::postLatencyEvent_Internal(const latencyBudget::Event& event)
{
	if (m_latencyMailbox)
		m_latencyMailbox->tryPush(event);
}

void ASimulationManagerUImpl::stampLatencyAfterStep_Internal(double stepStartSeconds)
{
	if (!m_latencyMailbox || !m_manager.has_value())
		return;

	const uint32 tick = m_manager->currentIntegratedTick();

	if (!m_manager->runsPrediction())
	{
		m_storage.forEachSimulatable<SimulatableBrawler>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32 captureTick =
					m_inputResolution.getLastUsedCaptureTick<SimulatableBrawler>(id);
				if (captureTick != kNoInputCaptureTick)
				{
					postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, captureTick, latencyBudget::Point::Consumed, stepStartSeconds));
				}
			});
		return;
	}

	postLatencyEvent_Internal(latencyBudget::Event::stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::ClientReceived, latencyBudget::Point::Consumed, stepStartSeconds));

	m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_inputResolution.getDiagnostics().relayedReadObservations<SimulatableBrawler>(id);
			if (ring == nullptr)
				return;
			for (std::size_t index = 0; index < ring->size(); ++index)
			{
				const RelayedReadObservation* const observation = ring->at(index);
				if (observation == nullptr || observation->simTick != tick)
					continue;
				if (observation->hasAppliedCaptureTick)
				{
					postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, observation->appliedCaptureTick, latencyBudget::Point::Consumed,
						stepStartSeconds));
				}
				return;
			}
		});

	if (m_latencyClockMailbox)
	{
		const ClientPredictionClock& clock = m_manager->getClientClock();
		LatencyClockCounts counts;
		counts.skips       = clock.getDiagnostics().skipCount();
		counts.stalls      = clock.getDiagnostics().stallCount();
		counts.hardResyncs = clock.getDiagnostics().hardResyncCount();
		if (!(counts == m_latencyClockPosted_Physics) && m_latencyClockMailbox->tryPush(counts))
			m_latencyClockPosted_Physics = counts;
	}
}

void ASimulationManagerUImpl::stampLatencyStepEnd_Internal()
{
	if (!m_latencyMailbox || !m_manager.has_value())
		return;

	const double stepEndSeconds = FPlatformTime::Seconds();
	const uint32 tick = m_manager->currentIntegratedTick();
	m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}

void ASimulationManagerUImpl::onLatencyPostTickFlush()
{
	if (!m_latencyProbe)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_latencyProbe->stampPending(latencyBudget::Stream::Input,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
	m_latencyProbe->stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
}

void ASimulationManagerUImpl::tickLatencyBudget_GameThread()
{
	if (!m_latencyProbe || !m_latencyMailbox || !m_manager.has_value())
		return;

	m_latencyMailbox->drain([this](const latencyBudget::Event& event) { m_latencyProbe->apply(event); });
	m_latencyMailboxDropped += m_latencyMailbox->takeDroppedCount();
	if (m_latencyClockMailbox)
	{
		m_latencyClockMailbox->drain(
			[this](const LatencyClockCounts& counts) { m_latencyClockLatest = counts; });
		m_latencyMailboxDropped += m_latencyClockMailbox->takeDroppedCount();
	}

	const double nowSeconds = FPlatformTime::Seconds();
	const bool isClient = m_manager->runsPrediction();

	if (!isClient)
	{
		for (const auto& entry : m_delayedInputComponentsById)
		{
			USimmableUpdateComponent* component = entry.second.Get();
			if (component == nullptr)
				continue;
			const uint32 correctionTick =
				component->getSyncedCorrectionStateBuffer().readFromBuffer<uint32>(
					correctionStateBuffer::kTickOffset);
			if (correctionTick == 0u)
				continue;
			uint32& lastHanded = m_latencyLastHandedCorrectionTick[entry.first];
			if (lastHanded == correctionTick)
				continue;
			lastHanded = correctionTick;
			m_latencyProbe->stamp(latencyBudget::Stream::State, entry.first, correctionTick,
				latencyBudget::Point::Handed, nowSeconds);
		}
	}

	m_latencyProbe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

#if OG_PHYSICS_BACKEND_CHAOS
	if (isClient)
	{
		FChaosScene* scene = GetWorld() != nullptr ? GetWorld()->GetPhysicsScene() : nullptr;
		if (scene != nullptr)
			m_chaosDilationWindow.add(static_cast<double>(scene->GetNetworkDeltaTimeScale()));

		const UWorld* world = GetWorld();
		const APlayerController* localController =
			world != nullptr ? world->GetFirstPlayerController() : nullptr;
		if (localController != nullptr && localController->GetNetworkPhysicsTickOffsetAssigned())
		{
			const int32 offset = localController->GetNetworkPhysicsTickOffset();
			if (!m_lastPhysicsTickOffset.has_value() || *m_lastPhysicsTickOffset != offset)
			{
				m_lastPhysicsTickOffset = offset;
				++m_physicsTickOffsetResets;
			}
		}
	}
#endif

	latencyBudget::WindowReport report;
	if (!m_latencyProbe->closeWindowIfDue(nowSeconds, report))
		return;

	const TCHAR* role = isClient ? TEXT("Client") : TEXT("Server");
	constexpr double kMillis = 1000.0;
	for (std::size_t index = 0; index < report.hopCount; ++index)
	{
		const latencyBudget::HopSummary& hop = report.hops[index];
		UE_LOG(LogOGLatencyBudget, Warning,
			TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
			TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u"),
			role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
			hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
			hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow);
	}
	UE_LOG(LogOGLatencyBudget, Warning,
		TEXT("[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u ")
		TEXT("mailboxDropped=%llu"),
		role, report.windowEndSeconds - report.windowStartSeconds, report.staleStamps,
		report.duplicateStamps, report.laneOverflow,
		static_cast<unsigned long long>(m_latencyMailboxDropped));
	m_latencyMailboxDropped = 0u;

#if OG_PHYSICS_BACKEND_CHAOS
	if (isClient)
	{
		UE_LOG(LogOGChaosDilation, Warning,
			TEXT("[ChaosDilation] role=Client min=%.4f mean=%.4f max=%.4f samples=%u resets=%u ")
			TEXT("offset=%d skips=%u stalls=%u hardResyncs=%u"),
			m_chaosDilationWindow.min, m_chaosDilationWindow.mean(), m_chaosDilationWindow.max,
			m_chaosDilationWindow.samples, m_physicsTickOffsetResets,
			m_lastPhysicsTickOffset.value_or(0),
			m_latencyClockLatest.skips - m_latencyClockAtWindowStart.skips,
			m_latencyClockLatest.stalls - m_latencyClockAtWindowStart.stalls,
			m_latencyClockLatest.hardResyncs - m_latencyClockAtWindowStart.hardResyncs);
		m_chaosDilationWindow.reset();
		m_physicsTickOffsetResets   = 0u;
		m_latencyClockAtWindowStart = m_latencyClockLatest;
	}
#endif
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
