// SPDX-License-Identifier: MPL-2.0

#include "SimulationInputRelay.h"
#include "ISimulationInputRelayListener.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"

#if UE_WITH_IRIS
#include "Net/Iris/ReplicationSystem/ReplicationSystemUtil.h"
#include "Iris/ReplicationSystem/ReplicationSystemTypes.h"
#endif // UE_WITH_IRIS

// ⚠ WARNING, NOT Log, AS THE CATEGORY DEFAULT. Everything this actor has to say
// is a wiring fault (an unresolvable owner, an unroutable OnRep) or a one-shot
// composition fact, and `Config/DefaultEngine.ini` runs the netcode categories at
// Warning on the dedicated server — a `Log`-level line here would simply not
// exist on the one machine it has to be greppable on. That is not a prediction:
// the archived T22 server log contains zero `[RelayDelayFloor]` lines for exactly
// this reason, and item 36 exists solely because of it.
DEFINE_LOG_CATEGORY(LogOGInputRelay);

ASimulationInputRelay::ASimulationInputRelay()
{
    bReplicates = true;

    // bAlwaysRelevant, NOT bOnlyRelevantToOwner — the opposite of
    // ASimulationConnectionRelay's setting, and the difference is the whole point
    // of the channel. A connection tier is for its own wire; a character's relayed
    // input is for EVERY peer EXCEPT its own wire. The "except" half is the
    // COND_SkipOwner condition on the property, not a relevancy filter, because
    // relevancy would also stop the actor itself from replicating and there would
    // then be nothing on the owning client for a future owner-visible tenant to
    // ride. (It also keeps the actor's existence independent of the condition, so
    // a mis-set Owner degrades to a wasted echo rather than to a missing actor.)
    bAlwaysRelevant = true;

    // Mirrors the character's own 60/60 (AOGBrawlerUECharacter's ctor). The ring
    // is written at up to 60 Hz by the relay tap and must be polled at least as
    // often, since Iris's poll period is floor(TickRate / NetUpdateFrequency):
    // anything below 60 here would silently coalesce capture ticks in server
    // memory before Iris ever compared them — which is upstream write clumping,
    // the exact loss class T22 measured and this initiative is removing.
    //
    // ⛔ [T34] DO NOT LOWER EITHER OF THESE, AND DO NOT REMOVE `bAlwaysRelevant`
    // ABOVE. Since flush-on-poll landed, this pair is no longer a tuning choice —
    // it is a CORRECTNESS PRECONDITION of the relay, and it is the one standing
    // constraint in this initiative that NO TEST PINS.
    //
    // The flush publishes the whole staged burst in `PreReplication`, which the
    // replication driver calls once per send update IN WHICH THIS OBJECT IS ON THE
    // POLL LIST. At `bAlwaysRelevant` + 60/60 against a 60 Hz server the host is on
    // that list every frame, so exactly one frame's arrivals are staged between two
    // flushes and the stage never holds more than a frame's worth.
    //
    // Halve this to 30 and the pairing breaks silently: the stage accumulates
    // across two frames, and everything past `relayedInputRing::kMaxDepth` (8) is
    // evicted before it is ever published. Relay coverage degrades, no error is
    // raised anywhere, and the ONLY signal is `m_stageOverflowDropCount` on the
    // `[RelayFlush]` line — which is precisely why that line exists.
    SetNetUpdateFrequency(60.0f);
    SetMinNetUpdateFrequency(60.0f);

    // AInfo's constructor already clears this; stated explicitly because it is a
    // named requirement of the design and a silent re-enable would put a
    // FRepMovement on every character's ring host — pure waste on an actor that
    // has no transform anyone reads.
    SetReplicatingMovement(false);
}

void ASimulationInputRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // ⭐ COND_SkipOwner — the owner-echo removal (T38 §5.3). The owning client
    // provably never reads its own character's ring: registerPredictionOwner
    // builds a RemoteInputCache only for provider-ABSENT ids, and collectInputAll
    // structurally cannot reach a store for an id that has a provider. Before this
    // change the ring carried a plain DOREPLIFETIME with no COND_, so every client
    // also received an echo of its own inputs — ~85 B per connection per round of
    // payload nobody could consume.
    //
    // ⚠ THIS CONDITION IS ONLY REAL IF Owner IS SET. Iris evaluates it per
    // connection through FReplicationConditionals::GetLifetimeConditionals, which
    // keys off ReplicationFiltering->GetOwningConnection(...). An owner-less actor
    // resolves to "no owning connection", every connection then counts as a
    // non-owner, and the skip silently does nothing. spawnForCharacter sets the
    // owner and warns if it cannot; the consuming component additionally reports
    // the divergence from the receiving end.
    DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner);
}

// ⭐ [og-netcode-v2-input-relay T34] THE FLUSH — bare C1, R = 0.
//
// Everything the relay tap staged since the last poll is published into the
// replicated ring, and the stage is emptied. The ring at poll N therefore carries
// `arrivals(N)` rather than "the newest arrival of N", which is the whole of what
// this task buys: ~89.3 % of relayed inputs survived the replace-latest write path,
// ~98.9 % survive this one.
//
// THE THREE PROPERTIES THAT MAKE IT CORRECT, none of them local to this function:
//   1. CAPACITY IS `kMaxDepth`, taken as a constant inside
//      `relayedInputRing::stageArrival` at the staging site. No depth is passed
//      anywhere on this path, and the session-configurable field that used to
//      size the retired replace-latest write path (its old identifier is on
//      record in RN-13, ReviewNotes.md; item 63 deleted it outright) is not
//      read — at its former session value of 1, every staged entry after the
//      first would have superseded its predecessor and bare C1 would have
//      silently degenerated back into replace-latest (T43 finding 1).
//   2. SUPPRESS-CLEAR-ON-EMPTY. `flushStagedInto` returns 0 and touches nothing on
//      an empty stage, so the ring stays byte-identical => not dirty => not
//      scheduled => zero bytes, AND a round Iris skipped under packet pressure
//      survives the quiet frames that follow and is retried. Under R = 0 that retry
//      is the only recovery mechanism there is (T43 finding 3).
//   3. ORDERING. This runs inside `PreSendUpdate`'s pre-update phase, strictly
//      before `PollAndCopy` — see the header block.
//
// NOTHING PER-ROUND IS LOGGED HERE, DELIBERATELY. This is a per-frame,
// per-character hook; a line per round at any verbosity would be a per-tick line on
// every character at once, which is how this initiative produced a 6.4 MB server
// log in 94 seconds once already. What IS emitted is one WINDOWED summary per
// `kFlushReportRounds` polls — the same shape and roughly the same volume as the
// `[RelayProbe.*]` windows.
//
// ⭐ [T34 rework] WHY THAT SUMMARY IS MANDATORY RATHER THAN NICE-TO-HAVE. These
// four counters are THE ONLY DIRECT OBSERVATION OF THE FLUSH THAT EXISTS. The
// server's `observableX1000` is computed from the write path's run lengths without
// reference to the flush at all, so it reads ~1000 even if `PreReplication` never
// fires — meaning item 34's acceptance gate can PASS on a dead mechanism. Until
// this line existed the counters were incremented, exposed through getters, and
// read by nothing anywhere in `Source/`: an instrument that exists but is never
// printed. That is the third variant of one defect class in this initiative
// (item 36: a proof line that does not exist; T34 review §5.1: a proof line that
// exists but cannot move), and it is why the line is `Warning` — `LogOGInputRelay`
// defaults to Warning and the dedicated server runs the netcode categories there,
// so a `Log` line would simply not exist on the one machine it must be greppable on.
void ASimulationInputRelay::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
    Super::PreReplication(ChangedPropertyTracker);

    const uint8 published =
        relayedInputRing::flushStagedInto(m_relayedInputRing, m_relayedInputStagingRing);

    if (published > 0)
    {
        ++m_flushedRoundCount;
        m_flushedEntryCount += published;
    }
    else
    {
        ++m_suppressedFlushCount;
    }

    // The window is counted in POLLS, not in published rounds: a host that is
    // polled but always suppressed is exactly the state an operator most needs to
    // see, and a rounds-driven window would go silent on it.
    if (++m_roundsSinceFlushReport >= kFlushReportRounds)
    {
        m_roundsSinceFlushReport = 0;
        emitFlushReport(TEXT("window"));
    }
}

// SESSION TOTALS, CUMULATIVE — diff two consecutive lines for a window's delta.
// Cumulative rather than per-window because three of the four counters are session
// FACTS an operator wants the running total of (did the flush ever run; has the
// stage ever overflowed), and a per-window reset would make a single overflow
// visible in exactly one line out of ~45.
void ASimulationInputRelay::emitFlushReport(const TCHAR* reason) const
{
    // entries per FLUSHED round, x100 — the coalescing factor the flush exists to
    // raise. 100 means every round carried exactly one entry (replace-latest would
    // have sufficed); the measured steady state is ~113, and a join burst pushes it
    // higher. 0 flushed rounds prints 0 rather than dividing by it.
    const uint32 entriesPerRoundX100 = (m_flushedRoundCount == 0)
        ? 0u
        : static_cast<uint32>((static_cast<uint64>(m_flushedEntryCount) * 100u) / m_flushedRoundCount);

    UE_LOG(LogOGInputRelay, Warning,
        TEXT("[RelayFlush] %s owner=%s flushedRounds=%u entries=%u entriesPerRoundX100=%u "
             "suppressed=%u stageOverflowDrops=%u"),
        reason,
        *GetNameSafe(GetOwner()),
        m_flushedRoundCount,
        m_flushedEntryCount,
        entriesPerRoundX100,
        m_suppressedFlushCount,
        m_stageOverflowDropCount);
}

// THE FINAL REPORT, and it is not redundant with the windowed one. It guarantees
// AT LEAST ONE `[RelayFlush]` line per relayed character per session — including
// for a session shorter than one window, and including the tail after the last
// window closed. "The flush never ran" and "the run was too short to report" are
// different findings and this is what separates them.
//
// AUTHORITY ONLY. `PreReplication` is called by the replication driver on the
// sending side alone, so a client's counters are all zero and a line there would
// read as a dead flush on every single client.
void ASimulationInputRelay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        emitFlushReport(TEXT("FINAL"));
    }

    Super::EndPlay(EndPlayReason);
}

void ASimulationInputRelay::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // The authority links the host directly at spawnForCharacter; there is
        // nothing for a listener to resolve, and no OnRep will ever fire here.
        return;
    }

    // CLIENT. The initial bunch's RepNotifies run before BeginPlay, so a ring may
    // already have landed and been dropped by the time we get here — asking for
    // the link now is what turns that drop into a one-frame delay instead of a
    // permanently dead channel. If the Owner pointer has not resolved yet this is
    // a no-op and OnRep_Owner will retry.
    requestListenerLink();
}

void ASimulationInputRelay::OnRep_Owner()
{
    Super::OnRep_Owner();

    // The owner reference is what the whole client-side resolution hangs off, and
    // it is itself replicated — so it can land after this actor does. This is the
    // second of the three independent link attempts (BeginPlay, here, and any
    // unrouted ring OnRep); none of them alone covers every arrival order, and all
    // three are idempotent.
    requestListenerLink();
}

void ASimulationInputRelay::requestListenerLink()
{
    if (HasAuthority())
        return;

    if (ISimulationInputRelayListener* listener =
            ISimulationInputRelayListener::instanceFor(/*isAuthority=*/false))
    {
        listener->onInputRelayHostReady(*this);
    }
    // No listener yet => nothing to do. The manager registers its listener in
    // BeginPlay, i.e. at world start, long before any character replicates, so
    // reaching this is a startup-order anomaly rather than a routine state — and
    // the ring OnRep path retries anyway, so it self-heals if it ever happens.
}

void ASimulationInputRelay::OnRep_RelayedInputRing()
{
    if (m_onRelayedInputReceivedCallback)
    {
        m_onRelayedInputReceivedCallback(m_relayedInputRing);
        return;
    }

    // UNROUTED. The ring arrived before this host found its consumer — most
    // likely the Owner pointer has not replicated yet. DROP IT, exactly as the
    // pre-T39 null-callback OnRep did, and count it.
    //
    // Dropping is correct rather than merely tolerable, and the reason is the
    // property's own nature: the ring is PERSISTENT, so the next replication
    // carries the full current state again. There is nothing to latch, and a
    // latch would only add a second copy of the ring to keep in sync (the same
    // ruling registerPredictionOwner records for its bind-time read).
    ++m_unroutedOnRepCount;

    // ...but try to resolve NOW before giving up on this arrival: the owner may
    // have landed since the last attempt. On success the linking side replays the
    // CURRENT ring — which is this arrival's payload — into the freshly bound
    // callback, so there is deliberately nothing to forward again here.
    requestListenerLink();

    if (!m_onRelayedInputReceivedCallback && !m_loggedUnroutedOnRep)
    {
        // ONE-SHOT. An unresolvable owner re-replicates its ring forever, so an
        // ungated line here would be ~53 lines/second/character of the same fact —
        // the volume class T19 measured as 98.4 % of a 10 MB server log. The
        // counter carries the magnitude; this line carries the fact.
        m_loggedUnroutedOnRep = true;
        UE_LOG(LogOGInputRelay, Warning,
            TEXT("[InputRelay] ring OnRep with no consumer bound (owner=%s) — dropped; "
                 "healed by the next replication if the owner resolves"),
            *GetNameSafe(GetOwner()));
    }
}

ASimulationInputRelay* ASimulationInputRelay::spawnForCharacter(UWorld& world, AActor& character)
{
    checkf(character.HasAuthority(),
        TEXT("ASimulationInputRelay::spawnForCharacter is authority-only (character=%s)"),
        *character.GetName());

    // Owner = the character itself. That is what COND_SkipOwner resolves against
    // (via the owning connection of the character's owner chain), and it is also
    // the identity findForOwner matches on.
    ASimulationInputRelay* host = world.SpawnActor<ASimulationInputRelay>(
        ASimulationInputRelay::StaticClass(), FTransform::Identity);

    if (host == nullptr)
    {
        UE_LOG(LogOGInputRelay, Warning,
            TEXT("[InputRelay] host spawn FAILED for character %s — relayed input will not "
                 "replicate for it"),
            *character.GetName());
        return nullptr;
    }

    host->SetOwner(&character);

#if UE_WITH_IRIS
    // ⭐ THE DEPENDENT-OBJECT ATTACHMENT AND THE TWO PRIORITIES — the entire
    // engine-facing surface of this feature, deliberately isolated in one place so
    // the whole thing reverts as a unit.
    //
    // ORDER IS LOAD-BEARING. AddDependentActor starts replicating the child if it
    // is not already registered (UEngineReplicationBridge::StartReplicatingActor
    // inside FReplicationSystemUtil::AddDependentActor), and SetStaticPriority
    // silently no-ops on an unregistered handle — so the attach must come first or
    // the ring would run at the default priority with no error anywhere.
    //
    // Both calls no-op cleanly when there is no net driver or Iris is not in use
    // (standalone PIE), which is why there is no netmode branch here.
    UE::Net::FReplicationSystemUtil::AddDependentActor(
        &character, host, UE::Net::EDependentObjectSchedulingHint::Default);

    UE::Net::FReplicationSystemUtil::SetStaticPriority(host, kRingStaticPriority);

    // The character carries the correction state, and this is the other half of
    // the ranking: an EXPLICIT 1.0 rather than the APawn NetPriority default of
    // 3.0 that BeginReplication installs for every bAlwaysRelevant actor. Setting
    // it does two things — it puts the state below the ring, and it stops all the
    // characters from tying with each other, which is why T37's overflow victim
    // was a coin flip per frame.
    UE::Net::FReplicationSystemUtil::SetStaticPriority(&character, kCharacterStaticPriority);
#endif // UE_WITH_IRIS

    // ⚠ THE OWNER-SKIP PRECONDITION, checked from the producing end. COND_SkipOwner
    // is only meaningful if this actor resolves to an owning CONNECTION; a
    // player-controlled character that does not is a silent regression to the
    // pre-T39 echo, costing ~85 B per connection per round with no error anywhere.
    // Warning-level and unconditional-on-the-fault so it survives
    // LogOGInputRelay=Warning on the dedicated server.
    const APawn* pawn = Cast<APawn>(&character);
    const bool isPlayerControlled = (pawn != nullptr) && pawn->IsPlayerControlled();
    if (isPlayerControlled && host->GetNetConnection() == nullptr)
    {
        UE_LOG(LogOGInputRelay, Warning,
            TEXT("[InputRelay] host for player-controlled character %s resolves NO owning "
                 "connection — COND_SkipOwner is inert and the owner echo is back"),
            *character.GetName());
    }

    return host;
}

ASimulationInputRelay* ASimulationInputRelay::findForOwner(const UWorld* world, const AActor* owner)
{
    if (world == nullptr || owner == nullptr)
        return nullptr;

    // Linear scan rather than a cached map, for the same reason
    // ASimulationConnectionRelay::findOrSpawnForConnection uses one: this runs
    // once per character per session (from the consuming component's registration
    // path), and a pointer-keyed cache would have to be invalidated on every
    // character death and level travel to avoid handing out a dead host.
    for (TActorIterator<ASimulationInputRelay> it(const_cast<UWorld*>(world)); it; ++it)
    {
        ASimulationInputRelay* host = *it;
        if (IsValid(host) && host->GetOwner() == owner)
            return host;
    }

    return nullptr;
}

void ASimulationInputRelay::detachFromParentAndDestroy()
{
    // The consumer is going away; make sure nothing can be pushed at it in the
    // window between here and actual destruction.
    clearOnRelayedInputReceivedCallback();

#if UE_WITH_IRIS
    // Detach BEFORE destroying so the parent's dependent list is never left
    // holding a handle to a dead actor.
    if (AActor* parent = GetOwner())
    {
        UE::Net::FReplicationSystemUtil::RemoveDependentActor(parent, this);
    }
#endif // UE_WITH_IRIS

    Destroy();
}
