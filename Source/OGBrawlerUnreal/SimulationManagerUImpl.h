// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include "Engine/World.h"
#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackInput.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGSimulationUnreal/PCTimeManagement/ChaosTickMapper.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/SimulationManagerConcept.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/Network/ConnectionSlotKey.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ServerInputDelayQueue.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"   // brawlerHitRouting::System (fourth-peer system)

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h"

#include "SimulationManagerUImpl.generated.h"

// Rare simulation lifecycle: TimeResync.*, Resim.*, ResimCheck.Divergence, ResimCheck.PrepareRestore
DECLARE_LOG_CATEGORY_EXTERN(LogOGSim, Log, All);
// Per-tick simulation chatter: AuthoritySimulation, ClientPrediction, CollectInput, ResimCheck.*
DECLARE_LOG_CATEGORY_EXTERN(LogOGSimTick, Log, All);
// Manager / simulatable lifecycle: SimulationManager:*, tryRegister:*, NewFramework:*
DECLARE_LOG_CATEGORY_EXTERN(LogOGMgmt, Log, All);
// Replication-channel events: ServerReceive, Send*ToClients, ReceiveCorrection*, InjectCorrection*
DECLARE_LOG_CATEGORY_EXTERN(LogOGNet, Log, All);
// Fallback for unrecognized prefixes
DECLARE_LOG_CATEGORY_EXTERN(LogOG, Log, All);
// Game-rule logging (DAttackMachine/Radial/Guard via OGBLOG_G)
DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

enum class TryRegisterStatus { Pending, Ready };

class USimmableUpdateComponent;
class ASimulationManagerUImpl;
class ASimulationTimingRelay;

struct FSimulationState2 : public Chaos::FSimCallbackOutput
{
	FSimulationState2()
		: FSimCallbackOutput()
		, bIsValid(false)
	{
	}

	virtual ~FSimulationState2() {}

public:
	void Reset() { bIsValid = false; }
	void Copy(const FSimulationState2& Value);
	bool IsValid() const { return bIsValid; }
	bool bIsValid;
	float DeltaTime;
};

struct FSimulationInput2 : public Chaos::FSimCallbackInput
{
	virtual ~FSimulationInput2() {}

	void Reset()
	{
		bInitialized = false;
		m_world = nullptr;
		m_manager = nullptr;
	}

	bool bInitialized = false;
	UWorld* m_world;
	ASimulationManagerUImpl* m_manager = nullptr;

private:
};

class ASimulationManagerUImpl;

class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FSimulationInput2,
	FSimulationState2,
	Chaos::ESimCallbackOptions::Presimulate |
	Chaos::ESimCallbackOptions::ContactModification |
	Chaos::ESimCallbackOptions::Rewind |
	Chaos::ESimCallbackOptions::PostSolve>
{
public:
	virtual FName GetFNameForStatId() const override;

	void setManager(ASimulationManagerUImpl* manager)
	{
		m_manager = manager;
	}

private:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPostSolve_Internal() override;
	virtual void ProcessInputs_Internal(int32 PhysicsStep);
	virtual void ProcessInputs_External(int32 PhysicsStep);

	virtual int32 TriggerRewindIfNeeded_Internal(int32 LastCompletedStep);

	virtual void ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input) override;

	virtual void FirstPreResimStep_Internal(int32 PhysicsStep);

	ASimulationManagerUImpl* m_manager = nullptr;
};

UCLASS()
class ASimulationManagerUImpl : public AActor, public ISimulationTimingRelayListener
{
    GENERATED_BODY()

public:
    ASimulationManagerUImpl();
    ~ASimulationManagerUImpl();

    // Slot 0 = authority world's manager (dedicated server / listen server / standalone).
    // Slot 1 = non-authority (pure client) world's manager.
    // PIE single-process mode can fill both concurrently — one per world.
    static ASimulationManagerUImpl* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    // Clock accessors — forwarded from the manager (type-erased here for callers outside the template).
    const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
    ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
    const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
    bool runsPrediction() const { return m_manager->runsPrediction(); }

    // [T11] Client tier-transition rollback (D5.3).
    //
    // Called from USimmableUpdateComponent::OnRep_ConnectionTier (GAME THREAD)
    // with the delay delta of an authoritative tier transition. Positive deltas
    // register rollback debt on the client clock; the clock itself ignores
    // non-positive ones.
    //
    // Deliberately a NARROW passthrough rather than an `editClientClock()`
    // accessor, for the same reason publishClientEffectiveInputDelayTicks is:
    // the client clock is otherwise driven exclusively by the core
    // SimulationManager's tick loop, and handing game-thread UObject code a
    // general mutable handle to it would invite exactly the cross-thread reach
    // this one-scalar entry point exists to bound.
    //
    // Guarded on runsPrediction(): a server/standalone manager has no client
    // clock at all (getClientClock() would std::terminate), and the whole
    // rollback concept is client-only.
    void requestTierTransitionRollback(int32 deltaDelayTicks)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;

        m_manager->editClientClock().requestTierTransitionRollback(deltaDelayTicks);
    }
    void onGameSimulation(const SimulationUpdateInfo& info)
    {
        // [ogsim-system-api] Cross-character inbound-hit routing runs INSIDE
        // m_manager->onGameSimulation, via brawlerHitRouting::System::postIntegrate
        // (fired by the SimulationSystemsExecutor after integrateAll on every tick —
        // including each resim replay tick — so the routed HitFlinch flags stay
        // deterministic, D4). There is no adapter-side routing wrapper anymore: the
        // former routeInboundHits() shim + its map were removed once the routing
        // system owned the whole pass (T8 emptied the adapter map, T10 deleted the
        // shim). No reset-order hazard remains — the single routing pass is the
        // system's postIntegrate.
        m_manager->onGameSimulation(info);
    }
    void onPostGameSimulation(const SimulationUpdateInfo& info) { m_manager->onPostGameSimulation(info); }
    unsigned int onCheckIsSimilar() { return m_manager->onCheckIsSimilar(); }
    void prepareResimulation(int32_t chaosStep, uint32_t simTick) { m_manager->prepareResimulation(chaosStep, simTick); }
    // Defined in .cpp — requires full USimmableUpdateComponent definition for NetSync template instantiation.
    void onPostSimulationGameThread();

    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // SimulationManagerOwnerConcept members — timing buffer forwarded through relay.
    FSmallSimulationStateSyncBuffer& getSyncedTimingBuffer();
    void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
    {
        m_onTimingInfoReceivedCallback = std::move(fn);
    }

    // Called from ASimulationTimingRelay::OnRep_Buffer on clients via ISimulationTimingRelayListener.
    virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) override
    {
        if (m_onTimingInfoReceivedCallback)
        {
            m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
            return;
        }
        if (!m_manager.has_value())
            return;
        m_manager->editNetworkEstimator().updateRTT(roundTripTime);
        m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
    }

    void OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime);
    void OnPhysicsStep(FPhysScene* Scene, float DeltaTime);
    void OnPostPhysicsStep(FChaosScene* Scene);

    TryRegisterStatus tryRegister(
        unsigned int id,
        SimulatableBrawler simulatable,
        USimmableUpdateComponent& owner,
        std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider,
        bool isAuthority);

    void unregisterFromNewFramework(unsigned int id, USimmableUpdateComponent& owner, bool isAuthority);

    void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);

    ChaosTickMapper& editChaosTickMapper() { return m_chaosTickMapper; }
    const ChaosTickMapper& getChaosTickMapper() const { return m_chaosTickMapper; }

    ChaosPhysicsBodyAdapter&  editPhysicsBodyAdapter() { return m_physAdapter.value(); }
    ChaosSpatialQueryAdapter& editQueryAdapter()       { return m_queryAdapter.value(); }

    // Read-only, game-thread-safe body adapter (reads GT-interpolated state).
    // First runtime consumer is the block-prediction viz. Const-return because the
    // PhysicsBodyReaderAdapter concept only requires const methods.
    const ChaosPhysicsBodyReaderAdapter& getPhysicsBodyReaderAdapter() const { return m_physReaderAdapter.value(); }
    SimulationObjectStorage<SimulatableBrawler>& editStorage() { return m_storage; }

    SimulationReconciliation<SimulatableBrawler>&       editReconciliation()       { return m_reconciliation; }
    const SimulationReconciliation<SimulatableBrawler>& editReconciliation() const { return m_reconciliation; }

    // [T9] The one shared TimeConfig, for game-thread consumers that must BIND to
    // it by reference rather than copy it (a copy could silently diverge from the
    // instance the clocks read). Returns nullptr until the core manager has been
    // constructed — callers emplace lazily and retry, exactly as the T10 tier
    // table does. Pointer rather than reference precisely so that pre-construction
    // state is representable instead of being undefined behaviour.
    const TimeConfig* getTimeConfigPtr() const
    {
        return m_manager.has_value() ? &m_manager->getTimeConfig() : nullptr;
    }

    // [T9 part 3] Publish the client's effective Layer-1 input delay to the
    // collect path.
    //
    // GAME THREAD -> PHYSICS THREAD. Called from
    // USimmableUpdateComponent::OnRep_ConnectionTier (and once at registration,
    // to establish the pre-arrival baseline). The value lands in a lone
    // std::atomic<int32> that SimulationNetSync::collectInputAll loads once per
    // tick on the physics thread — see setClientEffectiveInputDelayTicks for why
    // a bare atomic is sufficient here and why this is NOT the same problem as
    // T10's queue.
    //
    // Deliberately a NARROW passthrough rather than an editNetSync() accessor:
    // the net sync is otherwise driven exclusively by the core SimulationManager,
    // and handing game-thread UObject code a general handle to it would invite
    // exactly the cross-thread reach this method exists to keep to one scalar.
    void publishClientEffectiveInputDelayTicks(int32 delayTicks)
    {
        m_netSync.setClientEffectiveInputDelayTicks(delayTicks);
    }

    int32 getClientEffectiveInputDelayTicks() const
    {
        return m_netSync.getClientEffectiveInputDelayTicks();
    }

    // ---- C.2 server-authoritative RTT tier + input delay (T10) -------------
    //
    // OPTION A (C1 decision, 2026-07-19): the SERVER is the sole owner of the
    // RTT tier. It derives each connection's tier from its OWN per-connection
    // FNetPing RoundTrip reading and replicates the result to the owning client
    // (USimmableUpdateComponent::m_replicatedConnectionTier). The client never
    // computes a tier and never samples RTT for tier purposes — that is what
    // makes client/server tier disagreement impossible by construction rather
    // than merely unlikely.
    //
    // [T20] The whole reception subsystem — tier table, delay queue, claim map,
    // and the orchestration over them — now lives in the engine-agnostic core
    // ServerReceptionCoordinator. This manager owns ONE instance of it and has
    // shrunk to a thin transport adapter: it acquires engine primitives (Address,
    // playerSlot, RTT, sim-tick, wire decode) and forwards them in.
    //
    // AUTHORITY-role manager only, std::nullopt on a pure client: the coordinator
    // borrows `const TimeConfig&` from m_manager's owned config, so it can only be
    // emplaced after the manager is (BeginPlay, authority branch).
    using BrawlerReceptionCoordinator =
        ServerReceptionCoordinator<FUEConnectionHandle, SimulatableBrawler>;
    bool hasServerTierWiring() const { return m_receptionCoordinator.has_value(); }

    // [T21] ENGINE-PRIMITIVE ACCESSORS for the RPC-boundary transport adapter
    // (USimmableUpdateComponent::ServerReceiveRemoteMove). After T20 relocated the
    // reception POLICY into the core coordinator, T21 relocated the primitive
    // ACQUISITION up to the RPC handler (where the bundle and the owning actor
    // naturally live). These three accessors are all that the manager still
    // supplies: the coordinator instance it owns, the server sim tick, and the
    // id->component delivery-routing registration. None carries netcode policy.

    // The reception coordinator this manager owns, or nullptr on a pure client /
    // pre-BeginPlay. The RPC adapter resolves engine primitives and forwards them
    // straight into it; this manager owns the instance + its TimeConfig borrow.
    BrawlerReceptionCoordinator* getReceptionCoordinator()
    {
        return m_receptionCoordinator.has_value() ? &*m_receptionCoordinator : nullptr;
    }

    // The current server SIM TICK the coordinator stamps RTT samples with. This is
    // the sim-tick engine primitive (Godot must supply its own). NOTE (wart,
    // behaviour-preserved from the pre-T21 sample path): the server clock is
    // written on the physics thread and read here on the game thread; the sample
    // path has always tolerated this unsynchronized read, and the tier EMA is
    // insensitive to a one-tick skew. Left as-is to keep this refactor a pure
    // relocation; a follow-up may source it mapper-derived like the drain does.
    int32 getServerReceptionTick() const
    {
        return static_cast<int32>(getServerClock().getSimulationStep().getTick());
    }

    // Register the id->component mapping the coordinator's per-id `deliver` callback
    // (built in releaseDelayedInputsForStep) and the deliver-now fallback
    // (deliverRemoteInput) resolve against. [T24] Called ONCE at register-time
    // (USimmableUpdateComponent::tryRegisterWithNewFramework, authority), not per
    // parked slot — paired with the forgetOwner()/erase at unregisterFromNewFramework.
    // A plain overwrite: re-registering the same id is a no-op, and an id
    // legitimately replacing a dead one takes the slot over. `id ==
    // component.GetUniqueID()`. This is the delivery-routing BUFFER ACCESSOR engine
    // primitive (Godot supplies its own id->owner resolution).
    void noteDelayedInputComponent(unsigned int id, USimmableUpdateComponent& component);

    // [T24] This manager IS the ServerReceptionCoordinator's RemoteInputDeliverySink.
    // It routes a released/undelayed remote input to its owning component by id via
    // the id->component map — the ONE delivery method both the drain (per-id, in
    // releaseDelayedInputsForStep) and the core receive-loop fallback (malformed
    // slot, in ServerReceptionCoordinator::receiveInputBundle) go through. Resolves
    // id->component; a stale weak handle is dropped and the input discarded (the
    // owner was GC'd without an unregister). GAME THREAD only. The static_assert
    // that this manager satisfies RemoteInputDeliverySink lives beside the
    // definition in the .cpp.
    void deliverRemoteInput(unsigned int id, uint32 captureTick,
                            const simulatableBrawler::PlayerInput& input);

private:
    // ---- C.2 tier input delay: release (part 4) --------------------------
    //
    // The drain half of the transport adapter: pure engine-primitive acquisition
    // plus the core call. Computes the game-thread-safe upcoming sim tick
    // (ChaosTickMapper `+1` — the sim-tick mapping primitive), builds the per-id
    // `deliver` callback (the id->component routing primitive), and forwards to
    // ServerReceptionCoordinator::releaseDelayedInputs; it also drives
    // reapConnections on the same game-thread hook. GAME THREAD only — called from
    // InjectInputs_External. This is the tick-path counterpart to the RPC-path
    // adapter in USimmableUpdateComponent::ServerReceiveRemoteMove; both carry NO
    // netcode policy after the T20/T21 relocation.
    void releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps);

    // [T20] THE reception subsystem, relocated whole into the core. Authority-only
    // (std::nullopt on a pure client); borrows m_manager's TimeConfig, so emplaced
    // AFTER the manager in BeginPlay and reset BEFORE it in EndPlay.
    //
    // THREADING (load-bearing): every method on it is touched ONLY from the game
    // thread — fed in USimmableUpdateComponent::ServerReceiveRemoteMove (the RPC
    // receive path), drained + reaped in releaseDelayedInputsForStep
    // (InjectInputs_External).
    // Its owned containers have no internal synchronization, so none may be
    // touched from the physics thread, where onGameSimulationAuthority runs under
    // bTickPhysicsAsync=True. The game->physics transition stays where it always
    // was: the drain hands each released input to RemoteMoveQueue via the deliver
    // callback below — the seam the server input path already crosses (lead
    // resolution R2, 2026-07-20).
    std::optional<BrawlerReceptionCoordinator> m_receptionCoordinator;

    // Adapter-side delivery resolution: the core claim map is id-keyed (it cannot
    // hold a TWeakObjectPtr), so the coordinator's `deliver` callback hands back an
    // id and THIS map resolves it to the owning component. Populated via
    // noteDelayedInputComponent from the RPC adapter (which has both the id and
    // the component), pruned in the deliver callback when a weak handle goes stale,
    // and erased in
    // unregisterFromNewFramework (the unregister contract that replaces the core's
    // former GC-liveness read). `id == component GetUniqueID()`.
    std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>
        m_delayedInputComponentsById;


    FSimulationManagerAsyncCallback* m_asyncCallback;

    FDelegateHandle m_injectInputsExternalCallbackHandle;
    FDelegateHandle m_hysScenePostTickCallbackHandle;

    std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

    ASimulationTimingRelay* m_timingRelay = nullptr;
    ASimulationTimingRelay* findTimingRelay();

    // Adapters require the Chaos solver — emplaced in BeginPlay.
    std::optional<ChaosPhysicsBodyAdapter>   m_physAdapter;
    std::optional<ChaosPhysicsBodyReaderAdapter> m_physReaderAdapter;
    std::optional<ChaosSpatialQueryAdapter>  m_queryAdapter;

    // ---- ogsim-system-api alias chain (design §4.3) -----------------------
    // Single source of truth for the game's simulatable pack: widen this one
    // alias and every executor + storage type below inherits the widening
    // (adding SimulatableVehicle later = edit BrawlerSimulatables only).
    //
    // Kept CLASS-scoped (matching the pre-existing IntegrationLayerType /
    // ManagerType aliases this replaces) rather than the design's file-scope
    // depiction, so these names don't leak into the global namespace from a
    // widely-included UE adapter header. OGSim primitives named UNQUALIFIED —
    // the whole OGSim core is in the global namespace (lead D12, 2026-07-07).
    using BrawlerSimulatables   = SimulatableList<SimulatableBrawler>;
    using BrawlerStorage        = apply_t<SimulationObjectStorage,  BrawlerSimulatables>;
    using BrawlerNetSync        = apply_t<SimulationNetSync,        BrawlerSimulatables>;
    using BrawlerReconciliation = apply_t<SimulationReconciliation, BrawlerSimulatables>;

    // Owned simulation resources + peers — construction order matters:
    // storage → staticData → reconciliation → netSync, all top-to-bottom BEFORE
    // m_integrationLayer / m_manager (emplaced in BeginPlay) bind references to
    // them. Types come straight from the alias chain above (single source of
    // truth) — retyped in T12 so a BrawlerSimulatables widening cascades here.
    BrawlerStorage m_storage;

    // Game static data — the ownership ROOT for StaticData across the whole tree.
    // It MUST be constructed in place here and NEVER copied or moved: its nested
    // sub-StaticData members (m_attackSimulationStaticData, m_guardSimulationStaticData)
    // hold references bound to sibling members (m_attackSequences, m_attackCircle).
    // Copying/moving StaticData would silently leave those internal references
    // dangling (pointing into the moved-from original). Every downstream consumer
    // — the integration executor and the SimulationManager — therefore takes it
    // by const& only; there is NO by-value StaticData path anywhere in the tree.
    // (Invariant doc migrated from the now-deleted
    // SimulationIntegrationExecutor::getStaticData() site — T12.)
    simulatableBrawler::StaticData m_staticData;

    BrawlerReconciliation m_reconciliation{ m_storage };
    BrawlerNetSync        m_netSync{ m_storage, m_reconciliation };

    // SimulationIntegrationExecutor's leading three params are engine-specific
    // (StaticData is game-specific; the adapters are Chaos/UE), so apply_t can't
    // unpack the sim-pack marker into them. A per-adapter bind wrapper fixes
    // those three slots once, then apply_t applies the sim pack (C1 pick,
    // Option B) — preserving the single-source-of-truth property.
    template <typename... SimulatableTs>
    using BrawlerIntegrationExecFor_UE = SimulationIntegrationExecutor<
        simulatableBrawler::StaticData, ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter, SimulatableTs...>;
    using BrawlerIntegrationExec = apply_t<BrawlerIntegrationExecFor_UE, BrawlerSimulatables>;

    // Fourth peer — systems executor. Takes (a) the sim-pack marker, (b) the
    // StaticData type (systems look up game-static config in their hooks), and
    // (c) the system pack — just brawlerHitRouting::System at this point.
    using BrawlerSystemsExec = SimulationSystemsExecutor<
        BrawlerSimulatables,
        simulatableBrawler::StaticData,
        brawlerHitRouting::System>;

    // Value-owned; default-constructs the routing system (empty map, no heap).
    // Passed by reference into the manager at emplace() in BeginPlay.
    BrawlerSystemsExec m_systemsExec;

    // Integration layer and manager require adapters — emplaced in BeginPlay.
    using IntegrationLayerType = BrawlerIntegrationExec;
    std::optional<IntegrationLayerType> m_integrationLayer;

    using ManagerType = SimulationManager<
        BrawlerIntegrationExec, BrawlerNetSync, BrawlerReconciliation, BrawlerSystemsExec,
        BrawlerStorage, simulatableBrawler::StaticData>;
    std::optional<ManagerType> m_manager;

    static ASimulationManagerUImpl* s_instances[2];

    ChaosTickMapper m_chaosTickMapper;

    struct PendingRegistration
    {
        std::optional<SimulatableBrawler> simulatable;
        bool bodiesCreated = false;
        BodyId parentBodyId;
        std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider;
        bool isAuthority = false;
    };
    std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
};