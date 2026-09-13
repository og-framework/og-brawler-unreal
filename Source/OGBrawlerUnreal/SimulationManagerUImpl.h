// SPDX-License-Identifier: BUSL-1.1
//
// ===========================================================================
// ASimulationManagerUImpl - THE UNREAL COMPOSITION ROOT AND TRANSPORT ADAPTER
// ===========================================================================
// ORIENTATION - read this before the members. Every rationale, provenance note
// and worked derivation is in docs/SimulationManagerUImpl-rationale.md
// (BUSL-1.1, this subtree); the section marks below are that document.
//
// WHAT THIS CLASS IS. It owns every simulation peer, binds them to Chaos, and
// converts engine primitives into core calls. It carries NO netcode policy -
// the policy lives in OGSimulation, which never names an engine type.
//
// TWO INSTANCES PER PROCESS, one per world - instanceFor(isAuthority):
//   slot 0  AUTHORITY  dedicated server, listen-server host, standalone
//   slot 1  CLIENT     pure client; the only role that predicts and resims
// PIE fills both concurrently. They share nothing.
//
// THREADS. Every member here is GAME THREAD unless this table says otherwise.
//   GAME      BeginPlay/EndPlay, all four OnRep listeners, the RPC receipt
//             path, InjectInputs_External -> releaseDelayedInputsForStep,
//             deliverRemoteInput, relayRemoteInput
//   PHYSICS   FSimulationManagerAsyncCallback's five _Internal hooks and
//             everything the core SimulationManager runs beneath them
//   CROSSING  THREE as of [ringout task 5], and they are not alike. The WRITE
//             is one scalar:
//             publishClientEffectiveInputDelayTicks -> a std::atomic<int32>
//             that collectInputAll loads once per tick. The READ is an
//             ACCEPTED TEAR: the two input-history polls read the
//             physics-written LocalInputCache slots and correction-cache
//             lineage, for a display that decides nothing; the argument
//             for it is §1's, not this table's. The input-delay decomposition
//             inside pollInputHistoryLanes reads three GAME-THREAD members
//             (the tier consumer, the shared TimeConfig, the resolution
//             peer's published atomic) that this call already runs on, so
//             it is NOT a third crossing -- same thread as its readers. The
//             reader's hasCorrectionCache() presence test is one more: the
//             cache map it asks is mutated on the GAME THREAD alone.
//             The clock's seven diagnostic-view reads, taken beside that
//             same poll's prediction tick, are that SAME accepted tear and
//             not a new class of crossing. §1
//             ⭐ THE THIRD IS A SECOND READ, AND IT IS A NEW MEMBER, NOT MORE
//             READS INSIDE AN EXISTING ENTRY POINT - which is why it gets a
//             bullet where the clock reads deliberately did not. The ring-out
//             score push in OnPostPhysicsStep reads brawlerRingout::ScoreSystem's
//             score table off m_systemsExec on the GAME thread; postIntegrate
//             writes it on the PHYSICS thread. It is the SAME accepted tear,
//             for the same two reasons and no others: (a) the table cannot be
//             RESTRUCTURED under the reader - the only insert and erase are
//             onCharacterRegistered / onCharacterUnregistered, both driven from
//             tryRegister / unregisterFromNewFramework, both GAME THREAD - so
//             the read cannot chase a rehashed bucket; (b) the award reaches an
//             EXISTING entry and writes one naturally-aligned four-byte word,
//             which cannot tear, for a SCOREBOARD that decides nothing. Worst
//             case: one number one tick stale.
//             ⛔ (a) HAS A PRECONDITION - postIntegrate's operator[] INSERTS for
//             an unseeded id, deliberately. It is unreachable in a legal session
//             because the roster is seeded before tryRegister returns Ready, and
//             the push carries a checkf that says exactly that. A change that
//             makes an award reach an unseeded id breaks this crossing IN KIND,
//             not in degree. §1
//   The rest have NO internal synchronization: m_receptionCoordinator,
//   m_frameHealthProbe, m_relayWriteProbe, m_connectionBudgetProbe,
//   m_inputHistory and m_delayedInputComponentsById. §1
//   ⛔ NOTHING ELSE CROSSES.
//
// NARROW PASSTHROUGHS, NOT HANDLES. requestInputDelayIncreaseStall,
// publishClientEffectiveInputDelayTicks, getLastRelayedInput, getLocalInputCache,
// pollInputHistory, getInputHistoryRows, pollInputHistoryLanes, getInputHistoryLanes,
// noteResimRequest and noteResimGrant are one-purpose
// entry points rather than edit*() accessors, because a general mutable handle
// invites exactly the cross-thread reach each of them exists to bound. §1
// ⛔ DO NOT WIDEN ONE INTO AN ACCESSOR.
//
// CONSTRUCTION ORDER - declaration order IS construction order, and nothing
// enforces it but that rule (§2):
//   m_storage -> m_staticData -> m_reconciliation -> m_inputResolution ->
//   m_netSync, then BeginPlay emplaces m_integrationLayer -> m_manager ->
//   m_replicatedTierConsumer / m_receptionCoordinator. Those last two borrow
//   m_manager's TimeConfig, so both reset BEFORE it in EndPlay.
//
// THE CLIENT'S EFFECTIVE INPUT DELAY - the one formula this header serves:
//   effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
//                                    : rttTierInputDelays[kMaxConnectionTierIndex])
// Two independent channels feed it (session floor, per-wire tier) and either
// OnRep may land first, so ⛔ both go through one recompute
// (recomputeAndPublishEffectiveInputDelay) and neither writes the atomic alone. §5
//
// FOUR SESSION KNOBS, all read once in BeginPlay (.cpp), each with an
// unconditional Warning proof line, because a knob with no proof line cannot be
// told from one that never took: RelayDelayFloorTicks, CorrectionRotationK,
// ResimTriggerPolicy, and one retired ring-depth key. §3
//
// LOG CATEGORIES. The three probe families below each get their OWN category,
// because that is the only thing that silences a family's per-window Warning
// summaries independently of its per-event Verbose detail. `[Resim.` inherits
// LogOGSim=Verbose and `[ResimCheck.` is split across two categories, so neither
// can be switched as one thing. §4
// ⛔ ONE CATEGORY PER PROBE FAMILY.
// ⛔ NO PROBE FAMILY MAY BE FILED UNDER `[Resim.` OR `[ResimCheck.`
//
// BORROWED TIMECONFIG. m_replicatedTierConsumer and m_receptionCoordinator each
// hold `const TimeConfig&` from m_manager, so each is emplaced AFTER it in
// BeginPlay and reset BEFORE it in EndPlay. Neither may outlive it. §2
// ===========================================================================

#pragma once

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
// Explicit, not transitive through SimulationNetSync.h: the peer is a sibling now. §2
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/Network/ConnectionSlotKey.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ServerInputDelayQueue.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
// Server write-path diagnostics; separable, with an end date in its own banner.
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"   // brawlerHitRouting::System (fourth-peer system)
#include "OGBrawler/BrawlerRingoutSimulation.h"  // brawlerRingout::SpawnSlotAllocator (task 3)
#include "OGBrawler/BrawlerRingoutScoreSystem.h" // brawlerRingout::ScoreSystem (fourth-peer system, task 4)

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h"
#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"

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
// Client relay telemetry: RelayProbe.Read / .Arrival / .Stale. Own category, see the banner. §4
DECLARE_LOG_CATEGORY_EXTERN(LogOGRelayProbe, Log, All);
// Client prediction-vs-authority telemetry: DivergenceProbe.Correction / .Window. §4
DECLARE_LOG_CATEGORY_EXTERN(LogOGDivergenceProbe, Log, All);
// Client resim-gate telemetry: ResimProbe.Gate / .Chaos / .Apply / .Landing / .Request / .Stranded. §4
DECLARE_LOG_CATEGORY_EXTERN(LogOGResimProbe, Log, All);
// Game-rule logging (DAttackMachine/Radial/Guard via OGBLOG_G)
DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

// ⭐⭐ THE ONE-TIME MOVEMENT CVAR READ'S RESULT — [movement-sim task 16, USER RULING #3].
//
// Ruling #3 is ONE-TIME, FULL STOP: `OGBrawler.MovementModel`, `.MoveSpeed`, `.StepPeriodTicks`
// and `.StepSpeed` are read EXACTLY ONCE, when the simulation manager constructs its
// `StaticData`, and never again. Nothing in the tick path may consult a console variable: the
// values are authored data, every peer must agree on them for the whole session, and a value
// that could change mid-session would make a resimulated tick disagree with the tick it replays.
//
// ⛔ SO THIS STRUCT IS THE ONLY THING THAT CROSSES THE SEAM, and it is `const` at its one
// member. The read itself lives in `MovementSchemeCVar.cpp`, beside the variables it reads and
// the "changed too late" latch it arms — see `ASimulationManagerUImpl::readMovementStaticDataCVars`.
struct FMovementStaticDataCVars
{
	brawlerMovementSimulation::MovementModel model;
	float    maxWalkSpeed;
	uint32_t stepPeriodTicks;
	float    stepSpeed;
	// ⚠ NOT A CVAR. The ENGINE's gravity, captured at the same instant so the sim's own
	// gravity law starts life agreeing with it; `BeginPlay` then `checkf`s that agreement
	// against the WORLD's gravity, which a level is allowed to override and this is not.
	float    gravity;
};

enum class TryRegisterStatus { Pending, Ready };

// The provider signature, named once so the four sites passing one cannot drift apart. §5
// ⛔ The raw-capture history is a PARAMETER: the sequence matcher runs inside the provider.
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

// ⛔ THE OPTION SET IS A CONTRACT, NOT A WISH LIST. Every bit named below
// registers this object into one more Chaos dispatch list, and each list calls
// the matching *_Internal hook. A bit whose hook is NOT overridden here reaches
// the asserting base implementation the first time that list actually runs.
// ⛔ ContactModification was on this list from the initial release (e93cb39,
// 2026-05-26) with no OnContactModification_Internal override. It stayed dormant
// only because every body in the tree was ECR_Overlap on every channel, so Chaos
// never ran a collision modifier. The first ECR_Block body (ChaosPhysicsFactory's
// blockingCategories translation) took PIE straight into
// FPBDCollisionConstraints::ApplyCollisionModifier -> the assert. It is REMOVED
// rather than stubbed with a no-op, because ruling #5 chose solver separation
// with NO authored priority, so nothing in this tree wants the hook - and a
// registered no-op still pays a modifier callback per contact for nothing.
// ⛔ RE-ADDING THE BIT REQUIRES ADDING ITS OVERRIDE IN THE SAME EDIT.
class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FSimulationInput2,
	FSimulationState2,
	Chaos::ESimCallbackOptions::Presimulate |
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

// slot 0 = authority world, slot 1 = pure client. PIE can fill both.
    static ASimulationManagerUImpl* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    // Clock accessors — forwarded from the manager (type-erased here for callers outside the template).
    const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
    ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
    const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
    bool runsPrediction() const { return m_manager->runsPrediction(); }

// Stall debt for a positive tier delta. ⛔ getClientClock() std::terminates on a server. §5
    void requestInputDelayIncreaseStall(int32 deltaDelayTicks)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;

        m_manager->editClientClock().requestInputDelayIncreaseStall(deltaDelayTicks);
    }
    void onGameSimulation(const SimulationUpdateInfo& info)
    {
// Inbound-hit routing runs inside m_manager->onGameSimulation, via
// brawlerHitRouting::System::postIntegrate - every tick, resim replays included. §5
//
// There is no adapter-side routing wrapper anymore: the former routeInboundHits() shim and
// its map are gone, and the system's single postIntegrate pass leaves no reset-order hazard.
        m_manager->onGameSimulation(info);
    }
    void onPostGameSimulation(const SimulationUpdateInfo& info) { m_manager->onPostGameSimulation(info); }
    unsigned int onCheckIsSimilar() { return m_manager->onCheckIsSimilar(); }
    void prepareResimulation(int32_t chaosStep, uint32_t simTick) { m_manager->prepareResimulation(chaosStep, simTick); }

// The two Chaos-side resim-gate probe feeds (request, grant). §8
// ⛔ WHY OUR OWN COUNTERS: engine-side refusals sit behind DEBUG_REWIND_DATA, so a refused
// rewind is COMPLETELY SILENT and we retry next frame. These two are that gate's visibility.
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
// Defined in .cpp - needs the full USimmableUpdateComponent for NetSync instantiation.
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

// Read-only body adapter over GT-interpolated state; const because the concept requires only const.
    const ChaosPhysicsBodyReaderAdapter& getPhysicsBodyReaderAdapter() const { return m_physReaderAdapter.value(); }
    SimulationObjectStorage<SimulatableBrawler>& editStorage() { return m_storage; }

    SimulationReconciliation<SimulatableBrawler>&       editReconciliation()       { return m_reconciliation; }
    const SimulationReconciliation<SimulatableBrawler>& editReconciliation() const { return m_reconciliation; }

// The newest input the server RELAYED for `id`, or nullopt when there is none to have. §6
// ⛔ getLatestInput and its whole column are DELETED: this is the ONLY remote source now.
    std::optional<simulatableBrawler::PlayerInput> getLastRelayedInput(unsigned int id) const
    {
        return m_inputResolution.getLastRelayedInput<SimulatableBrawler>(id);
    }

// This client's own raw captures for `id`, or nullptr when none. Diagnostics only. §1
// ⛔ GAME-THREAD DOOR ONTO A PHYSICS-WRITTEN RING -- NOT same-thread with its writer, as
//   getLastRelayedInput is. Its one caller is pollInputHistory, which owns the tear. §1
// ⛔ m_reconciliation needs NO twin of this: editReconciliation() is already public.
    const LocalInputCache<simulatableBrawler::PlayerInput>* getLocalInputCache(unsigned int id) const
    {
        return m_inputResolution.getDiagnostics().localInputCache<SimulatableBrawler>(id);
    }

// ---- THE INPUT-HISTORY DISPLAY ----------------------------------------
//
// One row ring per LOCAL character, keyed by character id, fed by a render-rate poll.
// Nothing here is replicated, enters a correction payload, or reaches compute_checksum.

// Sweep `id`'s resident capture window into its ring. ⛔ THIS IS THE READ CROSSING. §1
    void pollInputHistory(unsigned int id, uint32 newestTick, float deadzone)
    {
        const LocalInputCache<simulatableBrawler::PlayerInput>* captures = getLocalInputCache(id);
        if (captures == nullptr)
            return;     // no capture line: a remote proxy, or not registered yet

        m_inputHistory.poll(id, *captures, newestTick, deadzone);
    }

// The folded rows for `id`, or nullptr when it has none. Read-only; the panel's one source.
    const brawlerInputHistoryVisualization::InputHistoryRowRing* getInputHistoryRows(
        unsigned int id) const
    {
        return m_inputHistory.findRows(id);
    }

// ---- THE RING-OUT SCOREBOARD [ringout task 6b] -------------------------
//
// `id`'s ring-out slice, or nullopt when no brawler in this world's storage carries that
// id. The scoreboard's ONE door onto the two facts it cannot get from the actor: dead,
// and the ABSOLUTE tick the respawn is due at. The score is NOT here and must not be --
// it is `AOGBrawlerUECharacter::GetRingoutScore()`, replicated, and reading it from the
// authority-only `ScoreSystem` instead would be a second code path nobody exercises.
//
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()`. `updateVisualizationAll(m_storage)`
//   takes `m_vizState = m_allState` once per game-thread pass in OnPostPhysicsStep, on the
//   line immediately above the score push; that copy is the SANCTIONED physics->game
//   handoff. A HUD reading `getAllState()` would be a fresh, unargued crossing of the
//   fence this header's banner draws, so this accessor does not offer one. §1
//
// ⛔ `const`, AND BY VALUE. The slice is 5 B of plain data; returning a reference would
//   hand a drawing surface a pointer into live storage for as long as it cared to keep it.
//   Together with the `const ASimulationManagerUImpl*` the HUD holds, "the scoreboard
//   writes no simulation state" is a compile error to break rather than a promise.
//
// ⚠ `State::respawnAtTick` IS MEANINGFUL ONLY WHILE `brawlerRingout::kFlagDead` IS SET and
//   is deliberately left at its last value once cleared. Read the flag first.
    std::optional<brawlerRingout::State> getRingoutVizState(unsigned int id) const
    {
        if (!m_storage.has<SimulatableBrawler>(id))
            return std::nullopt;

        return m_storage.get<SimulatableBrawler>(id)
            .getVizState().getState().get<brawlerRingout::State>();
    }

// Sweep `id`'s resident correction window into its provenance lane and file ONE live
// machine-state sample. `machineState` is read at the caller's own viz site: no seam here.
//
// The estimator's offset rides along and the poll pairs it with `liveTick` -- the very
// tick the lane axis is built from. ⛔ THE DISPLAY GETS ONE CLOCK SNAPSHOT PER POLL and
//   never reads the clock again at draw time, or the marker and its axis would disagree.
// ⚠ getNetworkEstimator() exists only on a predicting role, hence the guard. §5
//
// `includeDelay` gates the SAME poll's input-delay decomposition, read from three
// GAME-THREAD members this call already runs on -- the tier consumer, the shared
// TimeConfig and the resolution peer's published atomic -- so this is three more
// same-thread reads inside an existing passthrough, NOT a new crossing. §1
//
// The clock reading rides the same guard as the offset above it and is the same PAIR of
// postures already argued for that read: the estimator's two ticks are game-thread-written
// and read here on the game thread, and the clock's prediction tick is the accepted tear. §1
    void pollInputHistoryLanes(unsigned int id, uint32 liveTick, DAttackState machineState,
        std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput,
        bool pauseWhileIdle, bool includeDelay)
    {
        const std::optional<uint32> predictionOffsetTicks =
            (m_manager.has_value() && m_manager->runsPrediction())
                ? std::optional<uint32>(
                      m_manager->getNetworkEstimator().getPredictionOffsetTicks())
                : std::nullopt;

        const std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition> delay =
            (includeDelay && m_replicatedTierConsumer.has_value() && m_manager.has_value())
                ? std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition>(
                      brawlerInputHistoryVisualization::decomposeInputDelay(
                          m_replicatedTierConsumer->hasReceivedTier(),
                          m_replicatedTierConsumer->currentTierIndex(),
                          m_manager->getTimeConfig(),
                          m_replicatedTierConsumer->effectiveInputDelayTicks(),
                          m_inputResolution.getClientEffectiveInputDelayTicks()))
                : std::nullopt;

// ⛔ GUARDED LIKE THE OFFSET ABOVE: getClientClock() std::terminates on a server. §5
        std::optional<brawlerInputHistoryVisualization::ClockDriftReading> clock;
        if (m_manager.has_value() && m_manager->runsPrediction())
        {
            const ClientPredictionClock& predictionClock = m_manager->getClientClock();
            const NetworkTimeEstimator&  estimator       = m_manager->getNetworkEstimator();

            brawlerInputHistoryVisualization::ClockDriftReading reading;
            reading.predictionTick = predictionClock.getPredictionTick();
            reading.targetTick     = estimator.getTargetPredictionTick();
            reading.authorityTick  = estimator.getLastAuthorityTick();
// ⛔ CAST BEFORE THE SUBTRACTION -- a negative drift is the whole point, and two
//   unsigned ticks would wrap it into a vast positive.
            reading.driftTicks     = static_cast<int32_t>(reading.targetTick)
                                   - static_cast<int32_t>(reading.predictionTick);
            reading.pendingAction  = predictionClock.evaluateDrift();
            reading.stallDebtTicks = predictionClock.getRequiredInputDelayIncreaseStallTicks();
// ⛔ THE SEAM, THROUGH THE VIEW: the clock publishes no bare accessor for any of these.
// ⛔ COUNT BEFORE ITS TICK: the clock writes tick then count, so the tear is one-way.
            reading.skipCount              = predictionClock.getDiagnostics().skipCount();
            reading.lastSkipTick           = predictionClock.getDiagnostics().lastSkipTick();
            reading.stallCount             = predictionClock.getDiagnostics().stallCount();
            reading.lastStallTick          = predictionClock.getDiagnostics().lastStallTick();
            reading.hardResyncCount        = predictionClock.getDiagnostics().hardResyncCount();
            reading.lastHardResyncFromTick =
                predictionClock.getDiagnostics().lastHardResyncFromTick();
            reading.lastHardResyncToTick   = predictionClock.getDiagnostics().lastHardResyncToTick();

            clock = reading;
        }

        m_inputHistory.pollLanes(id,
            inputHistoryVisualizationUImpl::makeReconciliationSlotReader<SimulatableBrawler>(
                m_reconciliation, id),
            liveTick, machineState, liveInput, pauseWhileIdle, predictionOffsetTicks, delay,
            clock);
    }

// The per-tick lanes for `id`, or nullptr when it has none. Read-only; the bars' one source.
    const brawlerInputHistoryVisualization::InputHistoryTickLanes* getInputHistoryLanes(
        unsigned int id) const
    {
        return m_inputHistory.findLanes(id);
    }


// The one shared TimeConfig, to BIND not copy. ⛔ Pointer, so pre-construction is not UB. §2
    const TimeConfig* getTimeConfigPtr() const
    {
        return m_manager.has_value() ? &m_manager->getTimeConfig() : nullptr;
    }

// Publish the client's effective input delay - the one game->physics crossing. §1 §5
    void publishClientEffectiveInputDelayTicks(int32 delayTicks)
    {
        m_inputResolution.setClientEffectiveInputDelayTicks(delayTicks);
    }

    int32 getClientEffectiveInputDelayTicks() const
    {
        return m_inputResolution.getClientEffectiveInputDelayTicks();
    }

// ---- CLIENT TIER CONSUMPTION ------------------------------------------
//
// One consumer per WORLD, not per CHARACTER: a tier is a property of the wire, and N
// per-character consumers stalled one debt-accumulating clock N times per transition. §5

// A tier TRANSITION arrived: apply, republish, convert old->new into a stall.
    virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier) override;

// A tier latched before this bind: apply + republish, ⛔ never stall - it is the first. §5
    virtual void onConnectionTierReplayed(uint8_t tier) override;

// ---- THE RELAY-RING HOST BOUNDARY -------------------------------------
//
// ⛔ A BRIDGE and nothing more: OGSimulationUnreal must not depend on OGBrawlerUnreal. §6
//
// ⛔ NO MAP, DELIBERATELY: three lookups on data that already replicates beat a registry.
//
// IDEMPOTENT: three independent link paths call this.
    virtual void onInputRelayHostReady(ASimulationInputRelay& host) override;

// Read-only handle on the client tier cache (nullptr before BeginPlay built the manager).
    const ReplicatedTierConsumer* getReplicatedTierConsumer() const
    {
        return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
    }

// ---- THE RELAY DELAY FLOOR --------------------------------------------
//
// The SECOND recompute input. ⛔ Either OnRep can land first, so ONE recompute holds both. §5
//
// Both arms live in ReplicatedTierConsumer::effectiveInputDelayTicks, which the server mirrors.

// A floor CHANGE arrived: stamp, re-derive, republish, pay for an INCREASE with a stall.
    virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;

// A floor latched before this bind: apply + republish, ⛔ no stall. Mirrors the tier replay.
    virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) override;

// ---- SERVER-AUTHORITATIVE RTT TIER + INPUT DELAY ----------------------
//
// ⛔ THE SERVER IS THE SOLE OWNER OF THE RTT TIER: the client never computes or samples one,
// which is what makes tier disagreement impossible rather than merely unlikely. §5
//
// The whole reception subsystem is in the core coordinator; this manager is its adapter. §7
//
// ⛔ Authority-role only, nullopt on a pure client: it borrows m_manager's TimeConfig. §2
    using BrawlerReceptionCoordinator =
        ServerReceptionCoordinator<FUEConnectionHandle, SimulatableBrawler>;
    bool hasServerTierWiring() const { return m_receptionCoordinator.has_value(); }

// ---- ENGINE-PRIMITIVE ACCESSORS for the RPC-boundary adapter ----------
//
// ⛔ These three are all this manager supplies, and none carries netcode policy. §7

// The coordinator this manager owns, or nullptr on a pure client / pre-BeginPlay.
    BrawlerReceptionCoordinator* getReceptionCoordinator()
    {
        return m_receptionCoordinator.has_value() ? &*m_receptionCoordinator : nullptr;
    }

// The server SIM TICK for RTT samples. ⚠ WART: physics write, game read; the EMA absorbs it. §7
    int32 getServerReceptionTick() const
    {
        return static_cast<int32>(getServerClock().getSimulationStep().getTick());
    }

// The id->component mapping `deliver` resolves. ⛔ ONCE at register-time, not per slot. §7 §10
    void noteDelayedInputComponent(unsigned int id, USimmableUpdateComponent& component);

// The coordinator's RemoteInputDeliverySink - the ONE method drain and fallback share. §7
    void deliverRemoteInput(unsigned int id, uint32 captureTick,
                            const simulatableBrawler::PlayerInput& input);

// ALSO its RemoteInputRelaySink - the OUTBOUND half, staging for the OTHER clients. §6
//
// ⚠ That ring is `ASimulationInputRelay::m_relayedInputRing` under COND_SkipOwner - NOT the
// component's m_detachedRelayRing, which is the no-host fallback nothing replicates. §11
//
// `dA` is the SCHEDULE STAMP: a peer derives the application tick as captureTick + dA.
    void relayRemoteInput(unsigned int id, uint32 captureTick, uint8 dA,
                          const simulatableBrawler::PlayerInput& input);

// -----------------------------------------------------------------------
// THE PRE-DIET CHARACTER CAP.
//
// ⛔ DELETED BY THE WIRE DIET, and their ABSENCE afterwards IS the cap-lifted statement.
//
// WHY 4: every remote ring must fit one packet alone; N=4 clears that bound and N=5 does not,
// so at five characters an ORDINARY join crosses it with no server hitch required. §10
//
// The other half of the pre-diet configuration is TimeConfig::correctionRotationK. §3
//
// ⛔ CHECKED AT AUTHORITY REGISTRATION, which provably runs once per character. §10
//
// ⛔ [ringout task 4] COUPLED TO brawlerRingout::kMaxSpawnPoints, WHICH IS ALSO 4 - RAISING THIS
// ALONE SILENTLY STOPS RESPAWNING EVERY CHARACTER PAST THE 4th. The ring-out spawn table has
// exactly kMaxSpawnPoints entries and a character that gets no entry respawns with no teleport
// seed and one [Warning][Ringout.spawnSlot] per respawn. The two constants must move together.
    static constexpr int32 kPreDietCharacterCap = 4;

private:
// The cap's denominator. ⛔ A SET, not a counter: asymmetric ends would disarm the cap. §10
    std::set<unsigned int> m_authorityRegisteredIds;

// ---- THE RING-OUT SPAWN-SLOT TABLE (task 3) ---------------------------
//
// ⛔ AUTHORITY ONLY. It is populated by the registration seed below and drained by the
// unregister contract, on the authority role alone; on a client it stays empty for the
// life of the session, which is the same property `m_authorityRegisteredIds` above relies
// on and the reason both are drained ungated.
//
// ⭐ ALL THE LOGIC IS IN THE CORE TYPE, NOT HERE. `brawlerRingout::SpawnSlotAllocator`
// lives in the engine-free `BrawlerRingoutSimulation.h` so that the two properties that
// matter — two remote clients never share an index, and a released index is reused by a
// later join — are assertions in `BrawlerRingoutSimulationTest.cpp` rather than prose
// about a file the low-level-test target cannot reach. This member holds NO policy.
//
// ⛔ IT ADDS NOTHING TO THE WIRE. What rides the wire is the uint32 it produces, inside
// `brawlerRingout::InitialConditions`; the table itself is in no composite and has no
// `SerializableFields` specialization.
    brawlerRingout::SpawnSlotAllocator m_spawnSlots;


// ---- THE RING-OUT SPAWN TABLE, READ OFF THE LEVEL (task 9) ------------
//
// ⛔⛔ THIS IS THE ONE PLACE IN THE TREE THAT WRITES `StaticData` AFTER CONSTRUCTION, AND IT
// IS A DELIBERATE, ARGUED EXCEPTION — NOT AN OVERSIGHT TO BE "FIXED" BACK.
// The banner above states StaticData as constructed once and never moved. What that
// discipline actually protects is that NO TICK EVER SEES IT CHANGE: a constant the sim
// reads mid-session is a constant two peers can disagree about and a resim can replay
// against the wrong value. This write is a ONE-TIME INIT inside `BeginPlay`, BEFORE the
// integration layer and the manager are even constructed, so there is no tick for it to be
// visible to. The `checkf`s at the definition are what hold that, mechanically.
//
// ⛔ WHY IT CANNOT BE A CONSTRUCTOR ARGUMENT INSTEAD, which is the obvious "proper" fix:
// `m_staticData` is brace-initialised in a MEMBER INITIALIZER, which runs during actor
// construction, and level actors are not reachable then. `APlayerStart` exists at
// `BeginPlay` and not one moment earlier. The same sentence is already true of
// `readMovementStaticDataCVars()` above and of the gravity `checkf` in `BeginPlay`.
//
// ⭐ ALL THE POLICY IS IN THE CORE TYPE, exactly as `m_spawnSlots` above:
// `brawlerRingout::spawnPointsFromLevelPlacements` does the sort and the fallback, and it is
// covered by `Ringout.SpawnPoints.*` in `BrawlerRingoutSimulationTest.cpp`. This method holds
// the actor walk, the `FName` -> bytes conversion, and no decisions.
    void seedRingoutSpawnPointsFromLevel(UWorld& world);

// EXACTLY ONCE, and it is enforced rather than asserted — see the `checkf` at the definition.
// ⚠ PER MANAGER INSTANCE: a listen server runs TWO of these actors, and each owns its own
// `m_staticData`, so each seeds its own table once.
    bool m_ringoutSpawnPointsSeeded = false;


// ---- TIER INPUT DELAY: RELEASE ----------------------------------------
//
// Primitive acquisition plus the core call: upcoming sim tick (mapper +1, §9), the per-id
// `deliver` callback, the drain, the reap. GAME THREAD. ⛔ No netcode policy. §7
    void releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps);

// ---- CLIENT TIER CACHE INTERNALS --------------------------------------
//
// Feed one tier into the cache and republish through the shared recompute.
    void applyReplicatedConnectionTier(uint8 tier);

// Feed one FLOOR into the shared TimeConfig; `payForIncrease` tells an OnRep from a replay.
    void applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease);

// ADVISORY-ONLY, from BOTH intake points. ⛔ Never an assert: floor 0 must stay silent. §3
    void logRelayDelayFloorAdvisory(int32 floorTicks);

// THE SHARED TWO-INPUT RECOMPUTE over the floor and the tier. §5
//
// ⛔ ONE SITE, BOTH CHANNELS: two half-formula writers answer stale when OnReps interleave.
//
// RETURNS the change in published delay, so a caller that must pay for an increase can.
    int32 recomputeAndPublishEffectiveInputDelay();

// ⛔ THE DECISION is shouldStallForTierTransition, which has the LLT coverage this lacks. §5
    void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);

// The client's ENTIRE share of the tier system, over the lookups the server also uses. §5
//
// ⛔ Emplaced on BOTH roles: a listen-server host runs client paths on an AUTHORITY manager,
// where an unbound cache would answer 0 and the no-tier fallback is what is correct. §5
    std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

// The last published effective delay. ⛔ Kept here, not read back out of the atomic. §1
    int32 m_lastPublishedEffectiveInputDelayTicks = 0;

// THE reception subsystem, relocated whole into the core. Authority-only. §2 §7
//
// ⛔ THREADING, LOAD-BEARING: game thread ONLY - no container here synchronizes itself. §1
    std::optional<BrawlerReceptionCoordinator> m_receptionCoordinator;

// PROBE A - sim ticks per game-thread frame, i.e. FRAME HEALTH. Diagnostic only. §8
//
// ⛔ WHY HERE AND NOT IN SimulationNetSync with the other three: the only tick source legal
// on this thread is the ChaosTickMapper offset, which this actor owns and NetSync cannot reach. §9
//
// One instance per actor, one actor per role, so nothing is shared across roles. §1
    FrameHealthProbe m_frameHealthProbe;

// PROBES 5 + 6 - the SERVER WRITE PATH. Diagnostic only, like the one above. §8
//
// WHAT THEY CLOSE: the relay-loss hypothesis eliminated the server's own write on
// sim-paced reasoning, but the ring is written from the RPC RECEIPT path and is paced
// by PACKET ARRIVAL. §8
// ⛔ A CLIENT CANNOT TELL A COALESCED WRITE FROM A SEND-PATH DROP.
//
// m_connectionBudgetProbe replaces the DERIVED half of the budget model with a measured one. §8
//
// ⛔ Both fed from the GAME THREAD, server only, and neither has synchronization. §1
//
// Capacity is INJECTED: under flush-on-poll the ceiling is min(writesThisFrame, kMaxDepth). §6
    RelayWriteProbe       m_relayWriteProbe{
        RelayStageCapacity{ static_cast<uint32>(relayedInputRing::kMaxDepth) } };
    ConnectionBudgetProbe m_connectionBudgetProbe;

// Adapter-side delivery resolution: the core claim map is id-keyed, so its `deliver`
// callback hands back an id and THIS map resolves it to the owning component. §7
// ⛔ Erased in unregisterFromNewFramework - the contract replacing the core's GC read. §10
    std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>
        m_delayedInputComponentsById;

// The display's rings, one per polled character id. GAME THREAD, unsynchronized, like
// every other diagnostic member here. ⛔ Reaped in unregisterFromNewFramework. §1 §10
    inputHistoryVisualizationUImpl::InputHistoryStore m_inputHistory;


    FSimulationManagerAsyncCallback* m_asyncCallback;

    FDelegateHandle m_injectInputsExternalCallbackHandle;
    FDelegateHandle m_hysScenePostTickCallbackHandle;

    std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

    ASimulationTimingRelay* m_timingRelay = nullptr;
    ASimulationTimingRelay* findTimingRelay();

// Adapters require the Chaos solver - emplaced in BeginPlay.
    std::optional<ChaosPhysicsBodyAdapter>   m_physAdapter;
    std::optional<ChaosPhysicsBodyReaderAdapter> m_physReaderAdapter;
    std::optional<ChaosSpatialQueryAdapter>  m_queryAdapter;

// ⭐⭐ THE ONE-TIME CVAR READ - [movement-sim task 16, ruling #3]. DEFINED IN
// `MovementSchemeCVar.cpp`, NOT in SimulationManagerUImpl.cpp, and that is deliberate: it is
// the TU that registers the four variables, so the read is a direct load of the very objects
// the console writes rather than a `FindConsoleVariable` lookup by string that can miss a
// rename in silence. That TU also owns the "consumed" latch the four sinks test before warning
// that a mid-session change is being ignored, and the refused-name sweep (a stale ini naming a
// constant that no longer exists is reported LOUDLY there, per the obligation routed from
// task 56). Reads the engine's default gravity at the same instant.
    static FMovementStaticDataCVars readMovementStaticDataCVars();

// ---- SIMULATABLE-PACK ALIAS CHAIN -------------------------------------
//
// Single source of truth: widen this one alias and every type below inherits it.
// ⛔ CLASS-scoped so these names cannot leak to global scope from an adapter header. §2
    using BrawlerSimulatables    = SimulatableList<SimulatableBrawler>;
    using BrawlerStorage         = apply_t<SimulationObjectStorage,     BrawlerSimulatables>;
    using BrawlerNetSync         = apply_t<SimulationNetSync,           BrawlerSimulatables>;
// The resolution peer's own alias - a composition-root sibling, not a NetSync member.
    using BrawlerInputResolution = apply_t<SimulationInputResolution,   BrawlerSimulatables>;
    using BrawlerReconciliation  = apply_t<SimulationReconciliation,    BrawlerSimulatables>;

// Owned resources + peers, typed from the alias chain. ⛔ Order matters - see below. §2
    BrawlerStorage m_storage;

// ⭐ [movement-sim task 16] THE ONE-TIME CVAR READ, AND IT IS DECLARED HERE FOR A REASON:
// members construct in DECLARATION order, so this one must sit IMMEDIATELY ABOVE m_staticData,
// which reads it in its own initializer. Moving it below is not a style change - it is
// reading an uninitialized object, silently, exactly as the banner below warns. §2
    const FMovementStaticDataCVars m_movementStaticDataCVars = readMovementStaticDataCVars();

// Game static data - the ownership ROOT for StaticData across the whole tree.
//
// ⛔ NEVER copied or moved: nested sub-StaticData binds sibling references that a copy dangles. §2
// ⭐ Its FIVE movement parameters come from the one-time read above; every other constant it
// holds is still authored in SimulatableBrawlerTypes.h. This is the ONE call site in the tree
// that passes anything - every test peer default-constructs and therefore measures the
// authored literals.
    simulatableBrawler::StaticData m_staticData{
        m_movementStaticDataCVars.model,
        m_movementStaticDataCVars.maxWalkSpeed,
        m_movementStaticDataCVars.stepPeriodTicks,
        m_movementStaticDataCVars.stepSpeed,
        m_movementStaticDataCVars.gravity };

// ⛔ ENFORCED BY ONE THING ONLY: members construct in DECLARATION order, not list order.
//
// ⛔ NO `static_assert` and NO `-Wreorder`-as-error in any `.Build.cs`/`.Target.cs` here:
// a reorder compiles silently and constructs in the new, wrong order.
//
// ⛔ LOAD-BEARING the day a ctor BODY calls a sibling: UB, undiagnosed. Trivial today. §2
    BrawlerReconciliation  m_reconciliation{ m_storage };
// Constructed BEFORE m_netSync - netSync's ctor takes a reference to this peer. §2
    BrawlerInputResolution m_inputResolution{ m_storage, m_reconciliation };
    BrawlerNetSync         m_netSync{ m_storage, m_reconciliation, m_inputResolution };

// apply_t cannot fill the executor's three engine/game-specific slots; a bind wrapper does. §2
    template <typename... SimulatableTs>
    using BrawlerIntegrationExecFor_UE = SimulationIntegrationExecutor<
        simulatableBrawler::StaticData, ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter, SimulatableTs...>;
    using BrawlerIntegrationExec = apply_t<BrawlerIntegrationExecFor_UE, BrawlerSimulatables>;

// Fourth peer - systems executor over (pack marker, StaticData type, system pack).
    using BrawlerSystemsExec = SimulationSystemsExecutor<
        BrawlerSimulatables,
        simulatableBrawler::StaticData,
        brawlerHitRouting::System,
// ⛔ [ringout task 4] THE AWARD FIRES ON ALL THREE ROLES FROM HERE - authority, client
// prediction, and every resim replay tick - and it is GATED INSIDE the system by a flag
// BeginPlay supplies, because SimulationTimeStep cannot tell a role from a clock. The flag
// defaults to FALSE, so a wiring that forgets the call scores nothing rather than scoring
// twice. See brawlerRingout::ScoreSystem::setIsAuthority for why a setter and not the
// executor's piecewise_construct ctor.
        brawlerRingout::ScoreSystem>;

// Value-owned; default-constructs the routing and score systems. Passed by reference at emplace().
    BrawlerSystemsExec m_systemsExec;

// Integration layer and manager require adapters - emplaced in BeginPlay.
    using IntegrationLayerType = BrawlerIntegrationExec;
    std::optional<IntegrationLayerType> m_integrationLayer;

    using ManagerType = SimulationManager<
        BrawlerIntegrationExec, BrawlerNetSync, BrawlerInputResolution, BrawlerReconciliation,
        BrawlerSystemsExec, BrawlerStorage, simulatableBrawler::StaticData>;
    std::optional<ManagerType> m_manager;

    static ASimulationManagerUImpl* s_instances[2];

    ChaosTickMapper m_chaosTickMapper;

// ⭐ [movement-sim task 17] `BodyId parentBodyId` IS GONE. It cached the pawn root capsule's
// body id across the two-phase `tryRegister` for exactly two readers: the two-source tripwire
// (deleted with it) and the resolvability gate, which now reads the movement declaration's own
// `bindings.ownBodyId` — the same body, because that declaration's descriptor sets `isRoot`.
// The capsule id is still derived inside the first-call pass as a LOCAL, where the factory and
// `decl.bindings.parentBodyId` consume it; nothing needed it to survive the call.
    struct PendingRegistration
    {
        std::optional<SimulatableBrawler> simulatable;
        bool bodiesCreated = false;
        BrawlerInputProviderFn inputProvider;
        bool isAuthority = false;
    };
    std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
};