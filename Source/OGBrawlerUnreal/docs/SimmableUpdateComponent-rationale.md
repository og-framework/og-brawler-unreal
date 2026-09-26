<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `USimmableUpdateComponent` — rationale

Companion to `Source/OGBrawlerUnreal/SimmableUpdateComponent.h` and `SimmableUpdateComponent.cpp`.
Since the task-25 conversion both files carry only code and one-line tags. **The prohibitions are in
`SimmableUpdateComponent-guards.md`, one id per site; this file carries the reasoning, the provenance
and the worked derivations.** The old `§N` marks are gone from the source; §12 records where every
pre-conversion comment went.

Sibling document: `Source/OGBrawlerUnreal/docs/SimulationManagerUImpl-rationale.md`, which owns the
manager side of every boundary named below. Where a fact belongs to the manager it is cited there
rather than restated here.

<!-- ================= DECLARED LINT ESCAPES =================================
     Every token below is CORRECT and cannot resolve. None is here to silence a
     name that is wrong: the wrong names this pass found were FIXED, not escaped
     (§11). Three classes — engine symbols outside every scan root, names that
     are RETIRED and must NOT resolve, and one private-workspace document.
     ========================================================================= -->
<!-- lint-external-ref: PostLogin -- Unreal Engine method, outside every scan root; the engine is not vendored into this repository -->
<!-- lint-external-ref: LiveLink -- ⛔ MUST NOT RESOLVE. F-34-5: the baseline claimed this component ticks behind a LiveLink component and takes a tick prerequisite on one. No such component exists in this project; the day this name resolves, that claim has become true and Sec 2 must be re-checked -->
<!-- lint-external-ref: LiveLinkComponent -- ⛔ MUST NOT RESOLVE. F-34-8: the argument of the commented-out prerequisite call, deleted with it -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (Sec 9): the retired replicated input property whose lifetime registration this file fences. It must NOT resolve -->
<!-- lint-external-ref: getLatestInput -- RETIRED with the correction-input column; zero non-comment occurrences in the tree. It must NOT resolve -->
<!-- lint-external-ref: SimulationNetSync::collectInputAll -- DEAD OWNER, quoted in Sec 11 as the defect F-34-3 corrected: collectInputAll moved to SimulationInputResolution. The qualified form must NOT resolve -->
<!-- lint-external-ref: tryRegisterWithManager -- ⛔ FALSE SYMBOL, quoted verbatim in Sec 11 (F-34-9): no such function exists anywhere in the tree. The real ones are tryInitializeWithManager and tryRegisterWithNewFramework -->
<!-- lint-external-ref: m_simulationStateSyncedBuffer -- ⛔ MUST NOT RESOLVE. F-34-8: a member named only by the deleted dead scratch code; it never existed -->
<!-- lint-external-ref: EpicGamesAssignment -- ⛔ MUST NOT RESOLVE. F-34-8: a namespace named only by the deleted dead scratch code; it exists nowhere -->
<!-- lint-external-ref: StructeredLog -- ⛔ MUST NOT RESOLVE. F-34-8: a misspelled engine header named by two deleted commented-out includes; no file of that name exists -->
<!-- lint-external-ref: getSyncedCorrectionInputBuffer -- RETIRED (Sec 12): the authority accessor for the retired correction-input buffer. It must NOT resolve -->
<!-- lint-external-ref: getMachineVizState -- RETIRED (Sec 12): deleted with the pawn flinch-freeze predicate that was its only caller. It must NOT resolve -->
<!-- lint-external-ref: m_onCorrectionInputReceivedCallback -- RETIRED (Sec 12): removed with the correction-input channel. It must NOT resolve -->

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. It is deliberately not in
> the `og-simulation` docs tier, which is MPL-2.0 and travels with a different repository.

**Read §0 first.** It states the shape — one instance per character, the role test, the four
channels, the phase order, the concept surface. Everything below assumes it.

---

## §0 Orientation — `USimmableUpdateComponent`

*Moved from the orientation banner at the top of the `.cpp` (task 25), with the phase order brought up to
date.*

One instance per simulated character, on the character actor. This is the **adapter**: it registers
the character with the simulation, owns that character's wire surfaces, and is the RPC boundary for
client→server input. It holds no netcode policy of its own — all of it is core, reached through the
manager.

**Binding declaration — this is UE adapter code.** Every engine or project type these files name —
`USimmableUpdateComponent`, `ASimulationManagerUImpl`, `GEngine`, `ASimulationInputRelay`,
`ASimulationConnectionRelay`, `AOGBrawlerUECharacter`, and equally the replication vocabulary
(`UChildConnection`, `FNetPing`, `COND_SkipOwner`, `COND_OwnerOnly`, `NetSerialize`, Iris, `UObject`,
`FSimulationStateSyncBuffer`, `FSimulationInputSyncBuffer`) — is one adapter's binding for the role it
names, and another adapter substitutes its own. The engine-free core never sees any of them.

**Role.** One instance runs on each side and they are not the same object. The role test is
`GetNetMode() != NM_Client`, world-level, the same predicate the manager picks its own role with
(guard G-02).

**Phase order.** Every step polls, because every step can legitimately not be ready yet, and "not
yet" is never an error:

```
BeginPlay
  -> tryInitializeWithManager      until the manager exists; then the query volumes and the
                                   visualization state
  -> tryRegisterWithNewFramework   until the manager exists; then (authority) allocate the pawn's
                                   SimCharacterId, or (client) wait for it to replicate; then until
                                   the bodies resolve; then register, latch the provider decision,
                                   spawn (or find) the relay host, and -- authority only -- register
                                   the id -> component route
TickComponent                      visualization only, both roles
EndPlay
  -> unregister, then destroy (authority) or unbind (client) the relay host
```

**The concept surface**, and it is why this file contains forwarders: the core reaches this object
through an accessor pair and a callback pair, and nothing else. The relayed-input ring itself lives on
a separate per-character actor, and the core never learns that (§6).

**Provider presence is the identity test.** A locally-controlled character gets an input provider;
every other character gets none, and provider-*absence* is what gives it a relay store for the core
to predict from (§4).

---

## §1 What this component is, and the four channels

One `USimmableUpdateComponent` per simulated character, living on the character actor. It is the
**adapter**: the object that introduces a character to the simulation, holds that character's wire
surfaces, and turns engine primitives into the arguments the engine-free core expects. It decides
nothing about netcode.

Both roles run the same class, and on a listen server both run in the same process against two
different manager instances. The role test is `GetNetMode() != NM_Client` — **world-level**, never
`HasAuthority()`, because that answers `true` on a non-replicated actor and would disagree with the
gate the manager picks its own role with (`ASimulationManagerUImpl::instanceFor`).

| channel | direction | mechanism | site |
|---|---|---|---|
| correction state | server → owning client | replicated property + `OnRep_CorrectionState` | this component |
| input bundle | client → server | unreliable server RPC `ServerReceiveRemoteMove` | this component |
| relayed input | server → the **other** peers | the relay host actor's ring | `ASimulationInputRelay` |
| connection tier | server → owning client | a per-connection relay actor's property | `ASimulationConnectionRelay` |

Only the first two are properties of this component. The other two moved to their own actors, and
§5 and §6 are the reasons.

---

## §2 Construction, the tick group, and the optimize pair

`SetIsReplicatedByDefault(true)`, tick enabled, tick group `TG_DuringPhysics`.

⚠ **The tick group's stated reason was false and is corrected here** (§11, F-34-5). The baseline
said the component ticks after "the LiveLink component" and that *"we also use a tick prerequisite
on LiveLink components"*. Neither half holds: the prerequisite call is commented out on the line
below it, and **the string `LiveLink` occurs nowhere in this project's source outside that comment**
— no such component exists to be ordered behind. What survives is the choice itself: the tick group
is `TG_DuringPhysics`, and nothing in this file depends on a prerequisite.

`kMaxRegistrationAttempts = 600` bounds both polling loops (§3). Each attempt is one
`SetTimerForNextTick`, i.e. one **game frame**, so the budget is ten seconds only at 60 fps (2.5 s at
240 fps). ⚠ The shipped comment said "~10 seconds at 60 Hz", which reads as the 60 Hz simulation tick;
it is not (§11, F-25-1). Long enough for scene startup on a slow load, short enough that a genuine
wiring bug fails loudly rather than hanging forever. Both loops count against it independently
(`m_initializationAttempts`, `m_registrationAttempts`), and both end in a `checkf` rather than a silent
give-up. Within loop two, the manager wait, the task-25 id wait (§13) and the bodies-resolvable wait
all share the one `m_registrationAttempts` count.

**Deleted commented-out code (task 25).** Two commented-out overrides stood under the constructor in
the header, `ShouldCreatePhysicsState() const override { return true; }` and `OnCreatePhysicsState()`.
Neither was restorable intent anyone had recorded; they were removed as commented-out code.

**The optimize pair.** The file opens `OGSIM_OPTIMIZE_OFF` and — since the pragma-gate pass — closes
`OGSIM_OPTIMIZE_ON` at true end-of-file. It carried no closing pragma from its first commit, so the
whole translation unit, `TickComponent` included, compiled unoptimized in every build. Closing the
pair at the end changed no scope (there was no code after that point either); it made an implicit
"off to EOF" explicit so the gate has a balanced pair to find.

---

## §3 Reaching the manager — two polling loops, and why both poll

`BeginPlay` cannot do the work, because `ASimulationManagerUImpl::BeginPlay` and the character's
`BeginPlay` race and neither ordering is guaranteed. So `BeginPlay` schedules
`tryInitializeWithManager` for the next tick and returns.

**Loop one — `tryInitializeWithManager`.** Waits for the manager to exist. Once it does: cache the
owner's input-collection component, register the target-visualization query volume with the
manager's query adapter, construct the three visualization states, and schedule loop two.

**Loop two — `tryRegisterWithNewFramework`.** Waits for the manager, then for the pawn's
`SimCharacterId` (§13), then for the character's physics bodies to become resolvable — `tryRegister`
answers `Pending` until they are — and on success does four things in a deliberate order: register with the core, latch the provider decision (§4), spawn or find the relay
host (§6), and, on the authority only, register the id → component delivery route (§7).

Every step of both loops can legitimately answer "not yet", repeatedly, and "not yet" is never an
error. Only exhausting `kMaxRegistrationAttempts` is.

---

## §4 Provider presence, and what absence buys

A **local input provider** is a callable this component hands to the core at registration. It is
installed for exactly one kind of character: on a pure client, the locally-controlled one
(`ROLE_AutonomousProxy`). Every other character — every simulated proxy, and every character on the
authority — registers with a null provider.

That is not a convenience null-check. **Provider presence is the identity test**, and the core
branches on it:

- **provider present** → `SimulationInputResolution::registerLocalCharacter` builds the local
  delay line, and `SimulationNetSync` adds a sender-map entry so this character's captures go on the
  wire.
- **provider absent** → `SimulationInputResolution::registerRemoteCharacter` builds a neutral-seeded
  `RemoteInputCache` for the id, and `SimulationNetSync::registerPredictionOwner` binds the relayed-
  input callbacks by id. `SimulationInputResolution::collectInputAll` then predicts that character
  from the store through the scheduled read, instead of extrapolating the last correction.

**The provider lambda captures nothing but the input collection and the id.** It used to capture the
manager, and reached back through it for the motion matcher's input history; the core now hands the
character's own raw-capture delay line straight in as an argument, so the lambda reaches for nothing
outside its arguments. Capturing the manager again would re-create a lifetime edge the registration
ordering does not guarantee.

---

## §5 The connection tier

The tier is a **wire** property, not a character property. Split-screen siblings resolve to the same
root connection and therefore to the same relay actor, one wire and one tier property — which is
what makes sibling starvation structurally impossible on the transport side rather than merely
guarded against.

Two consequences shape this file:

**Nothing tier-related is initialized per character.** The client-side tier consumer used to be
bound here, lazily, once per character. It lives on the manager now — one per world, bound in the
manager's `BeginPlay` where the `TimeConfig` is created.

**The owner-only condition is gone from `GetLifetimeReplicatedProps`.** The tier no longer replicates
from this component at all; it replicates from `ASimulationConnectionRelay`, whose **owner-only
relevancy** performs the narrowing the replication condition used to perform. The absence fence at
that site is load-bearing: another file in the tree cites it as the evidence that the condition is
gone.

**`sendConnectionTierToOwningClient` is pure transport.** The core already applied the
"no reading" skip and the publish-only-on-change dedup, and calls this method only when the tier for
this owner actually changed — so there is deliberately no sentinel check and no changed-versus-
current test here. The null-root-connection early-out is defensive only: the sole caller is the RPC
receive path, which early-outs on a null root connection before it can reach
`ServerReceptionCoordinator::noteRttSample` at all.

---

## §6 The relay ring — host, forwarders, staging, owner-skip

### Why the ring is not on this component

The relayed-input ring used to be a replicated property of this component, registered with a plain
lifetime entry and no condition. It is now a property of `ASimulationInputRelay`, one per character,
registered `DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner)`.

**Two separate gains, and the smaller one is the famous one.** The condition skips the echo of a
character's own input back to its owner — about 85 bytes per connection per round of pure waste. But
the *reason for the move* is the other gain: on this component the ring and the correction state
shared one atomic replication batch, so a round the replication system dropped under packet pressure
killed both together. On its own dependent object the ring is scheduled and prioritised
independently. **Adding the condition here instead would have bought the bytes and left the coupling
in place** — which is why the absence fence in `GetLifetimeReplicatedProps` says so explicitly.

### The concept surface, and why this file has forwarders

`ASimulationInputRelay` lives in `OGSimulationUnreal`, which must not depend on `OGBrawlerUnreal`.
More importantly, the engine-free core must never name an engine type at all. So the core still sees
exactly four members on *this* object — the ring accessor pair and the callback pair — and never
learns that the ring is carried by a different actor. `SimmableUpdateComponentConceptTest.cpp`
passing unchanged across the move is the proof that the engine-facing change did not leak into the
core contract.

`getRelayedInputRing`, `stageRelayedInput`, `setOnRelayedInputReceivedCallback` and
`clearOnRelayedInputReceivedCallback` are that surface. Each resolves the host and degrades to a
never-replicated local fallback if there is none, so an unlinked component behaves identically on
the read and the write path.

### Linking, and why there are three paths into one function

`attachInputRelayHost` is reached from the authority's own spawn, from the client's pull at
registration, and from the manager's listener when the host replicates in. It is idempotent, and the
repeats are the design: the host and the component come up in an order nothing controls. On link it
installs a **weak** capture — the host can outlive this component by a frame during teardown, and a
raw `this` would then be a dangling call from a replicated notification — and then **replays the
current ring**, which covers the ordering where the host replicated and dropped one or more OnReps
before the link existed. A never-written ring reads as version 0 and the ingest no-ops, so the replay is
free on the authority and on a freshly spawned host.

### The spawn ordering, and what it protects

The host is spawned on the authority at the moment registration completes, **above** the delivery-
route registration, and that order is load-bearing. `noteDelayedInputComponent` is what lets
`ASimulationManagerUImpl::relayRemoteInput` find this component at all; if the host did not exist by
then, the first relay writes would stage into `m_detachedRelayStagingRing`, invisible to every client
and with nothing logged. Spawning first closes that window by construction rather than by timing.

This is also why the host is *not* spawned on demand from the write path, the way the connection
relay is: that actor has no always-reached creation point earlier than its first write, and
`PostLogin` misses seamless travel. This one does. The same asymmetry governs teardown: the
connection relay polls a one-second timer because its owning `PlayerController`'s death does not
destroy it and there is no other reliable signal, whereas this host has one — the component whose
character it belongs to is ending play, synchronously. **The authority destroys the host; a client
only drops its reference**, because a replicated actor is removed on clients by the destruction
bunch and destroying it locally would race that.

⚠ **The fallback container's name was wrong in the baseline** (§11, F-34-6). The baseline said those
writes would land in `m_detachedRelayRing`. Since the write path became a *staged* write they land in
`m_detachedRelayStagingRing`, a different member. Both exist; only the staging one is on that path.

### Staging, not writing

`stageRelayedInput` puts the arrival in the host's **staging** ring; the host's `PreReplication`
publishes the whole staged burst once per replication poll, so two arrivals in one server frame both
reach the wire instead of the second overwriting the first. It takes **no depth parameter**, and that
absence is the fence — a depth passed down here would cap every round at one entry and silently
restore the replace-latest behaviour the staging exists to remove, with no compile error anywhere.
Since task 25 this is a compile-time check: a `static_assert` beside the definition pins
`stageRelayedInput`'s exact signature, and was seen to fire (`C2338`) when a defaulted `uint32 depth`
parameter was added to both the declaration and the definition. The history the header carried: before
staging, the sink wrote the replicated ring directly at a session-configurable retention depth, so a
second arrival in one server frame overwrote the first in memory before replication ever compared the
property — about 11.6 % of relayed inputs, measured at the time; a depth parameter here would cap every
round at one entry and reproduce exactly that. The return value's `droppedOldest` means the burst
exceeded `relayedInputRing::kMaxDepth` in one frame and has already been counted on the host.

A burst longer than the stage drops its oldest entry, and that drop is **counted on the host** rather
than absorbed: it is the one input loss this side of a zero-redundancy configuration can actually
see. It is counted on the host because the host is what a run inspects, and because a drop against
the detached fallback is already covered by the much louder fault that no host exists at all.

### The owner-skip precondition

The owner-echo removal rests on an equivalence that nothing checked: **"owning connection"** — the
set the replication condition narrows on, decided server-side from the host's owner chain — and
**"provider present"** — the set that decides whether the core builds a relay store, decided
client-side from the local role. If those two ever name different sets the failure is silent in
*both* directions: a character in the first set but not the second still pays the echo, and a
character in the second but not the first loses its relayed input entirely.

`onRelayedInputRingArrived` asserts the observable half. A ring with resident entries arriving at a
character that **has** a local input provider means the skip did not apply where this build believes
it does — most likely an unset or wrong owner on the host. Nothing consumes the payload either way
(the core binds no store for a provider-present id), so this is pure diagnosis.

Three deliberate choices in that check, each of which has a guard at the site:

- **Gated on `num() > 0`.** An empty ring is not evidence. The host actor is `bAlwaysRelevant` and
  *does* replicate to the owning client — only the property is skipped — so an owner legitimately
  holds a host carrying a never-written ring. Asserting on that would fire on every clean run.
- **`ensure`, not `check`.** This is a bandwidth and correctness regression, not a memory-safety
  fault, and a session that hits it should stay playable enough to be diagnosed.
- **The provider decision is latched at registration, not recomputed at arrival**, so the two halves
  of the comparison are literally the same evaluation taken once.

---

## §7 The RPC boundary — primitives in, policy out

`ServerReceiveRemoteMove` is a thin transport adapter. Everything below its wire-format fence is
engine-primitive acquisition plus a forward into the engine-agnostic
`ServerReceptionCoordinator`. **No netcode policy lives here**: the tier derivation and its EMA,
capture-tick dedup, park and drain, the malformed-slot fence, and the per-slot receive loop itself
are all core. This side resolves only what a second engine would also have to supply — the root
connection, the player slot, the round-trip time, the server simulation tick — plus the wire buffer.

**The order is documented and load-bearing:** the wire-format fence, then one RTT sample, then the
core per-slot loop.

**Resolve the root connection once.** Split-screen siblings collapse to the single wire entry their
shared connection deserves; both the tier and the slot key derive from it. A null connection is the
documented "no wire identity" sentinel — standalone, or a listen server's local pawn.

**The no-wire fallback boundary is adapter-side.** With no coordinator or no wire, every slot takes
the legacy undelayed delivery path directly, without the coordinator, so **the core only ever sees a
valid wire.**

**One RTT sample per bundle, never per slot.** A bundle is one datagram, one arrival event; sampling
per slot would feed the same reading into the tier EMA up to `kMaxSlots` times and couple the
smoothing to redundancy depth. `noteRttSample` *drives the send itself* — it derives the tier and,
on a change for this owner, fires `sendConnectionTierToOwningClient` through the sink, which is this
component (§5).

**The slot primitive is which local player on that wire.** The tier keys on the root connection,
because latency is a property of the link; input keys on the slot as well, because input is per
character.

**The delay is applied exactly once, park-to-release.** The core's receive loop parks each slot in
the `ServerInputDelayQueue`; a released input keeps its **original** capture tick, and the
authority's consumer — `SimulationInputResolution::collectInputAll`, authority branch — pops the
`RemoteMoveQueue` in **arrival order**, so it contributes no offset of its own.

**The last argument is the relay sink**, the same manager object bound to a different boundary:
delivery routes an input *into* the simulation, the relay forwards it *out* to the other clients.
Two arguments, one object, deliberately.

**The id → component route is registered once, at registration time**, paired with the erase in
`unregisterFromNewFramework` — not per parked slot inside the receive loop. The route the
coordinator's drain and the malformed-slot deliver-now fallback resolve against therefore exists for
the whole registered lifetime of the component. Authority only: the coordinator, the delay queue and
the drain are all absent on a pure client, and the inbound delivery callback is itself wired only at
registration, so the route and its target come up together. The erase side is a pair as well: the
manager drops its own claim through `ServerReceptionCoordinator::forgetOwner` at the same moment.
Authority only — the coordinator, the delay queue and the drain are all `std::nullopt` or no-ops on
a pure client.

**This component satisfies `ConnectionTierSink`.** The concept is compile-checked at the
`noteRttSample` call site; the `static_assert` beside the definition exists so a breaking signature
change surfaces as a legible assertion rather than an opaque template error.

---

## §8 The wire-format compat fences

Both directions carry a version byte and both refuse rather than guess.

**Client side, `OnRep_CorrectionState`.** The sender's wire-format version is byte 0 of the payload,
captured by the buffer's serializer. On disagreement the handler logs an error, raises a one-time
on-screen build-mismatch toast, latches `m_wireFormatMismatchDetected`, and returns. **Once latched,
every later notification no-ops**, so a mismatched peer can never drive local correction logic with
bytes it cannot parse.

**Server side, `ServerReceiveRemoteMove`.** Same test, one extra case first: **an empty bundle is
idle traffic, not a mismatch.** A client with no pending input in the redundancy window sends a
bundle with no wire header and therefore no version byte, which reads as version 0. Treating that as
a mismatch would log a false alarm every idle frame. The per-slot iteration is a no-op on it anyway.

On a genuine mismatch the server logs and returns. **It does not disconnect the client** — the
disconnect path is engine-managed and is a dedicated-server-validation concern, not this fence's.

The wire fence for the state channel is `FSimulationStateSyncBuffer::kWireFormatVersion`, which is
**5** today: 1 → 2 for the applied-capture-tick reference, 2 → 3 when the ring-out
sub-simulation joined the state composite (ring-out task 2), 3 → 4 when the brawler radial's
`hasHitGuard` left the middle of the composite (og-netcode-v2-field-defects task 9), and 4 → 5 when the
projectile slot's `hitRootBodyId` left the wire (og-netcode-v2-field-defects task 17). ⚠ This section
said **4** until task 25's re-verification (§11, F-25-2). The header's latch comment called the case
"a pre/post-Stage-1 build mismatch"; the latch fires on **any** disagreement with the version constant.
The toast uses one stable key (`kWireFormatMismatchToastKey`), so a repeated call replaces the message
rather than stacking it. The second bump is the interesting
one — the composite only *grew*, every pre-existing offset held, and the bump was made anyway
because an older archived build does not compile the new sub-simulation in at all and the payload
layout cannot express that. The reasoning is written at the constant in
`CorrectionStateBufferCodec.h`.

---

## §9 Retired channels — what stood here, and why the absence is fenced

Three retirements leave fences in this file. Each names the symbol that is gone, so a reader who is
about to re-add it has something to recognise. They cannot be moved to a document, because a reader
about to *add* a line has no symbol to look up.

**`OnRep_CorrectionInput`.** It peeked the replicated input for a trace and forwarded the buffer to
the core's correction-input binding. The property, the notification, the trace and the binding were
all retired together; the reasoning lives at `SimulationNetSync::sendCorrectionAll`, which is where
the writer half was, and where the own-character-drop analysis is recorded.

**The `m_replicatedInputSyncedBuffer` lifetime registration.** Removed in the same edit as the
member, and the pairing is the point: a registration naming a member that no longer exists is a
compile error and so cannot be forgotten, while keeping the member and dropping only the
registration leaves a silently-never-replicated field. Both halves go together, always.

*Fence coherence, which is the part a wire-format editor needs:* removing a property leaves **no
hole and no stale version expectation.** Lifetime registrations are per-property entries, not fixed
wire offsets, and every surviving property carries its own self-describing serializer with a
watermark-trimmed length prefix. That is why the state channel's version was deliberately **not**
bumped for this removal: the state payload's layout is unchanged, and every build that speaks the
current version already agrees the input value is not on the wire. `FSimulationInputSyncBuffer` is
untouched and survives in its client→server role as `m_clientToServerInputSyncedBuffer`.

**The relayed-input ring's lifetime registration**, and the connection tier's owner-only one. Both
covered in §6 and §5 respectively.

---

## §10 The render-side input echo, and the visualization sites

`TickComponent` draws only. It resolves the manager for its role, skips if the character is not in
storage, and runs six visualization passes — radial, aim, projectile, movement (cvar-gated), target and
block-prediction — then the input-history poll. (This line said "five" before task 25's re-count.)

**One input source, resolved once per render frame.** `vizPlayerInput` is computed once and shared by
every input-carrying visualization site, so the aim indicator and the block-prediction wedge can
never render two different samples of the same frame.

**`hasInputComponent()` is the local-versus-remote discriminator.** It is true for a pure client's
autonomous proxy **and** for the listen-server host's own pawn, and false for every simulated proxy —
because the bindings it reports are installed by `setupBindings`, reached only from
`AOGBrawlerUECharacter::SetupPlayerInputComponent`, which the engine calls only for a
locally-controlled pawn. The registration-
time `ROLE_AutonomousProxy` test (§4) is deliberately **not** reused here: that one excludes the
host, and the host is exactly the character this echo was added to cover.

- **Local character:** sample live, at this render frame's rate.
- **Remote simulated proxy:** the relay store's **last-known** relayed input — the same character's
  real input as relayed by the authority, and the source the simulation's own proxy prediction
  resolves from. It is deliberately *not* the per-tick scheduled read: the visualization wants "what
  is this player doing", not "which input does tick N run on". This used to read the correction
  cache's input column through `editReconciliation().getLatestInput(...)`, roughly one round-trip
  behind; the relayed value is fresher by about the return leg, and both that column and
  `getLatestInput` itself have since been retired outright.

⚠ **The listen-server fallback holds, but the stated reason for it was wrong** (§11, F-34-1). The
accessor does answer nothing on the authority, so the host's pawn takes the live-sample path and
nothing depends on the remote read. But it is **not** because the authority allocates no relay store:
the authority registration facade calls `SimulationNetSync::registerPredictionOwner` with a null
provider, which takes the provider-absent branch and **does** allocate one for every character. The
store is simply never written there — the ring notification is client-only and the attach-time replay
sees a never-written ring — so `findLatest()` is invalid and the accessor answers nothing because the
store is **cold**, not because it is absent.

**The echoed value carries continuous fields only.** Every discrete field is pinned neutral by
`simulatableBrawler::makeVisualizationPlayerInput`, so an attack or a Hadouken edge structurally cannot render-echo. It is
cosmetic: never fed to the simulation, the RPC, or a cache.

**No tier consult.** Muting the echo on a degraded connection tier is a separate, optional change and
is not wired here.

**The block-prediction pass has no local-player gate on *whether* it draws** — it draws for every
character. Only the input source splits, and that decision was already made in `vizPlayerInput`. A
remote proxy with nothing relayed yet is skipped for that frame, which is the pre-existing behaviour
the nullopt contract preserves.

**The projectile pass** takes its current tick and delta from the **same** clock the simulation uses —
the prediction step on a client, the server simulation step on the authority — so the residual-
lifetime arithmetic lines up with the simulation's own pruning.


### The movement draw and the input-history poll

*Moved from `TickComponent` (task 25).*

**The movement draw reads a sim-tick snapshot on the render clock.** `attackSimAllState` is
`getVizState()` — the whole-`AllState` copy `updateVisualizationAll` takes once per completed sim tick in
`ASimulationManagerUImpl::OnPostPhysicsStep`. So every drawn `State` / `DerivedState` value is stale by
up to one 60 Hz tick, and a render frame above 60 Hz redraws the same snapshot; `StaticData` is authored
once per session. The State and DerivedState halves come from the **same** copy, so the probe reading
and the body pose describe one tick and can be combined — which the servo-error arithmetic in
`BrawlerMovementVisualization.h` rests on. The call is a pure reader: every argument is a `const&`.

**The movement cvar is read per frame, deliberately (guard G-26).** Task 16 made `StaticData` cvars
read once, at construction, so a tunable cannot move under a running session and put two peers on
different numbers. A viz cvar is the opposite case: it feeds nothing simulated, and its value is that a
tuner can type `OGBrawler.Viz.Movement 1` mid-session and see the next frame change. The draw is ~60
debug-line calls per character per frame, so off must cost one bool read. The same reasoning is written
at `brawlerMovementVisualization::visualize` and in `ScoreboardDisplay-rationale.md` §4.

**The input-history poll is the display's only feed.** The rings are keyed per character id, so a
couch sibling gets its own the moment anything polls it. The **lanes** are polled for every registered
character (each component runs the block for itself, exactly once, guard G-30), and the display picks
which to draw at draw time — binding the feed to the selection is what once made a remote proxy's
corrections invisible. The **row** poll keeps its gate (G-29): its source is this client's own capture
line, which a remote proxy does not have. The machine state comes off the same `attackSimState` the
block-prediction pass reads; there is no per-tick machine-state history, so a missed tick stays a hole
and is never back-filled. Each lanes ledger is bounded at `kTickLaneCapacity` (240 ticks). The poll is a
render-rate game-thread read of a physics-written capture line — the accepted tear argued in
`SimulationManagerUImpl-rationale.md`'s CROSSING table. Nothing decides on it.

**The aim and block-prediction passes** both consume the shared `vizPlayerInput`; a remote proxy with
nothing relayed yet is skipped for that frame. The block-prediction pass shares the target pass's
query-volume list (`m_targetVisualizationVolumeIds`). The legacy per-enemy range arcs
(`DAttackTargetVisualization.legacyEnemyRangeArcsEnabled`) are off by default; the block-prediction
pass is the primary view and the arcs remain for A/B comparison.

**`selectVisualizationInput`'s echo carries continuous fields only** (§10); **no tier consult** is wired
here — muting on a degraded tier is a separate, optional change.

---

## §11 Corrections — claims this file carried that were not true

Every claim of fact in the baseline was checked against the tree **before** being compressed. Nine
did not survive the check. Compression makes a false statement shorter, denser and more
authoritative, and coverage, subject and lint checks all pass it, because every symbol it names
exists — they verify existence, never truth.

| # | claim as it stood | what the tree says |
|---|---|---|
| **F-34-1** | *"the authority allocates no relay stores at all (`registerPredictionOwner`'s provider-absent branch is the only site that creates one, and it runs on the client)"* | **FALSE.** The authority registration facade calls `registerPredictionOwner` with a **null** provider, so the provider-absent branch runs there too and `SimulationInputResolution::registerRemoteCharacter` allocates a neutral-seeded store for **every** authority-registered character. The behaviour the sentence defends survives — the accessor does answer nothing on the authority — but because the store is never written, not because it is absent. ⚠ The same false sentence sits in two files this task does not own; see the routed findings below. |
| **F-34-2** | *"exactly the test the radial viz above already uses (line ~555)"* | **A STALE LINE JOIN.** The radial visualization's `hasInputComponent()` test is at `:831` in the baseline; `:555` is inside `attachInputRelayHost`, an unrelated function. Re-anchored on the symbol per the never-cite-a-line-number rule. |
| **F-34-3** | *"`SimulationNetSync::collectInputAll`, authority branch"* | **WRONG OWNER.** `collectInputAll` moved to `SimulationInputResolution` when input resolution was extracted; the qualified `SimulationNetSync` form is already declared a dead owner in `CorrectionCache-rationale.md`. Re-owned. |
| **F-34-4** | *"`noteDelayedInputComponent` is what makes `relayRemoteInput` able to find this component and **write the ring**"* | **HALF TRUE.** The route claim is right — `relayRemoteInput` does resolve through that map, verified at the manager. But since the write path became a staged write it **stages**, and the host's `PreReplication` publishes; nothing writes the ring directly from there. |
| **F-34-5** | *"This component is designed to tick after the LiveLink component… We also use a tick prerequisite on LiveLink components"* | **FALSE ON BOTH HALVES.** The prerequisite call is commented out on the next line, and `LiveLink` appears nowhere in this project's source outside those three comment lines — there is no such component to order behind. §2. |
| **F-34-6** | *"the first writes would land in `m_detachedRelayRing`"* | **WRONG MEMBER.** The staged write path falls back to `m_detachedRelayStagingRing`. Both members exist, which is what makes the error invisible to any check that only asks whether a name resolves. §6. |
| **F-34-7** | the block-prediction guard: *"render-rate live sample locally, **server correction ~1 RTT behind** for remote simulated proxies"* | **STALE, AND THE FILE CONTRADICTED ITSELF SEVENTY LINES APART.** The re-source block above it already said the remote source is the relay store's last-known relayed input, *"fresher by roughly the return leg"*. This guard still described the retired correction-cache column. §10. |
| **F-34-9** | *"the registration-time `ROLE_AutonomousProxy` test used in `tryRegisterWithManager`"* | **A SYMBOL THAT DOES NOT EXIST.** The only occurrence of `tryRegisterWithManager` anywhere in the tree was that comment. The two real functions are `tryInitializeWithManager` and `tryRegisterWithNewFramework`, and the test in question is in the second. Re-anchored on §4 rather than on a name a reader would grep for and never find. |
| **F-34-8** | seventeen lines of commented-out code | **UNRESTORABLE.** `m_simulationStateSyncedBuffer`, `EpicGamesAssignment`, `LiveLinkComponent` and the two `StructeredLog` headers (a misspelling) exist **nowhere** in this tree or the engine include paths. Un-commenting any of them is a compile error, so none is code that could be brought back. Deleted; the one fact they carried — that no tick prerequisite is installed — is stated in §2. (Task 25: no longer tagged — re-adding a prerequisite on a component that does not exist is a compile error, so there is no edit left to fence.) |
| **F-25-1** | *"~10 seconds at 60 Hz"* at `kMaxRegistrationAttempts` | **WRONG CLOCK.** One attempt per `SetTimerForNextTick`, i.e. per game frame, not per 60 Hz sim tick. §2. |
| **F-25-2** | this file's §8: *"`kWireFormatVersion` … is **4** today"* | **STALE.** It is **5** (`CorrectionStateBufferCodec.h`), bumped when `hitRootBodyId` left the wire. §8. |
| **F-25-3** | *"SPAWN THE HOST BEFORE THE noteDelayedInputComponent ROUTE -- until that route exists, relayRemoteInput stages into m_detachedRelayStagingRing"* | **WINDOW REVERSED.** Until the route exists `relayRemoteInput` finds no component and returns. The hazard is the route existing while no host is linked. Guard G-06. |
| **F-25-4** | *"NEVER HasAuthority() HERE -- it is always true on a non-replicated actor"* | **WRONG ACTOR.** That describes the manager (`bReplicates = false`); this component's owner is a replicated pawn. The prohibition survives on the other half: the predicate must match the one `ASimulationManagerUImpl::BeginPlay` fills its instance slot with. Guard G-02. |
| **F-25-5** | header, `m_detachedRelayRing`: *"A server-side write that landed here would be invisible to every client, so the spawn is sequenced BEFORE the relay tap's id->component route"* | **WRONG MEMBER, AGAIN.** F-34-6's error, still standing in the header: server writes are staged, and with no host they land in `m_detachedRelayStagingRing`. Nothing writes `m_detachedRelayRing` on the server path; it is the read fallback. §6. |
| **F-25-6** | header, the callbacks: *"Null until Task 7 wires the new path; OnRep_ handlers fall through to old logic when null"* | **FALSE.** There is no old logic. `OnRep_CorrectionState` calls the callback if bound and otherwise does nothing further. |
| **F-25-7** | header, the host's forward declaration: *"the two accessors that dereference it are defined in the .cpp"* | **UNDERCOUNTED.** Six members dereference the host in the `.cpp`: both `getRelayedInputRing` overloads, `stageRelayedInput`, `setOnRelayedInputReceivedCallback`, `attachInputRelayHost`, `EndPlay`. The forward declaration's reason (a widely included header) stands. |
| **F-25-8** | header, the tier sink: *"this component is its own target, so it asserts `id == GetUniqueID()`"* | **FALSE SINCE TASK 25.** The `check` compares `id` with the storage key of the pawn's replicated `SimCharacterId`. §13. |
| **F-25-9** | *"THE DELAY BAR IS A THIRD BAR ON THE METER, so it implies the LANE poll runs even when the other two bars are off"* | **STALE COUNT.** `anyBarEnabled()` counts four selections: provenance, input delay, character state, and relay health (nearest stack only). The conclusion — any bar implies the lane poll — holds. §10. |
| **F-25-10** | *"findOrSpawnForConnection logged the reason"* | **PARTLY.** It logs on its reachable null path (no owning actor on the wire). A null world or connection returns silently; the connection cannot be null here. |
| **F-25-11** | header, the correction-input absence: *"a remote character's input reaches peers on the relay ring below"* | **NOT BELOW.** The ring is a property of `ASimulationInputRelay`. Guard G-36. |
| **F-25-12** | *"the accepted tear argued at ASimulationManagerUImpl's CROSSING block"* | **RE-POINTED.** The manager header was converted; the CROSSING table now lives in `SimulationManagerUImpl-rationale.md`. §10. |
| **F-25-13** | the tier block's citation of a relay-delay-spectrum design document, §12 | **UNRESOLVABLE HERE.** That document is in a private workspace outside this repository (as R-34-d). Dropped; the reasoning it pointed at is in §5. |

### Routed — not this file's to fix

- **R-34-a.** `SimulationInputResolution.h`'s `getLastRelayedInput` banner states *"no store exists
  for a LOCAL character or on the **AUTHORITY** at all"*. The first half is right; the second is
  F-34-1's false claim, in the core, in the file that owns the accessor.
- **R-34-b.** `SimulationInputResolutionTest.cpp`'s listen-server case says *"the authority allocates
  no relay stores at all, **verified rather than assumed**"* — and the assertion under that sentence
  registers through `registerAuthorityOwner` alone, never through the authority `registerSimulatable`
  facade that production uses. The test passes; it verifies a path production does not take.
- **R-34-c.** The authority `registerSimulatable` facade's own guard reasons about *"an id exposed
  early reading a relay store that will never exist for it"*. By F-34-1 the store does exist on that
  path; the branch dispatch it protects is keyed on the remote-move queue, so the fence's conclusion
  is unaffected, but its stated premise is not.
- **R-34-d.** Two comments cite `risks_and_plan.md §5.2`, a document in a private research workspace
  outside this repository. Removed from this file; the same citation survives in five other files
  with no owner. The wire-format fence's reasoning is now in §8, which resolves standalone.

---

## §12 The task-25 conversion — where every pre-conversion comment went

Both files went from the Phase C convention (one-line `⛔ … §N` fences, long header banners) to
code plus tags. Nothing was discarded without a destination. **Guards** are
`SimmableUpdateComponent-guards.md` G-01 … G-37 (G-01 was written with task 25's code; the rest quote
the shipped fence they replaced). **One fence became a `static_assert`**: the header's "NO DEPTH
PARAMETER" (§6). Everything else is here.

| pre-conversion comment | where it went |
|---|---|
| `.cpp` orientation banner | §0 (phase order updated for §13) |
| movement-cvar block; `TickComponent` movement, poll, echo, aim, projectile and block-prediction prose | §10 and its movement/poll subsection; the prohibitions in them are G-24 … G-31 |
| `kMaxRegistrationAttempts` "~10 seconds at 60 Hz" | §2, corrected (F-25-1) |
| tick group / LiveLink one-liner | §2 (already there, F-34-5) |
| `BeginPlay` / "Kick off body creation" / loop comments | §3 |
| `⛔` one-liners in `tryInitializeWithManager`, `tryRegisterWithNewFramework`, `EndPlay` | G-02 … G-08, G-15; the CLIENT pull-half note is §6 "Linking" |
| `OnRep_CorrectionState` fences; the version-byte note | G-09; §8 |
| retired `OnRep_*` one-liners; `GetLifetimeReplicatedProps` absence fences | G-10, G-11, G-32 … G-34; §9 |
| relay forwarders (section banner, re-read at bind, idempotent link, weak capture, replay) | §6; G-12 … G-14 |
| owner-skip precondition, `num() > 0`, `ensure` | §6 "The owner-skip precondition"; G-15, G-16 |
| RPC-boundary one-liners; the `ConnectionTierSink` `static_assert` note | G-17 … G-23; §7 |
| tier sink one-liners (pure transport, relay actor, defensive null) | §5; F-25-10 |
| `OGSIM_OPTIMIZE_ON` fence | G-35; §2 |
| trailing labels (`GEngine` include, legacy arcs default, `RemoveDependentActor, then Destroy`, idempotent, "logged the reason"), the `//Component` banner, the relay-carrier include note | deleted as labels; their facts are in §6, §10 and G-08 |
| header: concept typedefs, the callbacks, forwarders, attach/arrival declarations | §6, §7 and "Carried header prose" below |
| header: tier block (Option A, why the sink stayed on the component) | "Carried header prose" below; §5 |
| header: `stageRelayedInput` banner | `static_assert` + §6 |
| header: retired members (correction-input property and callbacks, `getSyncedCorrectionInputBuffer`, the ring property, the tier property and consumer, `getMachineVizState`) | G-36, G-37; §9 and below |
| header: wire-format latch, toast key | §8 (latch wording corrected) |
| header: `m_inputRelayHost`, detached rings, `m_hasLocalInputProvider` | §6; G-12; F-25-5 |
| header: `UObject` constructor doc, `//Physics`, `//Visualization`, "To add mapping context" | deleted as labels |
| header: two commented-out physics-state overrides | deleted; recorded in §2 |

### Carried header prose (verified before the move)

**The concept typedefs.** `SyncedCorrectionBufferType` is the correction-state role
(`FSimulationStateSyncBuffer`); `SyncedRemoteInputBufferType` names **one** role since the correction-
input channel retired — the client→server buffer. `RelayedInputRingType` is named through a typedef for
the same reason: `SimulationNetSync` binds the arrival callback and reads the ring at registration, and
must do so without naming a UE type. `SimulatableOwnerTraits<SimulatableBrawler>` is specialised in
`SimulatableBrawlerOwnerTraits.h`; any site that instantiates `SimulationNetSync<SimulatableBrawler>`
includes that header.

**`sendLocalInputToAuthority`** builds an `FInputRedundancyBundle` from the most recent
`redundancyDepth` ticks still in the core's `PendingInputQueue` (the builder clamps the depth to the
bundle's slot capacity) and fires the unreliable `ServerReceiveRemoteMove`. It is defined in the `.cpp`
so the bundle-builder include stays out of this widely included header. The RPC is **unreliable**
since Stage 1: no head-of-line blocking on input RPCs, and a dropped datagram self-heals because each
send repeats the last `redundancyDepthTicks` ticks.

**`deliverDelayedRemoteInput`** is the delivery entry point for an input released from the server's
`ServerInputDelayQueue`, reached on the game thread from `ASimulationManagerUImpl::deliverRemoteInput`
before the authority physics step. It routes through the **same** callback the RPC's no-wire fallback
uses, with the **original** capture tick, so from `RemoteMoveQueue` onward a delayed input is
indistinguishable from an undelayed one and the capture-tick dedup keeps working. The delay is
expressed only as *when* this is called (G-23).

**The tier (Option A, server-authoritative).** The server derives the tier from its own per-connection
round-trip time (`ServerReceiveRemoteMove` resolves the primitive and forwards it to
`ServerReceptionCoordinator::noteRttSample`) and publishes it; the client only reads it. With one
producer there is no second estimator to disagree with, so the boundary-RTT tier split and its
recurring one-tick corrections cannot happen. The replicated property, its notification and the client
consumer moved off this per-character component onto the per-connection `ASimulationConnectionRelay`:
one actor per wire makes "one wire, one tier" structural. **Why the sink stayed here**: it must
resolve id → owning wire, and this object holds that link unconditionally. The manager's
id → component map is populated at registration, several frames after the first input RPC can
arrive, and the core records a tier as published **before** calling the sink
(`ServerReceptionCoordinator` updates its last-published table, then calls
`sendConnectionTierToOwningClient`), so a publish the sink could not route would never be retried. The
`ConnectionTierSink` `static_assert` pins that this component is the sink.

**The host link.** `m_inputRelayHost` is weak because the host is destroyed with the character and the
teardown order between the two is not guaranteed. It is set on the authority at spawn — registration
completion, strictly before the first relay write (G-06) — and on a client by whichever link path gets
there first. `m_detachedRelayRing` is the **read** fallback: the core reads the ring once at
registration bind on both roles and the concept types it as a reference, so an unlinked component
returns this never-replicated, always-empty ring (version 0, so the bind-time ingest no-ops).

**Retired, and not fenced by a tag:** `set/clearOnCorrectionInputReceivedCallback` and
`m_onCorrectionInputReceivedCallback` (retired with the correction-input channel);
`getSyncedCorrectionInputBuffer` (the authority's outbound surface is now the correction-state buffer,
carrying the applied-capture-tick ref, plus the relay ring); the tier property, its notification, the
pre-notification latch and the `ReplicatedTierConsumer` (now the manager's); `getMachineVizState()`
(its only caller was the pawn's flinch-freeze predicate on the retired movement-component path; the
freeze now happens inside `brawlerMovementSimulation::integrate`). The fenced retirements are G-10,
G-11, G-32 … G-37.

---

## §13 The simulation character id (task 25)

Every id this component hands the core — `tryRegister`, `noteDelayedInputComponent`,
`unregisterFromNewFramework`, the provider lambda, every `id=%u` log line, the storage lookup in
`TickComponent`, the relayed-input read, the input-history polls — is the pawn's replicated
`SimCharacterId`, read through the private helper `simCharacterId()` (which answers
`SimCharacterId::None` if the owner is not an `AOGBrawlerUECharacter`). It is **not** this component's
`GetUniqueID()`, which is a different number on every peer.

**How the id arrives.** In `tryRegisterWithNewFramework`, after the manager exists:

* **Authority:** if the pawn's id is still `None`, allocate one from
  `ASimulationManagerUImpl::allocateSimCharacterId` (count-up 1 … 255, never reused) and set it with
  `AOGBrawlerUECharacter::SetAuthoritativeSimCharacterId`. A refusal (all ids spent) is final —
  guard G-01; the manager logs it at Error and `OG_CHECK`s.
* **Client:** while the replicated id is still `None`, stay pending on the same retry loop and the same
  `m_registrationAttempts` budget, ending in a `checkf` if it never arrives.

The id is replicated with the pawn, not on the correction wire, so no wire format changes. The
`sendConnectionTierToOwningClient` `check` compares its `id` argument with the same storage key.
