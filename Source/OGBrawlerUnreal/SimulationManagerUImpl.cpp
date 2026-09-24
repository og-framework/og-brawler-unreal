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

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGatePolicy.h"

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
DEFINE_LOG_CATEGORY(LogOGBrawler);

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

namespace
{
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
	void bindCorrectionFieldDiffGate()
	{
		// ⛔G-50  docs/SimulationManagerUImpl-guards.md
		correctionFieldDiff::setEnabledPredicate(
			[]() { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
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

}

OGSIM_OPTIMIZE_OFF

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

	input->m_manager->onGameSimulation(updateInfo);
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
	Chaos::FRewindData* rewindData = solver.GetRewindData();
	if (rewindData == nullptr)
		return;

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
		Chaos::FGeometryParticleHandle* handle = proxy->GetHandle_LowLevel();
		if (handle == nullptr)
		{
			if (pushProbeActive)
				++m_pushProbeStashUnresolved;
			return;
		}

		LivePushProbeRead beforePush;
		if (pushProbeActive)
			beforePush = readLiveBodyForPushProbe(*proxy);

		rewindData->SetTargetStateAtFrame(
			*handle, PhysicsStep,
			Chaos::FFrameAndPhase::EParticleHistoryPhase::PostPushData,
			uglm::toFVector(bs.position),
			uglm::toFQuat(bs.rotation),
			uglm::toFVector(bs.linearVelocity),
			uglm::toFVector(bs.angularVelocity),
			/*bShouldSleep=*/false);

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

ASimulationManagerUImpl* ASimulationManagerUImpl::s_instances[] = {nullptr, nullptr};

ASimulationManagerUImpl::ASimulationManagerUImpl()
{
    bReplicates = false;
}

ASimulationManagerUImpl::~ASimulationManagerUImpl()
{
	if (GetWorld() != nullptr)
		GetWorld()->GetPhysicsScene()->OnPhysSceneStep.RemoveAll(this);

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

	FPhysScene* physScene = uWorld->GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	const ENetMode worldNetMode = GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);

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
		Chaos::FPBDRigidsSolver& rigidsSolverS = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverS);
		m_physReaderAdapter.emplace(rigidsSolverS);
		emplaceBrawlerQueryAdapter(m_queryAdapter, uWorld);
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(false, solver->GetAsyncDeltaTime(), ManagerType::Params{
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
		Chaos::FPBDRigidsSolver& rigidsSolverC = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverC);
		m_physReaderAdapter.emplace(rigidsSolverC);
		emplaceBrawlerQueryAdapter(m_queryAdapter, uWorld);
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(/*usePrediction=*/true, solver->GetAsyncDeltaTime(), ManagerType::Params{
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

		UE_LOG(LogOGResimProbe, Warning,
			TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
			ToString(LogOGResimProbe.GetVerbosity()),
			LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
			static_cast<uint32>(kResimGateProbeWindowSamples));
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

	physScene->OnPhysScenePreTick.AddUObject(this, &ASimulationManagerUImpl::OnPhysicsPreTick);
	physScene->OnPhysSceneStep.AddUObject(this, &ASimulationManagerUImpl::OnPhysicsStep);
	m_hysScenePostTickCallbackHandle = physScene->OnPhysScenePostTick.AddUObject(this, &ASimulationManagerUImpl::OnPostPhysicsStep);

	m_asyncCallback = solver->CreateAndRegisterSimCallbackObject_External<FSimulationManagerAsyncCallback>();
	m_asyncCallback->setManager(this);

	FNetworkPhysicsCallback* solverCallback = static_cast<FNetworkPhysicsCallback*>(solver->GetRewindCallback());
	if (solverCallback == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	m_injectInputsExternalCallbackHandle = solverCallback->InjectInputsExternal.AddUObject(this, &ASimulationManagerUImpl::InjectInputs_External);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

void ASimulationManagerUImpl::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
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

void ASimulationManagerUImpl::OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPhysicsStep(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPostPhysicsStep(FChaosScene* Scene)
{
	onPostSimulationGameThread();

	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}

	updateVisualizationAll(m_storage);

	if (m_manager.has_value() && m_systemsExec.has_value() && !m_manager->runsPrediction())
	{
		pushRingoutScoresToCharacters(m_systemsExec->get<brawlerRingout::ScoreSystem>(),
			m_delayedInputComponentsById);
	}
}

TryRegisterStatus ASimulationManagerUImpl::tryRegister(
    unsigned int id,
    SimulatableBrawler simulatable,
    USimmableUpdateComponent& owner,
    BrawlerInputProviderFn inputProvider,
    bool isAuthority)
{
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
            /*inputProvider=*/std::move(record.inputProvider));
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

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

void ASimulationManagerUImpl::noteDelayedInputComponent(
    unsigned int id, USimmableUpdateComponent& component)
{
    m_delayedInputComponentsById[id] = &component;
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

void ASimulationManagerUImpl::releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps)
{

    // ⛔G-71  docs/SimulationManagerUImpl-guards.md
    const int32 firstUpcomingSimTick =
        static_cast<int32>(m_chaosTickMapper.toSimulationTick(static_cast<int32_t>(physicsStep))) + 1;

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
        firstUpcomingSimTick, numSteps, deliver);

    m_receptionCoordinator->reapConnections(firstUpcomingSimTick);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    unsigned int id, USimmableUpdateComponent& owner, bool isAuthority)
{
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

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

void ASimulationManagerUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = GetWorld();
	asyncInput->m_manager = this;

	releaseDelayedInputsForStep(PhysicsStep, NumSteps);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
