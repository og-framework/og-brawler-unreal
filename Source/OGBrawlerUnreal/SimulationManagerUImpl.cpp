// SPDX-License-Identifier: BUSL-1.1

#include "SimulationManagerUImpl.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"
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
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

#include <algorithm>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGSim);
DEFINE_LOG_CATEGORY(LogOGSimTick);
DEFINE_LOG_CATEGORY(LogOGMgmt);
DEFINE_LOG_CATEGORY(LogOGNet);
DEFINE_LOG_CATEGORY(LogOG);
DEFINE_LOG_CATEGORY(LogOGBrawler);

namespace
{
// [T20] The per-connection reap deadline (kTierReapDeadlineDwellPeriods) moved
// into the core ServerReceptionCoordinator header alongside the reap logic it
// governs; it is no longer a UE file-local constant.

	// Matches any brawlerProjectileSimulation::PhysicsDeclaration<I> (one instantiation
	// per pool slot) so the tryRegister sub-StaticData selector can route all projectile
	// slot declarations to staticData.m_projectileStaticData. A partial specialization is
	// used rather than a concept because the declaration is parameterized on a non-type
	// (int) template argument.
	template <typename T> struct is_projectile_physics_decl : std::false_type {};
	template <int I> struct is_projectile_physics_decl<brawlerProjectileSimulation::PhysicsDeclaration<I>> : std::true_type {};
	template <typename T> inline constexpr bool is_projectile_physics_decl_v = is_projectile_physics_decl<T>::value;

	// Routes a SIMLOG message to the appropriate OG log category based on its
	// leading [Tag] prefix. Severity is derived from the optional [Verbose] or
	// [Warning] prefix; everything else lands at Log severity.
	void RouteOGMessage(const char* msg)
	{
		FString fmsg(msg);

		// Severity meta-prefix (rare; framework defaults to Log).
		ELogVerbosity::Type severity = ELogVerbosity::Log;
		// The category is chosen from the [Tag] that follows any severity prefix,
		// so tag-matching runs against `body` (fmsg minus the leading
		// [Verbose]/[Warning] token — both are 9 chars) while the full fmsg,
		// severity token included, is what actually gets logged. This lets a
		// message carry BOTH a severity escalation and a routable tag, e.g. the
		// T25 input-path diagnostics ([Warning][InputGap], [Warning][InputDrop], …)
		// route to LogOGNet AND show at Warning.
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

		// LogOGSim: rare simulation lifecycle events.
		if (body.StartsWith(TEXT("[TimeResync.")))           { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[Resim.")))           { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[ResimCheck.Divergence]")))     { EMIT_OG(LogOGSim); }
		else if (body.StartsWith(TEXT("[ResimCheck.PrepareRestore]"))) { EMIT_OG(LogOGSim); }
		// LogOGSimTick: per-tick simulation chatter, dominates log volume.
		else if (body.StartsWith(TEXT("[ResimCheck.Check]")))      { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ResimCheck.IsSimilar]")))  { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ResimCheck.TriggerRewind]"))) { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[AuthoritySimulation]")))   { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[ClientPrediction]")))      { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[PredictionSimulation]")))  { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[PostPrediction]")))        { EMIT_OG(LogOGSimTick); }
		else if (body.StartsWith(TEXT("[CollectInput]")))          { EMIT_OG(LogOGSimTick); }
		// LogOGNet: replication-channel events.
		else if (body.StartsWith(TEXT("[ServerReceive]")))              { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveLocalInput]")))          { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendCorrectionStateToClients]"))) { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendRemoteInputToClients]")))   { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[SendLocalInputToServer]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveCorrectionState]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[ReceiveCorrectionInput]")))     { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InjectCorrectionState]")))      { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InjectCorrectionInput]")))      { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[DrainOutOfOrder]")))            { EMIT_OG(LogOGNet); }
		// LogOGNet: input-path diagnostics (T25). [InputGap]/[InputDrop]/
		// [DelayShift]/[InputStats] carry a [Warning] severity prefix so they show
		// under the default LogOGNet=Warning; [Park]/[Release] are Log severity
		// (on-demand under LogOGNet Verbose). All route here off `body`.
		else if (body.StartsWith(TEXT("[InputGap]")))                   { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InputDrop]")))                  { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[DelayShift]")))                 { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[InputStats]")))                 { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[Park]")))                       { EMIT_OG(LogOGNet); }
		else if (body.StartsWith(TEXT("[Release]")))                    { EMIT_OG(LogOGNet); }
		// LogOGMgmt: manager / simulatable lifecycle.
		else if (body.StartsWith(TEXT("SimulationManager:"))) { EMIT_OG(LogOGMgmt); }
		else if (body.StartsWith(TEXT("tryRegister:")))       { EMIT_OG(LogOGMgmt); }
		else if (body.StartsWith(TEXT("NewFramework:")))      { EMIT_OG(LogOGMgmt); }
		// LogOG: fallback for unrecognized prefixes.
		else                                                  { EMIT_OG(LogOG); }

#undef EMIT_OG
	}

}

#pragma optimize( "", off )


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
	//const FSimulationInput2* input = this->GetConsumerInput_Internal();
	//if (input == nullptr)
	//	return INDEX_NONE;

	//if (input->m_manager == nullptr)
	//	return INDEX_NONE;

	ASimulationManagerUImpl* manager = m_manager;
	if (manager == nullptr)
	{
		UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d manager=null rewind=0"),
			LastCompletedStep);
		return INDEX_NONE;
	}

	// Authority is the source of truth — nothing to rewind toward.
	// Skip the divergence sweep entirely to avoid per-tick no-op work and log spam.
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

	// Convert from simulation tick space back to Chaos physics tick space
	const int32 unrealTickDifferenceAdjustedTick = manager->getChaosTickMapper().toChaosTick(static_cast<int32_t>(correctionTick));
	UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u chaosTick=%d rewind=1"),
		LastCompletedStep, correctionTick, unrealTickDifferenceAdjustedTick);
	return unrealTickDifferenceAdjustedTick;
}

void FSimulationManagerAsyncCallback::ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input)
{

}

void FSimulationManagerAsyncCallback::FirstPreResimStep_Internal(int32 PhysicsStep)
{
	// Server authority never resims — no rewind timeline exists there.
	if (m_manager == nullptr || !m_manager->runsPrediction())
		return;

	// Convert Chaos physics step back to simulation tick for prepareResimulation.
	const uint32_t simTick = static_cast<uint32_t>(
		m_manager->getChaosTickMapper().toSimulationTick(static_cast<int32_t>(PhysicsStep)));
	m_manager->prepareResimulation(PhysicsStep, simTick);

	// Push per-body target state into Chaos's rewind timeline at PostPushData.
	// Direct ptApi->SetX/SetV/SetW writes are a no-op on ResimAsFollower bodies
	// because the follower replays recorded history, not live particle state.
	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FRewindData* rewindData = solver.GetRewindData();
	if (rewindData == nullptr)
		return;

	auto pushBodyState = [&](BodyId bodyId, const PhysicsBodyState& bs)
	{
		Chaos::FSingleParticlePhysicsProxy* proxy =
			solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
		if (proxy == nullptr)
			return;
		Chaos::FGeometryParticleHandle* handle = proxy->GetHandle_LowLevel();
		if (handle == nullptr)
			return;
		rewindData->SetTargetStateAtFrame(
			*handle, PhysicsStep,
			Chaos::FFrameAndPhase::EParticleHistoryPhase::PostPushData,
			uglm::toFVector(bs.position),
			uglm::toFQuat(bs.rotation),
			uglm::toFVector(bs.linearVelocity),
			uglm::toFVector(bs.angularVelocity),
			/*bShouldSleep=*/false);
	};

	// BodyId lookup goes through the m_physics composite bindings (local-only);
	// the State's PhysicsBodyState no longer carries an identifier.
	m_manager->editStorage().forEachSimulatable(
		[&](unsigned int /*id*/, SimulatableBrawler& simulatable)
		{
			const simulatableBrawler::State& state = simulatable.getAllState().getState();
			simulatable.getPhysicsComposite().forEach([&](const auto& decl) {
				using D = std::decay_t<decltype(decl)>;
				using S = typename D::StateType;
				pushBodyState(decl.bindings.ownBodyId,
				              D::bodyStateOf(state.template get<S>()));
			});
		});
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

	// Null whichever slot points at us — avoids re-querying role during teardown,
	// where NetDriver state may already be gone.
	if (s_instances[0] == this) { s_instances[0] = nullptr; ISimulationTimingRelayListener::unregisterInstance(true); }
	if (s_instances[1] == this) { s_instances[1] = nullptr; ISimulationTimingRelayListener::unregisterInstance(false); }
}

void ASimulationManagerUImpl::BeginPlay()
{
	Super::BeginPlay();


	UWorld* uWorld = GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	FPhysScene* physScene = uWorld->GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	// World-level authority — true on dedicated server, listen server, and standalone;
	// false only on pure clients. Can't use HasAuthority() on this actor because it
	// is bReplicates=false, so its local Role is always ROLE_Authority regardless of
	// the world's net mode.
	const ENetMode worldNetMode = GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);
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
		// Adapters and integration layer emplaced here; manager constructed after.
		// Authority never runs prediction — it IS the truth.
		Chaos::FPBDRigidsSolver& rigidsSolverS = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverS);
		m_physReaderAdapter.emplace(rigidsSolverS);
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
			// [hit-resolution T13] Projectile category — routes to a previously
			// unused UE trace channel so projectile bodies register separately from
			// character hurtboxes and projectile-vs-projectile overlaps can be
			// distinguished at the sim layer.
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 }
		});
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(false, solver->GetAsyncDeltaTime(), ManagerType::Params{
			*m_integrationLayer, m_netSync, m_reconciliation, m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

		// [T9 part 4] Inject the game's zero input for the client input delay
		// line. Set on the AUTHORITY branch too, even though a dedicated server
		// never reads it (its effective client delay stays 0, so the line is
		// never consulted): a listen-server host runs the provider branch on this
		// manager, and leaving the neutral at a value-initialised PlayerInput
		// would put a (0,0,0) forward vector one config change away from reaching
		// normalisation. Uniform on both roles is cheaper than a latent trap.
		m_netSync.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		// ---- [T20] server reception coordinator (relocated from T10 wiring) --
		// AUTHORITY BRANCH ONLY. The coordinator owns the tier table + delay
		// queue + claim map internally; it borrows the manager's own TimeConfig by
		// reference, so it must be emplaced after m_manager and must never outlive
		// it (reset in EndPlay). Construction order of the owned table/queue (table
		// first, queue binding it) is enforced inside the coordinator; the C2
		// REPLACES semantics (effectiveDelay -> rttTierInputDelays[tier]) come with
		// the queue being given the tier table there.
		m_receptionCoordinator.emplace(m_manager->getTimeConfig());
		m_receptionCoordinator->setLogger(std::function<void(const char*)>(pctmloggerServer));
		// Process-global sinks for deeply-nested simulation templates that don't
		// have a logger parameter plumbed in.
		// simlog -> LogOG* categories (framework: tick/sync/reconciliation messages).
		// ogblog -> LogOGBrawler (game rules: DAttackMachine/Radial/Guard).
		simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogServer));
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
		// Non-authority branch = pure client — always runs prediction.
		Chaos::FPBDRigidsSolver& rigidsSolverC = solver->CastChecked();
		m_physAdapter.emplace(rigidsSolverC);
		m_physReaderAdapter.emplace(rigidsSolverC);
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 },
			// [hit-resolution T13] Projectile category — routes to a previously
			// unused UE trace channel so projectile bodies register separately from
			// character hurtboxes and projectile-vs-projectile overlaps can be
			// distinguished at the sim layer.
			{ collisionCategory::projectile,   ECollisionChannel::ECC_GameTraceChannel5 }
		});
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_manager.emplace(/*usePrediction=*/true, solver->GetAsyncDeltaTime(), ManagerType::Params{
			*m_integrationLayer, m_netSync, m_reconciliation, m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

		// [T9 part 4] The game's zero input fills the [0, effectiveDelay) window
		// at session start and after a hard resync. It is NOT PlayerInput{} —
		// getZeroPlayerInput builds (0,0,1) forward vectors — so this injection
		// is load-bearing, not defensive.
		m_netSync.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		// [T9 part 3] Establish the PRE-ARRIVAL baseline delay before any tier
		// has replicated. `forcedInputLatencyTicks` is the documented "no
		// per-connection tier is available" value and is exactly what the
		// server's ServerInputDelayQueue::effectiveDelay falls back to for a
		// connection it has not yet tiered — so publishing it here is what keeps
		// the two ends delaying by the same amount during the pre-tier window
		// instead of the client running 0 against the server's 2.
		// OnRep_ConnectionTier overwrites this the moment a tier lands.
		m_netSync.setClientEffectiveInputDelayTicks(
			m_manager->getTimeConfig().forcedInputLatencyTicks);
		// Process-global sinks for deeply-nested simulation templates that don't
		// have a logger parameter plumbed in.
		// simlog -> LogOG* categories (framework: tick/sync/reconciliation messages).
		// ogblog -> LogOGBrawler (game rules: DAttackMachine/Radial/Guard).
		simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogClient));
	}

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
	// [T20] The coordinator borrows `const TimeConfig&` from m_manager, so it may
	// not outlive it — reset here, before the manager is destroyed. Its owned
	// table/queue tear down in reverse construction order internally. The
	// adapter-side id->component map is cleared alongside it.
	m_delayedInputComponentsById.clear();
	m_receptionCoordinator.reset();

	// Clear the singleton slot before teardown so a subsequent PIE session
	// (which may BeginPlay before this actor is GC'd) doesn't hit the guard.
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/false);
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

void ASimulationManagerUImpl::OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPhysicsStep(FPhysScene* Scene, float DeltaTime)
{
}

void ASimulationManagerUImpl::OnPostPhysicsStep(FChaosScene* Scene)
{
	// Game thread — safe to call RPCs and Unreal API here.
	onPostSimulationGameThread();

	// HasAuthority() is unreliable on non-replicated actors — use runsPrediction() instead.
	// m_serverClock exists iff constructed with shouldRunPrediction=false (server path in BeginPlay).
	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}

	updateVisualizationAll(m_storage);
}

TryRegisterStatus ASimulationManagerUImpl::tryRegister(
    unsigned int id,
    SimulatableBrawler simulatable,
    USimmableUpdateComponent& owner,
    std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider,
    bool isAuthority)
{
    // Look up or insert the per-id pending record.
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
        // First-call body creation pass.
        ACharacter* character = Cast<ACharacter>(owner.GetOwner());
        checkf(character != nullptr, TEXT("USimmableUpdateComponent must be attached to an ACharacter"));
        FBodyInstanceAsyncPhysicsTickHandle parentHandle =
            character->GetCapsuleComponent()->GetBodyInstanceAsyncPhysicsTickHandle();
        const BodyId parentBodyId = m_physAdapter->getBodyId(parentHandle);
        record.parentBodyId = parentBodyId;

        // Stamp the authoritative capsule body id into the brawler's CharacterBindings
        // member (T33). SOURCE TODAY: the UE ACharacter's CapsuleComponent body
        // registered above from parentHandle — the capsule is created/owned/moved by
        // CharacterMovementComponent and we just learn its BodyId here. FUTURE: when
        // the planned character-movement sub-sim lands (modeled on radial/guard with
        // its own PhysicsDeclaration), the capsule body will be created and registered
        // through the forEach factory pass below — same path radial/guard use today —
        // and capsuleBodyId will be sourced from that sub-sim's bindings.ownBodyId
        // instead of from the UE-CapsuleComponent lookup. See BrawlerMovementSimulation.h
        // CharacterBindings comment for the full future-direction note.
        record.simulatable->setCharacterBindings({ /*.capsuleBodyId =*/ parentBodyId });

        AActor* ownerActor = owner.GetOwner();
        // Attach + parent-body are the same capsule component under the one-deep
        // hierarchy — passing the capsule to the factory expresses "these shapes
        // belong to this character" through a single handle. Previously we
        // passed attachParent (as USceneComponent*) AND parentBodyId separately;
        // callers could get them out of sync. The factory now derives the parent
        // BodyId from attachParent's body-instance handle internally.
        UPrimitiveComponent* attachParent = character->GetCapsuleComponent();
        const simulatableBrawler::StaticData& staticData = owner.getStaticData();

        // [hit-resolution T11] The factory-owned parentBodyId (derived from
        // attachParent's body handle) becomes the root for every shape it
        // registers, so overlap() emits an actor-level rootBodyId == capsule id.
        ChaosPhysicsFactory factory(*m_physAdapter, *m_queryAdapter, ownerActor, attachParent);

        record.simulatable->editPhysicsComposite().forEach([&](auto& decl)
        {
            using D = std::decay_t<decltype(decl)>;
            // Each PhysicsDeclaration's static methods take the sub-sim StaticData type.
            // Extract the right sub-data at compile time so the fold stays generic.
            const auto& subStaticData = [&]() -> const auto& {
                if constexpr (std::is_same_v<D, dAttackRadialSimulation::PhysicsDeclaration>)
                    return staticData.m_attackSimulationStaticData;
                else if constexpr (is_projectile_physics_decl_v<D>)
                    return staticData.m_projectileStaticData;
                else
                    return staticData.m_guardSimulationStaticData;
            }();
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

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    // Resolvability gate.
    bool allResolvable = m_physAdapter->isBodyResolvable(record.parentBodyId);
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

    // All resolvable — perform the actual registration.
    if (record.isAuthority)
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/std::move(record.inputProvider));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    // [ogsim-system-api T8] Notify the systems executor that the character is now in
    // storage, so brawlerHitRouting::System::onCharacterRegistered indexes it for
    // inbound-hit routing (§3.11 timing: the character IS in storage at this point, so
    // the system's view.get<>(id) resolves it and reads capsuleBodyId itself). This
    // replaces the old adapter-owned m_byRootBodyId insert — the routing system now owns
    // its map. Wiring the notify AND dropping the adapter insert land in the SAME commit
    // (SB-5): there is never a window where both the adapter map and the system map are
    // populated, so no inbound-hit stream is double-routed.
    m_manager->notifyCharacterRegistered(id);

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

// [T21] sampleAndDeriveConnectionTier and tryEnqueueDelayedRemoteInput are GONE.
// Their engine-primitive acquisition (wire-handle resolve, RTT read, player-slot
// derivation, owner id) moved UP to the RPC boundary
// (USimmableUpdateComponent::ServerReceiveRemoteMove), which now forwards straight
// into ServerReceptionCoordinator::noteRttSample (once per bundle) and
// receiveInputBundle (the whole per-slot loop, T24). Nothing of the RPC per-slot
// path remains manager-side; noteDelayedInputComponent below is a delivery-routing
// buffer accessor called once at register-time, not per slot, and deliverRemoteInput
// is this manager's RemoteInputDeliverySink terminus. No netcode policy either.

void ASimulationManagerUImpl::noteDelayedInputComponent(
    unsigned int id, USimmableUpdateComponent& component)
{
    // Delivery-routing registration for the coordinator's per-id `deliver` callback
    // (built in releaseDelayedInputsForStep) and the deliverRemoteInput fallback.
    // Plain overwrite: re-registering the same id is a no-op, and an id legitimately
    // replacing a dead one takes the slot over. [T24] Called ONCE at register-time
    // (tryRegisterWithNewFramework, authority), not per parked slot.
    m_delayedInputComponentsById[id] = &component;
}

// [T24] This manager satisfies the core RemoteInputDeliverySink concept — the
// delivery target the ServerReceptionCoordinator drives on the deliver-now path
// (a malformed slot in receiveInputBundle) and that the drain lambda above routes
// through. Compile-checked at the receiveInputBundle call site, but asserted here
// too so a breaking signature change surfaces legibly (mirrors the ConnectionTierSink
// assert on USimmableUpdateComponent).
static_assert(
    RemoteInputDeliverySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputDeliverySink so "
    "ServerReceptionCoordinator can drive remote-input delivery through it");

void ASimulationManagerUImpl::deliverRemoteInput(
    unsigned int id, uint32 captureTick, const simulatableBrawler::PlayerInput& input)
{
    // Resolve id -> owning component and hand the input to the SAME inbound path
    // the RPC uses (deliverDelayedRemoteInput -> m_onRemoteMoveReceivedCallback),
    // with the ORIGINAL captureTick. A stale weak handle means the owner was GC'd
    // without an unregister: drop the map entry and discard the input (a dead
    // component has nothing to receive it). GAME THREAD only.
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

// ---------------------------------------------------------------------------
// TICK ALIGNMENT — verified, and load-bearing. An off-by-one here shifts EVERY
// player's input by one tick, silently and uniformly.
//
// `physicsStep` is the UPCOMING solver step: Chaos passes
// MarshallingManager.GetInternalStep_External(), documented as "the internal
// step that the current PushData will be associated with once it is marshalled
// over" (ChaosMarshallingManager.h), and the solver's frame counter is only
// incremented at the END of a tick (`GetCurrentFrame()++`, PBDRigidsSolver.cpp),
// so step N's OnPreSimulate_Internal observes GetCurrentFrame() == N.
//
// ChaosTickMapper's offset is written in OnPreSimulate_Internal as
// `chaosTick - simulationTick`. Crucially it is written BEFORE
// onGameSimulation() runs, and onGameSimulationAuthority() advances the server
// clock as its FIRST action. So the `simulationTick` captured at step K is the
// tick simulated at step K-1, not at K:
//
//     offset = K - S(K-1)          where S(K) is the sim tick simulated at step K
//     S(K)   = S(K-1) + 1          authority advanceTick() is unconditional —
//                                  no Stall/Skip/resim exists on the server
//  => offset = K - (S(K) - 1) = (K - S(K)) + 1
//  => toSimulationTick(X) = X - offset = S(X) - 1
//
// `toSimulationTick(physicsStep)` therefore names the tick BEFORE the one that
// step will simulate, and the upcoming tick is that value PLUS ONE. Hence the
// `+ 1` below — it is a derived correction, not a fudge factor.
//
// Why not cross-check against the server clock at runtime: the clock is written
// on the physics thread and reading it here would introduce exactly the kind of
// unsynchronized cross-thread read this whole design exists to avoid. The
// mapper's offset is a std::atomic and is the only tick source safe to read from
// the game thread — which is why the resolution specifies it.
//
// Sub-stepping: with NumSteps > 1 the physics frame runs several sim ticks back
// to back, so this releases input for each of them. The normal fixed-tick case
// is NumSteps == 1 and collapses to a single drain.
// ---------------------------------------------------------------------------
void ASimulationManagerUImpl::releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps)
{
    // [T20] Thin transport adapter over ServerReceptionCoordinator::releaseDelayedInputs
    // (the drain) + reapConnections. The drain body — iterate the claim map,
    // resolve per-wire effectiveDelay, dequeue at captureTick+delay, purge stale —
    // now lives in the core coordinator; this side supplies only the game-thread-
    // safe upcoming sim tick and the per-id `deliver` callback.
    if (!m_receptionCoordinator.has_value())
        return;

    const int32 firstUpcomingSimTick =
        static_cast<int32>(m_chaosTickMapper.toSimulationTick(static_cast<int32_t>(physicsStep))) + 1;

    // The per-id delivery callback for the core drain. It answers "is this owner
    // still alive" (false => the coordinator drops the stale claim, mirroring the
    // old drain's `target.Get()==nullptr` prune) and, when alive, routes the actual
    // delivery through deliverRemoteInput — the SAME RemoteInputDeliverySink method
    // the core receive-loop fallback uses (T24 unification). The liveness check
    // stays HERE because the core drain's prune contract is a bool return, while
    // the sink method itself returns void per the concept.
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

    // [T20] Reap relocated here from the (former arrival-gated) RTT sample path.
    // It now runs once per physics frame regardless of traffic — the documented
    // benign cadence change (idle server now reaps). The coordinator gates it on
    // the dwell boundary internally. The tick is sourced from the game-thread-safe
    // mapper (same as the drain), NOT the physics-thread-written server clock.
    m_receptionCoordinator->reapConnections(firstUpcomingSimTick);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    unsigned int id, USimmableUpdateComponent& owner, bool isAuthority)
{
    // [ogsim-system-api T8] Notify the systems executor to drop this character's routing
    // entry BEFORE unregisterSimulatable destroys the SimulatableBrawler (§3.11 timing:
    // the character is still in storage here, so brawlerHitRouting::System::
    // onCharacterUnregistered resolves it through the view and erases by stored-pointer
    // identity — no stale entry survives, no dangling pointer is ever dereferenced by the
    // per-tick routing pass). This replaces the old adapter-owned m_byRootBodyId erase.
    // The has<> guard is preserved: if the character was never fully registered into
    // storage there is no system-map entry to drop, and the hook's view.get<>(id) would
    // be unsafe. Wiring the notify AND dropping the adapter erase land in the SAME commit
    // (SB-5).
    if (m_storage.has<SimulatableBrawler>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

    unregisterSimulatable<SimulatableBrawler>(
        m_storage, m_reconciliation, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

    // [T20] The unregister contract that replaces the core's former GC-liveness
    // read (the coordinator's claim map is id-keyed, not a TWeakObjectPtr). Drop
    // this owner's claim + dedup watermark from the coordinator and its
    // id->component mapping here, promptly, rather than waiting for GC to make an
    // engine handle stale. No-op on a pure client (coordinator is nullopt).
    if (m_receptionCoordinator.has_value())
    {
        m_receptionCoordinator->forgetOwner(id);
    }
    m_delayedInputComponentsById.erase(id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

void ASimulationManagerUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = GetWorld();
	asyncInput->m_manager = this;

	// [C.2 / T10 part 4] Release tier-delayed input for the tick(s) the upcoming
	// physics step will simulate. GAME THREAD — this callback is Chaos's
	// game-thread hook immediately preceding the step (see the tick-alignment
	// derivation on releaseDelayedInputsForStep). No-op on a client and on a
	// server with nothing parked.
	releaseDelayedInputsForStep(PhysicsStep, NumSteps);
}

#pragma optimize( "", on )
