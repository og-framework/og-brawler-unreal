#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "GameFramework/Info.h"

#include "RelayedInputRing.h"

#include <functional>

#include "SimulationInputRelay.generated.h"

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGInputRelay, Warning, All);

// ---------------------------------------------------------------------------
// ⭐ ASimulationInputRelay — the PER-CHARACTER host of the relayed-input ring.
// (og-netcode-v2-input-relay / T39, Stage 1 of input-first replication;
//  design_task38_input_first_replication.md §4.1, §5.1-5.3, §6 Candidate A.)
//
// Third sibling of ASimulationTimingRelay (session scope) and
// ASimulationConnectionRelay (wire scope). This one is CHARACTER scope:
//
//   Quantity                Scope       Audience            Vehicle
//   ----------------------  ----------  ------------------  -----------------------
//   Authority tick          session     everyone            ASimulationTimingRelay
//   Connection tier         ONE wire    that wire only      ASimulationConnectionRelay
//   Relayed input ring      ONE char    everyone BUT owner  THIS actor
//
// -------------------------------------------------------------------------
// WHY THE RING LEFT USimmableUpdateComponent — the T37 defect, in one paragraph
// -------------------------------------------------------------------------
// The ring and the correction state were two UPROPERTYs on the same actor's
// component, which means Iris wrote them inside ONE atomic batch: the whole
// root-plus-subobjects write sits in a single `FNetBitStreamRollbackScope`
// (`FReplicationWriter::WriteObjectBatch`) and "it is a fail if we fail to write
// any subobject with dirty state". So when the round overflowed the packet, the
// two payloads died TOGETHER — and a skipped batch is not deferred, it is
// COALESCED AWAY: "skipped objects won't be reserialized again", and next frame
// the object is re-quantized from its current value. T37 measured exactly that:
// depth 2 at two characters pushed the round past the ~952 B single-bunch
// capacity and every snapshot channel collapsed to ⅔ cadence in lockstep.
//
// That is intolerable for the RING specifically, because of an asymmetry the
// design verified from simulation source both ways:
//   * a missed CORRECTION STATE is SELF-HEALING — each snapshot is a complete
//     anchor with no delta chain, the client always reconciles against the
//     newest landed one, and holes are a named, designed-for case;
//   * a missed RELAYED INPUT is GONE — each capture tick is relayed exactly once
//     at receipt, there is no ack, no re-relay and no request-resend anywhere.
//
// ⛔ A SECOND COMPONENT ON THE SAME ACTOR WOULD HAVE BOUGHT NOTHING, and this is
// the tempting wrong fix, so it is recorded here: `FReplicationWriter::
// ScheduleObjects` builds its candidate list with subobjects MASKED OUT, so a
// subobject never gets its own sort key — it is reached only by recursion from
// its root and shares that root's fate completely. The decoupling mechanism is a
// DEPENDENT OBJECT (`UObjectReplicationBridge::AddDependentObject`), which is a
// root object in its own right: scheduled, sorted, written and skipped
// independently. Bonus, and it is the property that makes this design work:
// after a parent's batch succeeds, `HandleObjectBatchSuccess` pushes its dirty
// dependents onto `DependentObjectsPendingSend`, drained BEFORE the next
// scheduled object — the dependent gets first refusal on the remaining space.
//
// -------------------------------------------------------------------------
// THE THREE THINGS THIS ACTOR DOES, all of them load-bearing
// -------------------------------------------------------------------------
// 1. SPLIT — it is its own Iris root object, so its fit/skip fate is its own.
//    Cost is ~6-7 B of batch framing per packet, not the ~98 B the split was
//    feared to cost: that figure was dominated by fixed per-packet reservation
//    which no design change reclaims.
//
// 2. RANK — static priority 4.0 against the character's 1.0. Iris's scheduling
//    priority is an ACCUMULATOR whose per-frame increment IS the static value
//    (`FReplicationWriter::UpdatePriorities`), reset to 0 only on a SUCCESSFUL
//    send (`HandleObjectBatchSuccess`). So rings sort ahead of states every
//    frame, and when the packet fills it is the STATE that gets skipped — while
//    a state skipped repeatedly accumulates past 4.0 within 4 frames (~67 ms)
//    and jumps the queue once. That is a designed starvation BOUND, not a bug.
//    Setting the character explicitly also breaks the tie that made T37's
//    overflow victim a coin flip (`UEngineReplicationBridge::BeginReplication`
//    gives every bAlwaysRelevant actor `StaticPriority = NetPriority`, i.e. the
//    APawn default 3.0 for all of them, and `std::partial_sort` is not stable).
//
// 3. SKIP THE OWNER — `COND_SkipOwner` on the ring property. The owning client
//    provably never reads its own ring: `SimulationNetSync::registerPrediction
//    Owner` creates a `RelayedInputStore` only on the provider-ABSENT branch,
//    and `collectInputAll` structurally cannot reach a store for an id that has
//    a provider. That echo was ~85 B per connection per round of pure waste.
//    ⚠ `SetOwner(character)` IS REQUIRED FOR THIS, NOT COSMETIC: Iris resolves
//    the condition per connection via `FReplicationConditionals::
//    GetLifetimeConditionals` -> `ReplicationFiltering->GetOwningConnection(...)`,
//    so an unset Owner silently disables the skip and the echo comes back with
//    no error anywhere. The authority warns if it spawns a host for a
//    player-controlled character it cannot resolve an owning connection for.
//
// -------------------------------------------------------------------------
// LIFECYCLE — spawned at CHARACTER REGISTRATION, destroyed with the character
// -------------------------------------------------------------------------
// Deliberately NOT the spawn-on-demand shape ASimulationConnectionRelay uses.
// That actor spawns from its WRITE path because its value is produced long
// before (and independently of) any registration, and because PostLogin is not
// called for players carried through seamless travel. This one has a natural,
// always-reached creation point that is strictly EARLIER than its first write:
// the authority's `tryRegisterWithNewFramework` completion, which is also where
// the relay tap's id->component route is registered. Spawning there guarantees
// the host exists before the first `relayRemoteInput` can reach the ring, so no
// write can ever land in the component's detached fallback.
//
// Destruction is explicit at the component's EndPlay (via
// detachFromParentAndDestroy), NOT self-reaping on a timer: unlike the
// connection relay, this actor's owner is the character, and a character's
// EndPlay is a reliable, immediate signal. `RemoveDependentActor` runs first so
// the parent's dependent list never holds a stale handle.
//
// ⚠ ONE CREATION-FRAME COUPLING, recorded so it is not mistaken for a bug:
// while the host is still in its initial state, `FReplicationWriter::
// CanSendObject` gates parent/dependent ordering. That applies only until the
// dependent reaches `Created` — it is a creation-frame effect, not a standing
// constraint on the priority scheme.
//
// WIRE FORMAT IS UNTOUCHED. `FRelayedInputRing`, its codec, its
// `kWireFormatVersion` and its T29 Iris NetSerializer registration are all
// registered BY STRUCT TYPE and are completely indifferent to which UObject
// carries the property. Nothing about this move is a wire change; it is a
// scheduling change.
// ---------------------------------------------------------------------------
UCLASS()
class OGSIMULATIONUNREAL_API ASimulationInputRelay : public AInfo
{
    GENERATED_BODY()

public:
    ASimulationInputRelay();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void OnRep_Owner() override;

    // ⭐ [T34] THE FLUSH HOOK — bare C1 (R = 0). Publishes everything the relay tap
    // staged since the last poll into the replicated ring, then empties the stage.
    //
    // WHY THIS HOOK, VERIFIED FROM ENGINE SOURCE RATHER THAN ASSUMED (T43
    // finding 4): `UEngineReplicationBridge::BeginReplication` registers every actor
    // root with `.bNeedsPreUpdate = true` unconditionally; the bridge installs
    // `ActorReplicationBridgePreUpdateFunction`, which calls
    // `Actor->CallPreReplication(NetDriver)`; `AActor::CallPreReplication` calls
    // this, gated on `ShouldCallPreReplication()` (default true) and
    // `GetLocalRole() == ROLE_Authority` (true for the server-spawned host). And
    // `UObjectReplicationBridge::PreSendUpdate` runs every pre-update BEFORE
    // `PollAndCopy`/`QuantizeDirtyStateData`, so a write made here is seen by the
    // SAME pass — this initiative has already shipped one write that landed after
    // the dirty scan and was a silent no-op (T29), which is why the ordering is
    // stated with its citation instead of being trusted.
    //
    // ⚠ ONCE PER SEND UPDATE IN WHICH THE OBJECT IS POLLED, not unconditionally.
    // With `bAlwaysRelevant` and NetUpdateFrequency 60 against a 60 Hz server the
    // host is on the poll list every frame, so flush and poll are always paired.
    // LOWERING NetUpdateFrequency WOULD BREAK THAT PAIRING — the stage would
    // accumulate between polls and everything past `kMaxDepth` would be dropped.
    // That coupling between poll cadence and relay coverage is new with flush-on-
    // poll and is the reason the constructor pins 60/60.
    virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

    // Emits the FINAL `[RelayFlush]` line on the authority; see the .cpp for why a
    // teardown line is not redundant with the windowed one.
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // SERVER: create this character's ring host, own it by the character, attach
    // it as an Iris dependent object and install both static priorities. Returns
    // nullptr if the world cannot spawn it; the caller treats that as "no host",
    // which degrades to the component's detached fallback ring (i.e. the relay
    // stops delivering for that character) rather than crashing.
    //
    // `character` is deliberately an AActor rather than the concrete brawler
    // character type: this module must not depend on OGBrawlerUnreal. The only
    // things this actor needs from it are "something to be owned by" and "the
    // Iris parent to depend on", both of which are AActor-level.
    static ASimulationInputRelay* spawnForCharacter(UWorld& world, AActor& character);

    // Either side: the host owned by `owner`, if one is resident in this world.
    // Used by the client-side link path as a pull complement to the listener's
    // push, so the two objects find each other regardless of which replicated in
    // first. Linear scan, like ASimulationConnectionRelay's find-or-spawn: it
    // runs once per character per session, not per frame.
    static ASimulationInputRelay* findForOwner(const UWorld* world, const AActor* owner);

    // SERVER: detach from the Iris parent and destroy. Order matters — the
    // dependent list must not be left holding a handle to a destroyed actor.
    void detachFromParentAndDestroy();

    // READ handle for the replicated ring. On a client this is what the consumer
    // ingests; on the authority it is the published round.
    //
    // ⚠ [T34] NO LONGER THE SERVER WRITE HANDLE. The relay tap writes the STAGE
    // below; this ring is written only by the flush in `PreReplication`. A write
    // landing here directly would be published-and-then-overwritten by the next
    // non-empty flush, or — worse — would survive as a phantom entry through every
    // empty frame, because the flush deliberately does not touch this ring when the
    // stage is empty.
    FRelayedInputRing&       editRelayedInputRing()       { return m_relayedInputRing; }
    const FRelayedInputRing& getRelayedInputRing() const  { return m_relayedInputRing; }

    // ⭐ [T34] SERVER write handle — THE FLUSH STAGE.
    // `ASimulationManagerUImpl::relayRemoteInput` reaches this through the
    // component's forwarder and calls `stageArrival` on it, once per accepted
    // capture tick, on the game thread from the RPC receipt path. Every arrival of
    // a frame lands here; `PreReplication` publishes the whole burst.
    //
    // IT IS A SECOND RING OF THE SAME WIRE FORMAT, and that is the design rather
    // than a coincidence: the ring already IS a fixed-capacity, evict-oldest,
    // rewrite-in-place buffer with exactly the semantics a stage needs
    // (design_task30_c1_flush_on_poll.md §2.1 names this explicitly). Reusing it
    // means the flush is a byte copy with no InputType anywhere, which is what lets
    // this actor — which must not depend on the game module — own the whole
    // mechanism instead of calling back into it.
    FRelayedInputRing& editRelayedInputStagingRing() { return m_relayedInputStagingRing; }

    // --- flush telemetry -------------------------------------------------------
    // ⭐ [T34 rework] READ BY `emitFlushReport`, which puts all four on a
    // Warning-level `[RelayFlush]` line once per kFlushReportRounds polls and once
    // at teardown. They are the ONLY direct observation of the flush there is —
    // `observableX1000` reads ~1000 even on a dead `PreReplication` — so they must
    // never go back to being write-only. The getters remain for tests and the
    // debugger.
    uint32 getFlushedRoundCount()      const { return m_flushedRoundCount; }
    uint32 getFlushedEntryCount()      const { return m_flushedEntryCount; }
    uint32 getSuppressedFlushCount()   const { return m_suppressedFlushCount; }
    uint32 getStageOverflowDropCount() const { return m_stageOverflowDropCount; }

    // Counted by the relay tap when a staged burst exceeded `kMaxDepth` in ONE
    // frame and the oldest staged entry was superseded to make room. This is the
    // only input loss the SERVER side of R = 0 can observe at all (there is no
    // send-success signal — T38 §4.3), so it must never be silent; the client-side
    // `lostCaptureTicksX1000` counts the rest.
    void noteStageOverflowDrop() { ++m_stageOverflowDropCount; }

    // CLIENT: the arrival hook. Bound by the consuming component once the two
    // objects are linked; until then a ring OnRep is DROPPED and counted (see
    // getUnroutedOnRepCount). Dropping is safe because the ring is a persistent
    // property — the next replication carries the full current state again.
    void setOnRelayedInputReceivedCallback(std::function<void(const FRelayedInputRing&)> fn)
    {
        m_onRelayedInputReceivedCallback = std::move(fn);
    }

    void clearOnRelayedInputReceivedCallback()
    {
        m_onRelayedInputReceivedCallback = nullptr;
    }

    bool hasRelayedInputCallback() const { return static_cast<bool>(m_onRelayedInputReceivedCallback); }

    // Ring OnReps that landed with nobody to route them to. Expected to be a
    // small number during the join window and then constant forever; a value that
    // keeps climbing means the owner never resolved, i.e. the link is broken and
    // this character's relayed input is not reaching the simulation.
    uint32 getUnroutedOnRepCount() const { return m_unroutedOnRepCount; }

    // The static priorities, named rather than inlined at the call site so the
    // ratio is one greppable fact. Hardcoded — NO ini, NO cvar (item 35's
    // discipline: measurement can motivate a knob later, a knob cannot motivate
    // itself). 4.0 against 1.0 means a state skipped under packet pressure
    // accumulates past a ring within 4 frames (~67 ms at 60 Hz) and jumps the
    // queue once, which is the designed starvation bound described in the header
    // block above. Raising the ratio lengthens that bound; lowering it toward 1.0
    // reintroduces the tie T37's overflow victim was decided by.
    static constexpr float kRingStaticPriority      = 4.0f;
    static constexpr float kCharacterStaticPriority = 1.0f;

private:
    UFUNCTION()
    void OnRep_RelayedInputRing();

    // Ask the client-side listener to resolve this host to its consumer. Safe and
    // idempotent to call repeatedly; no-op on the authority (which links the host
    // directly at spawn) and no-op if no listener has registered yet.
    void requestListenerLink();

    // [T34 rework] One `[RelayFlush]` line carrying all four flush counters.
    // `reason` distinguishes the periodic line from the teardown one.
    void emitFlushReport(const TCHAR* reason) const;

    // Polls per `[RelayFlush]` window. 120 at the pinned 60 Hz poll cadence is ~2 s
    // per character — deliberately the same feel and the same volume class as
    // `kRelayArrivalProbeWindowSamples` and `kRelayWriteProbeWindowRuns`, so a PIE
    // log's flush lines interleave with the probe windows at a comparable rate
    // instead of drowning them.
    static constexpr uint32 kFlushReportRounds = 120;

    // THE PAYLOAD. Same struct, same codec, same wire format, same T29 Iris
    // serializer registration as when it lived on USimmableUpdateComponent — only
    // the carrier changed. Registered COND_SkipOwner; see point 3 of the header
    // block for why that is both safe and load-bearing.
    UPROPERTY(ReplicatedUsing = OnRep_RelayedInputRing)
    FRelayedInputRing m_relayedInputRing;

    // ⭐ [T34] THE FLUSH STAGE. Server-only in practice (a client never writes it),
    // Transient and NOT registered in GetLifetimeReplicatedProps, so it never rides
    // the wire — it is scratch space between two points in one server frame.
    UPROPERTY(Transient)
    FRelayedInputRing m_relayedInputStagingRing;

    std::function<void(const FRelayedInputRing&)> m_onRelayedInputReceivedCallback;

    uint32 m_unroutedOnRepCount = 0;
    bool   m_loggedUnroutedOnRep = false;

    // [T34] Flush telemetry. `m_suppressedFlushCount` counting UP is the healthy
    // idle signal, not a fault: an empty stage must leave the ring byte-identical
    // so a scheduler-skipped round stays dirty and gets retried.
    uint32 m_flushedRoundCount      = 0;
    uint32 m_flushedEntryCount      = 0;
    uint32 m_suppressedFlushCount   = 0;
    uint32 m_stageOverflowDropCount = 0;

    // Polls since the last `[RelayFlush]` window line. Counts POLLS, not published
    // rounds — a host that is polled but always suppressed is the state most worth
    // seeing, and a rounds-driven window would go silent on exactly that.
    uint32 m_roundsSinceFlushReport = 0;
};
