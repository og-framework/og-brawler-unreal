// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"
#include "OGSimulationUnreal/ISimulationConnectionRelayListener.h"
#include "OGSimulationUnreal/ISimulationInputRelayListener.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include <set>
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
// [og-netcode-v2-input-relay T22] Server-side write-path diagnostics. A separable
// unit with a stated end date — see the header's own banner.
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
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
// [og-netcode-v2-input-relay T19] Client-side relay telemetry: RelayProbe.Read,
// RelayProbe.Arrival, RelayProbe.Stale. Its OWN category, following the
// LogOGConnRelay precedent, so the channel can be traced (or silenced) without
// touching a noisy neighbour — which is also the only way the per-window Warning
// summaries and the per-event Verbose detail can be silenced independently of each
// other. See the ini block in Config/DefaultEngine.ini.
DECLARE_LOG_CATEGORY_EXTERN(LogOGRelayProbe, Log, All);
// [og-netcode-v2-input-relay T24] Client-side prediction-vs-authority telemetry:
// DivergenceProbe.Correction (per correction, at Verbose) and
// DivergenceProbe.Window (per class per window, at Warning). Its OWN category for
// the same reason LogOGRelayProbe has one — it is the only way the per-window
// summaries and the per-event detail can be silenced independently of each other
// and of every other channel. Deliberately NOT filed under a `Resim.` tag, which
// would inherit LogOGSim=Verbose and recreate the volume defect T19 fixed.
DECLARE_LOG_CATEGORY_EXTERN(LogOGDivergenceProbe, Log, All);
// [og-netcode-v2-input-relay item 42] Client-side RESIM-GATE telemetry:
// ResimProbe.Gate / .Chaos / .Apply / .Landing (per window, at Warning) and
// ResimProbe.Request / .Landing / .Stranded (per event, at Verbose). Its OWN
// category for the third time and for the third identical reason — it is the only
// way the per-window summaries and the per-event detail can be silenced
// independently of each other and of every other channel.
//
// ⛔ THE `[ResimProbe` PREFIX IS NOT `[Resim.` AND NOT `[ResimCheck.`, AND THAT IS
// THE POINT. `[Resim.` inherits LogOGSim=Verbose (T19's 10 MB defect); `[ResimCheck.`
// is split across LogOGSim and LogOGSimTick, so a family filed under it could not
// be turned on or off as one thing. Item 42 exists partly because item 31's own
// denominator, `[ResimCheck.IsSimilar]`, has ZERO occurrences in every log on disk
// for exactly that reason. Do not "tidy" this family under either of them.
DECLARE_LOG_CATEGORY_EXTERN(LogOGResimProbe, Log, All);
// Game-rule logging (DAttackMachine/Radial/Guard via OGBLOG_G)
DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

enum class TryRegisterStatus { Pending, Ready };

// [T15 / input relay] The local-input provider signature, named once so the four
// UE-side sites that pass one around cannot drift apart.
//
// The second parameter is the character's own raw-capture history. It arrives as
// a PARAMETER rather than being reached for through the manager because the
// motion-sequence matcher runs inside the provider and needs it — see the
// InputProviderMapFor comment in OGSimulation/SimulationNetSync.h for the
// read-before-push ordering this shape makes visible. NetSync binds the argument;
// nothing on this side ever constructs one.
using BrawlerInputProviderFn = std::function<simulatableBrawler::PlayerInput(
	const SimulationTimeStep&,
	const LocalInputCache<simulatableBrawler::PlayerInput>&)>;

class USimmableUpdateComponent;
class ASimulationManagerUImpl;
class ASimulationTimingRelay;
class ASimulationConnectionRelay;

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
class ASimulationManagerUImpl : public AActor,
                                public ISimulationTimingRelayListener,
                                public ISimulationConnectionRelayListener,
                                public ISimulationInputRelayListener
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

    // [T11] Client tier-transition stall (D5.3).
    //
    // Called from this manager's own applyTierTransitionStall (GAME THREAD),
    // driven by ASimulationConnectionRelay's OnRep since the T10 tier-channel
    // migration, with the delay delta of an authoritative tier transition.
    // Positive deltas
    // register stall debt on the client clock; the clock itself ignores
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
    // stall concept is client-only.
    void requestInputDelayIncreaseStall(int32 deltaDelayTicks)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;

        m_manager->editClientClock().requestInputDelayIncreaseStall(deltaDelayTicks);
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

    // [og-netcode-v2-input-relay item 42] THE TWO CHAOS-SIDE PROBE FEEDS (I3, I4).
    //
    // NARROW NAMED PASSTHROUGHS rather than an `editResimGateProbe()` handle, for
    // exactly the reason `requestInputDelayIncreaseStall` above is narrow: the
    // probe is a physics-thread-only object whose whole correctness rests on
    // nobody else touching it, and handing the adapter a general mutable handle
    // would invite precisely the cross-thread reach these two entry points exist
    // to bound. Both are called from FSimulationManagerAsyncCallback, on the
    // physics thread, which is the same thread every other feeder runs on.
    //
    // WHY THE ENGINE'S OWN NUMBERS ARE NOT AN OPTION. `FRewindData::
    // FindValidResimFrame` and `FPBDRigidsSolver::ConditionalApplyRewind_Internal`
    // log every refusal and every silent frame-skip behind `DEBUG_REWIND_DATA` /
    // `DEBUG_NETWORK_PHYSICS`, which are compiled out of any normal build. A
    // refused rewind is therefore COMPLETELY SILENT today, and our side simply
    // retries next frame because nothing cleared needsResimulation(). These two
    // calls are the only way that second gate becomes countable.
    // ([item 45] the retry property is now structural rather than incidental: only
    // the resim-completion edge can consume a pending anchor, so a refusal cannot
    // clear the gate even by accident. The counters' meaning is unchanged.)
    //
    // Guarded on manager presence + runsPrediction so a server or a
    // pre-composition frame is a no-op rather than a null deref.
    void noteResimRequest(unsigned int anchorTick, int32 lastCompletedStep, int32 requestedChaosFrame)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;
        m_manager->editResimGateProbe().noteRequest(
            static_cast<std::uint32_t>(anchorTick), lastCompletedStep, requestedChaosFrame);
    }

    void noteResimGrant(int32 grantedChaosFrame)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;
        m_manager->editResimGateProbe().noteGrant(grantedChaosFrame);
    }
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
        BrawlerInputProviderFn inputProvider,
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

    // [og-netcode-v2-input-relay T7] THE REMOTE-PROXY VISUALIZATION INPUT SOURCE.
    //
    // The newest input the server has RELAYED for character `id`, or nullopt when
    // there is none to have: no relay store exists (a locally-controlled character,
    // or any character on the AUTHORITY — a listen server allocates no stores), or
    // one exists but nothing has arrived on it yet.
    //
    // WHAT IT REPLACES. The input-carrying viz sites used to read the correction
    // cache's input column through `editReconciliation().getLatestInput(...)`, fed
    // by the SERVER->CLIENT correction-input channel. [T8] THAT CHANNEL IS NOW
    // DELETED, so this is not merely the preferred remote source — it is the only
    // one. [T16] The COLUMN is deleted too, `getLatestInput` with it. The nullopt
    // contract here was deliberately modelled on getLatestInput's, so the "cold
    // source => skip the viz this frame" behaviour at the call site is preserved
    // rather than re-specified: see BrawlerVisualizationInputSource.h.
    //
    // Deliberately a NARROW passthrough rather than an editNetSync() accessor, for
    // exactly the reason publishClientEffectiveInputDelayTicks below states: the
    // net sync is otherwise driven only by the core SimulationManager, and this
    // keeps the game thread's reach into it to one read-only query.
    //
    // GAME THREAD. The store's writer (OnRep_RelayedInputRing) is game-thread too,
    // so this reader is same-thread with the writer — see the threading note on
    // SimulationNetSync::getLastRelayedInput.
    std::optional<simulatableBrawler::PlayerInput> getLastRelayedInput(unsigned int id) const
    {
        return m_netSync.getLastRelayedInput<SimulatableBrawler>(id);
    }

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
    // recomputeAndPublishEffectiveInputDelay — on every relay tier OnRep, on every
    // relay delay floor OnRep, and
    // once in BeginPlay to establish the pre-arrival baseline. The value lands in a lone
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

    // ---- [T10 / og-netcode-v2-input-relay] CLIENT TIER CONSUMPTION ---------
    //
    // The client half of the tier system used to live on USimmableUpdateComponent
    // (one ReplicatedTierConsumer per CHARACTER, fed by that character's own
    // OnRep). It lives here now, one per WORLD, fed by the per-connection
    // ASimulationConnectionRelay through ISimulationConnectionRelayListener.
    //
    // WHY THAT IS THE RIGHT SHAPE: a tier is a property of the wire, and every
    // character on a machine shares one wire. The old shape had N characters each
    // driving the SAME single `m_clientEffectiveInputDelayTicks` atomic and each
    // requesting its own tier-transition stall — for a couch co-op client that
    // meant one wire transition producing TWO stall requests against a clock
    // that ACCUMULATES debt. One listener per world makes the request count match
    // the transition count. (Documented as an intended consequence of the
    // migration; single-character clients are unaffected.)

    // A tier TRANSITION arrived on the wire (relay OnRep). Applies the new tier to
    // the cache, republishes the effective delay, and converts (old -> new) into a
    // prediction stall.
    virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier) override;

    // A latched tier is being replayed because it landed before this listener
    // bound. Applies + republishes but does NOT stall — this is the FIRST tier
    // ever applied, so there are no ticks predicted against a previous tier's
    // delay to give back, and its "previous" state is the pre-arrival no-tier
    // fallback (item 62 / RN-12: `rttTierInputDelays[kMaxConnectionTierIndex]`)
    // rather than a tier at all.
    virtual void onConnectionTierReplayed(uint8_t tier) override;

    // ---- [T39 / og-netcode-v2-input-relay] THE RELAY-RING HOST BOUNDARY -----
    //
    // A per-character ASimulationInputRelay has become resolvable on this client
    // (it replicated in, or its Owner pointer just landed). This manager is the
    // BRIDGE and nothing more: OGSimulationUnreal must not depend on
    // OGBrawlerUnreal, so the host cannot name AOGBrawlerUECharacter or
    // USimmableUpdateComponent and needs something on this side of the module
    // boundary to resolve owner -> component for it.
    //
    // NO MAP, DELIBERATELY. The resolution is host -> GetOwner() -> character ->
    // component, three lookups on data that already exists — AActor::Owner
    // replicates, and the authority spawns the host with SetOwner(character). A
    // registry keyed by character would be a second structure to keep in step
    // with every spawn, death and travel, for nothing.
    //
    // IDEMPOTENT: the host calls this from BeginPlay, from OnRep_Owner, and again
    // from any ring OnRep that arrives while still unlinked. attachInputRelayHost
    // absorbs the repeats.
    virtual void onInputRelayHostReady(ASimulationInputRelay& host) override;

    // Read-only handle on the client tier cache (nullptr before BeginPlay has
    // built the core manager). It is the TIER half of the T11 shared recompute.
    const ReplicatedTierConsumer* getReplicatedTierConsumer() const
    {
        return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
    }

    // ---- [T11 / og-netcode-v2-input-relay] THE RELAY DELAY FLOOR -----------
    //
    // The floor is the SECOND input to the client's effective-delay recompute,
    // and it arrives on a DIFFERENT channel from the tier: session-scoped, over
    // ASimulationTimingRelay, versus wire-scoped over ASimulationConnectionRelay.
    // The two OnReps can land in either order, which is exactly why both of them
    // call ONE recompute holding BOTH cached inputs rather than each writing the
    // effective-delay atomic on its own:
    //
    //     effective = max(floor, tierKnown ? tierInputDelayTicks(tier) : forced)
    //
    // The floor half of that formula lives in the manager's TimeConfig (written
    // by the two methods below); the tier half lives in m_replicatedTierConsumer.
    // Both arms are evaluated inside ReplicatedTierConsumer::effectiveInputDelayTicks,
    // which is the single derivation the server's ServerInputDelayQueue mirrors.

    // A floor CHANGE arrived on the session channel. Stamps it into the shared
    // TimeConfig, re-derives + republishes the effective delay, and pays for an
    // INCREASE with a prediction stall (the same debt an upward tier transition
    // incurs — to the client the two are indistinguishable).
    virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;

    // A latched floor is being replayed because it landed before this listener
    // bound. Applies + republishes but does NOT stall: the pull happens inside
    // BeginPlay, before the first prediction tick, so no tick was predicted at
    // the pre-floor delay. Mirrors onConnectionTierReplayed exactly.
    virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) override;

    // ---- C.2 server-authoritative RTT tier + input delay (T10) -------------
    //
    // OPTION A (C1 decision, 2026-07-19): the SERVER is the sole owner of the
    // RTT tier. It derives each connection's tier from its OWN per-connection
    // FNetPing RoundTrip reading and replicates the result to the owning client
    // (ASimulationConnectionRelay::m_connectionTier since the T10 tier-channel
    // migration; formerly a COND_OwnerOnly property on the character component).
    // The client never
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

    // [T3 / og-netcode-v2-input-relay] This manager is ALSO the coordinator's
    // RemoteInputRelaySink — the OUTBOUND half of the receipt path. Where
    // deliverRemoteInput routes an input INTO the simulation for the character that
    // sent it, this writes the same (captureTick, dA, input) into that character's
    // replicated relay ring (FRelayedInputRing on USimmableUpdateComponent,
    // DOREPLIFETIME with NO COND_), which carries it to the OTHER clients so each
    // peer can simulate that character with its real input instead of extrapolating.
    // `dA` is the SCHEDULE STAMP: the effective input delay the authority held for
    // that wire at receipt, so a peer can derive the application tick as
    // `captureTick + dA` (RelayDelaySpectrumDesign.md §5).
    //
    // Resolves id->component through the SAME register-time map deliverRemoteInput
    // uses, with the same stale-handle pruning. GAME THREAD only. The static_assert
    // that this manager satisfies RemoteInputRelaySink lives beside the definition
    // in the .cpp, mirroring the delivery sink.
    void relayRemoteInput(unsigned int id, uint32 captureTick, uint8 dA,
                          const simulatableBrawler::PlayerInput& input);

    // -----------------------------------------------------------------------
    // ⭐ [og-netcode-v2-input-relay T34] THE PRE-DIET CHARACTER CAP.
    //
    // ⛔ THIS CONSTANT AND ITS CHECK ARE DELETED BY ITEM 40 (the wire diet). Their
    // ABSENCE after that item is the "cap lifted" statement — there is no flag to
    // flip and no value to raise, which is deliberate: a cap you can quietly widen
    // is not a cap.
    //
    // WHY 4, DERIVED (T38 §16.1, and the same arithmetic is asserted from inside
    // the suite by og-brawler-tests/RoundVsPacketBudgetTest.cpp's pre-diet table).
    // The binding constraint is the INPUT GUARANTEE: all the remote characters'
    // relay rings must fit one packet by themselves, because a ring that gets
    // scheduled out under R = 0 loses its whole staged burst with no recovery path.
    // Pre-diet that bound is `81*SumE + 13.1*(N-1) <= 943 B`. Modelling a join as
    // "the joiner's ring at the measured settling burst of 8, everyone else at the
    // measured average", N = 4 clears it with about nine tenths of one entry to
    // spare and N = 5 does NOT — at five characters an ORDINARY join crosses the
    // bound, with no server hitch required. Item 40's diet cuts the entry from 81 B
    // to ~45 B, which is what buys the 6-character target back.
    //
    // The second half of the pre-diet configuration is `correctionRotationK = 1`
    // (TimeConfig's compiled default, lowered by this same item): at K = 2 with
    // un-dieted 316 B states the second state's batch fails INSIDE Iris's
    // huge-object window on roughly a third of frames at N = 4, which chunks it and
    // blocks that character's newer snapshots for ~1 RTT. See that field's block.
    //
    // CHECKED AT CHARACTER REGISTRATION ON THE AUTHORITY — a path that provably
    // runs every session, once per character. A cap that depends on nobody spawning
    // a fifth character is not a fence, and this initiative has shipped three things
    // that were silently inert already (item 33's unread ini key, item 36's
    // invisible Log line, T43 finding 1's degenerate flush).
    static constexpr int32 kPreDietCharacterCap = 4;

private:
    // [T34] The pre-diet cap's denominator: the ids that completed AUTHORITY
    // registration and have not unregistered. A SET rather than a counter, and the
    // reason is that the two ends are not symmetric — `tryRegister` increments only
    // on the Ready path, while `unregisterFromNewFramework` runs for any component
    // ending play including one abandoned mid-Pending, so a bare counter would
    // silently drift downwards and disarm the cap. Bounded by live characters, and
    // reaped by the same unregister contract that reaps the coordinator's claim map.
    std::set<unsigned int> m_authorityRegisteredIds;


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

    // ---- [T10] client tier cache internals --------------------------------
    //
    // Feed one authoritative tier into the cache and republish the resulting
    // effective input delay through the shared recompute below.
    void applyReplicatedConnectionTier(uint8 tier);

    // [T11] Feed one authoritative FLOOR into the shared TimeConfig and republish
    // through the same recompute. `payForIncrease` distinguishes the two listener
    // entry points (a genuine OnRep pays a prediction stall for a rise in the
    // effective delay; a latch replay does not — see the interface header).
    void applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease);

    // [item 62 / RN-12] ADVISORY-ONLY floor classification, logged from BOTH
    // floor intake points (the ini override at the composition root and this
    // manager's own OnRep, in `applyReplicatedRelayDelayFloor`) — the same
    // belt-and-braces shape the A5 clamp already uses. Never an assert: see
    // `classifyRelayDelayFloor` (ConnectionTierTable.h) for why floor 0 must stay
    // silent. No-ops if `m_manager` has not been emplaced yet.
    void logRelayDelayFloorAdvisory(int32 floorTicks);

    // THE SHARED TWO-INPUT RECOMPUTE (T11, review amendment A2b). Derives
    //
    //     effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
    //                                       : rttTierInputDelays[kMaxConnectionTierIndex])
    //
    // from the two cached inputs — the floor in m_manager's TimeConfig and the
    // tier in m_replicatedTierConsumer — and pushes the result across to the
    // collect path (game thread -> the lone std::atomic<int32> collectInputAll
    // reads once per tick).
    //
    // ONE SITE, BOTH CHANNELS. T10 wired the tier OnRep straight to the atomic;
    // adding a second, independent input made that shape untenable — two writers
    // each holding half the formula would answer with a stale other-half whenever
    // their OnReps interleaved. Both OnReps (and the composition root's baseline
    // publish) now go through here.
    //
    // RETURNS the change in published effective delay (new - previous), so a
    // caller that must pay for an increase can. Callers that publish a baseline
    // rather than react to a transition (the composition root) ignore it.
    int32 recomputeAndPublishEffectiveInputDelay();

    // Turn one tier transition into a client prediction stall. The DECISION
    // (how many ticks, if any) is `shouldStallForTierTransition`
    // (ConnectionTierTable.h, item 69 / og-netcode-v2-input-relay) — a pure
    // function with its own LLT coverage, since this class has none. This
    // method is the UE plumbing around it: it resolves `hadAnyTier` (whether
    // the client had ALREADY received an authoritative tier before this call
    // — see the caller, `onConnectionTierReceived`, for why that must be
    // captured before this call rather than inferred from `oldTier`) and
    // forwards a positive result to the client clock; non-positive results are
    // dropped by the clock too, belt-and-braces.
    void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);

    // [T9, relocated by T10] The client's ENTIRE share of the tier system: it
    // stores the replicated tier and turns it into behaviour through the SHARED
    // ConnectionTierTable lookups the server's table also delegates to. Borrows
    // m_manager's TimeConfig by reference, so it is emplaced AFTER m_manager in
    // BeginPlay and reset BEFORE it in EndPlay.
    //
    // Emplaced on BOTH roles deliberately: a pure client is the obvious consumer,
    // but a listen-server host runs client-side code paths on an AUTHORITY
    // manager, and an unbound cache there would answer 0 delay where the
    // pre-arrival no-tier fallback (item 62 / RN-12:
    // `rttTierInputDelays[kMaxConnectionTierIndex]`) is correct. No tier ever
    // reaches an authority world — the server writes the relay property, it never
    // receives an OnRep for it — so the authority cache stays at that fallback
    // for the whole session, which is exactly what the retired per-character
    // path also converged on.
    std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

    // [T11] The last value recomputeAndPublishEffectiveInputDelay() pushed into
    // the collect path. Kept here rather than read back out of the net sync
    // because the delta is a GAME-thread bookkeeping quantity: the atomic exists
    // to hand one scalar to the physics thread, not to be used as shared state.
    // Seeded to the pre-publish 0 so the composition root's first publish reports
    // its full value as the delta — which that caller deliberately ignores, since
    // nothing has been predicted yet.
    int32 m_lastPublishedEffectiveInputDelayTicks = 0;

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

    // [og-netcode-v2-input-relay T20; extended to the client + renamed T49] PROBE A
    // — sim ticks per game-thread frame, i.e. FRAME HEALTH. Purely diagnostic;
    // nothing reads it but the log lines it feeds.
    //
    // WHY IT SITS HERE AND NOT IN SimulationNetSync, where the other three probes
    // live: this one is measured on THIS actor's game thread, from the Chaos
    // pre-step hook, and the only tick source that is legal to read there is the
    // ChaosTickMapper's atomic offset — which this actor owns and SimulationNetSync
    // has no access to. Fed from releaseDelayedInputsForStep (called from
    // InjectInputs_External), which now samples on BOTH roles — see that function's
    // banner in the .cpp for the T49 argument (same atomic, same hook, both proven
    // safe already for the server side) and for how the two roles are told apart in
    // the log.
    //
    // ONE INSTANCE PER ACTOR, one actor per role (see `instanceFor`), so a
    // server-role and a client-role actor in the same PIE process each own their
    // own probe — never shared, so no cross-role synchronization question arises.
    //
    // GAME THREAD ONLY. Same rule as m_receptionCoordinator above and for the same
    // reason: it has no internal synchronization.
    FrameHealthProbe m_frameHealthProbe;

    // [og-netcode-v2-input-relay T22] PROBES 5 + 6 — the SERVER WRITE PATH. Purely
    // diagnostic, like the one above; nothing reads them but the lines they feed.
    //
    // ⭐ WHAT THEY EXIST TO CLOSE. `RelayDepthCoverageHypothesis.md` §9.11a
    // eliminates every upstream stage and concludes the ~41 % relay loss is in
    // Iris's send path. Its elimination of the SERVER'S OWN WRITE is
    // "writes on every accepted receipt, and receipts are complete", plus
    // "1.003 sim ticks per frame". Both are true and neither covers the case that
    // matters: the ring is written from the RPC RECEIPT path, so its writes are
    // paced by PACKET ARRIVAL, not by the sim — and at the ring's then-compiled
    // retention depth of 1 entry, the ring is REPLACE-LATEST while Iris polls
    // once per game-thread frame. Two writes in one frame therefore lose the first
    // one in server memory, before replication ever sees it, and the client
    // observes that as exactly the same `gapCaptureTicks > 1` a send-path drop
    // produces. Nobody has measured writes-per-frame. m_relayWriteProbe does.
    //
    // m_connectionBudgetProbe replaces the derived half of the budget model with a
    // measured one: the connection's actual negotiated `CurrentNetSpeed`, its real
    // bytes/packets per tick, its ack-derived outgoing loss, and whether
    // `IsNetReady()` was ever false — the state in which Iris's
    // UDataStreamChannel::Tick returns without writing anything at all.
    //
    // BOTH ARE FED FROM THE GAME THREAD, SERVER ONLY. The write probe from
    // relayRemoteInput (the relay tap, which runs on the RPC receive path); the
    // budget probe from releaseDelayedInputsForStep, which is server-only by
    // construction. Neither has internal synchronization, and neither needs any.
    // [T34] The stage capacity is INJECTED, because it is what `observableX1000`
    // means: under flush-on-poll a frame's whole staged burst is published, so the
    // ceiling is `min(writesThisFrame, kMaxDepth)` rather than 1. Passing the codec
    // constant here keeps the probe STL-only and keeps the two in step by
    // construction.
    RelayWriteProbe       m_relayWriteProbe{
        RelayStageCapacity{ static_cast<uint32>(relayedInputRing::kMaxDepth) } };
    ConnectionBudgetProbe m_connectionBudgetProbe;

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
        BrawlerInputProviderFn inputProvider;
        bool isAuthority = false;
    };
    std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
};