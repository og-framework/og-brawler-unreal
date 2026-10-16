// SPDX-License-Identifier: BUSL-1.1
// docs/JoltBackendUImpl-rationale.md · docs/JoltBackendUImpl-guards.md

#if !OG_PHYSICS_BACKEND_CHAOS
#include "JoltBackendUImpl.h"

#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGSimulation/CompilerControl.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "Components/CapsuleComponent.h"
#include "HAL/IConsoleManager.h"
#include "Async/UniqueLock.h"
#include "Misc/StringBuilder.h"
#include "OGSimulation/BodySlotAssignment.h"
#include "OGSimulationJolt/JoltLayerTable.h"
#include "OGSimulationJolt/JoltStaticWorldBuilder.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"

#include <algorithm>
#include <array>
#include <vector>

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

namespace
{
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

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionSeconds(
		TEXT("og.Render.ErrorCorrectionSeconds"),
		0.3f,
		TEXT("Jolt configuration only. How long a resimulation's correction of a rendered body is blended out, in seconds, ")
		TEXT("converted to physics steps as Chaos converts p.RenderInterp.ErrorCorrectionDuration (0.3 there and here). ")
		TEXT("0 = no blending: every correction shows as a pop."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionMaxCm(
		TEXT("og.Render.ErrorCorrectionMaxCm"),
		250.f,
		TEXT("Jolt configuration only. A correction of a body by this many centimetres or more is not blended: the body ")
		TEXT("snaps to the corrected pose. Chaos's p.RenderInterp.MaximumErrorCorrectionBeforeSnapping (250 there and here)."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionDesyncSeconds(
		TEXT("og.Render.ErrorCorrectionDesyncSeconds"),
		0.6f,
		TEXT("Jolt configuration only. A correction shorter than the distance the body covers in this many seconds at its ")
		TEXT("corrected speed is blended even above og.Render.ErrorCorrectionMaxCm. Chaos's ")
		TEXT("p.RenderInterp.MaximumErrorCorrectionDesyncTimeBeforeSnapping (0.6 there and here). 0 or less = off."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarJoltContactCounts(
		TEXT("og.Jolt.ContactCounts"),
		0,
		TEXT("Jolt configuration only. 1 = count the step world's contacts with a passive Jolt contact listener and log one ")
		TEXT("[SimHost.Contacts] line per simulated step, forward and replayed, that touched a contact (LogOGSimHost, Verbose). ")
		TEXT("Read once, when the step world is built at the simulation manager's BeginPlay, so set it with ")
		TEXT("-ini:Engine:[ConsoleVariables]:og.Jolt.ContactCounts=1, not a later console command. 0 = nothing is installed."),
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
}

OGSIM_OPTIMIZE_OFF

void JoltBackendUImpl::buildWorld(UWorld& world, float, bool worldIsAuthority)
{
	decideJoltSteppingMode(worldIsAuthority);
	buildJoltWorld(world, worldIsAuthority);
}

void JoltBackendUImpl::startStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	startJoltStepping(world, dt, worldIsAuthority);
}

void JoltBackendUImpl::onEndPlayBegin()
{
	m_frameHost.unregisterTickFunctions();
	// ⛔G-01  docs/JoltBackendUImpl-guards.md
	m_frameHost.waitForOutstandingSteps();
}

void JoltBackendUImpl::beforePresent_GameThread()
{
	syncRenderPoses_GameThread();
}

void JoltBackendUImpl::updateVisualization_GameThread()
{
	updateVisualizationAtRenderPoses_GameThread();
}

physicsBackendUImpl::VizQuery& JoltBackendUImpl::editVizQuery()
{
	if (m_worlds.m_joltStepsInline)
	{
#if !UE_BUILD_SHIPPING
		++m_shadowWindow.vizQueryStep;
#endif
		return m_owner.m_queryAdapter.value();
	}
#if !UE_BUILD_SHIPPING
	++m_shadowWindow.vizQueryShadow;
#endif
	return m_worlds.m_shadowQuery.value();
}

const physicsBackendUImpl::VizReader& JoltBackendUImpl::getVizReader() const
{
	if (m_worlds.m_joltStepsInline)
	{
#if !UE_BUILD_SHIPPING
		++m_shadowWindow.vizReaderStep;
#endif
		return m_owner.m_physReaderAdapter.value();
	}
#if !UE_BUILD_SHIPPING
	++m_shadowWindow.vizReaderShadow;
#endif
	return m_worlds.m_shadowReader.value();
}

std::optional<QueryVolumeId> JoltBackendUImpl::registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
{
	const std::optional<uint32> slot = tryAcquireJoltSlot(owner);
	if (!slot.has_value())
		return std::nullopt;

	if (!m_worlds.m_joltStepsInline)
		return m_worlds.m_shadowQuery->registerVolume(descriptor, joltSlotRootBodyId(*m_worlds.m_shadowWorld, *slot));

	return m_owner.m_queryAdapter->registerVolume(descriptor, joltSlotRootBodyId(*m_worlds.m_joltWorld, *slot));
}

bool JoltBackendUImpl::createBodies_GameThread(USimmableUpdateComponent& owner, AOGBrawlerUECharacter& character,
	SimulatableBrawler& simulatable, unsigned int id)
{
	{
		const UCapsuleComponent* capsule = character.GetCapsuleComponent();
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
	const std::optional<uint32> acquiredSlot = tryAcquireJoltSlot(*ownerActor);
	if (!acquiredSlot.has_value())
		return false;
	const uint32 slot = *acquiredSlot;
	BodyId parentBodyId;
	{
		const double worldMutexWaitStartSeconds = FPlatformTime::Seconds();
		// ⛔G-02  docs/JoltBackendUImpl-guards.md
		JoltWorldLockScopeUImpl worldLock(m_worlds.m_joltWorldMutex, m_worlds.m_joltRoleBit);
		m_frameHost.noteWorldMutexWait_GameThread(FPlatformTime::Seconds() - worldMutexWaitStartSeconds);

		JoltPhysicsFactory factory(*m_owner.m_physAdapter, m_worlds.m_joltSlotTemplate, slot, id, JoltPhysicsFactoryOptions{},
			[this](BodyId body, uint32_t shapeIndex, std::optional<BodyId> rootBodyId)
			{
				return m_owner.m_queryAdapter->registerShape(body, shapeIndex, rootBodyId);
			});
		parentBodyId = factory.parentBodyId();

		simulatable.editPhysicsComposite().forEach([&](auto& decl)
		{
			using D = std::decay_t<decltype(decl)>;
			const auto& subStaticData = D::staticDataOf(staticData);
			auto r = factory.createPhysicalObject(D::descriptor(), D::name);
			decl.bindings.ownBodyId        = r.bodyId;
			decl.bindings.parentBodyId     = parentBodyId;
			decl.bindings.attachmentOffset = D::attachmentOffset(subStaticData);
			decl.bindings.shapeIds         = std::move(r.shapeIds);
			for (const auto& volDesc : D::queryVolumes(subStaticData))
				decl.bindings.queryVolumeIds.push_back(m_owner.m_queryAdapter->registerVolume(volDesc, parentBodyId));
		});
	}

	UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Bind] bind id=%u slot=%u body0=%u boundBodies=%u slotsInUse=%d"),
		id, slot, m_worlds.m_joltWorld->slotBodyId(slot, 0u).GetIndex(), boundJoltBodyCount(),
		static_cast<int32>(m_worlds.m_joltSlotByOwner.size()));

	if (m_worlds.m_shadowWorld.has_value())
		bindShadowSlot(slot, id, simulatable);
	return true;
}

void JoltBackendUImpl::onRegistered_GameThread(USimmableUpdateComponent& owner, unsigned int id)
{
	const auto slot = m_worlds.m_joltSlotByOwner.find(owner.GetOwner());
	checkf(slot != m_worlds.m_joltSlotByOwner.end(),
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
			m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, capsule->IsSimulatingPhysics() ? 1 : 0,
			*simulating);
	}
}

void JoltBackendUImpl::onUnregistered_GameThread(USimmableUpdateComponent& owner, unsigned int id)
{
	m_renderTargetsById.erase(id);
	if (const AActor* ownerActor = owner.GetOwner())
		releaseJoltSlot(*ownerActor, id);
}

void JoltBackendUImpl::decideJoltSteppingMode(bool worldIsAuthority)
{
	m_worlds.m_joltStepsInline = worldIsAuthority || CVarSimRunInline.GetValueOnGameThread() != 0;
	m_worlds.m_joltRoleBit     = worldIsAuthority ? kJoltAuthorityRoleBit : kJoltClientRoleBit;

	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Mode] role=%s stepping=%s viz=%s"),
		worldIsAuthority ? TEXT("Authority") : TEXT("Client"),
		m_worlds.m_joltStepsInline ? TEXT("inline") : TEXT("worker"),
		m_worlds.m_joltStepsInline ? TEXT("stepWorld") : TEXT("shadowWorld"));
}

void JoltBackendUImpl::buildJoltWorld(UWorld& world, bool worldIsAuthority)
{
	m_worlds.m_joltRuntime.emplace(&logJoltToSimHost);

	const UEStaticImportResult import = UEStaticGeometryImporter(brawlerCategoryChannels()).importWorld(world);
	UEStaticGeometryImporter::logReport(import.report, worldIsAuthority ? TEXT("authority") : TEXT("client"));

	StaticWorldDescription queryableStatics;
	StaticWorldDescription physicsOnlyStatics;
	for (std::size_t index = 0; index < import.description.shapes.size(); ++index)
	{
		StaticWorldDescription& target = import.physicsOnly[index] != 0u ? physicsOnlyStatics : queryableStatics;
		target.shapes.push_back(import.description.shapes[index]);
	}

	m_worlds.m_joltSlotTemplate = brawlerSlotTemplate();
	const auto rootTemplate = std::find_if(m_worlds.m_joltSlotTemplate.begin(), m_worlds.m_joltSlotTemplate.end(),
		[](const SlotBodyTemplate& body) { return body.descriptor.body.isRoot; });
	checkf(rootTemplate != m_worlds.m_joltSlotTemplate.end()
			&& std::none_of(rootTemplate + 1, m_worlds.m_joltSlotTemplate.end(),
				[](const SlotBodyTemplate& body) { return body.descriptor.body.isRoot; }),
		TEXT("buildJoltWorld: the brawler's physics composite must declare exactly one isRoot body (the capsule the ")
		TEXT("render apply moves)."));
	m_joltRootDeclarationIndex = static_cast<uint8>(rootTemplate - m_worlds.m_joltSlotTemplate.begin());

	JoltWorldConfig config;
	config.simulatableSlots = kMaxSimulatableSlots;
	config.slotTemplate     = m_worlds.m_joltSlotTemplate;
	config.staticLayers     = staticLayerKeysOf(queryableStatics);
	const std::vector<JoltLayerKey> physicsOnlyLayers = staticLayerKeysOf(physicsOnlyStatics);
	config.staticLayers.insert(config.staticLayers.end(), physicsOnlyLayers.begin(), physicsOnlyLayers.end());
	config.ringDepthTicks   = worldIsAuthority ? 0u : static_cast<uint32_t>(TimeConfig{}.rollbackWindowHardCap) + 2u;
	config.gravityCmPerS2   = glm::vec3(0.f, 0.f, m_owner.m_staticData.m_movementStaticData.gravity);

	std::vector<JPH::BodyID> stepStaticBodies;
	{
		JoltWorldLockScopeUImpl worldLock(m_worlds.m_joltWorldMutex, m_worlds.m_joltRoleBit);

		m_worlds.m_joltWorld.emplace(m_worlds.m_joltRuntime->runtime, config, &logJoltToSimHost);
		if (CVarJoltContactCounts.GetValueOnGameThread() == 1)
		{
			m_worlds.m_joltContactCounter.emplace();
			m_contactSteps_Step.reserve(config.ringDepthTicks + 1u);
			m_worlds.m_joltWorld->physics().SetContactListener(&*m_worlds.m_joltContactCounter);
			m_worlds.m_joltWorld->setAfterStepObserver(&JoltBackendUImpl::onJoltStepWorldStepped_Step, this);
		}

		JoltStaticWorldBuilder queryableBuilder(*m_worlds.m_joltWorld, &logJoltToSimHost);
		queryableBuilder.build(queryableStatics);
		JoltStaticWorldBuilder physicsOnlyBuilder(*m_worlds.m_joltWorld, &logJoltToSimHost);
		physicsOnlyBuilder.build(physicsOnlyStatics);

		m_owner.m_physAdapter.emplace(*m_worlds.m_joltWorld);
		m_owner.m_physReaderAdapter.emplace(*m_worlds.m_joltWorld, m_owner.m_physAdapter->bindTable());
		m_owner.m_queryAdapter.emplace(*m_worlds.m_joltWorld, &logJoltToSimHost, JoltSpatialQueryConfig{
			brawlerMappedCategories(),
			joltWorldAccessCheckFor(m_worlds.m_joltRoleBit, m_worlds.m_joltStepsInline) });
		m_owner.m_queryAdapter->excludeStaticBodiesFromQueries(physicsOnlyBuilder.stats().bodies);

		stepStaticBodies = queryableBuilder.stats().bodies;
		stepStaticBodies.insert(stepStaticBodies.end(), physicsOnlyBuilder.stats().bodies.begin(),
			physicsOnlyBuilder.stats().bodies.end());

		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.World] slots=%u bodiesPerSlot=%u ringDepthTicks=%u statics=%d physicsOnlyStatics=%d staticBodies=%d physicsOnlyBodies=%d"),
			m_worlds.m_joltWorld->simulatableSlots(), m_worlds.m_joltWorld->bodiesPerSlot(), config.ringDepthTicks,
			static_cast<int32>(queryableStatics.shapes.size()), static_cast<int32>(physicsOnlyStatics.shapes.size()),
			static_cast<int32>(queryableBuilder.stats().bodies.size()),
			static_cast<int32>(physicsOnlyBuilder.stats().bodies.size()));
	}

	if (!m_worlds.m_joltStepsInline)
		buildShadowWorld(config, queryableStatics, physicsOnlyStatics, stepStaticBodies);

	const bool stepListener   = m_worlds.m_joltWorld->physics().GetContactListener() != nullptr;
	const bool shadowListener = m_worlds.m_shadowWorld.has_value() && m_worlds.m_shadowWorld->physics().GetContactListener() != nullptr;
	UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Contacts] installed=%d stepListener=%s shadowListener=%s reserved=%u"),
		m_worlds.m_joltContactCounter.has_value() ? 1 : 0, stepListener ? TEXT("set") : TEXT("null"),
		!m_worlds.m_shadowWorld.has_value() ? TEXT("none") : shadowListener ? TEXT("set") : TEXT("null"),
		static_cast<uint32>(m_contactSteps_Step.capacity()));
	checkf(stepListener == m_worlds.m_joltContactCounter.has_value() && !shadowListener,
		TEXT("buildJoltWorld: the contact counter must be the step world's listener exactly when og.Jolt.ContactCounts ")
		TEXT("was 1 at world build, and the game-thread shadow world never has one (step listener %d, counter %d, shadow ")
		TEXT("listener %d)."),
		stepListener ? 1 : 0, m_worlds.m_joltContactCounter.has_value() ? 1 : 0, shadowListener ? 1 : 0);
}

void JoltBackendUImpl::buildShadowWorld(const JoltWorldConfig& stepConfig,
	const StaticWorldDescription& queryableStatics, const StaticWorldDescription& physicsOnlyStatics,
	const std::vector<JPH::BodyID>& stepStaticBodies)
{
	checkf(m_worlds.m_joltWorld->ring().slotBytes() == JoltWorldConfig{}.ringSlotBytes,
		TEXT("buildShadowWorld: the step world's ring slots hold %u bytes, but the shadow hand-over's pooled slots ")
		TEXT("are pre-reserved to JoltWorldConfig's default %u. Keep the two equal, or a hand-over allocates."),
		m_worlds.m_joltWorld->ring().slotBytes(), JoltWorldConfig{}.ringSlotBytes);

	JoltWorldConfig shadowConfig = stepConfig;
	shadowConfig.ringDepthTicks     = 0u;
	shadowConfig.tempAllocatorBytes = kShadowTempAllocatorBytes;

	// ⛔G-03  docs/JoltBackendUImpl-guards.md
	m_worlds.m_shadowWorld.emplace(m_worlds.m_joltRuntime->runtime, shadowConfig, &logJoltToSimHost);

	JoltStaticWorldBuilder queryableBuilder(*m_worlds.m_shadowWorld, &logJoltToSimHost);
	queryableBuilder.build(queryableStatics);
	JoltStaticWorldBuilder physicsOnlyBuilder(*m_worlds.m_shadowWorld, &logJoltToSimHost);
	physicsOnlyBuilder.build(physicsOnlyStatics);

	m_worlds.m_shadowBodyAdapter.emplace(*m_worlds.m_shadowWorld);
	m_worlds.m_shadowReader.emplace(*m_worlds.m_shadowWorld, m_worlds.m_shadowBodyAdapter->bindTable());
	m_worlds.m_shadowQuery.emplace(*m_worlds.m_shadowWorld, &logJoltToSimHost, JoltSpatialQueryConfig{
		brawlerMappedCategories(),
		&joltShadowAccessOnGameThread });
	m_worlds.m_shadowQuery->excludeStaticBodiesFromQueries(physicsOnlyBuilder.stats().bodies);

	std::vector<JPH::BodyID> shadowStaticBodies = queryableBuilder.stats().bodies;
	shadowStaticBodies.insert(shadowStaticBodies.end(), physicsOnlyBuilder.stats().bodies.begin(),
		physicsOnlyBuilder.stats().bodies.end());

	const uint32 stepBodies =
		m_worlds.m_joltWorld->simulatableSlots() * m_worlds.m_joltWorld->bodiesPerSlot() + static_cast<uint32>(stepStaticBodies.size());
	const uint32 shadowBodies =
		m_worlds.m_shadowWorld->simulatableSlots() * m_worlds.m_shadowWorld->bodiesPerSlot() + static_cast<uint32>(shadowStaticBodies.size());
	const bool sameStatics = shadowStaticBodies == stepStaticBodies;
	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Shadow] built bodies step=%u shadow=%u sameStatics=%d slots=%u bodiesPerSlot=%u ringDepthTicks=%u tempAllocatorBytes=%u"),
		stepBodies, shadowBodies, sameStatics ? 1 : 0, m_worlds.m_shadowWorld->simulatableSlots(), m_worlds.m_shadowWorld->bodiesPerSlot(),
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

std::optional<uint32> JoltBackendUImpl::tryAcquireJoltSlot(AActor& owner)
{
	if (const auto found = m_worlds.m_joltSlotByOwner.find(&owner); found != m_worlds.m_joltSlotByOwner.end())
		return found->second;

	AOGBrawlerUECharacter* const character = Cast<AOGBrawlerUECharacter>(&owner);
	checkf(character != nullptr,
		TEXT("tryAcquireJoltSlot: '%s' is not an AOGBrawlerUECharacter. The slot the authority chooses reaches the ")
		TEXT("clients on that class's replicated PhysicsSlotValue, so only that class can hold a Jolt slot."),
		*owner.GetName());

	std::array<bool, kMaxSimulatableSlots> taken{};
	for (const auto& entry : m_worlds.m_joltSlotByOwner)
		taken[entry.second] = true;
	const auto lowestFree = std::find(taken.begin(), taken.end(), false);

	const bool isAuthority = m_owner.GetNetMode() != NM_Client;
	// ⛔G-05  docs/JoltBackendUImpl-guards.md
	const BodySlotDecision decision = decideBodySlot(isAuthority, character->GetPhysicsSlot(), taken);

	if (decision.outcome == BodySlotOutcome::WaitForReplication || decision.outcome == BodySlotOutcome::WaitForRelease)
	{
		const bool forReplication = decision.outcome == BodySlotOutcome::WaitForReplication;
		uint8& loggedWaits = m_worlds.m_joltSlotWaitsLoggedByOwner[&owner];
		const uint8 waitBit = forReplication ? 1u : 2u;
		if ((loggedWaits & waitBit) == 0u)
		{
			loggedWaits |= waitBit;
			FString heldBy = TEXT("-");
			for (const auto& entry : m_worlds.m_joltSlotByOwner)
			{
				if (!forReplication && entry.second == decision.slot)
					heldBy = entry.first->GetName();
			}
			UE_LOG(LogOGSimHost, Log, TEXT("[SimHost.Slot] wait=%s owner=%s slot=%d heldBy=%s"),
				forReplication ? TEXT("WaitForReplication") : TEXT("WaitForRelease"), *owner.GetName(),
				forReplication ? -1 : static_cast<int32>(decision.slot), *heldBy);
		}
		return std::nullopt;
	}

	checkf(decision.outcome == BodySlotOutcome::Assigned,
		TEXT("tryAcquireJoltSlot: all %u Jolt slots are taken; '%s' cannot get one. The Jolt world has a fixed ")
		TEXT("body set of kMaxSimulatableSlots characters (design D7, task 10)."),
		kMaxSimulatableSlots, *owner.GetName());
	if (decision.outcome != BodySlotOutcome::Assigned)
		return std::nullopt;

	if (isAuthority)
		character->SetAuthoritativePhysicsSlot(decision.slot);
	m_worlds.m_joltSlotByOwner.emplace(&owner, decision.slot);
	m_worlds.m_joltSlotWaitsLoggedByOwner.erase(&owner);

	UE_LOG(LogOGSimHost, Log, TEXT("[SimHost.Slot] assigned role=%s owner=%s slot=%u lowestFreeLocally=%d"),
		isAuthority ? TEXT("Authority") : TEXT("Client"), *owner.GetName(), decision.slot,
		lowestFree != taken.end() ? static_cast<int32>(lowestFree - taken.begin()) : -1);
	return decision.slot;
}

void JoltBackendUImpl::releaseJoltSlot(const AActor& owner, unsigned int id)
{
	m_worlds.m_joltSlotWaitsLoggedByOwner.erase(&owner);

	const auto found = m_worlds.m_joltSlotByOwner.find(&owner);
	if (found == m_worlds.m_joltSlotByOwner.end())
		return;

	const uint32 slot = found->second;
	m_frameHost.pushOccupancy_GameThread(slot, false);
	m_owner.m_physAdapter->bindTable().releaseSlot(slot);
	m_worlds.m_joltSlotByOwner.erase(found);

	UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Bind] release id=%u slot=%u boundBodies=%u slotsInUse=%d"),
		id, slot, boundJoltBodyCount(), static_cast<int32>(m_worlds.m_joltSlotByOwner.size()));

	if (m_worlds.m_shadowWorld.has_value())
		releaseShadowSlot(slot);
}

BodyId JoltBackendUImpl::joltSlotRootBodyId(const JoltWorld& world, uint32 slot) const
{
	for (uint32 templateIndex = 0; templateIndex < m_worlds.m_joltSlotTemplate.size(); ++templateIndex)
	{
		if (m_worlds.m_joltSlotTemplate[templateIndex].descriptor.body.isRoot)
			return JoltPhysicsBodyAdapter::bodyIdOf(world.slotBodyId(slot, templateIndex));
	}
	checkf(false, TEXT("joltSlotRootBodyId: the slot template has no isRoot body"));
	return BodyId{};
}

uint32 JoltBackendUImpl::boundJoltBodyCount() const
{
	uint32 bound = 0u;
	for (uint32 slot = 0; slot < m_worlds.m_joltWorld->simulatableSlots(); ++slot)
	{
		for (uint32 templateIndex = 0; templateIndex < m_worlds.m_joltWorld->bodiesPerSlot(); ++templateIndex)
		{
			if (m_owner.m_physAdapter->bindTable().isBound(
					JoltPhysicsBodyAdapter::bodyIdOf(m_worlds.m_joltWorld->slotBodyId(slot, templateIndex))))
				++bound;
		}
	}
	return bound;
}

void JoltBackendUImpl::startJoltStepping(UWorld& world, float dt, bool worldIsAuthority)
{
	checkf(world.PersistentLevel != nullptr, TEXT("startJoltStepping: the world has no persistent level."));

	{
		JoltWorldLockScopeUImpl worldLock(m_worlds.m_joltWorldMutex, m_worlds.m_joltRoleBit);
		m_stepHooks.emplace(m_frameHost);
		m_stepDriver.emplace(*m_owner.m_manager, *m_worlds.m_joltWorld, *m_stepHooks, StepDriverConfig{ dt, ResimPolicy::OnRequest, 0u });
	}

	m_renderCorrectionStepSeconds = dt;
	m_frameHost.begin(m_owner, *this, *world.PersistentLevel,
		SimulationFrameHostUImpl::Config{ static_cast<double>(dt), worldIsAuthority, m_worlds.m_joltStepsInline });
}

void JoltBackendUImpl::runJoltStep_Step(uint64 physicsStep, double stepDeadlineSeconds)
{
	checkf(m_frameHost.isRunningBatchOnThisThread(),
		TEXT("runJoltStep_Step: called outside the frame host's step batch. It is the step driver's only door to ")
		TEXT("runTick: the frame host's batch is the one caller, so the steps stay serialized in the scheduler's ")
		TEXT("order and each step's deadline and offset belong to the batch the game thread dispatched."));
	checkf(!m_worlds.m_joltStepsInline || IsInGameThread(),
		TEXT("runJoltStep_Step: an inline step ran off the game thread. In the inline mode (the authority, and a client ")
		TEXT("under og.Sim.RunInline) the game thread owns the step driver: the authority's input release delivers into ")
		TEXT("UObjects, and the inline access predicates accept the game thread."));
	JoltWorldLockScopeUImpl worldLock(m_worlds.m_joltWorldMutex, m_worlds.m_joltRoleBit);
	JoltStepScopeUImpl      stepScope(m_worlds.m_joltRoleBit);
	m_stepHooks->currentStepDeadline = stepDeadlineSeconds;
	const TickOutcome outcome = m_stepDriver->runTick(physicsStep);
	if (m_worlds.m_joltContactCounter.has_value())
		logContactSteps_Step(physicsStep, outcome);
}

void JoltBackendUImpl::onJoltStepWorldStepped_Step(void* manager)
{
	static_cast<JoltBackendUImpl*>(manager)->recordContactStep_Step();
}

void JoltBackendUImpl::recordContactStep_Step()
{
	checkf(isJoltStepRunningOnThisThread(),
		TEXT("recordContactStep_Step: the step world stepped outside runJoltStep_Step. The contact lines take each ")
		TEXT("step's replay flag from its place in runTick's batch, so every step world step must run inside it."));
	checkf(m_contactSteps_Step.size() < m_contactSteps_Step.capacity(),
		TEXT("recordContactStep_Step: one runTick stepped the world more than the %u times reserved at world build ")
		TEXT("(the ring depth's replays plus the forward step)."),
		static_cast<uint32>(m_contactSteps_Step.capacity()));
	m_contactSteps_Step.push_back(ContactStepUImpl{ static_cast<uint32>(m_owner.m_manager->currentIntegratedTick()),
		m_worlds.m_joltWorld->physicsStepCount(), m_worlds.m_joltContactCounter->drain() });
}

void JoltBackendUImpl::logContactSteps_Step(uint64 physicsStep, const TickOutcome& outcome)
{
	const uint32 steps = static_cast<uint32>(m_contactSteps_Step.size());
	checkf(steps == outcome.replayedTicks + 1u,
		TEXT("logContactSteps_Step: one runTick drained %u world steps, but the step driver reports %u replayed ticks ")
		TEXT("and one forward step. Every step but the batch's last is logged as replayed."),
		steps, outcome.replayedTicks);
	if (UE_LOG_ACTIVE(LogOGSimHost, Verbose))
	{
		for (uint32 index = 0u; index < steps; ++index)
		{
			const ContactStepUImpl&      step   = m_contactSteps_Step[index];
			const JoltContactStepCounts& counts = step.counts;
			if (counts.manifolds() == 0u && counts.unrecordedIds == 0u && counts.unrecordedDeclarations == 0u)
				continue;
			TStringBuilder<512> ids;
			for (uint32 idIndex = 0u; idIndex < counts.idCount; ++idIndex)
			{
				const JoltContactIdCounts& id = counts.ids[idIndex];
				ids.Appendf(TEXT("%s%u:%u/%u/%u/%u/%u:0x%x"), idIndex == 0u ? TEXT("") : TEXT(","), id.simulatableId,
					id.pairCount(JoltContactPairClass::RootRoot), id.pairCount(JoltContactPairClass::RootChild),
					id.pairCount(JoltContactPairClass::RootStatic), id.pairCount(JoltContactPairClass::ChildChild),
					id.pairCount(JoltContactPairClass::ChildStatic), id.childDeclarations);
			}
			UE_LOG(LogOGSimHost, Verbose,
				TEXT("[Verbose][SimHost.Contacts] tick=%u step=%llu worldStep=%llu replay=%d manifolds=%u added=%u ")
				TEXT("rootRoot=%u rootChild=%u rootStatic=%u childChild=%u childStatic=%u staticStatic=%u ids=%s ")
				TEXT("idsDropped=%u declarationsDropped=%u"),
				step.tick, physicsStep, step.worldStep, index + 1u < steps ? 1 : 0, counts.manifolds(), counts.added,
				counts.pairCount(JoltContactPairClass::RootRoot), counts.pairCount(JoltContactPairClass::RootChild),
				counts.pairCount(JoltContactPairClass::RootStatic), counts.pairCount(JoltContactPairClass::ChildChild),
				counts.pairCount(JoltContactPairClass::ChildStatic), counts.pairCount(JoltContactPairClass::StaticStatic),
				counts.idCount == 0u ? TEXT("-") : ids.ToString(), counts.unrecordedIds, counts.unrecordedDeclarations);
		}
	}
	m_contactSteps_Step.clear();
}

bool JoltBackendUImpl::isJoltWorldMutexHeldOnThisThread() const
{
	return (t_joltWorldMutexHeldRoles & m_worlds.m_joltRoleBit) != 0u;
}

bool JoltBackendUImpl::isJoltStepRunningOnThisThread() const
{
	return (t_joltStepRunningRoles & m_worlds.m_joltRoleBit) != 0u;
}

void JoltBackendUImpl::publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds)
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

	fillRenderSnapshot(*snapshot, m_owner.m_storage, outcome, stepDeadlineSeconds);
	if (m_stepDriver->savesTicks())
		m_renderCorrectionCapture_Step.noteShownSnapshot(*snapshot);
	m_renderSnapshots.commit();
}

void JoltBackendUImpl::captureRenderCorrection_Step(uint64 physicsStep)
{
	RenderCorrection correction;
	const RenderCorrectionCaptureOutcome outcome = m_renderCorrectionCapture_Step.onStepAfterReplay(
		m_stepDriver->getDiagnostics().grantedResims(), static_cast<SimTick>(m_owner.m_manager->currentIntegratedTick()),
		physicsStep, m_owner.m_storage, correction);
	switch (outcome)
	{
	case RenderCorrectionCaptureOutcome::NoNewResim:
		return;
	case RenderCorrectionCaptureOutcome::NoShownHead:
		m_renderCorrectionResims.fetch_add(1u, std::memory_order_relaxed);
		return;
	case RenderCorrectionCaptureOutcome::TickMismatch:
		m_renderCorrectionResims.fetch_add(1u, std::memory_order_relaxed);
		m_renderCorrectionTickMismatches.fetch_add(1u, std::memory_order_relaxed);
		return;
	case RenderCorrectionCaptureOutcome::NoError:
		m_renderCorrectionResims.fetch_add(1u, std::memory_order_relaxed);
		m_renderCorrectionsWithoutError.fetch_add(1u, std::memory_order_relaxed);
		return;
	case RenderCorrectionCaptureOutcome::Captured:
		m_renderCorrectionResims.fetch_add(1u, std::memory_order_relaxed);
		if (m_renderCorrections.tryPush(correction))
			m_renderCorrectionsCaptured.fetch_add(1u, std::memory_order_relaxed);
		return;
	}
}

bool JoltBackendUImpl::buildRenderPoses_GameThread()
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

	if (!interpolateRenderPoses(m_renderSnapshots, params, m_renderPoses, m_renderFrame, &m_renderNext))
		return false;

	RenderCorrectionParams correction;
	correction.correctionSeconds = CVarRenderErrorCorrectionSeconds.GetValueOnGameThread();
	correction.maxCorrectionCm   = CVarRenderErrorCorrectionMaxCm.GetValueOnGameThread();
	correction.desyncSeconds     = CVarRenderErrorCorrectionDesyncSeconds.GetValueOnGameThread();
	correction.stepSeconds       = m_renderCorrectionStepSeconds;
	m_renderCorrectionSmoother.correctPoses(m_renderCorrections, m_renderFrame, correction, m_renderPoses,
		m_renderCorrectionFrame, &m_renderNext);

#if !UE_BUILD_SHIPPING
	const RenderCorrectionFrame& corrected = m_renderCorrectionFrame;
	RenderCorrectionWindowUImpl& window = m_renderCorrectionWindow;
	window.corrections      += corrected.corrections;
	window.smoothedBodies   += corrected.smoothedBodies;
	window.snappedBodies    += corrected.snappedBodies;
	window.pendingDrops     += corrected.pendingDrops;
	window.finished         += corrected.finished;
	window.finishedSteps    += corrected.finishedSteps;
	window.maxFinishedSteps  = FMath::Max(window.maxFinishedSteps, corrected.maxFinishedSteps);
	window.activeFrames     += corrected.active > 0u ? 1u : 0u;
	window.maxErrorCm        = FMath::Max(window.maxErrorCm, corrected.maxErrorCm);
	window.maxAppliedCm      = FMath::Max(window.maxAppliedCm, corrected.maxAppliedCm);
	window.policySnaps      += corrected.policySnaps;
	window.rootFramedFrames += corrected.rootFramed > 0u ? 1u : 0u;
	window.maxPolicySnapCm   = FMath::Max(window.maxPolicySnapCm, corrected.maxPolicySnapCm);
	window.maxPolicySnapDeg  = FMath::Max(window.maxPolicySnapDeg, corrected.maxPolicySnapDeg);
	window.straddleSnaps    += corrected.straddleSnaps;
	if (corrected.late > 0u && window.late == 0u)
	{
		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.RenderCorrection] LATE frame=%llu next=%llu late=%u: a resimulation's correction reached the game ")
			TEXT("thread after the corrected step it rides had already been drawn, so that step was drawn uncorrected (a pop)"),
			static_cast<unsigned long long>(GFrameCounter), static_cast<unsigned long long>(m_renderFrame.nextStep),
			corrected.late);
	}
	window.late += corrected.late;
	if (corrected.corrections > 0u && CVarRenderTraceFrames.GetValueOnGameThread() != 0)
	{
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.RenderCorrection] event frame=%llu prev=%llu next=%llu alpha=%.4f corrections=%u smoothed=%u ")
			TEXT("snapped=%u maxErrorCm=%.4f durationSteps=%d policySnaps=%u rootFramed=%u"),
			static_cast<unsigned long long>(GFrameCounter), static_cast<unsigned long long>(m_renderFrame.prevStep),
			static_cast<unsigned long long>(m_renderFrame.nextStep), m_renderFrame.alpha, corrected.corrections,
			corrected.smoothedBodies, corrected.snappedBodies, corrected.maxErrorCm,
			renderCorrectionDurationSteps(correction.correctionSeconds, correction.stepSeconds), corrected.policySnaps,
			corrected.rootFramed);
	}
#endif

	m_renderPosesValid = true;
	return true;
}

void JoltBackendUImpl::applyRenderSnapshot_GameThread()
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
			// ⛔G-04  docs/JoltBackendUImpl-guards.md
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
					m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, before.X, before.Y, before.Z,
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

void JoltBackendUImpl::syncRenderPoses_GameThread()
{
	if (m_worlds.m_shadowWorld.has_value())
		restoreShadowWorld_GameThread();
	applyRenderSnapshot_GameThread();
	overwriteShadowPoses_GameThread();
}

void JoltBackendUImpl::updateVisualizationAtRenderPoses_GameThread()
{
	updateVisualizationAll(m_owner.m_storage);
	if (!m_renderPosesValid)
		return;

	m_owner.m_storage.forEachSimulatable<SimulatableBrawler>([this](unsigned int id, SimulatableBrawler& simulatable)
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

void JoltBackendUImpl::overwriteShadowPoses_GameThread()
{
	if (!m_worlds.m_shadowWorld.has_value() || !m_renderPosesValid)
		return;
	checkf(m_shadowRestoredFrame == GFrameCounter,
		TEXT("overwriteShadowPoses_GameThread: the shadow world was not restored this frame before the render poses were ")
		TEXT("written into it. The restore loads the newest saved tick into every slot body, so a restore after this ")
		TEXT("overwrite would leave the visualizations querying the newest tick instead of the rendered pose."));

	const JoltBodyBindTable& shadowBindTable = m_worlds.m_shadowBodyAdapter->bindTable();
	for (const auto& [id, target] : m_renderTargetsById)
	{
		const AOGBrawlerUECharacter* const character = target.character.Get();
		if (character == nullptr)
			continue;
		const auto slot = m_worlds.m_joltSlotByOwner.find(character);
		if (slot == m_worlds.m_joltSlotByOwner.end())
			continue;

		for (uint32 declarationIndex = 0u; declarationIndex < m_worlds.m_shadowWorld->bodiesPerSlot(); ++declarationIndex)
		{
			const RenderBody* const pose = findRenderBody(m_renderPoses, id, static_cast<uint8>(declarationIndex));
			if (pose == nullptr)
				continue;
			const BodyId body = JoltPhysicsBodyAdapter::bodyIdOf(m_worlds.m_shadowWorld->slotBodyId(slot->second, declarationIndex));
			if (!shadowBindTable.isBound(body))
				continue;

			glm::mat4 transform = pose->hasRotation != 0u ? glm::mat4_cast(pose->rotation)
			                                              : m_worlds.m_shadowBodyAdapter->getBodyTransform(body);
			transform[3] = glm::vec4(pose->positionCm, 1.f);
			m_worlds.m_shadowBodyAdapter->setBodyTransform(body, transform);

#if !UE_BUILD_SHIPPING
			RenderApplyWindowUImpl& window = m_renderApplyWindow;
			const glm::mat4 queried = m_worlds.m_shadowReader->getBodyTransform(body);
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
void JoltBackendUImpl::checkRenderTargetsHeld_GameThread()
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
				m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, planePoint.X, planePoint.Y, planePoint.Z,
				target.appliedCm.X, target.appliedCm.Y, target.appliedCm.Z, driftCm);
		}
	}
}

void JoltBackendUImpl::checkRenderApply_GameThread()
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
				m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), id, planePoint.X, planePoint.Y, planePoint.Z,
				pose.X, pose.Y, pose.Z, errorCm, static_cast<unsigned long long>(m_renderPoses.physicsStep));
		}
	}
}

void JoltBackendUImpl::traceRenderFrame_GameThread() const
{
	if (CVarRenderTraceFrames.GetValueOnGameThread() == 0)
		return;

	const RenderInterpolationFrame& bracket = m_renderFrame;
	for (const auto& [id, target] : m_renderTargetsById)
	{
		const RenderBody* const nextBody = findRenderBody(m_renderNext, id, m_joltRootDeclarationIndex);
		if (!target.applied || nextBody == nullptr)
			continue;
		const glm::dvec3 errorCm = m_renderCorrectionSmoother.appliedErrorCm(bracket, id, m_joltRootDeclarationIndex);
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.RenderTrace] role=%s frame=%llu id=%u pos=(%.4f,%.4f,%.4f) nextPos=(%.4f,%.4f,%.4f) host=%.6f ")
			TEXT("render=%.6f mode=%s prev=%llu/%s next=%llu/%s back=%u alpha=%.4f newest=%llu err=(%.4f,%.4f,%.4f)"),
			m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), static_cast<unsigned long long>(GFrameCounter), id,
			target.appliedCm.X, target.appliedCm.Y, target.appliedCm.Z,
			nextBody->positionCm.x, nextBody->positionCm.y, nextBody->positionCm.z,
			m_frameHost.hostNowSeconds_GameThread(), bracket.renderTimeSeconds,
			bracket.newestOnly ? TEXT("newest") : TEXT("interp"),
			static_cast<unsigned long long>(bracket.prevStep), bracket.hasPrev ? stepKindText(bracket.prevKind) : TEXT("-"),
			static_cast<unsigned long long>(bracket.nextStep), stepKindText(bracket.nextKind), bracket.nextBack,
			bracket.alpha, static_cast<unsigned long long>(bracket.newestStep), errorCm.x, errorCm.y, errorCm.z);
	}
}

void JoltBackendUImpl::logRenderApplyWindow_GameThread()
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
		m_owner.runsPrediction() ? TEXT("Client") : TEXT("Authority"), window.frames, window.appliedFrames,
		static_cast<int32>(m_renderTargetsById.size()), window.applied, window.missing, window.exact,
		window.outOfTolerance, window.maxErrorCm, window.held, window.moved, window.maxHeldDriftCm, window.pathCm,
		static_cast<unsigned long long>(window.firstAppliedStep), static_cast<unsigned long long>(window.lastAppliedStep),
		static_cast<unsigned long long>(m_renderSnapshots.drops()),
		window.newestMode > 0u ? TEXT("newest") : TEXT("interp"),
		FMath::Clamp(static_cast<double>(CVarRenderInterpolationDelaySteps.GetValueOnGameThread()), 0.0,
			kRenderMaxInterpolationDelaySteps),
		window.interpolated, window.atNewest, window.beyondOldest, window.snaps, window.maxNextBack, *displacement);

	if (m_worlds.m_shadowWorld.has_value())
	{
		UE_LOG(LogOGSimHost, Log,
			TEXT("[SimHost.RenderSync] role=Client frames=%u shadowBodies=%u shadowExact=%u shadowOutOfTolerance=%u ")
			TEXT("shadowMaxErrorCm=%.6f shadowMaxAngleRad=%.6f"),
			window.frames, window.shadowBodies, window.shadowExact, window.shadowOutOfTolerance, window.shadowMaxErrorCm,
			window.shadowMaxAngleRad);
	}

	if (m_owner.runsPrediction())
		logRenderCorrectionWindow_GameThread();

	std::vector<float> samples = std::move(window.frameDisplacementCm);
	samples.clear();
	window = RenderApplyWindowUImpl{};
	window.frameDisplacementCm = std::move(samples);
}

void JoltBackendUImpl::logRenderCorrectionWindow_GameThread()
{
	RenderCorrectionWindowUImpl& window = m_renderCorrectionWindow;
	const uint64 resims       = m_renderCorrectionResims.load(std::memory_order_relaxed);
	const uint64 captured     = m_renderCorrectionsCaptured.load(std::memory_order_relaxed);
	const uint64 withoutError = m_renderCorrectionsWithoutError.load(std::memory_order_relaxed);
	const uint64 tickMismatch = m_renderCorrectionTickMismatches.load(std::memory_order_relaxed);
	const uint64 ringDrops    = m_renderCorrections.drops();
	const float  seconds      = CVarRenderErrorCorrectionSeconds.GetValueOnGameThread();
	const double stepSeconds  = static_cast<double>(m_renderCorrectionStepSeconds);
	const double decayMeanS   = window.finished > 0u
		? static_cast<double>(window.finishedSteps) / static_cast<double>(window.finished) * stepSeconds : 0.0;

	UE_LOG(LogOGSimHost, Log,
		TEXT("[SimHost.RenderCorrection] role=Client frames=%u resims=%llu captured=%llu withoutError=%llu tickMismatches=%llu ")
		TEXT("ringDrops=%llu corrections=%u smoothedBodies=%u snappedBodies=%u maxErrorCm=%.3f maxAppliedCm=%.3f decays=%u ")
		TEXT("decayMeanS=%.3f decayMaxS=%.3f activeFrames=%u late=%u pendingDrops=%u seconds=%.3f durationSteps=%d ")
		TEXT("maxCm=%.1f desyncS=%.2f policySnaps=%u maxPolicySnapCm=%.3f maxPolicySnapDeg=%.1f rootFramedFrames=%u ")
		TEXT("straddleSnaps=%u"),
		m_renderApplyWindow.frames, static_cast<unsigned long long>(resims - window.resimsAtStart),
		static_cast<unsigned long long>(captured - window.capturedAtStart),
		static_cast<unsigned long long>(withoutError - window.withoutErrorAtStart),
		static_cast<unsigned long long>(tickMismatch - window.tickMismatchAtStart),
		static_cast<unsigned long long>(ringDrops - window.ringDropsAtStart), window.corrections, window.smoothedBodies,
		window.snappedBodies, window.maxErrorCm, window.maxAppliedCm, window.finished, decayMeanS,
		static_cast<double>(window.maxFinishedSteps) * stepSeconds, window.activeFrames, window.late, window.pendingDrops,
		seconds, renderCorrectionDurationSteps(seconds, m_renderCorrectionStepSeconds),
		CVarRenderErrorCorrectionMaxCm.GetValueOnGameThread(), CVarRenderErrorCorrectionDesyncSeconds.GetValueOnGameThread(),
		window.policySnaps, window.maxPolicySnapCm, window.maxPolicySnapDeg, window.rootFramedFrames, window.straddleSnaps);

	window = RenderCorrectionWindowUImpl{};
	window.resimsAtStart       = resims;
	window.capturedAtStart     = captured;
	window.withoutErrorAtStart = withoutError;
	window.tickMismatchAtStart = tickMismatch;
	window.ringDropsAtStart    = ringDrops;
}
#endif

JoltBackendUImpl::ShadowStateSlotUImpl::ShadowStateSlotUImpl()
{
	state.bytes.assign(JoltWorldConfig{}.ringSlotBytes, 0u);
	state.bodyIds.reserve(kMaxSimulatableSlots * physicsDeclarationCountOf<BrawlerPhysicsComposite>::value);
}

void JoltBackendUImpl::bindShadowSlot(uint32 slot, unsigned int id, const SimulatableBrawler& simulatable)
{
	JoltPhysicsFactory factory(*m_worlds.m_shadowBodyAdapter, m_worlds.m_joltSlotTemplate, slot, id, JoltPhysicsFactoryOptions{},
		[this](BodyId body, uint32_t shapeIndex, std::optional<BodyId> rootBodyId)
		{
			return m_worlds.m_shadowQuery->registerShape(body, shapeIndex, rootBodyId);
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

void JoltBackendUImpl::releaseShadowSlot(uint32 slot)
{
	m_worlds.m_shadowBodyAdapter->bindTable().releaseSlot(slot);
	compareShadowBindTables_GameThread(TEXT("release"), slot, 0u, true);
}

void JoltBackendUImpl::compareShadowBindTables_GameThread(const TCHAR* after, uint32 slot, unsigned int id,
	bool sameResults)
{
#if !UE_BUILD_SHIPPING
	const JoltBodyBindTable& stepTable   = m_owner.m_physAdapter->bindTable();
	const JoltBodyBindTable& shadowTable = m_worlds.m_shadowBodyAdapter->bindTable();
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
			const BodyId stepBody   = JoltPhysicsBodyAdapter::bodyIdOf(m_worlds.m_joltWorld->slotBodyId(slotIndex, templateIndex));
			const BodyId shadowBody = JoltPhysicsBodyAdapter::bodyIdOf(m_worlds.m_shadowWorld->slotBodyId(slotIndex, templateIndex));
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

void JoltBackendUImpl::publishShadowSlot_Step(const TickOutcome& outcome)
{
	if (!m_worlds.m_shadowWorld.has_value() || !stepAllocatesFrontierSlot(outcome.kind))
		return;

	const JoltStateSlot* const saved = m_worlds.m_joltWorld->ring().find(outcome.tick);
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

void JoltBackendUImpl::restoreShadowWorld_GameThread()
{
	ShadowWindowUImpl& window = m_shadowWindow;
	++window.frames;
	m_shadowRestoredFrame = GFrameCounter;

	if (const ShadowStateSlotUImpl* const newest = m_shadowSlots.peekNewest(0))
	{
		if (newest->sequence > m_shadowRestoredSequence)
		{
			const double startSeconds = FPlatformTime::Seconds();
			const bool restored = m_worlds.m_shadowWorld->restoreFromSnapshot(newest->state);
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
				if (m_worlds.m_shadowWorld->liveStateHash() == newest->state.stateHash)
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

void JoltBackendUImpl::logShadowWindow_GameThread()
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

OGSIM_OPTIMIZE_ON

#undef HasAuthority

#endif
