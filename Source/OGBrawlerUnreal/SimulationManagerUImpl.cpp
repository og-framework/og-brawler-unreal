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
#include "OGBrawler/OGBrawlerLog.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"

DEFINE_LOG_CATEGORY(LogOGSim);
DEFINE_LOG_CATEGORY(LogOGSimTick);
DEFINE_LOG_CATEGORY(LogOGMgmt);
DEFINE_LOG_CATEGORY(LogOGNet);
DEFINE_LOG_CATEGORY(LogOG);
DEFINE_LOG_CATEGORY(LogOGBrawler);

namespace
{
	// Routes a SIMLOG message to the appropriate OG log category based on its
	// leading [Tag] prefix. Severity is derived from the optional [Verbose] or
	// [Warning] prefix; everything else lands at Log severity.
	void RouteOGMessage(const char* msg)
	{
		FString fmsg(msg);

		// Severity meta-prefix (rare; framework defaults to Log).
		ELogVerbosity::Type severity = ELogVerbosity::Log;
		if (fmsg.StartsWith(TEXT("[Verbose]")))
		{
			severity = ELogVerbosity::Verbose;
		}
		else if (fmsg.StartsWith(TEXT("[Warning]")))
		{
			severity = ELogVerbosity::Warning;
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
		if (fmsg.StartsWith(TEXT("[TimeResync.")))           { EMIT_OG(LogOGSim); }
		else if (fmsg.StartsWith(TEXT("[Resim.")))           { EMIT_OG(LogOGSim); }
		else if (fmsg.StartsWith(TEXT("[ResimCheck.Divergence]")))     { EMIT_OG(LogOGSim); }
		else if (fmsg.StartsWith(TEXT("[ResimCheck.PrepareRestore]"))) { EMIT_OG(LogOGSim); }
		// LogOGSimTick: per-tick simulation chatter, dominates log volume.
		else if (fmsg.StartsWith(TEXT("[ResimCheck.Check]")))      { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[ResimCheck.IsSimilar]")))  { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[ResimCheck.TriggerRewind]"))) { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[AuthoritySimulation]")))   { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[ClientPrediction]")))      { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[PredictionSimulation]")))  { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[PostPrediction]")))        { EMIT_OG(LogOGSimTick); }
		else if (fmsg.StartsWith(TEXT("[CollectInput]")))          { EMIT_OG(LogOGSimTick); }
		// LogOGNet: replication-channel events.
		else if (fmsg.StartsWith(TEXT("[ServerReceive]")))              { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[ReceiveLocalInput]")))          { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[SendCorrectionStateToClients]"))) { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[SendRemoteInputToClients]")))   { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[SendLocalInputToServer]")))     { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[ReceiveCorrectionState]")))     { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[ReceiveCorrectionInput]")))     { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[InjectCorrectionState]")))      { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[InjectCorrectionInput]")))      { EMIT_OG(LogOGNet); }
		else if (fmsg.StartsWith(TEXT("[DrainOutOfOrder]")))            { EMIT_OG(LogOGNet); }
		// LogOGMgmt: manager / simulatable lifecycle.
		else if (fmsg.StartsWith(TEXT("SimulationManager:"))) { EMIT_OG(LogOGMgmt); }
		else if (fmsg.StartsWith(TEXT("tryRegister:")))       { EMIT_OG(LogOGMgmt); }
		else if (fmsg.StartsWith(TEXT("NewFramework:")))      { EMIT_OG(LogOGMgmt); }
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
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 }
		});
		m_integrationLayer.emplace(*m_physAdapter, *m_queryAdapter, m_storage);
		m_manager.emplace(false, solver->GetAsyncDeltaTime(), *m_integrationLayer, m_netSync, m_reconciliation, std::function<void(const char*)>(pctmloggerServer));
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));
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
		m_queryAdapter.emplace(uWorld, std::initializer_list<ChaosCategoryMapping>{
			{ collisionCategory::body,         ECollisionChannel::ECC_GameTraceChannel2 },
			{ collisionCategory::guard,        ECollisionChannel::ECC_GameTraceChannel3 },
			{ collisionCategory::queryRouting, ECollisionChannel::ECC_GameTraceChannel4 }
		});
		m_integrationLayer.emplace(*m_physAdapter, *m_queryAdapter, m_storage);
		m_manager.emplace(/*usePrediction=*/true, solver->GetAsyncDeltaTime(), *m_integrationLayer, m_netSync, m_reconciliation, std::function<void(const char*)>(pctmlogger));
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));
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
        const BodyId parentBodyId = m_physAdapter->registerBody(parentHandle);
        record.parentBodyId = parentBodyId;

        AActor* ownerActor = owner.GetOwner();
        USceneComponent* attachParent = ownerActor->GetRootComponent();
        const simulatableBrawler::StaticData& staticData = owner.getStaticData();

        ChaosPhysicsFactory factory(*m_physAdapter, *m_queryAdapter, ownerActor, attachParent);

        record.simulatable->editPhysicsComposite().forEach([&](auto& decl)
        {
            using D = std::decay_t<decltype(decl)>;
            // Each PhysicsDeclaration's static methods take the sub-sim StaticData type.
            // Extract the right sub-data at compile time so the fold stays generic.
            const auto& subStaticData = [&]() -> const auto& {
                if constexpr (std::is_same_v<D, dAttackRadialSimulation::PhysicsDeclaration>)
                    return staticData.m_attackSimulationStaticData;
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

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    unsigned int id, USimmableUpdateComponent& owner, bool isAuthority)
{
    unregisterSimulatable<SimulatableBrawler>(
        m_storage, m_reconciliation, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);
    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

void ASimulationManagerUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = GetWorld();
	asyncInput->m_manager = this;
}

#pragma optimize( "", on )
