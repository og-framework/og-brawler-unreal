<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `ASimulationManagerUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/SimulationManagerUImpl.h` and
`Source/OGBrawlerUnreal/SimulationManagerUImpl.cpp`. This file carries the reasoning, the
provenance and the worked derivations.

**Both source files carry no prose.** Each holds a two-line docs pointer, code, assertions whose
messages say what they replaced, and one-line tags: `⛔G-nn` → an entry in
`SimulationManagerUImpl-guards.md`, `∴D-nn` → the heading in this file that ends with that id. The
header's ids are below 50; the `.cpp`'s start at `G-50` / `D-50`, so the two files' ids cannot
collide.

* **The header's** pre-conversion comment text is carried here verbatim: the orientation banner
  in §0, everything else in §12 (og-netcode-v2-field-defects task 11).
* **The `.cpp`'s** comments were mostly already stated in §1–§11, which were written from them.
  What those sections did not carry is quoted in §13, with the compile-time checks that replaced
  six of its prose fences (§13.9) and the claims it made that were not true (§13.10)
  (og-netcode-v2-field-defects task 12).

<!-- ================= DECLARED LINT ESCAPES =================================
     Every token below is CORRECT and cannot resolve. None is here to silence a
     name that is wrong: the two wrong names this lint found were FIXED, not
     escaped (§11 F-32-5 and F-32-6).
     ========================================================================= -->
<!-- lint-external-ref: AActor::Owner -- Unreal Engine type, outside every scan root; the engine is not vendored into this repository -->
<!-- lint-external-ref: UNetConnection::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::WriteData -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: GameEngine::GetMaxTickRate -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::FindValidResimFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::SetTargetStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::RewindToFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::ApplyTargets -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsEvolutionGBF::Integrate -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: SetXR -- Chaos engine particle-handle method, outside every scan root -->
<!-- lint-external-ref: PushStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: ApplyCallbacks_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: StatPeriod -- Unreal Engine stat window, outside every scan root -->
<!-- lint-external-ref: DesiredTickRate -- Unreal Engine net-driver field, outside every scan root -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root -->
<!-- lint-external-ref: BaseEngine.ini -- Unreal Engine's own config file, outside every scan root -->
<!-- lint-external-ref: ChaosMarshallingManager.h -- Chaos engine header, outside every scan root -->
<!-- lint-external-ref: PBDRigidsSolver.cpp -- Chaos engine source, outside every scan root -->
<!-- lint-external-ref: NoLogging -- an Unreal log-verbosity enumerator, written as an ini value -->
<!-- lint-external-ref: notReady -- a field NAME inside one probe's log format string, not a declared identifier -->
<!-- lint-external-ref: writesThisFrame -- a term in the stated ceiling formula, not a declared identifier -->
<!-- lint-external-ref: .Build.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: .Target.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (§3): the ini key that was built and removed on a ruling. It must NOT resolve; the day it does, that ruling has been reversed -->
<!-- lint-external-ref: routeInboundHits -- ABSENCE FENCE (§5): the retired adapter-side routing shim. It must NOT resolve -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (§6): the retired second write of the discharged dual-write fence. It must NOT resolve -->
<!-- lint-external-ref: getLatestInput -- ABSENCE FENCE (§6): the deleted correction-input read. It must NOT resolve -->
<!-- lint-external-ref: sampleAndDeriveConnectionTier -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->
<!-- lint-external-ref: tryEnqueueDelayedRemoteInput -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. It is deliberately not
> in the `og-simulation` docs tier, which is MPL-2.0 and travels with a different repository.

**Read §0 first** — the header's orientation block, carried below verbatim. It states the shape —
two role instances, the thread table, the construction order, the one delay formula, the four
session knobs — in about a hundred lines. Everything below assumes it.

---

## §0 Orientation — the header's banner, carried

*Task 11 moved this out of `SimulationManagerUImpl.h`, where it was the first 105 lines. The bytes
below are the shipped text at `b9f6d81`; the corrections after the block are R0 findings, and they
are annotations, not edits to the carried text.*

<!-- header lines 2-105 at b9f6d81 -->
```
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
```

**R0 annotations (task 11) — read these with the block, not instead of it:**

* *"FSimulationManagerAsyncCallback's five _Internal hooks"* — **six** `_Internal` overrides, two
  of them empty (§11 C5).
* *"CONSTRUCTION ORDER - … nothing enforces it but that rule"* — **no longer true.** Since task 11
  a `static_assert` in the header pins `m_storage < m_movementStaticDataCVars < m_staticData <
  m_reconciliation < m_inputResolution < m_netSync`, and was seen to fire on three poisoned orders.
  The list above also omits `m_movementStaticDataCVars`, which `m_staticData` reads (§2).
* *"the section marks below are that document"* — the header has no section marks any more; this
  section and §12 are where its prose went.
* The CROSSING table's count is **three** (a write and two reads). §1's opening sentence said two
  until task 11 (§11 C16).
* *"FOUR SESSION KNOBS … each with an unconditional Warning proof line"* — **false for two of the
  four** (§11 C18). The relay-delay floor's proof line is at `Log` and only runs when the ini sets
  the key; the retired ring-depth key has no line at all.
* *"`[Resim.` inherits LogOGSim=Verbose"* — at `b9f6d81` `Config/DefaultEngine.ini` sets
  **`LogOGSim=Log`** (§11 C17). The argument — a family filed under `[Resim.` cannot be switched
  independently of `LogOGSim` — does not depend on the level and stands.

⛔ **The rules this block states are enforced where they are typed, not here.** *"⛔ DO NOT WIDEN
ONE INTO AN ACCESSOR"*, *"⛔ ONE CATEGORY PER PROBE FAMILY"* and *"⛔ NOTHING ELSE CROSSES"* have
no single site in the header — the wrong edit could be a new member anywhere in the class, or a
router arm in the `.cpp` — so they carry no tag (§1, §4). *"⛔ both go through one recompute"* does
have a site, and it is `⛔G-16` on `recomputeAndPublishEffectiveInputDelay`.

---

## §1 Threads, and why every entry point here is narrow

`ASimulationManagerUImpl` straddles two threads and owns the boundary between them.

| | runs on | what it is |
|---|---|---|
| `BeginPlay` / `EndPlay` | GAME | the composition root |
| `onConnectionTierReceived` / `Replayed`, `onRelayDelayFloorReceived` / `Replayed`, `onInputRelayHostReady` | GAME | the four replication listeners |
| `deliverRemoteInput`, `relayRemoteInput`, `noteDelayedInputComponent` | GAME | the transport sinks, driven from the RPC receipt path |
| `InjectInputs_External` → `releaseDelayedInputsForStep` | GAME | the drain, the reap and PROBE A |
| `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`, `OnPostSolve_Internal`, `ProcessInputs_Internal`, `TriggerRewindIfNeeded_Internal`, `FirstPreResimStep_Internal`, `ApplyCorrections_Internal` | PHYSICS | the six Chaos hook overrides — `ProcessInputs_Internal` and `ApplyCorrections_Internal` are empty (§11 C5) — and everything the core `SimulationManager` runs beneath them |
| `pollInputHistory`, `pollInputHistoryLanes` | GAME | the input-history display's render-rate feed, driven from `USimmableUpdateComponent::TickComponent` |

**There are three crossings, and they are not alike.** One is a write of one scalar; the other
two are reads — of a whole capture here, and of the ring-out score table below — and each is an
accepted tear rather than a synchronized access. *(This sentence said "two crossings" until task
11; the ring-out push added the third in ring-out task 5 — §11 C16.)*

**The write crossing carries one scalar.**
`publishClientEffectiveInputDelayTicks` writes a lone `std::atomic<int32>` inside
`SimulationInputResolution`, which `SimulationInputResolution::collectInputAll` loads once per tick on the physics thread.
The worst a race can do is apply a new delay one tick late.

**Everything else on this class is game-thread-only and has no internal synchronization:**
`m_receptionCoordinator`, `m_frameHealthProbe`, `m_relayWriteProbe`, `m_connectionBudgetProbe`,
`m_inputHistory` and `m_delayedInputComponentsById`. `m_manager->editResimGateProbe()` is the mirror case on the
other side: physics-thread-only, and its correctness rests on nothing else touching it.

### The read crossing — `pollInputHistory`, and why its tear is accepted

`getLocalInputCache` opens a game-thread door onto `m_localInputCaches`, whose *contents* are
written on the physics thread by `SimulationInputResolution::collectInputAll` (`LocalInputCache::push`)
and by `wipeAllForResync` (`LocalInputCache::clear`). Unlike its neighbour `getLastRelayedInput`,
this reader is **not** same-thread with that writer. `pollInputHistory` is the only caller that
**dereferences** it, so the argument belongs here rather than at the door.
`isLocallyControlledOnThisPeer` also calls it, but only compares the pointer with `nullptr`, which
reads no slot byte (§11 C6).

**What is *not* racing, and it is the larger half.** The `std::unordered_map` that holds the lines
is only ever restructured on the **game thread** — `registerLocalCharacter` and
`unregisterCharacter` run from registration, which the peer's own thread roster records as
game-thread in full. The poll's `find` is therefore same-thread with the only writer of the map's
structure, and the container cannot rehash under it. `LocalInputCache`'s slot vector is sized once
in its constructor and never resized, so the storage a read lands in cannot be reallocated either.
**The exposure is confined to the bytes of one slot**, not to the container that holds it.

**What can tear, and what the worst observable consequence is.** A slot is
`{ tick, occupied, input }`, and `push` writes those in that order, so a game-thread read can
observe a new `tick` beside the previous occupant's payload — or a payload half-updated. A
`simulatableBrawler::PlayerInput` is several vectors rather than the single byte
`SlotStateProvenance` rides the same unsynchronized access with, so the precedent is cited but
**not borrowed**. The worst outcome is that **one capture tick is classified from wrong values**:
one row of a diagnostic panel briefly shows the wrong direction glyph, button mask or action id.
Because the row fold is idempotent, a wrong classification does not self-heal — it persists as a
mis-drawn or mis-split row until that row scrolls out of the 64-row ring.

**Why it cannot corrupt the ring.** `InputHistoryRowRing::appendCapture` is keyed on the capture
tick **alone**, and the poll passes its own loop index, never a value read out of the slot. A torn
payload therefore cannot move a tick, cannot reach `++tickCount` twice, and cannot make a stale
tick rejoin an older row. The blast radius of a tear is the three *fields* of at most one row.

**Why it cannot reach the simulation.** The ring is client-local: it is never replicated, never
enters a correction payload and never reaches `compute_checksum`. The poll's route into the core is
`const`-only in both directions — `getLocalInputCache` returns a pointer to `const`, and both
reconciliation seams are `const` members reached through a `const` reference (`slotStateProvenance`
on the diagnostics view, `getAppliedCaptureTickRef` on the peer itself) — so there is no write path
back at all, and nothing downstream of the ring decides anything.

**Why it cannot fault.** A torn read yields wrong values, never a wrong pointer: a
`static_assert` at the poll pins `simulatableBrawler::PlayerInput` trivially destructible (it owns
no memory) and the sub-input the display actually reads trivially copyable. A member that owned an
allocation would turn this tear into a crash, and that assert is what stops one landing quietly.
*(The composite fails `is_trivially_copyable` through `std::tuple`'s implementation alone, not
through any member of its own — `std::tuple<int, float>` fails the same trait identically.)*

**And it is bounded in time.** The read is diagnostics-only and gated on one predicate at its call
site, so it can be switched off outright without reshaping anything.

**The second caller shares this crossing and heals faster than the first.** `pollInputHistoryLanes`
reads the physics-written correction cache through the same two `const` seams, fronted by
`ReconciliationSlotReader` so both are asked at one simulation tick. Its worst outcome is one wrong
*cell*, not one wrong row — and unlike the row fold, which is idempotent and therefore keeps a bad
classification until it scrolls away, a provenance cell is **rewritten on every poll while its tick
is still resident**, so a torn lineage byte is corrected on the next frame. The machine-state lane
crosses nothing at all: its sample is read at the caller's own visualization site, from state the
block-prediction visualization on the line above already holds.

**A third feed inside the same passthrough, and it crosses NOTHING at all.** The input-delay
decomposition `pollInputHistoryLanes` computes for `OGBrawler.InputHistoryInputDelay` reads
`m_replicatedTierConsumer` (GAME-THREAD-written, §5), `m_manager->getTimeConfig()`
(GAME-THREAD-bound at `BeginPlay`, mutated only by the OnRep listeners) and
`m_inputResolution.getClientEffectiveInputDelayTicks()` (the atomic the WRITE crossing above
publishes — GAME-THREAD-written, and this reader is on the SAME thread as that writer, not the
physics thread that only loads it). `pollInputHistoryLanes` itself already runs on the game
thread. Three same-thread reads of three game-thread-written values is not a crossing, so the
CROSSING table above names it in the READ row's prose rather than adding a third bullet, and
this paragraph is the same-thread argument that entry names. ⛔ **NO NEW PUBLIC ACCESSOR was
added for any of the three** — the tier consumer and the TimeConfig pointer were already public
(`getReplicatedTierConsumer`, `getTimeConfigPtr`), and the resolution peer's atomic is read
through the SAME private member this method already touches for the offset.

**The presence test crosses nothing either.** `ReconciliationSlotReader::hasCorrectionCache()`
asks `m_reconciliation.findCorrectionCache<T>(id) != nullptr` through the same reader the two
diagnostic seams already share, inside the same `pollInputHistoryLanes` call; the map it
inspects is mutated on the GAME THREAD alone (`createCacheFor`/`removeCacheFor`, from the
registration facade), never the physics thread, so this is a same-thread read like the three
above it, not a fourth crossing.

### The second read crossing — the ring-out score push, and why its tear is the same one

*(ring-out initiative task 5, 2026-09-13)*

`ASimulationManagerUImpl::OnPostPhysicsStep` now copies each character's ring-out score out of
`brawlerRingout::ScoreSystem` — reached through `m_systemsExec`, on the **game thread** — and
onto that character's replicated `RingoutScore` property. `brawlerRingout::ScoreSystem`'s
`postIntegrate` writes that table on the **physics thread**, beneath `onGameSimulation`. The two
are not same-thread, so this is a crossing and the CROSSING table (§0, formerly the header's
banner) gains a bullet.

⭐ **It gets a bullet where the clock's seven diagnostic reads deliberately did not**, and the
difference is worth stating so the precedent is not misread. Those were *more reads inside an
entry point already on the list* (`pollInputHistoryLanes`), of words the argument above already
covered. This is a **different member** — `m_systemsExec` — read from a **different call site**,
neither of which the existing argument had ever looked at. A crossing table that silently absorbs
new members is a table nobody can trust.

**Why the tear is accepted, and it is the same two-part argument, not a new one.**

* **The container cannot be restructured under the reader — the larger half, exactly as for
  `pollInputHistory`.** `ScoreSystem`'s table is a `std::unordered_map`, and its only inserting
  and erasing calls are `onCharacterRegistered` and `onCharacterUnregistered`. Both are driven
  from `ASimulationManagerUImpl::tryRegister` and
  `ASimulationManagerUImpl::unregisterFromNewFramework` — through `notifyCharacterRegistered` /
  `notifyCharacterUnregistered` — and this class's own thread roster records both as game-thread
  in full. The push's `scoreOf` is therefore same-thread with the only writer of the map's
  structure, and the container cannot rehash under it.
* **What can tear is one word, and the consequence is bounded.** The award reaches an entry that
  already exists and adds to a `uint32_t`. A naturally-aligned four-byte load on x64 cannot tear,
  so the worst observable outcome is that **one character's score is read one tick stale** and a
  scoreboard row shows the previous number for one frame. The next pass corrects it, and the
  value is cosmetic by construction — it reaches no integrator, no reconciler and no wire but its
  own `UPROPERTY`.

⛔ **THE FIRST BULLET HAS A PRECONDITION, AND IT IS THE ONE THING TO PROTECT.** `postIntegrate`
awards through `operator[]`, which **inserts** for an id the roster has not got — a deliberate
choice, so that an award to an unseeded id is a real award rather than a silently dropped one.
An insert can rehash, **on the physics thread**, and that would break the first bullet outright
rather than degrade it. It is unreachable in a legal session for a reason that is structural and
not merely likely: `onCharacterRegistered` seeds every authority-registered id, and it is called
from `tryRegister` **before** that function returns `TryRegisterStatus::Ready`, which is before
the route entry the push walks is registered at all. The push carries a `checkf` stating exactly
that, so the day the ordering changes, a Development build says so instead of racing.

⚠ **A future system added to `m_systemsExec` does NOT inherit this argument.** It holds for
`brawlerRingout::ScoreSystem` because of that type's own container discipline, not because
`m_systemsExec` is safe to read. Anything else read from there needs its own paragraph here.

### Why the passthroughs are narrow rather than `edit*()` accessors

`requestInputDelayIncreaseStall`, `publishClientEffectiveInputDelayTicks`, `getLastRelayedInput`,
`getLocalInputCache`, `pollInputHistory`, `getInputHistoryRows`, `pollInputHistoryLanes`,
`getInputHistoryLanes`, `noteResimRequest` and
`noteResimGrant` are one-purpose entry points. Each of the objects behind
them is otherwise driven **exclusively** by the core `SimulationManager`'s tick loop. Handing
game-thread `UObject` code a general mutable handle to one of them would invite exactly the
cross-thread reach the entry point exists to bound. **Do not widen one into an accessor.**

⚠ **This list has now gone stale twice**, both times because a new member matching the pattern
exactly was added without extending the sentence. The rule it states is about the *class* of
member, not about these ten names: **every public member of `ASimulationManagerUImpl` that
forwards to one core-owned object for one purpose belongs here**, and the enumeration is a
reading aid. Replacing the list outright with that sentence would end the drift, at the cost of
the ten anchors `doc_anchor_lint.ps1` currently resolves through it — a trade worth making only
if the lead wants the anchors spent elsewhere.

`getInputHistoryRows` and `getInputHistoryLanes` both return a pointer to `const` for the same
reason: the panel that draws the rows, and the bars that draw the cells, must not write one.

`requestInputDelayIncreaseStall` additionally guards on `runsPrediction()`: a server or
standalone manager has no client clock at all, and `getClientClock()` would `std::terminate`.

`pollInputHistoryLanes` guards on the same predicate for the same reason, around the one
line that reads `getNetworkEstimator()`: an authority manager has no prediction offset, and
it passes `std::nullopt` rather than a zero, because a display told "the offset is 0" would
draw an authority marker on the newest cell. It guards the input-delay decomposition the same
way, on `m_replicatedTierConsumer.has_value() && m_manager.has_value()`, and for the same
reason: a role with neither has nothing to decompose.

⛔ **THE LIST ABOVE IS UNCHANGED BY THE INPUT-DELAY DISPLAY, AND THAT IS THE POINT.**
`pollInputHistoryLanes` already names the one entry point this display reaches through; its
delay decomposition is three more same-thread reads INSIDE that existing passthrough, not a
new one beside it. Widening what one narrow entry point does is exactly what "narrow" is
meant to allow — it is adding a second *name* that would have been the drift this banner
warns about.

⛔ **THE CLOCK READING ADDS NO NEW CROSSING CLASS, AND THIS SAYS SO RATHER THAN LEAVING IT
SILENT.** The `ClockDriftReading` the same passthrough builds for the frame meter's clock line
(`InputHistoryDisplay-rationale.md` §7.12) is the SAME PAIR of postures argued two paragraphs
above for the prediction offset, not a third thing. `getTargetPredictionTick` and
`getLastAuthorityTick` read `NetworkTimeEstimator` state its own contract declares
GAME-THREAD-written, by `updateRTT` and `recordAuthorityTick`, and this reader is on that
thread — same-thread, nothing to tear. `getPredictionTick`, `evaluateDrift` and
`getRequiredInputDelayIncreaseStallTicks` reach the physics-written prediction tick and stall
debt, which is the accepted tear already taken: a naturally-aligned four-byte load on x64
cannot tear, so the worst case is one line of diagnostic text one tick stale. **The CROSSING
table gains no bullet and the NARROW PASSTHROUGHS list gains no name** — `pollInputHistoryLanes`
was already on it, and this is more reads inside that one entry point.
⛔ It takes `runsPrediction()` for the harder of the two reasons: `getClientClock()` does not
return a wrong number on a role that does not predict, it calls `std::terminate`.

⛔ **THE CLOCK'S EVENT SEAM IS SEVEN MORE READS INSIDE THAT SAME PASSTHROUGH, AND STILL NO
NEW CROSSING CLASS.** `skipCount`, `lastSkipTick`, `stallCount`, `lastStallTick`,
`hardResyncCount`, `lastHardResyncFromTick` and `lastHardResyncToTick` are plain
physics-written words on `ClientPredictionClock`, taken under the SAME `runsPrediction()` guard,
in the SAME call, for the SAME display that decides nothing. Each is a naturally-aligned
four-byte load that cannot tear, so the accepted tear argued above covers all seven and **the
CROSSING table gains no bullet and the NARROW PASSTHROUGHS list gains no name**.
⛔ **THEY ARE READ THROUGH `getDiagnostics()`, NEVER THROUGH A BARE ACCESSOR** — the clock
deliberately publishes none, so the display cannot acquire a second, unfenced route to the same
words.

⭐ **THE COUNT OF EACH PAIR IS READ BEFORE ITS TICK, AND THE ORDER IS THE ARGUMENT.** The clock
writes them the other way round — tick, then count — at every site in `advancePrediction`. Two
opposed orders make one of the two possible straddles unreachable: a reader that has already
seen the incremented count is, by the writer's program order, past the tick-write as well, and
its tick-read comes later still. ⛔ **A FRESH COUNT BESIDE A STALE TICK CANNOT BE OBSERVED**
here. What can is the reverse — a stale count beside a fresh tick — and it costs nothing: the
poll's difference is zero, so it files nothing and the next poll picks the event up whole.

> ⚠ **R0, task 11 — the paragraph above overclaims (§11 C14).** Both sides of each pair are plain
> `unsigned int`s: the writes are not releases and the reads are not acquires, so C++ promises no
> ordering between them, and an optimizing compiler may reorder either pair. Program order makes
> the harmful straddle *unlikely*; it does not make it *unobservable*. The read order is still
> worth keeping — reversing it makes the harmful case the expected one — and it is `⛔G-09` in the
> header. A guarantee would need acquire/release atomics in `ClientPredictionClock`, which is
> `og-simulation`'s to change.
⛔ **DO NOT REVERSE THE READ ORDER.** It is not a style choice: reading tick-first makes the
unreachable case reachable, and that case is the one that would attribute an event to a tick
that predates it — a wrong answer rather than a late one, on the one crossing in this file whose
consumer draws a position from the value.

⭐ **The offset is read at the POLL, not at the draw, and that placement is the whole point.**
The lane axis is built from the `liveTick` this same call is given, so pairing the offset
with it there makes the authority marker and the axis it is measured against one snapshot by
construction. An earlier version handed the display a `const` accessor that read the
prediction tick a second time at HUD draw time; a physics step landing between the two reads
moved the marker a column while its printed offset held, and the accessor is gone rather than
tolerated. ⛔ Do not reintroduce a clock accessor for a display: give the display the poll's
  reading.

### One acknowledged wart

`getServerReceptionTick()` reads `m_serverClock` through `getServerClock()`, which is written on the physics thread, from
the game thread with no synchronization. The RTT sample path has always tolerated this and the
tier EMA is insensitive to a one-tick skew. It was left as-is so that relocating the reception
subsystem into the core stayed a pure relocation; a follow-up may source it mapper-derived, the
way the drain already does.

---

## §2 Construction, ownership and teardown

**Declaration order in the header IS construction order**, because C++ constructs class members
in declaration order regardless of initializer-list order:

```
m_storage → m_movementStaticDataCVars → m_staticData → m_reconciliation → m_inputResolution → m_netSync
```

*(`m_movementStaticDataCVars` was missing from this list and from the header's banner until task
11. `m_staticData`'s initializer reads it, so it is part of the chain.)*

then `BeginPlay` emplaces, in order:

```
m_physAdapter / m_physReaderAdapter / m_queryAdapter → m_integrationLayer → m_manager
    → m_replicatedTierConsumer   (borrows m_manager's TimeConfig)
    → m_receptionCoordinator     (borrows m_manager's TimeConfig, authority only)
```

`EndPlay` unwinds the last two **before** `m_manager`, because both hold `const TimeConfig&`
into it.

### The reorder hazard, stated exactly

**Until task 11** nothing enforced the peer ordering but the language rule, and this section said
so: *"There is no `static_assert` and no `-Wreorder`-as-error anywhere in this tree's `.Build.cs` /
`.Target.cs` files. A future edit that reordered `m_reconciliation` / `m_inputResolution` /
`m_netSync` while leaving the initializer lists textually unchanged would compile silently and
construct in the new, wrong order."*

**Since task 11 a reorder does not compile.** `ASimulationManagerUImpl::compositionContractsHold()`
asserts, over `__builtin_offsetof`, that `m_storage < m_movementStaticDataCVars < m_staticData <
m_reconciliation < m_inputResolution < m_netSync` in layout. All six are private members, and
members of one access level are laid out in declaration order, which is construction order. Task
11 poisoned three reorders (the cvar result below `m_staticData`; `m_netSync` above
`m_inputResolution`; `m_storage` below `m_reconciliation`) and each fired the assertion's own
message. There is still no `-Wreorder`-as-error, so members *outside* that chain are unprotected.

It has not bitten because every one of those three constructors is a **trivial reference store**:
none calls into a sibling peer during construction, so a reorder today would only bind a
reference to a not-yet-constructed-but-not-yet-*accessed* object. **It becomes load-bearing the
day a constructor BODY — not just its initializer list — calls a method on one of those sibling
references.** That is undefined behaviour for which this tree has no compiler diagnostic.

### `m_staticData` is the ownership root

`simulatableBrawler::StaticData` must be constructed in place and **never copied or moved**. Its
nested sub-`StaticData` members — `m_attackSimulationStaticData` and
`m_guardSimulationStaticData` — hold references bound to sibling members `m_attackSequences` and
`m_attackCircle`. A copy or move would leave those internal references dangling into the
moved-from original — which is why `StaticData` deletes all four copy and move operations
(`SimulatableBrawlerTypes.h`). A copy is a compile error, not a silent dangle: task 11 measured it
(`C2280`). Every downstream consumer (the integration executor, the
`SimulationManager`) therefore takes it by `const&` only; there is **no by-value `StaticData`
path anywhere in the tree.** This invariant was migrated here from the now-deleted
`SimulationIntegrationExecutor::getStaticData()` site.

### The alias chain

`BrawlerSimulatables` is the single source of truth for the game's simulatable pack: widen that
one alias and every storage and executor type below it inherits the widening. The aliases are
**class-scoped**, not file-scoped, so these names cannot leak into the global namespace from a
widely-included adapter header. OGSim primitives are named unqualified because the whole OGSim
core lives in the global namespace.

`SimulationIntegrationExecutor`'s leading three parameters are engine- and game-specific, so
`apply_t` cannot unpack the pack marker into them; `BrawlerIntegrationExecFor_UE` fixes those
three slots first and `apply_t` then applies the pack, preserving the single-source-of-truth
property.

`getTimeConfigPtr()` returns a **pointer, not a reference**, precisely so that the
pre-construction state is representable rather than undefined behaviour. Callers emplace lazily
and retry.

---

## §3 The session knobs

Three ini keys are read at composition, in `BeginPlay`, through the only `GConfig` uses in this
codebase. **Every one takes the same four steps:**

| step | what | why it is not optional |
|---|---|---|
| 1 INTAKE | read the ini once, before the manager exists where possible | a value that arrives after the first publish has to be absorbed as a *change* |
| 2 CLAMP / VALIDATE | out of range is **reported**, never silently corrected | a floor that big is a sizing mistake the operator needs to see |
| 3 SET | stamp the effective value into the one shared `TimeConfig` | every derivation site must read one number |
| 4 PROVE | an **unconditional** `Warning` line naming the value actually stored | see below |

**Step 4 is at `Warning`, not `Log`**, because `LogOGNet` is meant to run at `Warning` — the
ini's own comment calls that its default — and a `Log` line does not exist at `Warning`. *(This
sentence said `Config/DefaultEngine.ini` "sets `LogOGNet=Warning`" until task 11. At `b9f6d81` it
set `LogOGNet=Verbose`, an investigation setting committed. The reason for `Warning` is unchanged
— §11 C17.)* ⚠ **Not every knob meets step 4:** the relay-delay floor's proof line is at `Log` and
inside the key-present branch (§11 C18). Both ini homes are accepted for
every key — `DefaultGame.ini` first, then `DefaultEngine.ini` — because the former is the
conventional place for a gameplay-tuning value and the latter is where this project's other
netcode settings already live. **And it is unconditional**,
because a line that is absent both when the key was read and when it was not cannot distinguish
those two cases — which is the whole question a proof line exists to answer. Each proof line
reports values **read back from `TimeConfig`**, never the parsed request, so it cannot claim a
setting the manager did not store.

**None of these may become a cvar.** The rotation width, because a run's probe output is read
*against* the cadence and a value that moves mid-run makes those readings unattributable. The
resim policy, because it is pushed into every `StateCorrectionCache` and read on the game thread
at the correction-landing site **with no synchronization** — which is sound only because it is
written once at composition, before any correction can land.

### `RelayDelayFloorTicks` — the session floor

**Authority only, and that is correctness rather than economy.** The floor is server-owned
session state that is *replicated* to clients. A client reading its own ini could disagree with
the server, which is the two-ends-diverge failure the whole tier ruling exists to avoid.

The clamped value is stamped into `TimeConfig` and then published on the session timing relay,
which the composition root (`USimulationManagerSubsystem::OnWorldBeginPlay`) spawns BEFORE this
manager precisely so that write has somewhere to land: reaching the failure branch there means no
client will ever learn a nonzero floor.

Two intake points share one clamp: this ini override, and `onRelayDelayFloorReceived` where the
value arrives off the wire as a `uint8` and is clamped rather than trusted (a corrupt or
version-mismatched byte must not schedule a read past the point where the delay line has already
evicted the capture). The setter clamps again; the call sites exist so an out-of-range value is
*visible* in the log rather than silently corrected.

`logRelayDelayFloorAdvisory` is **advisory only, never an assert**, and is called from both
intake points. Floor `0` is the documented "scheduled regime OFF" mode, so
`classifyRelayDelayFloor` (`Network/ConnectionTierTable.h`) never flags it; that function carries
the full classification table.

### `CorrectionRotationK` — the state rotation width

Controls how many characters' correction-state buffers `SimulationNetSync::sendCorrectionAll`
writes per tick, round-robin, so each character's state replicates at `60 * K / N` Hz. A
character not written is not dirty and costs zero bytes, so this is a real wire saving rather
than a deferred write. It turns an emergent cadence into a decided number: at N characters the
per-character correction rate should read `60*K/N` in `[DivergenceProbe.Window]`.

**Authority only, but NOT for the floor's reason.** `K` is never replicated at all — only the
authority sends corrections, and a receiver reconciles against whatever arrives without needing
to know the sender's cadence. A client read would have no reader.

> ⚠ **The compiled default is `TimeConfig::correctionRotationK` and this document does not
> restate its value.** See §11: the `.cpp` comment restated it, the value moved, and the comment
> did not. Read it at the declaration.

**Sentinel collision, harmless and knowingly.** An ini that literally reads
`CorrectionRotationK=-1` cannot be told from an absent key, because `-1` is also the intake
variable's own "not present" sentinel. The proof line reports the effective number either way.
(`-1 == absent` is the intake variable's own convention, not the field's.)

### `ResimTriggerPolicy` — the resim-gate policy

Decides which landed corrections open the resim gate: `FrontierExact`, the compiled default,
which reproduces the legacy gate; or `OnDisagreement`, the designed trigger. The field lives on
`TimeConfig` (`PCTimeManagement/TimeConfig.h`); the mechanism is `OGSimulation/ResimGatePolicy.h`
and `StateCorrectionCache`. The shipped configuration is set by the ini key, currently
`ResimTriggerPolicy=OnDisagreement`.
**`TimeConfig::resimTriggerPolicy`'s own declaration states the compiled default and the shipped
configuration as two separate facts, and they differ — read them there.**

**There is deliberately no `ResimCooldownTicks` key.** A trigger-rate ceiling was built here and
removed on a user ruling: it defers acting on a correction *already known to disagree*, which is
the defect this mechanism repairs. The throttle is structural instead. If you are here because a
design document names that key, the code is right and the document predates the ruling.

**Not authority-gated, unlike the two above, and that is the point rather than an oversight.**
The resim gate exists only on a predicting client — an authority allocates no correction caches
and never rewinds. Gating this intake on authority would read the ini on the one role that cannot
use it and skip it on the role that can. Applying it on both roles keeps one `TimeConfig` shape
and costs nothing; on a server the value simply has no reader.

**Presence is a bool, not a sentinel value.** The value is a string, so there is no numeric
sentinel to collide with, and `GetString` already reports presence. The step-2 parse is
case-insensitive because an ini is hand-written, and an unrecognised policy string is **reported
with the compiled default kept**: a typo that quietly selected the other value would change gate
behaviour on a build nobody thinks they changed.

For this knob the proof line is also the **behaviour-neutrality receipt**. The gate ships
defaulted to reproduce the old one, so every claim made from a later run — "the ratios are
indistinguishable from the baseline" — is a claim about *which policy was live*. Without that
line in the log it is an assertion about the source rather than an observation about the run.
`depthPolicy` therefore carries `(inert under FrontierExact)` rather than falling silent, and
`rateLimit = none (structural)` is stated rather than omitted, because the absence of a cooldown
is a ruling: a reader must be able to tell "the ceiling is off" from "this build predates it".

### The retired fourth knob

There was a fourth key: a session-configurable **relay-ring retention depth**. The bare-C1
flush-on-poll write path replaced the mechanism it sized — the stage capacity is
`relayedInputRing::kMaxDepth`, a compile-time constant with no ini key to feed — and the key, its
clamp intake, its setter and its `[RelayDepth]` startup proof line were removed together. Guard
**G-56** marks where the intake used to sit in the `.cpp`. It replaces the two ⛔ RETIRED prose
fences that marked the intake and the apply block. **It is an absence fence:** the identifier it is
about occurs nowhere in this tree, so there is no symbol to grep and the guard is the only record.

---

## §4 Log categories and `RouteOGMessage`

`RouteOGMessage` routes one SIMLOG string to one `LogOG*` category by its leading `[Tag]`.

**Tag-matching runs against `body`** — `fmsg` minus a leading `[Verbose]` or `[Warning]` token,
both nine characters — while the **full** `fmsg`, severity token included, is what is logged. So
one message can carry both a severity escalation and a routable tag.

### Order is load-bearing

`[Resim.Input]` must be matched **ahead of** the `[Resim.` catch-all, because the catch-all is a
prefix of it. `[Resim.Input]` is a per-character-per-resim-tick line — the same volume class as
`[CollectInput]` — and it landed in the rare-lifecycle bucket purely by inheriting the `[Resim.`
prefix from `[Resim.Pre]` / `[Resim.Post]`, which really are rare, while `LogOGSim` shipped at
Verbose. So the highest-frequency line in the system was filed as a lifecycle event and printed
by default.

It was **re-routed rather than renamed.** Both were available, and renaming would have silenced
it just as well — but `[Resim.Input]` is the name the impl notes, the PIE scripts and the
spectrum design all use when they tell an operator what to grep for. A rename invalidates every
one of those instructions for a cosmetic gain. Re-routing keeps the string an operator already
knows and changes only which knob controls it: `LogOGSimTick=Verbose` now shows the resim
table, exactly as it shows `[CollectInput]`, which is routed there under the comment
*"dominates log volume"*.

### Three probe families, three categories

`LogOGRelayProbe`, `LogOGDivergenceProbe` and `LogOGResimProbe` each own a category, and the
reason is the same all three times: **both severities ride one tag family** — per-window
summaries at `Warning`, per-event detail at `Verbose` — so its own category is the only thing
that lets `LogOGRelayProbe=Warning` (the shipped default) keep the summaries while dropping the
detail, and `LogOGRelayProbe=NoLogging` drop both, without disturbing any other channel. The
same holds for `LogOGResimProbe=Warning` and `LogOGDivergenceProbe=Warning`. Routed to `LogOGNet` instead, a family would be
inseparable from the whole replication bucket; left unrouted, its Verbose half would fall to
`LogOG=Warning` and be unreachable with no way to turn it on.

**No probe family may be filed under `[Resim.` or `[ResimCheck.`.** The first inherits
whatever `LogOGSim` is set to — the probe could not be switched on its own. *(This said "inherits
`LogOGSim=Verbose`" until task 11; `LogOGSim` shipped at `Verbose` when T19 measured it, and is
`Log` at `b9f6d81` — §11 C17.)* The second is split across `LogOGSim` and `LogOGSimTick`, so a family filed
under it could not be switched on or off as one thing — which is why `[ResimCheck.IsSimilar]`
has zero occurrences in every log on disk.

`[ResimProbe` sits ahead of the `[Resim.` catch-all **defensively**. It does not strictly need
to: `[Resim.` requires the dot and `[ResimProbe` has a `P` there, so the two cannot collide
today. It sits there because the one thing that would make them collide is somebody widening the
catch-all to `[Resim`, and the consequence would be silent.

**One `StartsWith` covers each family** — the router tests `body.StartsWith("[ResimProbe")` and
its two siblings — so a future sub-tag needs no router edit.

⭐ **Since task 12 the order is checked by the compiler, not by this paragraph.** The router is a
`constexpr` table, `kOGLogRoutes`, walked in order. A `static_assert` fails the build if any
prefix begins with an earlier one, compared case-insensitively as `FString::StartsWith` compares.
It was seen to fire for `[Resim.` moved above `[Resim.Input]` and for `[Resim.` widened to
`[Resim` (§13.9).

`[RelayProbe.Frame]` is the relay family's original **server-side** member. `[RelayProbe.Write]`
(PROBE 5) and `[RelayProbe.Budget]` (PROBE 6) are server-side too (§13.10, C13-3). It shares that
category deliberately: it measures the *cause* of the cadence `[RelayProbe.Arrival]` measures the
*effect* of, and the two are only interpretable together, so one knob should turn both on.

`[DivergenceProbe.*]`'s signal is not new — `StateCorrectionCache::tryInsertingCorrectState` has
computed the verdict on every correction for as long as that method has existed. What was missing
was a **route**: the cache's own line carries no tag, so it lands on the `LogOG` fallback at
`Log` severity and `LogOG=Warning` suppresses it. The router branch is the whole of that fix.

### The field-diff cost gate — this adapter's half

Routing that family is only half of what `LogOGDivergenceProbe` decides here. Since
og-netcode-v2-field-defects task 6 the correction line carries a TAIL naming the first
disagreeing FIELD and its magnitude, and producing that tail costs a per-field walk of the
whole composite — roughly ten times the boolean fold it re-asks. **og-simulation ships that
walk closed**: its enabling predicate is an empty `std::function`, so a host that installs
none walks no field on any correction, ever. That default is a property of the core, not of
this adapter, and it is what keeps every other game that links the submodule paying nothing.

`bindCorrectionFieldDiffGate` is the only thing that opens it in this project. It is called
on **both** the authority and the client branch, beside the `setGlobal` sink installs, and
what it installs is:

```
correctionFieldDiff::setEnabledPredicate(
    []() { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
```

**What it guarantees.** At `LogOGDivergenceProbe=Warning` the predicate answers false (⚠ at
`b9f6d81` the committed `Config/DefaultEngine.ini` sets this category to `Verbose`, so the walk is
live there; see §13.10, C13-1); `tryInsertingCorrectState` tests it as the last of three `&&` operands, *before* the
walk's arguments are formed, so `describeFirstDivergingField` is never entered and
`correctionFieldDiff::walkCount` stays at ZERO across a run of disagreeing corrections. The
`[DivergenceProbe.Correction]` line is then byte-for-byte its pre-task-6 self and the tail
costs nothing. At `Verbose` the walk runs once per disagreeing correction and the line gains
`field=` and a magnitude. Both halves are asserted, not argued, in
`Network/CorrectionFieldDivergenceTest.cpp` — including the anti-vacuity arm in which the
predicate counts its own invocations, so "zero walks because the gate was never consulted"
cannot be mistaken for "zero walks because the gate was shut".

**A predicate, not a latched bool, and that is why it is a lambda.** `UE_LOG_ACTIVE` re-reads
the category's CURRENT verbosity, so `LogOGDivergenceProbe Verbose` typed into the console
mid-session starts naming fields on the next correction, and `Warning` stops it again.
Reading the verbosity once at BeginPlay would pin the session to whatever the ini said.

⛔ **It must name the same category, at the same verbosity, that `RouteOGMessage` sends the
`[DivergenceProbe` prefix to.** Re-route that prefix without moving this predicate and the
adapter either pays for a walk whose line is then dropped, or drops a walk whose line is
printed. This predicate and `RouteOGMessage`'s `[DivergenceProbe` arm are the only two sites in
code that USE this category — one reads its verbosity, the other logs to it; the `DECLARE`, the
`DEFINE` and the ini value are the only other mentions. Change one and you must change the
other. Guard **G-50** sits on the predicate and says so.

⚠ **Not yet confirmed in PIE.** The binding compiles and the gate is unit-tested against a
stand-in predicate; that the live `UE_LOG_ACTIVE` answers as expected under PIE is unverified,
so the first `Verbose` run should check that `field=` actually appears before any conclusion
is drawn from its absence.

### The tag families, in full

The source names each family by its prefix; the members are:

| category | tags |
|---|---|
| `LogOGRelayProbe` | `[RelayProbe.Read]`, `[RelayProbe.Arrival]`, `[RelayProbe.Stale]`, `[RelayProbe.Miss]`, `[RelayProbe.Delta]`, `[RelayProbe.Frame]`, `[RelayProbe.Write]`, `[RelayProbe.Budget]` |
| `LogOGResimProbe` | `[ResimProbe.Gate]`, `[ResimProbe.Chaos]`, `[ResimProbe.Apply]`, `[ResimProbe.Landing]`, `[ResimProbe.Request]`, `[ResimProbe.Stranded]`, `[ResimProbe.Session]`, `[ResimProbe.Frame]`, `[ResimProbe.PushTarget]`, `[ResimProbe.PushVerdict]`, `[ResimProbe.SlotMap]` |
| `LogOGDivergenceProbe` | `[DivergenceProbe.Correction]`, `[DivergenceProbe.Window]` |

`[RelayProbe.Miss]` says *why* each miss missed — an in-span coverage hole, asking above the
newest arrival, or below the oldest; `[RelayProbe.Delta]` is the signed probe-tick-to-newest
distribution; `[RelayProbe.Stale]` is the longest consecutive fallback run. `[ResimProbe.Chaos]`
is the request/grant/refusal ledger and the requested-vs-granted pin; `[ResimProbe.Apply]` is the
apply edge and the replay span; `[ResimProbe.Landing]` is the frontier-landing split;
`[ResimProbe.Stranded]` is the per-event Verbose companion to it.

---

## §5 The client's effective input delay

Two independent channels feed one number:

```
effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
                                 : rttTierInputDelays[kMaxConnectionTierIndex])
```

The pre-arrival case is therefore `max(floor, rttTierInputDelays[kMaxConnectionTierIndex])`. The
published value lands in `SimulationInputResolution`'s
`m_clientEffectiveInputDelayTicks` atomic.

The **floor** rides the session relay (`ASimulationTimingRelay`); the **tier** rides the
per-connection relay (`ASimulationConnectionRelay`). The two `OnRep`s are genuinely independent
and can land in either order, which is exactly why both call **one** recompute holding **both**
cached inputs rather than each writing the effective-delay atomic on its own: two writers each
holding half the formula would answer with a stale other-half whenever their `OnRep`s
interleaved. The composition root's baseline publish goes through the same site.

Both arms are evaluated inside `ReplicatedTierConsumer::effectiveInputDelayTicks`, which is the
single derivation the server's `ServerInputDelayQueue` mirrors — that shared helper is what makes
the two ends agree by construction rather than by coincidence.

`recomputeAndPublishEffectiveInputDelay` **returns the change** in published delay
(new − previous) so a caller that must pay for an increase can. A caller publishing a baseline
rather than reacting to a transition ignores it. `m_lastPublishedEffectiveInputDelayTicks` is kept
on this class rather than read back out of the net sync, because the delta is a game-thread
bookkeeping quantity: the atomic exists to hand one scalar to the physics thread, not to be used
as shared state. It is seeded to `0` so the composition root's first publish reports its full
value as the delta — which that caller deliberately ignores, since nothing has been predicted yet.

### The tier is server-owned

The **server is the sole owner of the RTT tier**. It derives each connection's tier from its own
per-connection ping reading and replicates the result to the owning client as
`ASimulationConnectionRelay::m_connectionTier`. The client never
computes a tier and never samples RTT for tier purposes — that is what makes client/server tier
disagreement impossible *by construction* rather than merely unlikely.

**One consumer per WORLD, not per CHARACTER.** A tier is a property of the wire, and every
character on a machine shares one wire. The retired per-character shape had N characters each
driving the *same* effective-delay atomic and each requesting its own tier-transition stall — so
for a couch co-op client, one wire transition produced two stall requests against a clock that
*accumulates* debt. One listener per world makes the request count match the transition count.
Single-character clients are unaffected.

`m_replicatedTierConsumer` is emplaced on **both** roles deliberately. A dedicated server never
reads the value it publishes there, but a **listen-server host does** — its local player's input
delay comes from that cache, and an unbound cache would answer `0` delay where the pre-arrival
no-tier fallback is correct. No tier ever reaches an authority world, because the server *writes*
the relay property and `OnRep` is a non-authority callback, so the authority cache stays at that
fallback for the whole session — which is exactly what the retired per-character path also
converged on.

### The preserved tier-0 quirk, and its two consequences

**The core never publishes tier 0 as a FIRST value** (`m_lastPublishedTier` is baselined at 0). A wire that never leaves tier 0 therefore
produces no publish, no relay actor and no call into `onConnectionTierReceived` at all: the
client sits on the pre-arrival no-tier fallback while the server parks at tier-0 delay. That
standing divergence is today's behaviour and is preserved deliberately — a later change *widened*
it as a side effect of the worst-tier ruling but did not introduce it and did not change its
cause. Changing the cause changes felt input lag and belongs with the floor work, not with a
transport migration.

**The second consequence of the same preservation** is the one that was a real defect. That
fabricated `oldTier = 0` used to reach `applyTierTransitionStall` as if it were the client's real
prior tier, requesting a spurious multi-tick prediction stall on the connection's **first** real
tier resolution, in the wrong direction for every `newTier` except `0`. It was fixed by giving
the stall decision `hadAnyTier` — whether the client had *already* received an authoritative tier
— as an explicit input rather than inferring "had a tier" from `oldTier`'s value, so a first-ever
resolution — `hadAnyTier == false` — requests zero stall regardless of which tier arrives. **The standing divergence above
is unchanged by that fix:** `hadAnyTier` only ever suppresses a stall.

`hadAnyTier` **must be captured before `applyReplicatedConnectionTier`**, because that call feeds
`m_replicatedTierConsumer`, whose `hasReceivedTier()` becomes true unconditionally as a side
effect. Reading it afterwards would make every call report "had a tier" — including the very
first one.

The decision itself lives in core, as `shouldStallForTierTransition`
(`Network/ConnectionTierTable.h`), a pure function with its own low-level test coverage — which
this UE-bound class has none of. `applyTierTransitionStall` is only the plumbing around it, and
non-positive results are dropped by the clock too, belt-and-braces.

### Replayed values never stall

`onConnectionTierReplayed` and `onRelayDelayFloorReplayed` apply and republish but **do not**
stall. For the tier, it is the first tier ever applied, so no tick was predicted against a
previous tier's delay, and its "previous" state is the pre-arrival no-tier fallback rather than a
tier at all. For the floor, the pull happens inside this manager's own `BeginPlay`, before the
first prediction tick.

A floor **rise** on the live channel is indistinguishable from an upward tier transition to the
client: the frontier must fall back by the difference, which the clock pays down as Stall ticks.

Both listeners **bind, then PULL**. Each property dirties only on change, so an `OnRep` that
fired before the bind would never be re-notified; the relay latches it and hands it over at the
pull. The timing relay is `bAlwaysRelevant`, so on a client it may legitimately already exist or
not — a missing relay simply means no floor has arrived, and the relay's own client `BeginPlay`
replays it if it turns up later.

### The zero input

`getZeroPlayerInput()` fills the `[0, effectiveDelay)` window at session start and after a hard
resync. **It is not `PlayerInput{}`** — `getZeroPlayerInput` builds `(0,0,1)` forward vectors —
so the injection is load-bearing rather than defensive.

It is set on the **authority** branch too, and that is not a precaution: a dedicated server reads
this value on every tick it substitutes an input for a remote character (the remote branch of
`collectInputAll` on a queue underrun) and seeds each character's replicated applied-input with it
at `registerAuthorityOwner` (the value itself lives in the resolution peer's `m_neutralInputs`). Deleting the call would make the authority simulate — and publish to
every peer — the zero-forward-vector input for the whole of every join window. `netSync` warns at
registration if the line ever stops running before it. **Ordering is load-bearing: it must precede
every `registerAuthorityOwner` call**, and registration happens per character, later.

### Inbound-hit routing

Cross-character inbound-hit routing runs **inside** `m_manager->onGameSimulation`, via
`brawlerHitRouting::System::postIntegrate`, fired by the systems executor after `integrateAll` on
every tick — including each resim replay tick — so the routed `HitFlinch` flags stay
deterministic. There is no adapter-side routing wrapper any more: the former `routeInboundHits()`
shim and its map were removed once the routing system owned the whole pass, and no reset-order
hazard remains because the single routing pass *is* the system's `postIntegrate`.

---

## §6 The relay ring

### The host boundary

`ASimulationInputRelay` lives in `OGSimulationUnreal`, which **must not depend on
`OGBrawlerUnreal`**, so it cannot name `AOGBrawlerUECharacter` or `USimmableUpdateComponent` and
cannot resolve its own owner to the component that consumes relayed input. This manager is the
bridge — and **only** the bridge: it holds no state for this channel and no per-arrival traffic
passes through it. Once the two objects are linked, ring `OnRep`s (`OnRep_RelayedInputRing`) go straight from the
host to the component's callback.

**No map, deliberately.** The resolution is host → `GetOwner()` → character → component: three
lookups on data that already exists, because `AActor::Owner` replicates and the authority spawns
the host with `SetOwner(character)`. A registry keyed by character would be a second structure to
keep in step with every spawn, death and travel, for nothing.

**Idempotent.** The host calls in from `BeginPlay`, from `OnRep_Owner`, and again from any ring
`OnRep` that arrives while still unlinked; `attachInputRelayHost` absorbs the repeats. Every hop
can legitimately fail during the join window, and every failure is a plain "not yet".

### The relay tap: stage, do not write

`relayRemoteInput` is the **outbound** half of the receipt path. Where `deliverRemoteInput` routes
an input *into* the simulation for the character that sent it, this stages the same
`(captureTick, dA, input)` for that character's replicated relay ring, which carries it to the
**other** clients so each peer simulates that character with its real input instead of
extrapolating. `dA` is the **schedule stamp** — the effective input delay the authority held for
that wire at receipt — so a peer derives the application tick as `captureTick + dA`.

Under bare-C1 flush-on-poll the arrival is **staged**, and the host actor's `PreReplication`
publishes the whole staged burst once per replication poll, so two arrivals in one server frame
both reach the wire instead of the second overwriting the first.

**No depth is read at that site any more, and that is the point.** It used to pass a
session-configurable retention depth into `writeLatest`. On the flush path that same value would
make every staged entry after the first supersede its predecessor, the ring would carry exactly
one entry per round, and bare C1 would silently become replace-latest again — with no compile
error and no warning. `stageRelayedInput` has no depth parameter; the capacity is
`relayedInputRing::kMaxDepth`, taken as a constant inside the codec. The machine-checked fence
that replaces the retired startup proof line lives in
`Source/OGSimulationTests/extern/og-simulation-tests/Source/OGSimulationTests/Network/RelayRedundancyDepthTest.cpp`.

The staging outcome is deliberately unchecked: `accepted` can only be false on the stale-write
arm, which the coordinator's monotonic `acceptedNew` gate makes unreachable from this call site,
and `droppedOldest` is already counted on the host.

**The dual-write fence is discharged.** The relay tap originally wrote only the ring, leaving
`SimulationNetSync::sendCorrectionAll`'s input write into `m_replicatedInputSyncedBuffer` running
beside it so no reader had to move and be moved back. The readers were then switched and that
second write — and the whole correction-input channel with it — was removed. This tap is now the
only path by which a character's input reaches other clients. The viz call site reads it through
`SimulationInputResolution::getLastRelayedInput`, whose nullopt contract was modelled on the
retired `editReconciliation().getLatestInput(...)` so `BrawlerVisualizationInputSource.h`'s
cold-source skip did not have to be re-specified.

> ⚠ **Which ring.** The replicated ring is `ASimulationInputRelay::m_relayedInputRing`, registered
> `DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing, COND_SkipOwner)`. It is
> **not** `USimmableUpdateComponent::m_detachedRelayRing`, which is the no-host fallback whose
> writes nothing replicates. See §11 — the header said otherwise until this pass.

---

## §7 The reception coordinator and the transport adapter

The whole server reception subsystem — the tier table, the delay queue, the claim map, and the
orchestration over them — lives in the engine-agnostic core `ServerReceptionCoordinator`. This
manager owns **one** instance of it and has shrunk to a thin transport adapter: it acquires engine
primitives (address, player slot, RTT, sim tick, wire decode) and forwards them in. **Authority
role only** — `std::nullopt` on a pure client, because the coordinator borrows `const TimeConfig&`
from `m_manager`'s owned config.

**Reception *policy* is in the core; primitive *acquisition* is at the RPC boundary**
(`USimmableUpdateComponent::ServerReceiveRemoteMove`), where the bundle and the owning actor
naturally live. Three accessors are all this manager still supplies — the coordinator instance,
the server sim tick, and the id→component delivery-routing registration — and **none carries
netcode policy.**

`sampleAndDeriveConnectionTier` and `tryEnqueueDelayedRemoteInput` are **gone**; their
engine-primitive acquisition moved up to that RPC boundary, which now forwards straight into
`ServerReceptionCoordinator::noteRttSample` (once per bundle) and
`ServerReceptionCoordinator::receiveInputBundle` (the whole per-slot loop). Nothing of
the RPC per-slot path remains manager-side.

### The two sinks

This manager satisfies both `RemoteInputDeliverySink` and `RemoteInputRelaySink`. Both are
compile-checked at the `receiveInputBundle` call site, and both are asserted again beside their
definitions in the `.cpp` so a breaking signature change surfaces legibly.

`deliverRemoteInput` is the **one** delivery method that both the per-id drain (built in
`releaseDelayedInputsForStep`) and the core receive-loop fallback (a malformed slot in
`receiveInputBundle`) route through. It resolves id → component through
`m_delayedInputComponentsById` and hands the input to the same inbound path the RPC uses
(`deliverDelayedRemoteInput` -> `m_onRemoteMoveReceivedCallback`), with the **original**
`captureTick`. A stale weak handle means the owner was garbage-collected without an
unregister: the map entry is dropped and the input discarded, because a dead component has nothing
to receive it.

`m_delayedInputComponentsById` exists because the core claim map is **id-keyed** — it cannot hold
a `TWeakObjectPtr` — so the coordinator's `deliver` callback hands back an id and this map resolves
it. It is populated by `noteDelayedInputComponent` from the RPC adapter
(`USimmableUpdateComponent::tryRegisterWithNewFramework`, which has both the id and the
component), pruned in the deliver callback when a weak handle goes stale, and erased in
`unregisterFromNewFramework`. `id == component GetUniqueID()`.

The per-id `deliver` callback answers "is this owner still alive", so the coordinator can drop a
stale claim — mirroring the retired drain's own `target.Get()==nullptr` prune, and routes a live delivery through `deliverRemoteInput`. **The liveness check stays
on the adapter side** because the drain's prune contract is a `bool` return while the sink method
itself returns `void` per the concept.

### The drain and the reap

`releaseDelayedInputsForStep` supplies only the game-thread-safe upcoming sim tick (§9) and the
per-id callback, then calls `ServerReceptionCoordinator::releaseDelayedInputs` and `reapConnections`. The reap was relocated
here off the former arrival-gated RTT sample path: it now runs once per physics frame regardless
of traffic — a documented benign cadence change, since an idle server now reaps — and the
coordinator gates it on the dwell boundary internally. Its tick comes from the mapper, **not** the
physics-thread-written server clock.

The coordinator's early return guards **only** the drain, not PROBE A below it, because that probe
runs on both roles and a pure client never has a coordinator.

---

## §8 The four probes

All four are purely diagnostic: nothing reads them but the log lines they feed. **Volume
convention: per-window summaries at `Warning`, per-event detail at `Verbose`, and nothing
per-tick or per-write at any verbosity.**

### PROBE A — frame health (`m_frameHealthProbe`), both roles

Sim ticks per game-thread frame.

**What it settled on the server.** Clients measure a relay-ring arrival gap of about two capture
ticks where the design expects about one. Property replication runs once per server game-thread
frame and `NetServerMaxTickRate` is a **cap** on that, not a floor — so a 60 Hz sim on a 30 fps
server advances two sim ticks per replication, and a depth-1 replace-latest ring could only carry
the newer one. If this ratio equals the clients' measured gap, the gap is a **host performance
artefact** rather than a netcode defect.

**Why the client needed it too.** `[RelayProbe.Frame]` was, until it gained a client role, the
only wall-clock timing instrument anywhere in the netcode surface, and it was server-only by
construction — yet the server never resims. Every client cost figure backing the shipped
`ResimTriggerPolicy` was derived from `ResimGateProbe` window cadence, which sees only the physics
tick and is blind to game-thread or render hitching, which is precisely where a resim-driven cost
would show up.

**Why it lives on this class and not beside the core's other probes** — `RelayReadProbe` in the
resolution peer's `InputResolutionTelemetry`, `RelayArrivalProbe` in `NetSyncTelemetry`,
`ResimGateProbe` in `SimulationManager` (this said *"in `SimulationNetSync` with the other three
probes"* until task 11 — §11 C13): it is
measured on this actor's game thread, from the Chaos pre-step hook, and the only tick source legal
to read there is the `ChaosTickMapper`'s atomic offset — which this actor owns and
`SimulationNetSync` has no access to.

**Why this hook and not `OnPostPhysicsStep`, on either role:** that hook is game-thread on both
roles too, but it is handed only an `FChaosScene`. It has no physics step number and therefore no
route to a sim tick through the only source safe on that thread. Sampling there would have
required either reading the physics-thread-written clock or inventing a second counter, both ruled
out. This hook already has both numbers: the step, and Chaos's own sub-step count.

**Why reusing `firstUpcomingSimTick` is safe on the client** even though the `+1` derivation in §9
is proved under an authority-only assumption: the probe never consumes the value directly, only
**deltas** between consecutive samples, and a constant additive skew cancels under subtraction. A
client-side departure from unconditional advance — a resim replay, a stall — surfaces either as an
ordinary sample, if the tick delta stays plausible, or as a counted discontinuity
(`kFrameHealthDiscontinuityTicks`) if it does not. Neither corrupts the measurement. What *would*
be unsafe is a **different** tick source, such as reading the prediction clock off the physics
thread; this reuses the identical atomic read the server call site already relied on, so no new
thread-safety question is introduced.

**The ratio is hook-independent**, which is why the metric is defined against `GFrameCounter`
rather than an invocation count, and it is why the probe gained a second role without a second
implementation. It additionally reports how its *own* invocations distributed over frames — once
per frame, more than once, or less often — so this hook's cadence is measured and reported rather
than assumed. On a resimming client that sub-step cross-check is the whole point:
`numStepsAboveOne > 0` reads as "resim or sub-stepping ran"; `== 0` with a ratio above 1 reads as
"the game thread simply hitched".

**Category decided per role, not inherited.** The server line keeps `[RelayProbe.Frame]` for the
original reason (it pairs with `[RelayProbe.Arrival]`). That reason does not transfer to the
client, which never emits `[RelayProbe.Arrival]` against its own frame rate; on the client the
natural pairing is frame health against **resim cost**, so the client line rides
`[ResimProbe.Frame]` and inherits routing for free from the `[ResimProbe` catch-all. The two tags
are deliberately different families rather than one tag with a role suffix — a prior review
mis-assigned which process emitted `[RelayProbe.Frame]` and had to re-derive roles from other
receipts. Both lines additionally carry an explicit `role=` field, belt and braces.

`runsPrediction()` is this codebase's established role check, and `false` covers **both** a
dedicated server and a standalone or listen-server-host manager — so a standalone session's
frame-health line rides the server tag, matching how its tick clock is already treated everywhere
else in this file.

**Window comparability with `ResimGateProbe`:** `kFrameHealthProbeWindowSamples` and
`kResimGateProbeWindowSamples` are both 120 samples, so a reader can line one `[ResimProbe.Frame]`
window up against the surrounding `[ResimProbe.Gate]` windows without a resim-tick count on this
line. They are **comparable, not identical**: this window closes on `noteFrame`'s `GFrameCounter`
cadence, that one on `noteCheck` — one call per `checkDivergenceAll` — which is a logically different event. The two drift apart over a
session exactly as far as their emitters' cadences do, which is itself diagnostic.

### PROBE 5 — relay writes per frame (`m_relayWriteProbe`), server only

**What it settles.** Replication polls a replicated property once per server game-thread frame and
compares the live value against its shadow. **When this probe was built** the ring shipped at depth
1 — replace-latest — so a second write inside the same frame overwrote the first in server memory,
and no client and no client-side probe could tell that apart from a send-path drop: both surface as
`[RelayProbe.Arrival] gapCaptureTicks > 1`. Flush-on-poll has since removed that mechanism (§6);
the probe stayed because the **quantity** — writes per frame — is still the one nobody had
measured.

The relay-loss elimination chain does not cover this. It eliminated the server's own write with
"writes on every accepted receipt, and receipts are complete", which is true and is about the wire
*into* the server; and the server frame rate with "about one sim tick per frame", which is true and
about a different clock — these writes are paced by **packet arrival**, not by the sim.

**`GFrameCounter`, not an invocation count.** The frame is the unit replication polls on, so the
engine's own frame identity is the only correct key; a local counter incremented at the tap would
measure the tap's call rate instead.

**Three fractions, never collapsed.** `receivedX1000` is upstream completeness (only input
redundancy raises it). `observableX1000` is the coalescing ceiling (depth raises exactly this one),
and it reports the **flush** ceiling since the write path changed:
`min(writesThisFrame, kMaxDepth)` rather than 1, which is why the stage capacity is *injected* into
the probe from the codec constant rather than assumed. `deliverableX1000` is their product and is
the only one comparable to the client's arrival rate. A single merged "loss" number would decide
the remedy on merged evidence. `replaceLatestObservableX1000` recomputes the same window the way
the retired write path imposed it, so archived windows stay directly comparable and the improvement
is two numbers on one line rather than a claim.

Line 2 carries the run-length shape plus the capture-tick range, so a server window can be aligned
against a client window: owner ids are per-**process** and do not match across logs, but capture
ticks do.

**Volume: two `Warning` lines per 120 writing frames per relayed character.** Nothing per-write is
emitted at any verbosity, deliberately — a per-write line is a per-tick line.

### PROBE 6 — per-connection send budget (`m_connectionBudgetProbe`), server only

**Why arithmetic was not enough.** The budget model is
`allowance = CurrentNetSpeed / DesiredTickRate` bytes per tick, with up to two ticks of bankable
credit. Both halves are read from engine source, but **every term in it is derived**:
`CurrentNetSpeed` is what the server clamped the client's request to at runtime, on a path nobody
here has watched execute, and the modelled round counts two properties out of an unknown total.
This probe measures all of them: the negotiated net speed, the real bytes and packets on the wire,
the ack-derived outgoing loss, and `notReady` — frames on which `QueuedBits + SendBuffer > 0`,
which is exactly the state in which `UDataStreamChannel::Tick` returns having written
**nothing**; `IsNetReady()` is the same question asked of the connection.

**Read before `TickFlush`, which is the right place.** `QueuedBits` is updated at the *end* of
`UNetConnection::Tick`, so the value sampled here is the credit this frame's replication write will
actually be judged against.

**Cumulative counters, not `OutBytes`/`OutPackets`:** the latter are `StatPeriod` accumulators the
engine zeroes on its own schedule, so differencing them across our window would silently drop
whatever it reset mid-window.

The allowance denominator is `NetServerMaxTickRate`, what `GameEngine::GetMaxTickRate` clamps a
dedicated server to and therefore what `DesiredTickRate` resolves to when `MaxNetTickRate`
(`BaseEngine.ini`) does not bind. It is read **from the net driver** rather than hardcoded, so a config change
cannot silently invalidate the occupancy figure. `netSpeedBps` is printed rather than assumed,
because if it is not the configured ceiling the whole derivation is wrong. `QueuedBits` is a debt
counter, so `min` is the *most* headroom and `max` the closest to saturation; `lost` is
ack-derived, i.e. the emulation's actual outgoing loss rather than a configured percentage.

**The capacity pin.** Every byte table in this work budgets against a **derived** usable
single-bunch capacity computed from `UNetConnection::GetMaxSingleBunchSizeBits()`, and an earlier
published figure for the same quantity does not reproduce from the formula. Budgeting against an
unverified derived constant is precisely how this tree's inert-ini defect happened, so the running
engine gets to be the referee — once, cheaply, from the real connection.
`MaxPacketHandlerBits` is the term that can move it, and it can only move it **down** (encryption
or any registered packet handler reserves bits out of the same budget), so the literal pinned in
`Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp` is an **upper bound**: if the value logged here
is ever smaller than that literal, the literal is optimistic and must be lowered there. The
derivation is `GetMaxSingleBunchSizeBits()` word-rounded by `UDataStreamChannel::WriteData`; the
line reports `occupancyPctX10=340` for 34.0 % occupancy. One shot
per session, on the first connection, at `Warning` — the value is a property of the build and the
handler stack, not of the connection.

### The resim-gate feeds (`noteResimRequest` / `noteResimGrant`), client only

**Why our own counters at all.** `FRewindData::FindValidResimFrame` and
`FPBDRigidsSolver::ConditionalApplyRewind_Internal` log every refusal and every silent frame-skip
behind `DEBUG_REWIND_DATA` / `DEBUG_NETWORK_PHYSICS`, which are compiled out of any normal build. A
refused rewind is therefore **completely silent** today, and our side simply retries next frame
because nothing cleared `needsResimulation()`. These two calls are the only way that second gate
becomes countable, and `requests − grants` is the refusal count. (The retry property is now
structural rather than incidental: only the resim-completion edge can consume a pending anchor, so
a refusal cannot clear the gate even by accident. The counters' meaning is unchanged.)

**The request is counted after the tick conversion**, so the Chaos frame recorded is exactly the
one Chaos receives and `clampedGrants` compares a grant against the number actually asked for. The
per-event `Verbose` line carries the mapper **offset**, which is the discriminator: the offset
legitimately moves ±1 across Stall/Skip steps, and a ±1 skew at trigger time converts directly into
a request landing on a refused frame or a replay one frame short of the clock's catch-up need.

**The grant** is recorded where Chaos starts the rewind, at `PhysicsStep`, which can differ from
the frame we asked for **only by being deeper**: an engine-side requester merged in via a *min*,
or the validation walking down (`FMath::Min` on the engine side). A *shallower* clamp is structurally impossible on this wiring —
validation walks downward, the merge can only deepen, and the replay-loop push-data skip that could
start a replay late is dead code on this engine. So `clampedGrants` reads a constant `0` by
construction and a nonzero value is an **engine-behaviour-change alarm**. The engine may also
rewind on its own initiative with no request of ours on record; that stays uncharged. See `ResimGateProbe::noteGrant`.

`noteResimGrant` is called **before** `prepareResimulation`, so a grant is recorded even if
anything below early-returns: `grants` and `prepares` are counted on opposite sides of that
boundary on purpose, and their agreement is the wiring check.

**The startup proof line** (`[ResimProbe.Session]`) is the fourth step of that family's
category path — declaration, router branch, ini block, then this — and the one that makes the
other three checkable from a log instead of from the source. It reports the **effective runtime
verbosity**, not a constant: `verboseDetail` says whether the per-event half will emit at all,
which is the single most likely thing to be silently off when somebody goes looking, and
`windowSamples` states the denominator every per-window line is divided by, taken from the constant
rather than retyped. **Its own absence is also information:** no `[ResimProbe.Session]` line in a
client log means either the category was set to `NoLogging` or the branch never ran, and both are
worth knowing before reading a zero off any counter.

### The rewind-push probe (`[ResimProbe.PushTarget]` / `[ResimProbe.PushVerdict]`), client only

**What it exists to decide, and that a REFUTATION is the point.** og-netcode-v2-field-defects
task 10 asks whether the body-state push `FirstPreResimStep_Internal` makes is ever read.
That initiative's bug report infers that it is not; AC 1 makes that inference
falsifiable. **A mismatch on an adoption-terminated swing confirms; a match REFUTES and closes
the task as "not a defect".** The probe is therefore built so the refuting reading is exactly as
easy to quote as the confirming one: `verdict=ALL_MATCH` on one line, no re-derivation.

⛔⛔ **CORRECTED, rework cycle 1.** The paragraph that stood here said: *"The AFTER read is
the verdict value. Taking it after the push is what keeps the refutation reachable."* **It does
not, and the claim was wrong in the one way that mattered.** `FRewindData::SetTargetStateAtFrame`
delegates to `PushStateAtFrame`, which writes the target history buffers and touches no particle.
So the after-read equals the before-read BY CONSTRUCTION, on every body, on every rewind — which
means `match=1` could only ever mean *the pushed value already equalled the live one*, i.e. the
push was inert, and the one shape that could REFUTE the hypothesis, a **non-inert match**, was
unreachable. Worse, a binary whose target consumer runs after the push and before integrate would
have printed exactly the same line as a binary that reads the target never. The reading moved; the
sentence is kept here rather than deleted because a reader who saw the old revision is entitled to
know which claim was withdrawn and why.

**Where the reading is taken, since rework cycle 1 — TWO HOOKS, THREE READS.**

| read | where | what it yields |
|---|---|---|
| 1 | `FirstPreResimStep_Internal`, immediately BEFORE `FRewindData::SetTargetStateAtFrame` | `beforePush`, stashed. Feeds `inert=` and `dBefore=` |
| 2 | the same hook, immediately AFTER that call | `moved=` only — an EAGER-APPLY discriminator, never the verdict |
| 3 | `OnPreSimulate_Internal`, on the frame where `IsResetting()` holds, at the TOP of the hook | `match=`, the verdict |

The pushed value and read 1 wait in `m_pushProbeStash` (`FRewindPushProbeStashedBody`, physics
thread only) between hook one and hook two. By the time read 3 is taken,
`FPBDRigidsSolver::ConditionalApplyRewind_Internal` has run `FRewindData::ApplyTargets` and
everything else it runs for this step, and the solver task has reached
`ApplyCallbacks_Internal` — so a target consumer anywhere in that window IS visible here.

⛔ **Read 3 must stay above `onGameSimulation`.** The radial's own `setIdlePose` /
`setInitialConditions` write the live body during the replayed step. A reading taken after them
measures OG's writes, not the engine's, and would report a mismatch that says nothing about
whether the target was read.

⚠ **The residual blind spot, stated rather than hidden.** A consumer that runs INSIDE the
evolution after `ApplyCallbacks_Internal` — between read 3 and `FPBDRigidsEvolutionGBF::Integrate`
— is still invisible. The rework narrows the window; it does not close it. A `verdict=ALL_MATCH`
refutation is a statement about consumers up to that point, and the runbook says so.

⛔⛔ **`inert=` IS WHAT MAKES A REFUTATION MEAN ANYTHING, and it is why the verdict rule
changed.** A push whose value already equals the state the body is in cannot distinguish "the
engine read the target" from "the engine read nothing" — whatever the replay then does, the
comparison matches. A rewind in which every push was inert therefore used to read
`compared=6 matched=6 verdict=ALL_MATCH`: a session in which the shape never reproduced,
presenting itself as a clean refutation. So `inert=` is printed per body, over the same `cmp=`
field set and through the same production predicate as `match=`, and **the verdict is computed
over NON-INERT comparisons only**:

| | verdict |
|---|---|
| `nonInert=0` (nothing resolved, or every push inert) | `VACUOUS` — the rewind tested nothing, and NEITHER reading may be taken from it |
| every non-inert comparison matched | `ALL_MATCH` — refutes |
| any non-inert comparison mismatched | `MISMATCH` — confirms |

⭐ A refutation therefore needs at least one `body=WeaponAxis inert=0 match=1` line. "No
mismatches in the log" is **not** a refutation on its own.

⛔ **A stash that never reaches read 3 is PRINTED, not dropped** — `verdict=UNREAD` with
`readFrame=-1` and `reason=newPushBeforeRead` or `reason=notResetting`, under the same tag so the
runbook's single `grep -v verdict=ALL_MATCH` surfaces it. An absent line is indistinguishable from
a rewind that never happened, and a reader who cannot tell those apart cannot trust a refutation.
That branch firing at all is an engine-ordering finding in its own right.

⚠ **One mismatch source the probe does NOT exclude: mapper skew.** `toChaosTick(Tc)` uses the
CURRENT offset, so a Stall or Skip between frame S(Tc) and the rewind moves F by ±1 and
`RewindToFrame(F)` restores a different instant than `slot[Tc]` — at which point EVERY body
mismatches for a reason that has nothing to do with ordering (§9's hazard). The per-body rows are
the control: on a genuinely confirming rewind, `CharacterCapsule` and `GuardAxis` must **not** all
read `match=0`. If they do, read `[ResimProbe.Request] mapperOffset=` before concluding anything.

⛔ **It reads X/R, and `ChaosPhysicsBodyAdapter::captureBodyState` reads P/Q, and both are
right.** That adapter runs from PostSolve, where P/Q hold the solved pose and X/R still hold the
start-of-step pose. This probe runs at the opposite phase: on UE 5.6's public source
`FRewindData::RewindToFrame` restores the pose with `SetXR`, `FRewindData::ApplyTargets` applies a
target with `SetXR`, neither writes P/Q, and `FPBDRigidsEvolutionGBF::Integrate` begins the
replayed step from `XCom()`/`RCom()`. X/R is therefore the state the replay starts from. A P/Q
read here would report a stale pre-rewind pose and print a mismatch on every body on every
rewind — a probe that cannot fail is worth nothing, and this one was one line away from being
that. `dXP=` carries the X-to-P distance so the skew stays visible rather than assumed.

⛔ **The match predicate is the production one.** `isSimilarToField`
(`SimulationComparisonGlm.h`) at `kDefaultSimilarityEpsilon` (`SimulationTypes.h`) — the same
test `isSimilarTo` applies to a correction, so "the probe disagrees but the cache does not" is
not a state this can reach. Its quaternion arm is `|abs(dot) - 1|`, which matters: an
antipodal-but-identical rotation is a match, and a hand-rolled component compare would have
called it a large mismatch. **Measured, not argued** — the task-10 notes carry a standalone run
in which two plausible wrong rules (component-wise quaternion; ignore the wire shape) are scored
against the same cases, and **both err toward MISMATCH**, i.e. both would have confirmed the
hypothesis for free.

⛔ **Only the fields the body's wire shape carries are compared.** A declaration whose
`bodyStateOf` yields `LinearBodyState` — the character capsule — fabricates an identity rotation
and a zero angular velocity on the way to `PhysicsBodyState`. Comparing those two would report a
mismatch about a value no wire ever carried. `cmp=` names the set that was compared, and
anything that is not exactly `PhysicsBodyState` takes the conservative position-plus-linear-
velocity set.

⛔ **`verdict=VACUOUS` is a third answer, not a rounding of `ALL_MATCH`.** With `compared=0` the
counters read all-zero, which is indistinguishable from a clean refutation. An unresolved proxy or
handle is counted in `unresolved=`, never silently skipped. ⛔ **Rework cycle 1 widened what
reaches this verdict**: `compared=0` was never the only way to test nothing — see the `nonInert`
table above, which is now the rule.

**Volume and cost.** Per-body detail and the per-rewind summary, both at `Verbose`, and every
read and format sits behind one `UE_LOG_ACTIVE(LogOGResimProbe, Verbose)`, so at the shipped
`LogOGResimProbe=Warning` the whole probe is one predicate per body. It is deliberately NOT a
per-window summary: the question is per-rewind and an aggregate would hide which body disagreed.

⚠ **The engine facts are about the API, not about the running binary.** `ApplyTargets(Step,
bFirst)` before `PreResimStep_Internal(Step, bFirst)`, and the repeat gated on
`np2.Resim.ApplyTargetsWhileResimulating` (default `false`), are read off public
`EpicGames/UnrealEngine@5.6`. This project builds against a local source engine, and the two can
differ. **That gap is precisely what the probe measures**, so nothing here may be quoted as the
answer AC 1 asks for.

⚠ **Not yet run.** Confirming or refuting needs a PIE session with an adoption-terminated swing
and task 2's fix off, which only the user can produce. The runbook is in that task's
implementation notes.

---

## §9 Tick alignment — the `+ 1`

**An off-by-one here shifts every player's input by one tick, silently and uniformly.**

`physicsStep` is the **upcoming** solver step: Chaos passes
`MarshallingManager.GetInternalStep_External()`, documented in `ChaosMarshallingManager.h` as *"the internal step that the current PushData will
be associated with once it is marshalled over"*, and the solver's frame counter is
only incremented at the **end** of a tick (`GetCurrentFrame()++`, `PBDRigidsSolver.cpp`), so step `N`'s `OnPreSimulate_Internal` observes
`GetCurrentFrame() == N`.

`ChaosTickMapper`'s offset is written in `OnPreSimulate_Internal` as `chaosTick - simulationTick`.
Crucially it is written **before** `onGameSimulation()` runs, and `onGameSimulationAuthority()`
advances the server clock as its first action. So the `simulationTick` captured at step `K` is the
tick simulated at step `K−1`:

```
offset = K - S(K-1)          where S(K) is the sim tick simulated at step K
S(K) = S(K-1) + 1            authority advanceTick() is unconditional -
                             no Stall, Skip or resim exists on the server
=>  offset = K - (S(K) - 1) = (K - S(K)) + 1
=>  toSimulationTick(X) = X - offset = S(X) - 1
```

`toSimulationTick(physicsStep)` therefore names the tick **before** the one that step will
simulate, and the upcoming tick is that value **plus one**. The `+ 1` in
`releaseDelayedInputsForStep` is a derived correction, not a fudge factor.

**Why not cross-check against the server clock at runtime:** the clock is written on the physics
thread, and reading it from the game thread is exactly the unsynchronized cross-thread read this
whole design exists to avoid. The mapper's offset is a `std::atomic` and is the only tick source
safe to read from the game thread, which is why the resolution specifies it.

**Sub-stepping:** with `NumSteps > 1` the physics frame runs several sim ticks back to back, so the
drain releases input for each of them. The normal fixed-tick case is `NumSteps == 1` and collapses
to a single drain.

The Chaos hook `OnPhysicsPreTick` → `InjectInputs_External` → `releaseDelayedInputsForStep` is
game-thread and immediately precedes the step. The drain is a no-op on a client and on a server
with nothing parked; the frame-health probe inside the same function is **not** a no-op on either
role.

---

## §10 Registration and unregistration

`tryRegister` is a two-phase contract. The first call creates bodies; later calls re-test
resolvability and, once every body resolves, perform the registration and return `Ready`.

**The capsule body id.** `setCharacterBindings` stamps the authoritative capsule `BodyId` into the
brawler's `CharacterBindings`, and its **source is the movement sub-simulation's own
`PhysicsDeclaration`** — `bindings.ownBodyId`, read out of the physics composite *after* the
creation fold has run. Before the fold that id is still zero, which is why the stamp sits below it.

The value is the capsule's, and it is the capsule's *by construction* rather than by coincidence:
the movement descriptor sets `isRoot`, so `ChaosPhysicsFactory` **adopts** the engine character's
existing capsule instead of creating a body under it, and its adopt-root arm ends in a `checkf`
that the adopted body's id equals the parent id the factory derived from that same component. One
body, one id, reached through the sub-simulation's own declaration.

A **two-source tripwire** used to sit at this stamp: a second `checkf`, comparing the declaration's
`ownBodyId` against a capsule id cached on `PendingRegistration` for as long as the two sources
coexisted. Both are gone. The cached `parentBodyId` field had exactly two readers — that tripwire
and the resolvability gate — and the gate now reads the same declaration, so the field had none
left. Removing it removes a duplicate, not the only witness: the factory's own assertion, which
this file does not own, still watches the identity. The capsule id is still derived inside the
first-call pass, as a **local**, because the factory and each declaration's own
`bindings.parentBodyId` consume it there.

**The movement sub-simulation, and what it costs this file.** It is registered by exactly the same
generic fold as radial, guard and the three projectile slots — `staticDataOf` hands the fold its
static-data slice, `descriptor()` the body and shapes, `queryVolumes` the probe volume — so no
line in this file branches on it. Three things about it are nonetheless this file's business:

* **It adopts; it does not create.** `isRoot` on its descriptor means the engine character's
  capsule *is* its body. `simulatePhysics = true` on that adopted component is what switches the
  engine's own character-movement component off, and the sub-simulation's `drivesBody` is the
  other half of that pair. Neither may be flipped alone: `simulatePhysics` without `drivesBody`
  leaves nobody driving the capsule, and `drivesBody` without `simulatePhysics` leaves two.
* **Its collision category is hand-written engine work, and this file pays it.** `character` needs
  one row in EACH of the two mapping tables in `BeginPlay` — authority branch and client branch,
  duplicated with no shared constant — and the channel itself must be declared in the project's
  collision settings. Generic creation, hand-written collision; the fold's own comment states both
  halves, because stating only the first one overstates the claim.
* **Its spawn pose is seeded here, exactly once.** The first-call pass writes `teleportPending`
  and `teleportPos` into the sub-simulation's `InitialConditions` from the capsule's current
  transform. That edge is on the wire, so a respawn replays identically on a client and through a
  resim, and it is the one body write that ignores `drivesBody` — which is safe here only because
  it is seeded from the capsule's own location and is therefore a value no-op.

**Attach parent and parent body are the same capsule** under the one-deep hierarchy, so passing one
handle expresses "these shapes belong to this character". They used to be passed separately, which
let callers get them out of sync; the factory derives the parent `BodyId` from the attach parent's
body-instance handle internally now, and that factory-owned parent id becomes the root for every
shape it registers — so `overlap()` emits an actor-level `rootBodyId` equal to the capsule id.

**On the authority path**, registration also notifies the systems executor that the character is
now *in storage*, so `brawlerHitRouting::System::onCharacterRegistered` indexes it for inbound-hit
routing and reads `capsuleBodyId` itself. Wiring that notify and dropping the old adapter-owned
`m_byRootBodyId` insert landed in the **same** change, so there was never a window in which both
the adapter map and the system map were populated and an inbound-hit stream could be double-routed.

**Unregistration is a contract, not a garbage-collection hope.** `unregisterFromNewFramework`:

1. notifies the systems executor to drop the routing entry **before** `unregisterSimulatable`
   destroys the `SimulatableBrawler`, while the character is still in storage — so the hook
   resolves it through the view and erases by stored-pointer identity, and the per-tick routing
   pass never dereferences a dangling pointer. The `has<>` guard is preserved: a character never
   fully registered into storage has no system-map entry to drop, and the hook's `view.get<>(id)`
   would be unsafe;
2. drops this owner's claim and dedup watermark from the reception coordinator and its
   id→component mapping — **promptly**, rather than waiting for GC to make an engine handle stale.
   This is what replaced the core's former GC-liveness read, since the core claim map is id-keyed
   and cannot hold an engine weak pointer. No-op on a pure client;
3. drops the write probe's per-owner state, so its map stays bounded by live ids exactly as the
   coordinator's does. A half-open run belonging to a dead owner is discarded rather than reported,
   which is correct: its length is unknowable;
4. erases the id from `m_authorityRegisteredIds`.

### The pre-diet character cap ∴D-01

`kPreDietCharacterCap = 4`, checked at character registration on the authority — a path that
provably runs every session, once per character. **A cap that depends on nobody spawning a fifth
character is not a fence**, and this tree has already shipped three things that were silently inert.

**Why 4, derived.** The binding constraint is the **input guarantee**: all the remote characters'
relay rings must fit one packet by themselves — pre-diet, `81*SumE + 13.1*(N-1) <= 943 B`, because a ring that gets scheduled out under
redundancy 0 loses its whole staged burst with no recovery path. Modelling a join as "the joiner's
ring at the measured settling burst, everyone else at the measured average", N = 4 clears that
bound with about nine tenths of one entry to spare and N = 5 does **not** — at five characters an
*ordinary* join crosses the bound, with no server hitch required. The same arithmetic is asserted
from inside the suite by `Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp`'s pre-diet table. The
other half of the pre-diet configuration is `TimeConfig::correctionRotationK` (§3).

**Emission is once per over-cap character**, not per frame and not per session: registration runs
exactly once per character, so the emission site is its own throttle. No memoization is needed and
none is used — a per-session latch would report the fifth character and stay silent about the
sixth. It is a `Warning` rather than a `Log` because a `Log` line does not exist on a dedicated
server, and it is not an `ensure`/`check` because an over-cap session still **runs**: it runs with
input-loss margins the design has not underwritten, which is a thing an operator must be told, not
a thing that should take the server down mid-brawl.

**The denominator is a `std::set`, not a counter**, and the reason is that the two ends are not
symmetric: `tryRegister` increments only on the `Ready` path, while `unregisterFromNewFramework`
runs for any component ending play including one abandoned mid-`Pending`, so a bare counter would
silently drift downwards and disarm the cap. Reaping on unregister means a session that churns
characters is judged on the roster actually resident rather than on a high-water mark, and erasing
an id that never completed registration is a no-op — which is exactly why a set is the right shape.

**Both the constant and its check are deleted by the wire diet**, and their **absence** afterwards
is the "cap lifted" statement: there is no flag to flip and no value to raise, which is deliberate,
because a cap you can quietly widen is not a cap.

⛔ **The cap is coupled to `brawlerRingout::kMaxSpawnPoints`, and since task 11 the header asserts
it:** `static_assert(kPreDietCharacterCap <= kMaxSpawnPoints)`. The cap only WARNS — it allocates
nothing. The ring-out spawn table has exactly `kMaxSpawnPoints` entries, so a character that
registers while the table is full gets `SpawnSlotAllocator::kNoFreeSlot` and respawns with **no
teleport seed**, logging one `[Warning][Ringout.spawnSlot]` per respawn. A cap above the table
would stop warning about exactly the characters that cannot be placed. *(The header's prose said
raising the cap alone "SILENTLY STOPS RESPAWNING EVERY CHARACTER PAST THE 4th". Both halves were
wrong — §11 C7.)*

⚠ **This heading carries `∴D-01`**, the derivation tag on `kPreDietCharacterCap`'s declaration.
When the diet deletes the constant, it deletes that tag's site, so the diet must retire `D-01` in
the same change. The tag lint's orphan check will say so if it does not.

---

## §11 Corrections — claims these files carried that were not true

This pass verified every claim before compressing it. Four did not survive, and are recorded here
rather than quietly repaired, because an archive that repairs itself stops being a record.
**Task 11's R0 pass over the header added C5–C19:** twelve false claims (C5–C13, C17–C19 — the
last three found by task 12 and missed by task 11's own R0), two overstatements (C14, C15), and one
set of this document's own stale counts (C16).

### C1 — `correctionRotationK`'s compiled default was stated, and the value had moved

`SimulationManagerUImpl.cpp`'s rotation-width intake block said:

> *"ABSENT => the compiled default (2 — every-frame at two characters, which is what keeps the
> archived two-character baselines comparable across this change)."*

`TimeConfig::correctionRotationK` is **1**. Three independent sources agree: the field's own
declaration; `Config/DefaultEngine.ini`, whose `[OGNetcode]` block states *"THE COMPILED DEFAULT IS
NOW 1"* and leaves `CorrectionRotationK` commented out so the compiled default ships; and
`Source/OGSimulationTests/extern/og-simulation-tests/Source/OGSimulationTests/PCTimeManagement/TimeConfigDefaultsTest.cpp`, which records that the value
will be *restored to 2* by the wire diet.

**The pair contradicted itself**: `SimulationManagerUImpl.h`'s pre-diet-cap block already said
`correctionRotationK = 1`, correctly, a few hundred lines away.

⛔ **A second paragraph in the same block was void as a consequence.** Its sentinel-collision note
argued that `clampK(-1)` would be `1`, *"which is NOT the compiled default — so a `-1` written on
purpose silently takes the compiled default 2 instead of clamping to 1"*. At the real default the
two are the same number and the difference it describes does not exist.

**Fix:** the block now points at `TimeConfig::correctionRotationK` and does not restate the value.
The sentinel note keeps the true half — that `-1` cannot be told from an absent key — and drops the
arithmetic that depended on the wrong default.

### C2 — the `RelayedInputRingCodec.h` include justification named a call that does not exist

The include comment said the header was named directly because *"relayedInputRing::clampDepth — the
shared depth guard **the intake below calls** before it logs the effective depth"*.

`clampDepth` occurs in that translation unit **only inside that comment**. The intake it refers to
was retired together with the ring-depth ini key — a fact stated by the ⛔ RETIRED fence about
five hundred lines further down the same file. The justification survived the retirement it sits
above.

**Fix:** the include comment now names `relayedInputRing::kMaxDepth`, which the pair really does
use (`SimulationManagerUImpl.h` initialises `m_relayWriteProbe` with it), and states that no
`clampDepth` call remains.

### C3 — the relay ring's owner and its replication condition were both wrong

`SimulationManagerUImpl.h`'s `relayRemoteInput` comment said this writes

> *"into that character's replicated relay ring (`FRelayedInputRing` on
> `USimmableUpdateComponent`, `DOREPLIFETIME` with **NO** `COND_`)"*.

Both halves are false in the current tree:

- the replicated ring is **`ASimulationInputRelay::m_relayedInputRing`**. `USimmableUpdateComponent`
  holds `m_detachedRelayRing`, whose own comment says it is the *no-host fallback* and that a write
  landing there *"would be invisible to every client"*;
- the registration is **`DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing,
  COND_SkipOwner)`**. `SimmableUpdateComponent.cpp` states in its own words that the old
  `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` "stood on" the component and was
  replaced.

A third clause was stale in a third way: it said this *writes* the ring, where the write path has
since become **staging** and the `.cpp` at the same call site says so explicitly.

⛔ This is the fourth site of a relay-ring mis-attribution class that two earlier reviews of this
initiative found and routed as unowned. **Grep every symbol before carrying it into a comment**:
this one would have been carried into a compressed guard with a checker's blessing, because every
symbol it names exists.

### C4 — a probe banner described a retired mechanism in the present tense

PROBE 5's banner said *"This ring ships at depth 1, i.e. replace-latest. So a second write inside
the same frame OVERWRITES the first in server memory"*. The ring's stage capacity is
`relayedInputRing::kMaxDepth`, which is **8**, and the write path is flush-on-poll — as the
paragraph eight lines below the banner already said, in the same file.

**Fix:** the sentence is now closed-tense — *"when this probe was built"* — and states that the
mechanism was removed while the quantity remains unmeasured, which is why the probe stayed.

### C5 — the banner counted "five _Internal hooks"; there are six

`SimulationManagerUImpl.h`'s thread table said *"FSimulationManagerAsyncCallback's five _Internal
hooks"*. The class overrides **six**: `OnPreSimulate_Internal`, `OnPostSolve_Internal`,
`ProcessInputs_Internal`, `TriggerRewindIfNeeded_Internal`, `ApplyCorrections_Internal`,
`FirstPreResimStep_Internal`. Two have empty bodies (`ProcessInputs_Internal`,
`ApplyCorrections_Internal`). The claim had **four copies**: the header banner, the `.cpp`'s
orientation block (twice) and §1's table here, whose five names left out `ProcessInputs_Internal`.
**Fix:** §1's table names all six; §0 carries the banner verbatim with an annotation. The `.cpp`'s
two copies are routed to task 12, which owns that file.

### C6 — `getLocalInputCache` was said to have one caller; it has two

The header said of `getLocalInputCache`: *"Its one caller is pollInputHistory, which owns the
tear."* §1 said the same: *"`pollInputHistory` is its only caller"*. `isLocallyControlledOnThisPeer`
also calls it, and it is reached from `pollInputHistoryLanes` and from `AOGBrawlerUEHUD`. The
tear argument survives, because that caller only compares the pointer with `nullptr` and reads no
slot byte. **Fix:** §1 now says "the only caller that dereferences it".

### C7 — the pre-diet cap's coupling to the spawn table was stated backwards

The header said: *"COUPLED TO brawlerRingout::kMaxSpawnPoints, WHICH IS ALSO 4 - RAISING THIS ALONE
SILENTLY STOPS RESPAWNING EVERY CHARACTER PAST THE 4th."* Three things are wrong with it:

* the cap **allocates nothing** — it only compares a count and warns — so raising it changes
  nothing about who gets a spawn slot. A fifth resident character gets no slot at cap 4 too;
* such a character **does** respawn: `brawlerRingout::integrate` clears the dead bit and writes no
  teleport seed;
* it is **not silent**: every such respawn logs `[Warning][Ringout.spawnSlot]`.

What *is* true is the coupling itself. A cap above the table stops warning about exactly the
characters the table cannot place. **Fix:** a `static_assert(kPreDietCharacterCap <=
brawlerRingout::kMaxSpawnPoints)` whose message states the real mechanism, seen to fire at cap 5
(§10).

### C8 — three of the log-category one-liners described the wrong members

The `DECLARE_LOG_CATEGORY_EXTERN` comments said:

* `LogOGSim` — *"Rare simulation lifecycle: TimeResync.*, Resim.*, …"*. `[Resim.Input]` is
  routed to `LogOGSimTick`, not here (§4, *Order is load-bearing*).
* `LogOGSimTick` — *"… ResimCheck.*"*. `[ResimCheck.Divergence]` and `[ResimCheck.PrepareRestore]`
  are routed to `LogOGSim`. The family is split across both categories, which §4 says is the whole
  reason no probe may be filed under it.
* `LogOGRelayProbe` — *"Client relay telemetry"*. `[RelayProbe.Frame]` is emitted by the
  **server** (`role=Server`).

**Fix:** the one-liners are gone from the header (§12 carries them); §4's tables are the maintained
statement.

### C9 — "cast before the subtraction" protected nothing

The header said, at `ClockDriftReading::driftTicks`: *"⛔ CAST BEFORE THE SUBTRACTION -- a negative
drift is the whole point, and two unsigned ticks would wrap it into a vast positive."*
`ClockDriftReading::driftTicks` is `int32_t`. Assigning the unsigned difference to it is a
modular conversion under C++20, and yields the same negative number. **Measured, not argued:**
task 11 compiled both spellings with this module's own flags, evaluated at compile time on three
cases (negative, positive, at tick zero). They agreed, with no warning at `/W4`. A control that
widens the destination to `long long` **did** fire. The fence's consequence holds only for a
destination wider than 32 bits. The edit it forbade is harmless today, so it earns no guard. If
the field is ever widened, the warning belongs at the field's declaration in og-brawler.

### C10 — `m_staticData` was said to be the only construction that passes arguments

The header said the manager's `m_staticData` initializer was *"the ONE call site in the tree that
passes anything - every test peer default-constructs and therefore measures the authored
literals."* `RuntimeTweakablesRefusedNamesTest.cpp` constructs a `simulatableBrawler::StaticData`
with all five movement arguments, deliberately, to prove they reach the sub-sim; its own comment
calls itself *"the only place in either suite that passes anything"*. The true statement is **one
production call site, and one test that mirrors it**.

### C11 — "a listen server runs TWO of these actors"

Said at `m_ringoutSpawnPointsSeeded`, and also in `MovementSchemeCVar.cpp` (*"A LISTEN SERVER HAS
TWO MANAGERS"*). `USimulationManagerSubsystem::OnWorldBeginPlay` spawns one manager per game
world, and a listen-server host is one world — one manager, in slot 0. `BeginPlay`'s
`checkf(false, …)` on a second authority instance makes two authority managers in one process a
crash, not a configuration. Two managers in one process is the **PIE** case: one per PIE world.
The per-instance consequence both comments draw (each manager seeds its own table / does its own
read) stands. **Fix:** `MovementSchemeCVar.cpp`'s copy now names PIE. The header's copy is carried
in §12 with this annotation.

### C12 — two positional words in the spawn-table block pointed the wrong way

The block at `seedRingoutSpawnPointsFromLevel` said *"The banner above states StaticData as
constructed once and never moved"*. No banner above it said that; the statement stood at
`m_staticData`, **below** it. The block also said *"`readMovementStaticDataCVars()` above"*; that
declaration is **below** it too. The substance of both sentences is true. **Fix:** carried
verbatim in `⛔G-14`'s entry, with this annotation.

### C13 — PROBE A was said to sit apart from "the other three" probes in `SimulationNetSync`

Header and §8 both said so. Today `RelayReadProbe` lives in `InputResolutionTelemetry` (the
resolution peer), `RelayArrivalProbe` in `NetSyncTelemetry`, and `ResimGateProbe` in
`SimulationManager`. Only one of the three is in NetSync's orbit. The sentence predates the
resolution-peer split. **Fix:** §8 names where each one lives.

### C14 — "a fresh count beside a stale tick cannot be observed" is a program-order argument about non-atomic data

§1 and the header's `⛔ COUNT BEFORE ITS TICK` fence argued an impossibility from the order of
plain, non-atomic reads and writes. C++ gives that order no force across threads, and the optimizer
may reorder either pair. The read order still makes the harmful case unlikely, so it stays as
`⛔G-09`. The word "cannot" does not survive. **Fix:** annotated in §1 and in the guard's entry.

### C15 — "THE ONLY PLACE IT IS SPELLED" (overstatement)

`isLocallyControlledOnThisPeer`'s block called itself *"THE ONE LOCALITY TEST THE DISPLAY MAKES,
AND THE ONLY PLACE IT IS SPELLED."* `pollInputHistory`, twenty lines below it, spells the same
test again: `getLocalInputCache(id) == nullptr`, commented *"no capture line: a remote proxy"*.
It has to, because it needs the pointer the test produces. The rule the sentence protects — every
**meter** asks here — is intact. The absolute claim is not.

### C16 — this document's own counts, stale against the header

Three counts in this document had drifted:

* §1 opened *"There are two crossings"* while the header's table said three;
* the reading instruction said the orientation block took *"about fifty lines"* — it was 102;
* §2's construction chain left out `m_movementStaticDataCVars`.

All three were copies of what the header said at an earlier date. **Fix:** corrected in place, each
marked.

### C17 — "`[Resim.` inherits `LogOGSim=Verbose`": the ini sets `Log`

The header's LOG CATEGORIES paragraph said *"`[Resim.` inherits LogOGSim=Verbose"*. §4 said *"The
first inherits `LogOGSim=Verbose`"*. At `b9f6d81`, `Config/DefaultEngine.ini`'s `[Core.Log]` sets
**`LogOGSim=Log`**. The Verbose level was true when T19 measured it, and was lowered later. §3 had
the same drift for a second category: it said the ini *"sets `LogOGNet=Warning`"*; the ini sets
`LogOGNet=Verbose`. Both arguments survive, because neither depended on the level. **Fix:** §3 and
§4 corrected in place; §0 annotated. The ini's own comment above `LogOGResimProbe` carries the
`LogOGSim=Verbose` sentence too — **routed** (`Config/`).

*Found by og-netcode-v2-field-defects task 12's R0 on the `.cpp`, and missed by task 11's R0 on the
header, which checked the routing table but not the ini levels.*

### C18 — "FOUR SESSION KNOBS … each with an unconditional Warning proof line"

The header's banner said all four session knobs are *"read once in BeginPlay (.cpp), each with an
unconditional Warning proof line"*. At `b9f6d81` that is true of two of them. `RelayDelayFloorTicks`'
proof line (`[RelayDelayFloor] session floor = %d ticks (ini override)`) is at **`Log`**, and sits
**inside** `if (configuredRelayDelayFloorTicks >= 0)`, so an absent key prints nothing. The retired
ring-depth key has no proof line, because it has no intake. §3's four-step table states the rule
without the exception. **Fix:** §0 annotated, §3 notes the exception. Whether the floor *should*
get an unconditional `Warning` line is a behaviour change, and not this task's.

*Found by task 12's R0, like C17.*

### C19 — the spawn-table fence attributed "no tick sees it change" to the never-copied-or-moved rule

The block at `seedRingoutSpawnPointsFromLevel` said *"The banner above states StaticData as
constructed once and never moved. What that discipline actually protects is that NO TICK EVER SEES
IT CHANGE."* §2's rule — construct in place, never copy or move — protects `StaticData`'s
**internal sibling references**, and `StaticData` enforces it by deleting its copy and move
operations. It does not forbid writing a member. "No tick ever sees `StaticData` change" is a real
property. It is a separate one, and the two `checkf`s at `seedRingoutSpawnPointsFromLevel`'s
definition are what hold it. Task 11's C12 said the sentence's *substance* was true, and
`⛔G-14`'s "what breaks" repeated the attribution. **Fix:** both corrected in the guards doc. The
`.cpp`'s copy of the same argument is §13's C13-8.

*Found by task 12's review of task 11's text.*

### Routed, not fixed — an observation about a neighbouring file

A wave-6 sweep of the engine-free core retired the token `TestYo` "to 0" on the finding that *"the
TestYo layer exists nowhere in this tree"*. It does exist: `Config/DefaultEngine.ini`'s
`[/Script/Engine.Engine]` block carries six live `ActiveGameNameRedirects` /
`ActiveClassRedirects` entries naming `TestYo`, redirecting it to `/Script/OGBrawlerUnreal`. It is
this module's **original project name**. The two files that use the phrase are in the
`og-simulation` tree and are not owned by this pass; the finding is recorded here so the next
editor of either has the evidence.

---

## §12 The header's declaration comments, carried

*Task 11 removed every comment from `SimulationManagerUImpl.h` except the licence line. The banner
is §0. **Everything else is here, verbatim, in file order** — 99 comment runs, 387 lines — each
with where its content went. Where a numbered section above covers the same ground, that section
is the maintained text and the block below is the record of what the header said at `b9f6d81`.
The R0 annotations point at §11.*

*Nothing below is a guard. A guard's live text is its entry in `SimulationManagerUImpl-guards.md`.*

#### Header lines 137-137 — above `#include "OGSimulation/SimulationInputResolution.h"`

<!-- header lines 137-137 at b9f6d81 -->
```
// Explicit, not transitive through SimulationNetSync.h: the peer is a sibling now. §2
```

**Where it went:** Rationale — include hygiene. Task 11 deleted this include and the header still compiled (the peer arrives transitively), so nothing enforces it and nothing breaks without it. The include stays as code; no tag.

#### Header lines 145-145 — above `#include "OGSimulation/Network/RelayWritePathProbe.h"`

<!-- header lines 145-145 at b9f6d81 -->
```
// Server write-path diagnostics; separable, with an end date in its own banner.
```

**Where it went:** Orientation — §8, PROBE 5.

#### Header lines 164-164 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGSim, Log, All);`

<!-- header lines 164-164 at b9f6d81 -->
```
// Rare simulation lifecycle: TimeResync.*, Resim.*, ResimCheck.Divergence, ResimCheck.PrepareRestore
```

**Where it went:** Orientation — §4. ⚠ Wrong member list (§11 C8).

#### Header lines 166-166 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGSimTick, Log, All);`

<!-- header lines 166-166 at b9f6d81 -->
```
// Per-tick simulation chatter: AuthoritySimulation, ClientPrediction, CollectInput, ResimCheck.*
```

**Where it went:** Orientation — §4. ⚠ Wrong member list (§11 C8).

#### Header lines 168-168 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGMgmt, Log, All);`

<!-- header lines 168-168 at b9f6d81 -->
```
// Manager / simulatable lifecycle: SimulationManager:*, tryRegister:*, NewFramework:*
```

**Where it went:** Orientation — §4.

#### Header lines 170-170 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGNet, Log, All);`

<!-- header lines 170-170 at b9f6d81 -->
```
// Replication-channel events: ServerReceive, Send*ToClients, ReceiveCorrection*, InjectCorrection*
```

**Where it went:** Orientation — §4.

#### Header lines 172-172 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOG, Log, All);`

<!-- header lines 172-172 at b9f6d81 -->
```
// Fallback for unrecognized prefixes
```

**Where it went:** Orientation — §4.

#### Header lines 174-174 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGRelayProbe, Log, All);`

<!-- header lines 174-174 at b9f6d81 -->
```
// Client relay telemetry: RelayProbe.Read / .Arrival / .Stale. Own category, see the banner. §4
```

**Where it went:** Orientation — §4. ⚠ *"Client"* is wrong for `[RelayProbe.Frame]` (§11 C8).

#### Header lines 176-176 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGDivergenceProbe, Log, All);`

<!-- header lines 176-176 at b9f6d81 -->
```
// Client prediction-vs-authority telemetry: DivergenceProbe.Correction / .Window. §4
```

**Where it went:** Orientation — §4.

#### Header lines 178-178 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGResimProbe, Log, All);`

<!-- header lines 178-178 at b9f6d81 -->
```
// Client resim-gate telemetry: ResimProbe.Gate / .Chaos / .Apply / .Landing / .Request / .Stranded. §4
```

**Where it went:** Orientation — §4. The list is not exhaustive; §4's table is (it adds `.Session`, `.Frame`, `.PushTarget`, `.PushVerdict`).

#### Header lines 180-180 — above `DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);`

<!-- header lines 180-180 at b9f6d81 -->
```
// Game-rule logging (DAttackMachine/Radial/Guard via OGBLOG_G)
```

**Where it went:** Orientation — §4.

#### Header lines 183-193 — above `struct FMovementStaticDataCVars`

<!-- header lines 183-193 at b9f6d81 -->
```
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
```

**Where it went:** Rationale. The seam's two enforceable halves are held elsewhere: the member `m_movementStaticDataCVars` is `const` (task 11 measured a write through it → `C3490`), and the read happens once per manager, in a member initializer in `MovementSchemeCVar.cpp`, behind the consumed latch. *"Nothing in the tick path may consult a console variable"* is a rule about code in other files; it has no site here and gets no tag.

#### Header lines 200-202 — above `float    gravity;`

<!-- header lines 200-202 at b9f6d81 -->
```
	// ⚠ NOT A CVAR. The ENGINE's gravity, captured at the same instant so the sim's own
	// gravity law starts life agreeing with it; `BeginPlay` then `checkf`s that agreement
	// against the WORLD's gravity, which a level is allowed to override and this is not.
```

**Where it went:** Rationale — the gravity `checkf` in `BeginPlay` is what enforces the agreement.

#### Header lines 208-209 — above `using BrawlerInputProviderFn = std::function<simulatableBrawler::PlayerInput(`

<!-- header lines 208-209 at b9f6d81 -->
```
// The provider signature, named once so the four sites passing one cannot drift apart. §5
// ⛔ The raw-capture history is a PARAMETER: the sequence matcher runs inside the provider.
```

**Where it went:** **`⛔G-01`**.

#### Header lines 257-275 — above `struct FRewindPushProbeStashedBody`

<!-- header lines 257-275 at b9f6d81 -->
```
// ── [netcode-v2 task 10 rework 1] ONE BODY'S HALF OF THE REWIND-PUSH PROBE ─────
//
// ⭐⭐ THIS TYPE EXISTS BECAUSE THE VERDICT READ HAD TO MOVE, and moving it split
// the probe across two physics-thread hooks. `FRewindData::SetTargetStateAtFrame`
// writes a history buffer; it does NOT touch the particle. So a read taken beside
// the push returns the pre-push state BY CONSTRUCTION, and a "match" read there
// could only ever mean the pushed value already equalled the live one - it can
// never mean "the engine read the target". The verdict read is therefore taken one
// hook later, at the top of OnPreSimulate_Internal on the resetting frame, after
// the engine has had its chance to consume the target and before OG's own systems
// write the body. These are the values that have to survive that gap.
//
// ⛔ PHYSICS THREAD ONLY, and written only while LogOGResimProbe is at Verbose.
// Nothing reads it from the game thread, so it is not a fourth crossing.
//
// ⛔ `bodyName` IS A STRING LITERAL (a declaration's `D::name`), never owned and
// never freed. Storing a `const char*` is safe only for that reason.
//
// Rationale, the token table and the residual blind spot: docs/SimulationManagerUImpl-rationale.md §8
```

**Where it went:** Lines 272-273 → **`⛔G-02`**. The rest is rationale — §8, *The rewind-push probe*.

#### Header lines 287-300 — above `class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<`

<!-- header lines 287-300 at b9f6d81 -->
```
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
```

**Where it went:** **Converted to a `static_assert`** (task 11): for every `ESimCallbackOptions` bit whose base hook is `check(false)`, the header asserts the bit is absent or the hook is overridden. Seen to fire when `ContactModification` is re-added and when `OnPostSolve_Internal`'s override is removed. The `ContactModification` history is rationale and lives only here.

#### Header lines 330-337 — above `TArray<FRewindPushProbeStashedBody> m_pushProbeStash;`

<!-- header lines 330-337 at b9f6d81 -->
```
// ── [netcode-v2 task 10 rework 1] THE PROBE'S STASH AND ITS TWO READ POINTS ────
//
// ⛔ `m_pushProbeStashFrame == INDEX_NONE` IS THE "NOTHING PENDING" STATE, and it is
// the ONLY thing that decides whether the drain runs. A stash still pending when the
// next rewind starts, or when OnPreSimulate_Internal runs on a frame that is not
// resetting, was NEVER READ at its verdict point - discardPushProbeStash says so out
// loud as `verdict=UNREAD` rather than dropping it, because a silently dropped stash
// is indistinguishable from a rewind that never happened. §8
```

**Where it went:** Rationale — §8 (the `UNREAD` verdict). The forbidden edits (silently dropping a stash, deciding the drain on anything but the frame sentinel) are typed in the `.cpp`, which keeps its own fences.

#### Header lines 360-360 — above `static ASimulationManagerUImpl* instanceFor(bool isAuthority)`

<!-- header lines 360-360 at b9f6d81 -->
```
// slot 0 = authority world, slot 1 = pure client. PIE can fill both.
```

**Where it went:** Orientation — §0.

#### Header lines 366-366 — above `const ServerTickClock& getServerClock() const { return` …

<!-- header lines 366-366 at b9f6d81 -->
```
    // Clock accessors — forwarded from the manager (type-erased here for callers outside the template).
```

**Where it went:** Orientation.

#### Header lines 372-372 — above `void requestInputDelayIncreaseStall(int32 deltaDelayTicks)`

<!-- header lines 372-372 at b9f6d81 -->
```
// Stall debt for a positive tier delta. ⛔ getClientClock() std::terminates on a server. §5
```

**Where it went:** **`⛔G-03`**.

#### Header lines 382-386 — above `m_manager->onGameSimulation(info);`

<!-- header lines 382-386 at b9f6d81 -->
```
// Inbound-hit routing runs inside m_manager->onGameSimulation, via
// brawlerHitRouting::System::postIntegrate - every tick, resim replays included. §5
//
// There is no adapter-side routing wrapper anymore: the former routeInboundHits() shim and
// its map are gone, and the system's single postIntegrate pass leaves no reset-order hazard.
```

**Where it went:** Rationale — §5, *Inbound-hit routing*.

#### Header lines 393-395 — above `void noteResimRequest(unsigned int anchorTick, int32 lastCompletedStep, int32` …

<!-- header lines 393-395 at b9f6d81 -->
```
// The two Chaos-side resim-gate probe feeds (request, grant). §8
// ⛔ WHY OUR OWN COUNTERS: engine-side refusals sit behind DEBUG_REWIND_DATA, so a refused
// rewind is COMPLETELY SILENT and we retry next frame. These two are that gate's visibility.
```

**Where it went:** Rationale — §8, *The resim-gate feeds*.

#### Header lines 410-410 — above `void onPostSimulationGameThread();`

<!-- header lines 410-410 at b9f6d81 -->
```
// Defined in .cpp - needs the full USimmableUpdateComponent for NetSync instantiation.
```

**Where it went:** **Already held by the compiler.** Task 11 inlined the body and it failed to compile (`C2027`, `USimmableUpdateComponent` undefined inside `SimulationNetSync.h`). No tag.

#### Header lines 416-416 — above `FSmallSimulationStateSyncBuffer& getSyncedTimingBuffer();`

<!-- header lines 416-416 at b9f6d81 -->
```
    // SimulationManagerOwnerConcept members — timing buffer forwarded through relay.
```

**Where it went:** Orientation.

#### Header lines 423-423 — above `virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime)` …

<!-- header lines 423-423 at b9f6d81 -->
```
    // Called from ASimulationTimingRelay::OnRep_Buffer on clients via ISimulationTimingRelayListener.
```

**Where it went:** Orientation — §0.

#### Header lines 458-458 — above `const ChaosPhysicsBodyReaderAdapter& getPhysicsBodyReaderAdapter() const {` …

<!-- header lines 458-458 at b9f6d81 -->
```
// Read-only body adapter over GT-interpolated state; const because the concept requires only const.
```

**Where it went:** Rationale.

#### Header lines 465-466 — above `std::optional<simulatableBrawler::PlayerInput> getLastRelayedInput(unsigned int` …

<!-- header lines 465-466 at b9f6d81 -->
```
// The newest input the server RELAYED for `id`, or nullopt when there is none to have. §6
// ⛔ getLatestInput and its whole column are DELETED: this is the ONLY remote source now.
```

**Where it went:** Rationale — §6. `getLatestInput` is an absence fence in this document's lint escapes.

#### Header lines 472-475 — above `const LocalInputCache<simulatableBrawler::PlayerInput>* getLocalInputCache` …

<!-- header lines 472-475 at b9f6d81 -->
```
// This client's own raw captures for `id`, or nullptr when none. Diagnostics only. §1
// ⛔ GAME-THREAD DOOR ONTO A PHYSICS-WRITTEN RING -- NOT same-thread with its writer, as
//   getLastRelayedInput is. Its one caller is pollInputHistory, which owns the tear. §1
// ⛔ m_reconciliation needs NO twin of this: editReconciliation() is already public.
```

**Where it went:** Rationale — §1. ⚠ *"Its one caller"* is false (§11 C6). Pointer-to-const is held by the compiler (task 11: `C2440`).

#### Header lines 481-485 — above `bool isLocallyControlledOnThisPeer(unsigned int id) const`

<!-- header lines 481-485 at b9f6d81 -->
```
// A capture line exists for exactly the characters this client drives, which stays right
// under couch co-op and on a listen host -- both of which a ROLE test gets wrong. Every
// meter that needs the answer asks HERE; a second test anywhere in the meter path would
// let two parts of one stack disagree about whose character it is drawing.
// ⛔⛔ THE ONE LOCALITY TEST THE DISPLAY MAKES, AND THE ONLY PLACE IT IS SPELLED. §1
```

**Where it went:** Rationale. The forbidden edit — a second locality test in a meter — is typed in another file. At the one site in this header where it could be typed, it is **`⛔G-06`**. ⚠ *"ONLY PLACE IT IS SPELLED"* overstates (§11 C15).

#### Header lines 491-494 — above `void pollInputHistory(unsigned int id, uint32 newestTick, float deadzone)`

<!-- header lines 491-494 at b9f6d81 -->
```
// ---- THE INPUT-HISTORY DISPLAY ----------------------------------------
//
// One row ring per LOCAL character, keyed by character id, fed by a render-rate poll.
// Nothing here is replicated, enters a correction payload, or reaches compute_checksum.
```

**Where it went:** Orientation — §1.

#### Header lines 496-496 — above `void pollInputHistory(unsigned int id, uint32 newestTick, float deadzone)`

<!-- header lines 496-496 at b9f6d81 -->
```
// Sweep `id`'s resident capture window into its ring. ⛔ THIS IS THE READ CROSSING. §1
```

**Where it went:** Rationale — §1, *The read crossing*.

#### Header lines 506-506 — above `const brawlerInputHistoryVisualization::InputHistoryRowRing*` …

<!-- header lines 506-506 at b9f6d81 -->
```
// The folded rows for `id`, or nullptr when it has none. Read-only; the panel's one source.
```

**Where it went:** Orientation. Pointer-to-const is held by the compiler (task 11: `C2440`).

#### Header lines 513-533 — above `std::optional<brawlerRingout::State> getRingoutVizState(unsigned int id) const`

<!-- header lines 513-533 at b9f6d81 -->
```
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
```

**Where it went:** Lines 521-525 → **`⛔G-04`**. Lines 527-530 → **converted to a `static_assert`** on the accessor's signature (const member, returns by value), seen to fire on a non-const and on a pointer-returning poison. The rest is rationale.

#### Header lines 543-558 — above `void pollInputHistoryLanes(unsigned int id, uint32 liveTick, DAttackState` …

<!-- header lines 543-558 at b9f6d81 -->
```
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
```

**Where it went:** Line 549 → **`⛔G-05`**. The rest is rationale — §1.

#### Header lines 569-571 — above `const bool isLocallyControlled = isLocallyControlledOnThisPeer(id);`

<!-- header lines 569-571 at b9f6d81 -->
```
// The same test `pollInputHistory` makes, and for the same reason: a capture line
// exists for exactly the characters this client controls. ⛔ NOT A ROLE TEST -- a
// client can control several brawlers, and a listen-server host controls one. §1
```

**Where it went:** **`⛔G-06`**.

#### Header lines 574-575 — above `const std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition>` …

<!-- header lines 574-575 at b9f6d81 -->
```
// A remote proxy's stack shows the relay it is being predicted from instead.
// ⛔ THE TIER DECOMPOSITION IS THE LOCAL CONNECTION'S and is not read for a remote.
```

**Where it went:** **`⛔G-07`**.

#### Header lines 588-588 — above `std::optional<brawlerInputHistoryVisualization::ClockDriftReading> clock;`

<!-- header lines 588-588 at b9f6d81 -->
```
// ⛔ GUARDED LIKE THE OFFSET ABOVE: getClientClock() std::terminates on a server. §5
```

**Where it went:** **`⛔G-08`**.

#### Header lines 599-600 — above `reading.driftTicks     = static_cast<int32_t>(reading.targetTick)`

<!-- header lines 599-600 at b9f6d81 -->
```
// ⛔ CAST BEFORE THE SUBTRACTION -- a negative drift is the whole point, and two
//   unsigned ticks would wrap it into a vast positive.
```

**Where it went:** **Dropped — the stated consequence is false** (§11 C9, measured).

#### Header lines 605-606 — above `reading.skipCount              = predictionClock.getDiagnostics().skipCount();`

<!-- header lines 605-606 at b9f6d81 -->
```
// ⛔ THE SEAM, THROUGH THE VIEW: the clock publishes no bare accessor for any of these.
// ⛔ COUNT BEFORE ITS TICK: the clock writes tick then count, so the tear is one-way.
```

**Where it went:** Line 605 is **already held by the compiler** (task 11: `C2039` / `C2248`). Line 606 → **`⛔G-09`**, with an R0 annotation (§11 C14).

#### Header lines 619-624 — above `const RelayedReadObservationRing* const relayedReads =`

<!-- header lines 619-624 at b9f6d81 -->
```
// A REMOTE proxy's delay-bar client half comes from the relayed reads this client
// actually served for it, which is the one thing a tier reading cannot say. Absent
// when the delay bar is off or this id has no ring, and then the half stays empty --
// which the verdict already has a word for.
// ⛔ ONE SOURCE PER POLL, CHOSEN HERE AND NOWHERE ELSE: the two calls below differ in
//   the TYPE they pass, so neither poll can compile the other's read. §1
```

**Where it went:** Rationale. The type selection it describes is **already held by the compiler**: the pure poll's pairing `static_assert` fired when task 11 passed mismatched sources (`C2338`).

#### Header lines 628-629 — above `const RelayedInputArrivalRing* const relayedArrivals =`

<!-- header lines 628-629 at b9f6d81 -->
```
// The relay-health bar's second source, taken beside the first so a poll can never hold
// one without the other -- the pure poll static_asserts exactly that pairing.
```

**Where it went:** Rationale — the pairing `static_assert` is real (task 11 fired it).

#### Header lines 633-637 — above `const uint32 rollbackWindowTicks =`

<!-- header lines 633-637 at b9f6d81 -->
```
// How far back a resim may still reach, which is what separates an arrival that could
// still have been replayed into a tick from one that could not.
// The per-tier ceiling beside it escalates nothing today and its own banner says so, and
// a negative value disables the authority's future guard and is no window at all here.
// ⛔ THE SHIPPED WINDOW, `TimeConfig::rollbackWindowTicks`. §5
```

**Where it went:** **`⛔G-10`**.

#### Header lines 647-650 — above `if (relayedReads != nullptr && relayedArrivals != nullptr)`

<!-- header lines 647-650 at b9f6d81 -->
```
// The relay-health bar rides the same two sources and is its own toggle, so the delay bar
// being off must not blind it; `includeDelay` still decides the tier decomposition above,
// which is the reading it actually names.
// ⛔ NOT GATED ON `includeDelay`.
```

**Where it went:** **`⛔G-11`**.

#### Header lines 664-668 — above `const RelayedReadObservationRing* getRelayedReadObservations(unsigned int id)` …

<!-- header lines 664-668 at b9f6d81 -->
```
// The relayed reads this client SERVED for `id`, or nullptr when it holds none -- a
// locally controlled character, or one whose registration has not completed.
// The accepted game-thread read of a physics-written ring is argued at that ring's own
// declaration; this adds no posture of its own.
// ⛔ POINTER TO CONST, straight through the resolution peer's diagnostic view. §1
```

**Where it went:** Rationale. Pointer-to-const is held by the compiler (task 11: `C2440`).

#### Header lines 675-677 — above `const RelayedInputArrivalRing* getRelayedInputArrivals(unsigned int id) const`

<!-- header lines 675-677 at b9f6d81 -->
```
// When each relayed capture for `id` first arrived, or nullptr when it holds none. It is
// game-thread on both sides -- the arrival door and this read -- so it opens no crossing.
// ⛔ POINTER TO CONST, straight through the resolution peer's diagnostic view. §1
```

**Where it went:** Rationale. Pointer-to-const is held by the compiler (task 11: `C2440`).

#### Header lines 684-690 — above `brawlerInputHistoryVisualization::RelayReadReadout getRelayReadReadout(unsigned` …

<!-- header lines 684-690 at b9f6d81 -->
```
// The relay reading the nearest stack shows where the primary shows its tier line: the
// newest schedule stamp, and how the scheduled read has been going across the
// observations the ring still holds.
// This counts, the pure header defines what is true, and the HUD builds the string.
// ⛔ THE SPLIT `gatherScoreboardRows` ALREADY KEEPS.
// ⛔ A TALLY OVER WHAT IS RESIDENT, never a session total -- a run of misses that has
//   scrolled out is not a run of misses that is happening.
```

**Where it went:** Rationale.

#### Header lines 716-718 — above `if (!anyObservation || observation->simTick > newestSimTick)`

<!-- header lines 716-718 at b9f6d81 -->
```
// The stamp of the NEWEST observation, found by its own tick: the ring is addressed by
// sim tick and therefore walked out of order, so the last slot read is not the last one
// written. ⛔ NEVER THE LAST SLOT VISITED.
```

**Where it went:** **`⛔G-12`**.

#### Header lines 732-739 — above `void gatherNearestCandidates(`

<!-- header lines 732-739 at b9f6d81 -->
```
// Every registered brawler's id and its viz-copy position in the ground plane -- the
// one gather the nearest-character selection needs, and the only thing this class says
// about it. The CHOICE is the pure header's.
//
// `updateVisualizationAll(m_storage)` is the sanctioned physics->game handoff, and a HUD
// reading live state would be a fresh, unargued crossing.
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()` -- the ring-out slice's reason. §1
// ⛔ FILLS A FIXED LIST AND ALLOCATES NOTHING -- this runs once per drawn frame.
```

**Where it went:** Lines 736-738 → **`⛔G-13`**. Line 739 is rationale: the parameter is a fixed-capacity `std::array` wrapper whose `add` refuses when full, so nothing here allocates. A future allocation would have to be written deliberately, and nothing checks for it.

#### Header lines 754-754 — above `const brawlerInputHistoryVisualization::InputHistoryTickLanes*` …

<!-- header lines 754-754 at b9f6d81 -->
```
// The per-tick lanes for `id`, or nullptr when it has none. Read-only; the bars' one source.
```

**Where it went:** Orientation.

#### Header lines 762-762 — above `const TimeConfig* getTimeConfigPtr() const`

<!-- header lines 762-762 at b9f6d81 -->
```
// The one shared TimeConfig, to BIND not copy. ⛔ Pointer, so pre-construction is not UB. §2
```

**Where it went:** **Converted to a `static_assert`** on `getTimeConfigPtr`'s signature (returns `const TimeConfig*`), seen to fire on a reference-returning poison. §2 keeps the reasoning.

#### Header lines 768-768 — above `void publishClientEffectiveInputDelayTicks(int32 delayTicks)`

<!-- header lines 768-768 at b9f6d81 -->
```
// Publish the client's effective input delay - the one game->physics crossing. §1 §5
```

**Where it went:** Rationale — §1, §5.

#### Header lines 779-782 — above `virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)` …

<!-- header lines 779-782 at b9f6d81 -->
```
// ---- CLIENT TIER CONSUMPTION ------------------------------------------
//
// One consumer per WORLD, not per CHARACTER: a tier is a property of the wire, and N
// per-character consumers stalled one debt-accumulating clock N times per transition. §5
```

**Where it went:** Rationale — §5.

#### Header lines 784-784 — above `virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)` …

<!-- header lines 784-784 at b9f6d81 -->
```
// A tier TRANSITION arrived: apply, republish, convert old->new into a stall.
```

**Where it went:** Rationale — §5.

#### Header lines 787-787 — above `virtual void onConnectionTierReplayed(uint8_t tier) override;`

<!-- header lines 787-787 at b9f6d81 -->
```
// A tier latched before this bind: apply + republish, ⛔ never stall - it is the first. §5
```

**Where it went:** Rationale — §5, *Replayed values never stall*. The forbidden edit is typed in the `.cpp`.

#### Header lines 790-796 — above `virtual void onInputRelayHostReady(ASimulationInputRelay& host) override;`

<!-- header lines 790-796 at b9f6d81 -->
```
// ---- THE RELAY-RING HOST BOUNDARY -------------------------------------
//
// ⛔ A BRIDGE and nothing more: OGSimulationUnreal must not depend on OGBrawlerUnreal. §6
//
// ⛔ NO MAP, DELIBERATELY: three lookups on data that already replicates beat a registry.
//
// IDEMPOTENT: three independent link paths call this.
```

**Where it went:** Rationale — §6, *The host boundary*.

#### Header lines 799-799 — above `const ReplicatedTierConsumer* getReplicatedTierConsumer() const`

<!-- header lines 799-799 at b9f6d81 -->
```
// Read-only handle on the client tier cache (nullptr before BeginPlay built the manager).
```

**Where it went:** Orientation.

#### Header lines 805-809 — above `virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;`

<!-- header lines 805-809 at b9f6d81 -->
```
// ---- THE RELAY DELAY FLOOR --------------------------------------------
//
// The SECOND recompute input. ⛔ Either OnRep can land first, so ONE recompute holds both. §5
//
// Both arms live in ReplicatedTierConsumer::effectiveInputDelayTicks, which the server mirrors.
```

**Where it went:** Rationale — §5.

#### Header lines 811-811 — above `virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;`

<!-- header lines 811-811 at b9f6d81 -->
```
// A floor CHANGE arrived: stamp, re-derive, republish, pay for an INCREASE with a stall.
```

**Where it went:** Rationale — §5.

#### Header lines 814-814 — above `virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) override;`

<!-- header lines 814-814 at b9f6d81 -->
```
// A floor latched before this bind: apply + republish, ⛔ no stall. Mirrors the tier replay.
```

**Where it went:** Rationale — §5.

#### Header lines 817-824 — above `using BrawlerReceptionCoordinator =`

<!-- header lines 817-824 at b9f6d81 -->
```
// ---- SERVER-AUTHORITATIVE RTT TIER + INPUT DELAY ----------------------
//
// ⛔ THE SERVER IS THE SOLE OWNER OF THE RTT TIER: the client never computes or samples one,
// which is what makes tier disagreement impossible rather than merely unlikely. §5
//
// The whole reception subsystem is in the core coordinator; this manager is its adapter. §7
//
// ⛔ Authority-role only, nullopt on a pure client: it borrows m_manager's TimeConfig. §2
```

**Where it went:** Rationale — §5, §7. The forbidden edits (a client-side tier computation; emplacing on a pure client) are typed in other files or in `BeginPlay`.

#### Header lines 829-831 — above `BrawlerReceptionCoordinator* getReceptionCoordinator()`

<!-- header lines 829-831 at b9f6d81 -->
```
// ---- ENGINE-PRIMITIVE ACCESSORS for the RPC-boundary adapter ----------
//
// ⛔ These three are all this manager supplies, and none carries netcode policy. §7
```

**Where it went:** Rationale — §7.

#### Header lines 833-833 — above `BrawlerReceptionCoordinator* getReceptionCoordinator()`

<!-- header lines 833-833 at b9f6d81 -->
```
// The coordinator this manager owns, or nullptr on a pure client / pre-BeginPlay.
```

**Where it went:** Orientation.

#### Header lines 839-839 — above `int32 getServerReceptionTick() const`

<!-- header lines 839-839 at b9f6d81 -->
```
// The server SIM TICK for RTT samples. ⚠ WART: physics write, game read; the EMA absorbs it. §7
```

**Where it went:** Rationale — §1, *One acknowledged wart*.

#### Header lines 845-845 — above `void noteDelayedInputComponent(unsigned int id, USimmableUpdateComponent&` …

<!-- header lines 845-845 at b9f6d81 -->
```
// The id->component mapping `deliver` resolves. ⛔ ONCE at register-time, not per slot. §7 §10
```

**Where it went:** Rationale — §7, §10. The forbidden edit is at the caller, in `SimmableUpdateComponent.cpp`.

#### Header lines 848-848 — above `void deliverRemoteInput(unsigned int id, uint32 captureTick,`

<!-- header lines 848-848 at b9f6d81 -->
```
// The coordinator's RemoteInputDeliverySink - the ONE method drain and fallback share. §7
```

**Where it went:** Rationale — §7, *The two sinks*.

#### Header lines 852-857 — above `void relayRemoteInput(unsigned int id, uint32 captureTick, uint8 dA,`

<!-- header lines 852-857 at b9f6d81 -->
```
// ALSO its RemoteInputRelaySink - the OUTBOUND half, staging for the OTHER clients. §6
//
// ⚠ That ring is `ASimulationInputRelay::m_relayedInputRing` under COND_SkipOwner - NOT the
// component's m_detachedRelayRing, which is the no-host fallback nothing replicates. §11
//
// `dA` is the SCHEDULE STAMP: a peer derives the application tick as captureTick + dA.
```

**Where it went:** Rationale — §6.

#### Header lines 861-876 — above `static constexpr int32 kPreDietCharacterCap = 4;`

<!-- header lines 861-876 at b9f6d81 -->
```
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
```

**Where it went:** The derivation (*"WHY 4"*) → **`∴D-01`**, which points at §10's *The pre-diet character cap ∴D-01*. Lines 873-876 → **converted to a `static_assert`** against `brawlerRingout::kMaxSpawnPoints`, seen to fire at cap 5. ⚠ Their stated mechanism was wrong (§11 C7). *"DELETED BY THE WIRE DIET"* describes the future (item 40); nothing has deleted the constant yet.

#### Header lines 880-880 — above `std::set<unsigned int> m_authorityRegisteredIds;`

<!-- header lines 880-880 at b9f6d81 -->
```
// The cap's denominator. ⛔ A SET, not a counter: asymmetric ends would disarm the cap. §10
```

**Where it went:** **Converted to a `static_assert`** (`std::is_same_v<decltype(m_authorityRegisteredIds), std::set<unsigned int>>`), seen to fire on a counter. §10 keeps the reasoning.

#### Header lines 883-898 — above `brawlerRingout::SpawnSlotAllocator m_spawnSlots;`

<!-- header lines 883-898 at b9f6d81 -->
```
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
```

**Where it went:** Rationale. The authority-only population is typed in the `.cpp`; the two allocator properties are `Ringout.SpawnSlots.*` cases in `BrawlerRingoutSimulationTest.cpp`.

#### Header lines 902-922 — above `void seedRingoutSpawnPointsFromLevel(UWorld& world);`

<!-- header lines 902-922 at b9f6d81 -->
```
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
```

**Where it went:** **`⛔G-14`**, with R0 annotations (§11 C12, C19).

#### Header lines 925-927 — above `bool m_ringoutSpawnPointsSeeded = false;`

<!-- header lines 925-927 at b9f6d81 -->
```
// EXACTLY ONCE, and it is enforced rather than asserted — see the `checkf` at the definition.
// ⚠ PER MANAGER INSTANCE: a listen server runs TWO of these actors, and each owns its own
// `m_staticData`, so each seeds its own table once.
```

**Where it went:** Rationale. *"EXACTLY ONCE"* is held by the `checkf` at the definition. ⚠ *"a listen server runs TWO of these actors"* is false; it is PIE (§11 C11).

#### Header lines 931-934 — above `void releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps);`

<!-- header lines 931-934 at b9f6d81 -->
```
// ---- TIER INPUT DELAY: RELEASE ----------------------------------------
//
// Primitive acquisition plus the core call: upcoming sim tick (mapper +1, §9), the per-id
// `deliver` callback, the drain, the reap. GAME THREAD. ⛔ No netcode policy. §7
```

**Where it went:** Rationale — §7, §9.

#### Header lines 937-939 — above `void applyReplicatedConnectionTier(uint8 tier);`

<!-- header lines 937-939 at b9f6d81 -->
```
// ---- CLIENT TIER CACHE INTERNALS --------------------------------------
//
// Feed one tier into the cache and republish through the shared recompute.
```

**Where it went:** Rationale — §5.

#### Header lines 942-942 — above `void applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease);`

<!-- header lines 942-942 at b9f6d81 -->
```
// Feed one FLOOR into the shared TimeConfig; `payForIncrease` tells an OnRep from a replay.
```

**Where it went:** Rationale — §5.

#### Header lines 945-945 — above `void logRelayDelayFloorAdvisory(int32 floorTicks);`

<!-- header lines 945-945 at b9f6d81 -->
```
// ADVISORY-ONLY, from BOTH intake points. ⛔ Never an assert: floor 0 must stay silent. §3
```

**Where it went:** **`⛔G-15`**.

#### Header lines 948-952 — above `int32 recomputeAndPublishEffectiveInputDelay();`

<!-- header lines 948-952 at b9f6d81 -->
```
// THE SHARED TWO-INPUT RECOMPUTE over the floor and the tier. §5
//
// ⛔ ONE SITE, BOTH CHANNELS: two half-formula writers answer stale when OnReps interleave.
//
// RETURNS the change in published delay, so a caller that must pay for an increase can.
```

**Where it went:** **`⛔G-16`**.

#### Header lines 955-955 — above `void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);`

<!-- header lines 955-955 at b9f6d81 -->
```
// ⛔ THE DECISION is shouldStallForTierTransition, which has the LLT coverage this lacks. §5
```

**Where it went:** Rationale — §5. The decision's own coverage is `shouldStallForTierTransition`'s low-level tests.

#### Header lines 958-961 — above `std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;`

<!-- header lines 958-961 at b9f6d81 -->
```
// The client's ENTIRE share of the tier system, over the lookups the server also uses. §5
//
// ⛔ Emplaced on BOTH roles: a listen-server host runs client paths on an AUTHORITY manager,
// where an unbound cache would answer 0 and the no-tier fallback is what is correct. §5
```

**Where it went:** Rationale — §5. The forbidden edit (emplacing on one role only) is typed in `BeginPlay`.

#### Header lines 964-964 — above `int32 m_lastPublishedEffectiveInputDelayTicks = 0;`

<!-- header lines 964-964 at b9f6d81 -->
```
// The last published effective delay. ⛔ Kept here, not read back out of the atomic. §1
```

**Where it went:** Rationale — §5.

#### Header lines 967-969 — above `std::optional<BrawlerReceptionCoordinator> m_receptionCoordinator;`

<!-- header lines 967-969 at b9f6d81 -->
```
// THE reception subsystem, relocated whole into the core. Authority-only. §2 §7
//
// ⛔ THREADING, LOAD-BEARING: game thread ONLY - no container here synchronizes itself. §1
```

**Where it went:** Rationale — §1, §7.

#### Header lines 972-977 — above `FrameHealthProbe m_frameHealthProbe;`

<!-- header lines 972-977 at b9f6d81 -->
```
// PROBE A - sim ticks per game-thread frame, i.e. FRAME HEALTH. Diagnostic only. §8
//
// ⛔ WHY HERE AND NOT IN SimulationNetSync with the other three: the only tick source legal
// on this thread is the ChaosTickMapper offset, which this actor owns and NetSync cannot reach. §9
//
// One instance per actor, one actor per role, so nothing is shared across roles. §1
```

**Where it went:** Rationale — §8, PROBE A. ⚠ *"with the other three"* is stale (§11 C13).

#### Header lines 980-991 — above `RelayWriteProbe       m_relayWriteProbe{`

<!-- header lines 980-991 at b9f6d81 -->
```
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
```

**Where it went:** Line 991 → **`⛔G-17`**. The rest is rationale — §8, PROBES 5 and 6.

#### Header lines 996-998 — above `std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>`

<!-- header lines 996-998 at b9f6d81 -->
```
// Adapter-side delivery resolution: the core claim map is id-keyed, so its `deliver`
// callback hands back an id and THIS map resolves it to the owning component. §7
// ⛔ Erased in unregisterFromNewFramework - the contract replacing the core's GC read. §10
```

**Where it went:** Rationale — §7, §10.

#### Header lines 1002-1003 — above `inputHistoryVisualizationUImpl::InputHistoryStore m_inputHistory;`

<!-- header lines 1002-1003 at b9f6d81 -->
```
// The display's rings, one per polled character id. GAME THREAD, unsynchronized, like
// every other diagnostic member here. ⛔ Reaped in unregisterFromNewFramework. §1 §10
```

**Where it went:** Rationale — §1, §10.

#### Header lines 1017-1017 — above `std::optional<ChaosPhysicsBodyAdapter>   m_physAdapter;`

<!-- header lines 1017-1017 at b9f6d81 -->
```
// Adapters require the Chaos solver - emplaced in BeginPlay.
```

**Where it went:** Orientation.

#### Header lines 1022-1029 — above `static FMovementStaticDataCVars readMovementStaticDataCVars();`

<!-- header lines 1022-1029 at b9f6d81 -->
```
// ⭐⭐ THE ONE-TIME CVAR READ - [movement-sim task 16, ruling #3]. DEFINED IN
// `MovementSchemeCVar.cpp`, NOT in SimulationManagerUImpl.cpp, and that is deliberate: it is
// the TU that registers the four variables, so the read is a direct load of the very objects
// the console writes rather than a `FindConsoleVariable` lookup by string that can miss a
// rename in silence. That TU also owns the "consumed" latch the four sinks test before warning
// that a mid-session change is being ignored, and the refused-name sweep (a stale ini naming a
// constant that no longer exists is reported LOUDLY there, per the obligation routed from
// task 56). Reads the engine's default gravity at the same instant.
```

**Where it went:** Rationale. `MovementSchemeCVar.cpp`'s include comment used to say *"see that declaration's comment for why"*. Since task 11 it points here.

#### Header lines 1032-1035 — above `using BrawlerSimulatables    = SimulatableList<SimulatableBrawler>;`

<!-- header lines 1032-1035 at b9f6d81 -->
```
// ---- SIMULATABLE-PACK ALIAS CHAIN -------------------------------------
//
// Single source of truth: widen this one alias and every type below inherits it.
// ⛔ CLASS-scoped so these names cannot leak to global scope from an adapter header. §2
```

**Where it went:** **`⛔G-18`**.

#### Header lines 1039-1039 — above `using BrawlerInputResolution = apply_t<SimulationInputResolution` …

<!-- header lines 1039-1039 at b9f6d81 -->
```
// The resolution peer's own alias - a composition-root sibling, not a NetSync member.
```

**Where it went:** Rationale — §2.

#### Header lines 1043-1043 — above `BrawlerStorage m_storage;`

<!-- header lines 1043-1043 at b9f6d81 -->
```
// Owned resources + peers, typed from the alias chain. ⛔ Order matters - see below. §2
```

**Where it went:** **Converted to a `static_assert`** — the construction-order assertion (§2).

#### Header lines 1046-1049 — above `const FMovementStaticDataCVars m_movementStaticDataCVars =` …

<!-- header lines 1046-1049 at b9f6d81 -->
```
// ⭐ [movement-sim task 16] THE ONE-TIME CVAR READ, AND IT IS DECLARED HERE FOR A REASON:
// members construct in DECLARATION order, so this one must sit IMMEDIATELY ABOVE m_staticData,
// which reads it in its own initializer. Moving it below is not a style change - it is
// reading an uninitialized object, silently, exactly as the banner below warns. §2
```

**Where it went:** **Converted to a `static_assert`** — the construction-order assertion (§2) includes `m_movementStaticDataCVars < m_staticData`.

#### Header lines 1052-1058 — above `simulatableBrawler::StaticData m_staticData{`

<!-- header lines 1052-1058 at b9f6d81 -->
```
// Game static data - the ownership ROOT for StaticData across the whole tree.
//
// ⛔ NEVER copied or moved: nested sub-StaticData binds sibling references that a copy dangles. §2
// ⭐ Its FIVE movement parameters come from the one-time read above; every other constant it
// holds is still authored in SimulatableBrawlerTypes.h. This is the ONE call site in the tree
// that passes anything - every test peer default-constructs and therefore measures the
// authored literals.
```

**Where it went:** Line 1054 is **already held by the compiler**: `StaticData` deletes copy and move, and task 11 measured `C2280`. Lines 1055-1058 are rationale. ⚠ *"the ONE call site … every test peer default-constructs"* is false (§11 C10).

#### Header lines 1066-1071 — above `BrawlerReconciliation  m_reconciliation{ m_storage };`

<!-- header lines 1066-1071 at b9f6d81 -->
```
// ⛔ ENFORCED BY ONE THING ONLY: members construct in DECLARATION order, not list order.
//
// ⛔ NO `static_assert` and NO `-Wreorder`-as-error in any `.Build.cs`/`.Target.cs` here:
// a reorder compiles silently and constructs in the new, wrong order.
//
// ⛔ LOAD-BEARING the day a ctor BODY calls a sibling: UB, undiagnosed. Trivial today. §2
```

**Where it went:** **Converted to a `static_assert`** — the construction-order assertion. Its own sentence *"NO `static_assert`"* is therefore closed-tense history (§2).

#### Header lines 1073-1073 — above `BrawlerInputResolution m_inputResolution{ m_storage, m_reconciliation };`

<!-- header lines 1073-1073 at b9f6d81 -->
```
// Constructed BEFORE m_netSync - netSync's ctor takes a reference to this peer. §2
```

**Where it went:** **Converted to a `static_assert`** — the construction-order assertion.

#### Header lines 1077-1077 — above `template <typename... SimulatableTs>`

<!-- header lines 1077-1077 at b9f6d81 -->
```
// apply_t cannot fill the executor's three engine/game-specific slots; a bind wrapper does. §2
```

**Where it went:** Rationale — §2, *The alias chain*.

#### Header lines 1083-1083 — above `using BrawlerSystemsExec = SimulationSystemsExecutor<`

<!-- header lines 1083-1083 at b9f6d81 -->
```
// Fourth peer - systems executor over (pack marker, StaticData type, system pack).
```

**Where it went:** Orientation.

#### Header lines 1088-1093 — above `brawlerRingout::ScoreSystem>;`

<!-- header lines 1088-1093 at b9f6d81 -->
```
// ⛔ [ringout task 19] NO ROLE LOGIC LIVES HERE. Each system declares its own
// kRoleAffinity and the executor gates every hook on the role SimulationManager hands it at
// each fire - brawlerHitRouting::System is AllRoles and fires on all three, including every
// resim replay tick; brawlerRingout::ScoreSystem is AuthorityOnly and fires nowhere else.
// Nothing is wired at composition and nothing is stored: this alias just names the pack, and
// its ORDER is the firing order. See OGSimulation/SystemRoleAffinity.h.
```

**Where it went:** Rationale. The affinities are declared in `BrawlerHitRoutingSystem.h` and `BrawlerRingoutScoreSystem.h`. `ScoreSystem`'s is pinned by a `STATIC_REQUIRE` in `BrawlerRingoutScoreSystemTest.cpp`; nothing in this header can type a role, so there is no site to tag.

#### Header lines 1096-1096 — above `BrawlerSystemsExec m_systemsExec;`

<!-- header lines 1096-1096 at b9f6d81 -->
```
// Value-owned; default-constructs the routing and score systems. Passed by reference at emplace().
```

**Where it went:** Orientation.

#### Header lines 1099-1099 — above `using IntegrationLayerType = BrawlerIntegrationExec;`

<!-- header lines 1099-1099 at b9f6d81 -->
```
// Integration layer and manager require adapters - emplaced in BeginPlay.
```

**Where it went:** Orientation.

#### Header lines 1112-1117 — above `struct PendingRegistration`

<!-- header lines 1112-1117 at b9f6d81 -->
```
// ⭐ [movement-sim task 17] `BodyId parentBodyId` IS GONE. It cached the pawn root capsule's
// body id across the two-phase `tryRegister` for exactly two readers: the two-source tripwire
// (deleted with it) and the resolvability gate, which now reads the movement declaration's own
// `bindings.ownBodyId` — the same body, because that declaration's descriptor sets `isRoot`.
// The capsule id is still derived inside the first-call pass as a LOCAL, where the factory and
// `decl.bindings.parentBodyId` consume it; nothing needed it to survive the call.
```

**Where it went:** Rationale — §10, *The capsule body id*.

---

## §13 `SimulationManagerUImpl.cpp` — what its comments carried that §1–§12 did not

<!-- ================= DECLARED LINT ESCAPES — §13 ==========================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: FastLess -- Unreal Engine FName comparator, outside every scan root -->
<!-- lint-external-ref: FNameFastLess -- Unreal Engine FName comparator, outside every scan root -->
<!-- lint-external-ref: CompareIndexes -- Unreal Engine FName method, outside every scan root -->
<!-- lint-external-ref: bGlobalGravitySet -- Unreal Engine AWorldSettings field, outside every scan root -->
<!-- lint-external-ref: GlobalGravityZ -- Unreal Engine AWorldSettings field, outside every scan root -->
<!-- lint-external-ref: RewindToFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: PendingRegistration::parentBodyId -- ABSENCE FENCE (13.4): the record field deleted with the two-source tripwire. It must NOT resolve -->

The implementation file was converted to guard tags (`SimulationManagerUImpl-guards.md`, ids
**G-50 onward**) and one derivation tag (**D-50**, §13.8). Most of what its comments said was
already in §1–§11, which were written from those same comments (§12 carries the header's). This section holds the
rest: every comment whose content appeared nowhere above. Each is quoted as it shipped, so
this is a move and not a rewrite. Where verification found a quoted sentence false, the
correction sits beneath the quote and the quote is left intact. The corrections are collected
in §13.10.

### 13.1 The ring-out respawn points come from the level

*`seedRingoutSpawnPointsFromLevel`, and its call in `BeginPlay`.*

> ⭐⭐ [ringout task 9, 2026-09-13] THE RESPAWN POINTS ARE THE LEVEL'S `APlayerStart` ACTORS.
>
> USER RULING, 2026-09-13, out of a PIE session: the authored table in
> `brawlerRingout::StaticData` is placeholder authoring at (±200, ±200, Z=200), the platform
> this mode ships on is somewhere else, and every respawn therefore dropped the fighter off
> the map. Respawn slot N is now PLAYER START N, so moving a spawn point is level design.
>
> ══ ⛔⛔ WHY THIS WRITES `StaticData` AFTER CONSTRUCTION, WHICH THE BANNER FORBIDS ══════════
> §2's ownership rule is that `StaticData` is constructed in place once and never moves. What
> that rule PROTECTS is that no tick ever sees it change: a constant the simulation reads
> mid-session is a constant two peers can disagree about and a resim can replay against the
> wrong value. This is a ONE-TIME INIT that happens before the first tick can exist, which is
> inside what the rule protects rather than an exception to it. ⛔ DO NOT "FIX" IT BACK INTO
> A CONSTRUCTOR ARGUMENT: `m_staticData` is brace-initialised in a member initializer, which
> runs during ACTOR CONSTRUCTION, and no level actor is reachable then. `APlayerStart` exists
> at `BeginPlay` and not one moment earlier. (`readMovementStaticDataCVars()` has the same
> shape for the same reason, one member above it.)

⚠ **Correction C13-8.** §2 does not forbid a write. Its rule is that `StaticData` is never
**copied or moved**, and what it protects is the internal references its nested members hold to
their siblings. It says nothing about a value changing mid-session. So the conflict this banner
resolves does not exist. The two facts the banner relies on are still true: the write happens
once, before any tick, and no level actor is reachable at construction.

> ══ ⭐ AND THE ORDERING IS ENFORCED, NOT ASSUMED ═══════════════════════════════════════════
> The two `checkf`s below are the mechanical form of "exactly once, before the first
> integrate". The second is the load-bearing one: every simulation step in this process runs
> through `m_manager`, which is emplaced LATER IN THIS SAME `BeginPlay` (both role branches),
> and the physics delegates that drive it (`OnPhysScenePreTick` / `OnPhysSceneStep` /
> `OnPhysScenePostTick`) plus the async callback are bound at the very END of it. An empty
> `m_manager` is therefore proof that no tick has happened yet. ⚠ It is also why this must not
> drift below the `emplace` calls: `m_integrationLayer` holds a REFERENCE to `m_staticData`,
> so a later write would still be visible and would still compile — the assertion is what
> makes the ordering a stated requirement instead of an accident of line order.
>
> ══ ⛔ BOTH ROLES, DELIBERATELY ════════════════════════════════════════════════════════════
> Unlike `m_spawnSlots` (authority-only bookkeeping), this table is read by
> `brawlerRingout::integrate` on EVERY PEER — a client predicts its own respawn. So a client
> that skipped this would predict to the placeholder point and be corrected on every respawn.
> There is no role branch here and there must not be one.

⭐ **Now a check rather than a sentence.** A `checkf(m_ringoutSpawnPointsSeeded, …)` directly
after the two role branches in `BeginPlay` fails on any peer that did not seed. A role gate
wrapped around the seed call therefore fails on the first client. It is compiled into this
build, but nobody has run it in PIE to watch it fire (the UE layer has no low-level test target).

> ⭐ ALL THE POLICY IS IN THE ENGINE-FREE CORE. The sort, the fallback and the (0,0,0) guard
> are `brawlerRingout::spawnPointsFromLevelPlacements`, asserted by `Ringout.SpawnPoints.*` in
> `BrawlerRingoutSimulationTest.cpp`. This function walks actors and converts a name to bytes.

⚠ **Correction C13-12.** `spawnPointsFromLevelPlacements` has no `(0,0,0)` check. It sorts, and
it overwrites the first `min(count, kMaxSpawnPoints)` entries of a copy of the authored table.
The only protection against origins is that **zero** placements leave the authored table whole
(`Ringout.SpawnPoints.ZeroPlayerStartsKeepsTheWholeAuthoredTable`).

> ⛔ THE NAME, AS BYTES, AND THIS CONVERSION IS THE WHOLE REASON THE CORE TAKES A
> `std::string`. `FName`'s own ordering (`FastLess` / `FNameFastLess` / `CompareIndexes`)
> compares the NAME TABLE INDEX, which the engine documents as *"only stable during this
> process' lifetime"* - the order the string was first interned in THIS process. A server and
> a client never intern in the same order, so sorting FNames would hand two peers different
> tables while looking exactly like a deterministic sort. `GetName()` is the string saved in
> the map package; every peer that loaded the map has the same bytes.

This needs no tag, because the type enforces it: `brawlerRingout::LevelSpawnPoint::name` is a
`std::string` in an engine-free header that cannot name `FName`.

> ⛔ THE FALLBACK IS THE TABLE AS IT STANDS RIGHT NOW, which - because nothing else ever
> writes it and this runs once - is exactly the authored default. So "fewer player starts than
> slots leaves the rest authored" is literally "leaves the rest alone", and ZERO player starts
> is a no-op rather than a table of origins.

> ⛔ `UE_LOG`, NOT `OGBLOG_G`, AND THAT IS NOT A STYLE CHOICE. The `ogblog` sink routes
> everything to `LogOGBrawler`, which the shipped ini pins at `Warning` - which is why the
> sub-sim's own `[Ringout.respawn]` line appears in NO captured log and could not be used to
> answer "where did it teleport to?" while this task was being scoped. This is the line that
> answers that question, at Warning, on its own category, once per session per manager.
>
> Volume: one line plus `kMaxSpawnPoints` rows, at composition. Not on any tick path.

⚠ **Correction C13-4.** At `b9f6d81` the shipped `Config/DefaultEngine.ini` sets
`LogOGBrawler=Verbose`, not `Warning`. The line still belongs on `LogOGMgmt` at `Warning`,
because that is where the other composition-time lines are. But the reason given, that the
`ogblog` route is silenced, does not hold for the ini as committed.

At the call site in `BeginPlay`:

> ⭐⭐ [ringout task 9] THE RING-OUT SPAWN TABLE IS SEEDED FROM THE LEVEL, HERE, ONCE.
>
> ⛔ POSITION IS LOAD-BEARING AND IS ASSERTED AT THE DEFINITION. This sits ABOVE both
> role branches, so it runs before `m_integrationLayer` and `m_manager` are emplaced and
> therefore before any tick can read `StaticData::spawnPoints`. Moving it below either
> `emplace` still COMPILES - the integration layer holds a reference, so a later write is
> still visible - which is exactly why the ordering is a `checkf` and not a comment.
>
> ⛔ NOT ROLE-GATED: a client predicts its own respawn, so it needs the same table.

### 13.2 The gravity cross-check

*The first `checkf` in `BeginPlay`.*

> ⭐⭐ THE SIM'S GRAVITY MUST AGREE WITH THE ENGINE'S — [movement-sim task 16].
>
> ⛔ THIS IS NOT A TAUTOLOGY, EVEN THOUGH THE SIM'S VALUE WAS READ FROM THE ENGINE.
> `readMovementStaticDataCVars()` runs in a member initializer, during construction, where
> no world exists — so it can only read the PROJECT DEFAULT
> (`UPhysicsSettings::DefaultGravityZ`). A level is free to override gravity in its
> `WorldSettings` (`bGlobalGravitySet` / `GlobalGravityZ`), and `UWorld::GetGravityZ()`
> here is the first moment that override is observable. The two disagreeing means the
> character falls at one rate while every prop in the level falls at another.
>
> ⚠ WHY IT MATTERS DESPITE THE BODY'S OWN GRAVITY BEING OFF (ruling #16 a): the sim applies
> `StaticData::gravity` itself precisely BECAUSE engine gravity would double-apply on the
> character. Everything the character shares a floor with is still on the engine's number.

Verified: `MovementSchemeCVar.cpp` reads `UPhysicsSettings::DefaultGravityZ`. The check's own
message says most of this, and it is code.

### 13.3 The collision-category table

*Now one file-local function, `emplaceBrawlerQueryAdapter`, called from both role branches.*

⭐ **The duplication is gone, and with it the fence.** The source carried the table twice, once
per role branch, under "KEEP IN SYNC … adding to one silently diverges client from server". It
is now one function, so the two roles cannot hold different tables. The comments that stood
beside the two copies:

> Projectile category - its own trace channel, so projectile overlaps stay distinguishable.

> [movement-sim T39] Static level geometry. The movement sub-sim's ground/wall probe and its
> capsule sweeps search this category; no DAttack-authored shape belongs to it.
>
> ⛔ THE LOAD-BEARING EFFECT IS NOT THE RETURN VALUE, IT IS THE TABLE'S SIZE.
> ChaosSpatialQueryAdapter resizes m_toEngine to (largest mapped category + 1), and
> toObjectQueryParams iterates `cat < m_toEngine.size()`. With only categories 0-3 mapped the loop
> stopped at 4, so bit 4 was never tested, AddObjectTypesToQuery was never called, and a
> `worldOnly` search went out with EMPTY object query params - which is well-formed and matches
> NOTHING.
>
> ⚠ ECC_WorldStatic IS ECollisionChannel(0) - the very value toEngineChannel returns for an
> UNMAPPED category. So from this line on, "mapped to WorldStatic" and "never mapped" are
> INDISTINGUISHABLE by return value; only m_toEngine.size() tells them apart. Task 40 exists to
> make an unmapped category loud instead of silently channel 0. Do not read a WorldStatic result
> as proof that a mapping exists.
>
> ⚠ KEEP IN SYNC with the client-branch table below - the two tables are duplicated with no
> shared constant, and adding to one silently diverges client from server.

⚠ **Correction C13-6.** The table's size does **not** tell "mapped" from "never mapped". The
adapter's constructor pads the table with `ECollisionChannel(0)`, so a gap below the highest
mapped category is in range and still unmapped. The adapter decides mapped-ness from its own
bitmask of explicitly mapped categories, and its source says so in as many words ("never from
the table's size"). The task that "exists to make an unmapped category loud" has landed:
`[SpatialQuery.UnmappedCategory]` is reported.

⚠ **Correction C13-7.** No wall probe exists. The movement sub-simulation has exactly one query:
its ground probe, a capsule sweep of `queryVolumeIds[0]` against `collisionCategory::worldOnly`.

> [movement-sim T43] The movement sub-sim's OWN body. `brawlerMovementSimulation::PhysicsSetup::body`
> (BrawlerMovementSimulation.h) registers its shape under this category, and PhysicsDeclaration is
> in the shipped composite (SimulatableBrawler.h), so this runs for every character in every
> session.
> ⚠ [movement-sim task 17] THE SHAPE IS A CAPSULE, NOT A SPHERE. Task 11 replaced the skeleton's
> 30 cm sphere with `CapsuleGeometry{42.f, 96.f}` and set `isRoot`, so the factory ADOPTS the
> pawn's own root capsule rather than creating anything. The category, and every sentence below
> about what an unmapped category would have done to it, are unaffected — only the noun was stale.
> Channel is user ruling #6, closed 2026-09-04 and lead-verified free: ch1 is `Damageable` in
> DefaultEngine.ini and ch2-5 are the four entries above.
>
> ⛔ THIS LINE IS A FIX, NOT A NEW CAPABILITY. Without it toEngineChannel(5) fell through to the
> unmapped fallback ECollisionChannel(0) - which IS ECC_WorldStatic - so
> ChaosPhysicsFactory::applyDescriptor typed every character's movement body as STATIC LEVEL
> GEOMETRY. Harmless while nothing searched WorldStatic; LIVE from task 39 on, because a
> `worldOnly` object query searches exactly that object type and would hand the movement sim
> OTHER characters' capsules as ground. Task 40's [SpatialQuery.UnmappedCategory] category=5 line
> in the 2026-09-05 18:11 run is what finally said so out loud.
>
> ⚠ TASK 13 OWNS THE FULL CHANNEL MAP. `character -> ECC_GameTraceChannel6` is the ONLY entry
> task 43 added, at this site and the client one; do not double-add it there.
>
> ⚠ The reverse map moves too: the ctor writes m_toDAttack[GTC6] = character, a slot that was
> kUnmapped before. It collides with nothing (the other five occupy WorldStatic and GTC2-GTC5)
> and it is unreachable in production today, because no shipped query volume searches
> `character` and resolveHitIdentity only ever sees channels an object query asked for.

Verified: `collisionCategory::world` is `4` and `character` is `5`. The capsule's shape is
`CapsuleGeometry{42.f, 96.f}`. Channel 1 is `Damageable` in `DefaultEngine.ini`. The only query
volume in the movement sub-simulation searches `worldOnly`. The "task 13" ownership note is
workspace provenance and could not be verified from the tree.

The client branch's copy said only that it matched, and why the client needs it too:

> Projectile category - its own trace channel, as on the authority branch.

> [movement-sim T39] Static level geometry, as on the authority branch. Both caveats are spelled
> out in full there: it is m_toEngine.size() (not the returned channel) that makes
> toObjectQueryParams test bit 4, and ECC_WorldStatic == ECollisionChannel(0) == the unmapped
> fallback. KEEP IN SYNC with the authority table above.

> [movement-sim T43] The movement sub-sim's own body, as on the authority branch - and the client
> needs it for the same reason the server does: it predicts the same sub-sim. The full rationale
> (why an unmapped category silently became ECC_WorldStatic, and what the reverse map does) is
> spelled out at the authority table above. KEEP IN SYNC with it.

### 13.4 Registration's first-call pass

*`tryRegister`.*

> First-call body creation pass.
> ⭐ [movement-sim task 19] THIS CAST IS THE REGISTRATION PATH, AND IT IS NOT AN ACCESSOR SWAP.
> It used to name the engine's walking-pawn base; task 19 rebased `AOGBrawlerUECharacter` on
> `APawn`, so that base is no longer in the hierarchy and the old cast would return NULL HERE —
> on the FIRST-CALL body-creation pass — which is registration failing outright: no bodies, no
> simulation, no character. The root capsule this pass needs is declared by
> `AOGBrawlerUECharacter` itself now, so that class IS the type the contract requires.
> ⚠ `checkf` COMPILES OUT IN SHIPPING (task 36). There a wrong owner type is a null dereference
> on the very next line rather than an assert, which is why the message below names the exact
> class rather than a family.

> ⚠ CharacterBindings is NO LONGER STAMPED HERE. [movement-sim T13] moved it BELOW the
> physics fold, because its source is now that fold's output. See the §10 banner there.

> ⛔ Attach and parent-body are the SAME capsule, so ONE handle - two let callers desync. §10

> The factory's parentBodyId roots every shape, so overlap() emits the capsule id.

> Generic — each declaration names its own slice (PhysicsDeclaration.h). Adding a
> body-owning sub-simulation therefore edits no engine file to have its body CREATED,
> BOUND, CAPTURED, REWOUND and CHECKSUMMED.
>
> ⚠ [movement-sim T13, from the task-10 review] THAT IS THE WHOLE OF THE CLAIM, and the
> earlier unqualified wording overstated it. Making that body COLLIDABLE OR QUERYABLE is
> still hand-written engine work: its `collisionCategory` needs one entry in EACH of the two
> ChaosCategoryMapping tables in BeginPlay above (authority branch and client branch,
> duplicated with no shared constant), or ChaosPhysicsFactory::applyDescriptor types the
> shape by the unmapped fallback and no object query can ask for it. Tasks 39 and 43 paid
> exactly that cost for `world` and `character`. Generic creation, hand-written collision —
> state both halves.

Since this conversion there is **one** table, `emplaceBrawlerQueryAdapter` (§13.3), so a new
category needs one row, not two.

> Stamp the authoritative capsule body id into the brawler's CharacterBindings. §10
>
> SOURCE SINCE [movement-sim T13]: the movement sub-simulation's OWN PhysicsDeclaration
> bindings, not the pawn's own capsule lookup. That is why this stamp sits AFTER the fold —
> before it, `bindings.ownBodyId` is still zero. (The `#if DO_CHECK` block above catches a
> zero id in development and test builds; being `checkf`, it is compiled out of Shipping, so
> it is a development instrument and not a Shipping-build guarantee.)
>
> ⭐ THE VALUE IS UNCHANGED, AND THAT IS THE POINT. Task 11's descriptor sets `isRoot`, so
> the factory ADOPTS the pawn's existing root capsule instead of creating a body; that is
> what makes `ownBodyId == parentBodyId == capsuleBodyId` true by construction. This
> identity is exactly what `isRoot` buys, and it is why task 11's review refused to defer
> `isRoot` to a later task. ⛔ Anyone "simplifying" `isRoot` away silently breaks this line.

⚠ **Correction C13-13.** Not silently: `BrawlerMovementSimulation.h` has a `static_assert` that
`kCharacterCapsuleBody` has both `simulatePhysics` and `isRoot`. Removing `isRoot` fails the build.

> ⭐ [movement-sim task 17] THE TWO-SOURCE TRIPWIRE IS GONE. It asserted
> `movementOwnBodyId == record.parentBodyId` for as long as the two sources coexisted;
> `PendingRegistration::parentBodyId` — the record field that held the second one — is deleted
> with it, and the resolvability gate below now reads this same declaration. The identity it
> watched is UNCHANGED and still stated above: it is `isRoot` that makes it hold, not an
> assertion.
>
> ⭐ AND THE IDENTITY IS STILL ASSERTED, one layer down and by a check this task does not touch:
> `ChaosPhysicsFactory::createPhysicalObject`'s adopt-root arm ends in
> `checkf(bodyId == m_parentBodyId, …)` (`ChaosPhysicsFactory.cpp:195`), and its `m_parentBodyId`
> is derived from the SAME capsule component this function passed as the attach parent. So the
> removal drops a duplicate, not the only witness.
> ⚠ Neither was ever a Shipping-build guarantee — `checkf` compiles out there (task 36).

⚠ **Correction C13-14, and the check that replaces the paragraph.** The factory's adopt-root
`checkf` runs only on the adopt arm. So it does not witness either of the two edits this file
warned about at the stamp: (a) the stamp moved above the fold, which stamps a zero id, and (b)
the factory creating a body instead of adopting one. The `#if DO_CHECK` block does not catch (a)
either, because the fold populates the ids after such a misplaced stamp has already read them.
There is now a `checkf(movementOwnBodyId == parentBodyId, …)` directly above
`setCharacterBindings`. `parentBodyId` is still a local of the first-call pass, and the check
witnesses both edits. It is compiled but has not been run in PIE.

> THE TELEPORT SEED — spawn. BrawlerMovementSimulation.h's InitialConditions is a
> COUNTER-FREE edge: the UE layer sets `teleportPending` non-zero here, the sub-sim's first
> step consumes it and clears it back to zero in the same tick. It is ON THE WIRE (16 B) so
> that a respawn replays identically on a client and through a resim.
>
> ⛔ FIRST-CALL PASS ONLY. This branch runs once per character (guarded by
> `record.bodiesCreated`), and the record is moved wholesale into storage at registration
> below, so the seed survives to the sub-sim's first integrate. Seeding it per call would
> re-teleport the character every tick until it registers.
>
> The pose comes from the capsule component, which is where the engine has the character
> standing at spawn — this branch runs on the FIRST tryRegister call, before the sub-sim has
> integrated once, so nothing has driven the capsule yet whatever `drivesBody` says. (It says
> `true`: task 15 flipped it together with `simulatePhysics` and retired the CMC. This is
> still a READ of the engine's authoritative spawn pose, and the reason is the ORDER, not a
> second authority.)
>
> ⚠ THE CONSUMER DOES WRITE BACK. The teleport branch is documented as the ONE body write
> that ignores `drivesBody` — it calls setBodyTransform + setBodyLinearVelocity(0) on the
> capsule. Seeded from the capsule's OWN current location that is a value no-op, but it is
> a real engine call, and a future seed from any other source would MOVE the character.

Verified: `InitialConditions` is `uint32_t teleportPending` plus `glm::vec3 teleportPos`, which
is 16 B. `drivesBody` defaults to `true`. The teleport arm calls `setBodyTransform` and
`setBodyLinearVelocity(0)`.

> THE SPAWN-SLOT SEED - ring-out, task 3. Same first-call branch, and for the same reason:
> `brawlerRingout::InitialConditions::spawnSlot` is written ONCE per character and read on
> every respawn for the life of that character. Unlike the teleport seed above it is NOT a
> counter-free edge - nothing consumes it and nothing clears it.
>
> ⛔ FIRST-CALL PASS ONLY, inherited from the branch it sits in. Re-seeding per call would
> not merely be wasteful: `acquire` is idempotent, so it would return the same index, but a
> character whose slot had been RELEASED and handed to someone else would silently change
> spawn point mid-session. Once, at registration, is the contract.

⚠ **Not reachable as stated.** The only `release(id)` call for a character is its own
`unregisterFromNewFramework`, after which no further `tryRegister` call for that id occurs
(ids are component `GetUniqueID()`s). A re-seed per call would therefore just return the same
slot. The contract of seeding once is still right; the failure described cannot happen.

> ⛔⛔ AUTHORITY ONLY, AND THIS IS NOT HasAuthority(). `bReplicates = false` makes
> `GetLocalRole()` report authority on this actor in every role, which is why this file uses
> the WORLD-level test everywhere. `record.isAuthority` IS that test: the caller
> (`USimmableUpdateComponent::tryRegisterWithNewFramework`) computes it as
> `(GetNetMode() != NM_Client)` - literally the expression `BeginPlay` names
> `worldIsAuthority` - and passes it in. The `checkf` states that equivalence rather than
> leaving it to a comment, because `tryRegister` is a public entry point.
>
> ⭐ WHY A CLIENT MAY SAFELY LEAVE ITS OWN COPY AT THE DEFAULT 0, AND WHY THAT CANNOT BE
> READ BEFORE THE SERVER'S NUMBER ARRIVES. `tryRegister` runs on BOTH roles, so a client
> registers its own character locally with `spawnSlot` at its in-class initialiser, 0. That
> value is read by exactly one thing: the respawn arm of `brawlerRingout::integrate`, which
> is reached only after the character has (a) crossed the kill plane and (b) waited
> `StaticData::respawnDelayTicks` - 120 ticks, 2.0 s at 60 Hz - for `tick >= respawnAtTick`.
> The correction that carries the authority's `InitialConditions` is an ORDINARY state
> correction on the already-running rotation, arriving within a handful of ticks of
> registration and restoring the whole struct by assignment. So the client's 0 is
> overwritten two orders of magnitude before the only code that reads it can run, and a
> resim RESTORES the authority's value rather than recomputing it. There is no path on
> which the placeholder is read first: death itself cannot occur earlier than the first
> correction and still leave 120 ticks of countdown to run before the value matters.

Verified: `respawnDelayTicks` defaults to `120u`. The caller computes `isAuthority` as
`(GetNetMode() != NM_Client)`.

> Resolvability gate.
>
> ⭐ [movement-sim task 17] THE SOURCE IS THE MOVEMENT DECLARATION'S OWN `ownBodyId`, which is
> what `PendingRegistration::parentBodyId` used to hold and no longer exists to hold. The value
> is the same body — the descriptor's `isRoot` makes the factory ADOPT the pawn's root capsule,
> so the movement declaration's own id IS the capsule id (stated in full at the stamp above).
>
> ⚠ AND IT IS DELIBERATELY REDUNDANT WITH THE FOLD BELOW, which visits every declaration and
> therefore visits this one too. It is kept as the named, order-first read so the gate says out
> loud WHICH body a `Pending` is waiting on; it adds no guarantee the fold does not already give.

### 13.5 The ring-out score push

*`pushRingoutScoresToCharacters`, and its call in `OnPostPhysicsStep`. The cross-thread
argument is in §1, the second read crossing. What follows is the rest.*

> [ringout task 5] ⭐ THE AUTHORITY-SIDE SCORE PUSH
>
> WHAT IT IS. One game-thread pass that copies each character's authority-side ring-out
> score out of `brawlerRingout::ScoreSystem` and onto that character's replicated
> `RingoutScore` property, from which UE replicates it to every client. It carries NO
> POLICY — who scores, how much and when is entirely task 4's law, in engine-free core.
>
> ⛔ WHY A FILE-LOCAL FUNCTION AND NOT A MEMBER. Everything it needs is passed in, so it
> adds no name to the class's surface and no member to its threading table. The class
> banner's NARROW PASSTHROUGHS list exists precisely to stop this kind of thing becoming an
> accessor; a free function that the one call site below hands two references to cannot be
> reached by anything else.
>
> ⛔ THE ROUTE TABLE IS `m_delayedInputComponentsById`, AND REUSING IT IS A REAL COUPLING —
> stated here because nothing else would state it. That map is registered in
> `USimmableUpdateComponent::tryRegisterWithNewFramework` under `if (isAuthority)` and
> erased in `unregisterFromNewFramework`, so it is exactly "the authority's live characters,
> resolvable to their actors" — which is what this push needs and what no other member is.
> ⚠ IF THAT REGISTRATION EVER NARROWS (a condition beyond `isAuthority`, a later call site,
> a role that stops registering), THIS PUSH SILENTLY LOSES THOSE CHARACTERS: their score
> never leaves the server and their scoreboard row freezes at whatever last replicated. It
> fails quiet, so it is written down rather than left to be rediscovered.

The edit that would break this coupling is typed in `SimmableUpdateComponent.cpp`, not here,
which is why it has no tag in this file. Verified: the route is registered under
`if (isAuthority)` in `tryRegisterWithNewFramework`.

> ⛔⛔ THIS IS A CROSS-THREAD READ, AND THE ARGUMENT FOR IT IS THE CLASS BANNER'S THIRD
> CROSSING — read that before touching this. The short form: `ScoreSystem`'s score table is
> WRITTEN on the physics thread (`postIntegrate`, beneath `OnPreSimulate_Internal`) and read
> here on the GAME thread. What makes it the same ACCEPTED TEAR the input-history poll takes
> rather than a new hazard is that the table cannot be RESTRUCTURED under this reader: the
> only inserting and erasing calls are `onCharacterRegistered` / `onCharacterUnregistered`,
> both driven from `tryRegister` / `unregisterFromNewFramework`, both GAME THREAD. The award
> itself reaches an EXISTING entry, so it writes one naturally-aligned four-byte word and
> cannot rehash. Worst case: a scoreboard number one tick stale, on a display that decides
> nothing — which is the same bound `BrawlerColor` and this property both rest on.
>
> ⭐ THAT ARGUMENT HAS A PRECONDITION, AND THE PRECONDITION IS MACHINE-CHECKED BELOW rather
> than asserted in prose. `postIntegrate` awards through `operator[]`, which INSERTS — and
> therefore MAY REHASH — for an id the roster has not got. Task 4 chose that deliberately so
> an award to an unseeded id is a real award rather than a dropped one. It is unreachable in
> a legal session because `onCharacterRegistered` seeds every authority-registered id, and
> it seeds it BEFORE `tryRegister` returns `Ready`, which is before the route entry this
> walk reads even exists. The `checkf` states exactly that ordering, so a future change that
> breaks it fails loudly in Development instead of racing silently.

> A stale handle is SKIPPED, not erased. The two transport sinks prune the map when they
> meet one because they are its owners; this is a reader and pruning here would mutate the
> container mid-walk for no benefit - `unregisterFromNewFramework` erases it promptly anyway.

Now enforced by type, so it carries no tag: the function takes the route table by `const&`, so
`routes.erase(…)` inside the walk does not compile (C2663, seen in §13.9).

> ⛔ THE UNCHANGED-VALUE GUARD IS INSIDE THE SETTER, not here. One write site, one guard:
> a second early-out here could drift out of step with it and neither would be obviously wrong.

> ---- [ringout task 5] THE SCORE PUSH ----------------------------------
>
> ⛔ THE GATE IS `!runsPrediction()`, THE SAME EXPRESSION TWELVE LINES ABOVE, and it is now
> the layer's ONLY role site for ring-out - [ringout task 19] deleted the BeginPlay wiring
> line, the flag it wrote, and the two-sided checkf that cross-checked them, because there is
> no longer a second gate to disagree with: brawlerRingout::ScoreSystem declares
> kRoleAffinity = AuthorityOnly and SimulationSystemsExecutor skips it off the authority.
> ⛔ NOT HasAuthority(): `bReplicates = false` pins Role to authority on every peer.
>
> ⚠ THIS GATES THE REPLICATION, NOT THE AWARD. A client whose gate here were deleted would
> push scores its ScoreSystem never computed - an empty roster, so zeroes over the replicated
> values - which is loud rather than silent. Per F26 nothing mechanical checks this line.
>
> WHY HERE, beside updateVisualizationAll. This is the one game-thread point that runs
> directly after the simulation has advanced, and harvesting a physics-side result for
> PRESENTATION is exactly what its neighbour already does. The push is a poll, not an event:
> it costs one int compare per character per pass when nothing changed, and the score
> changes at most once per death tick.

⚠ **Correction C13-10.** The other `!runsPrediction()` in `OnPostPhysicsStep` was 26 lines
above, not twelve. A distance is not a fence, and the conversion removes it.
⚠ **Correction C13-11.** Something mechanical does check this line.
`AOGBrawlerUECharacter::SetAuthoritativeRingoutScore` opens with a `checkf` on `HasAuthority()`.
The pawn replicates, so on the pawn that test discriminates. A client that reached the push
would fire it in Development.

### 13.6 The router — per-line notes the router's table no longer carries

*`RouteOGMessage` is now a `constexpr` table, `kOGLogRoutes`, with a `static_assert` that no
prefix begins with an earlier one (§13.9). The notes that stood beside its lines:*

> [InputGap]/[InputDrop]/[DelayShift]/[InputStats] carry [Warning]; [Park]/[Release] do not.

> The out-of-domain gate: one [InputDomain] per burst, naming a client outside our domain.

> The relay tap: Verbose per skipped capture tick, Warning per [InputStats] window.

> The relay family: [RelayProbe.Read], .Arrival (in CAPTURE ticks), .Stale, .Miss, .Delta, .Frame. §8
>
> ⚠ .Frame IS THE ONE SERVER-SIDE MEMBER: it measures the CAUSE of .Arrival's EFFECT.
>
> ⛔ Under LogOGNet it would be inseparable; unrouted its Verbose half is unreachable. §4

⚠ **Correction C13-3.** The relay family has **eight** members in the tree, not six:
`[RelayProbe.Write]` (PROBE 5) and `[RelayProbe.Budget]` (PROBE 6) are also emitted, both from
this file and both **server-side**. So `.Frame` is not the one server-side member. §4's table and
its sentence about `.Frame` carry the same omission.

> The RESIM-GATE family: [ResimProbe.Gate], .Chaos, .Apply, .Landing, .Request, .Stranded. §8
>
> ⛔ AHEAD OF `[Resim.`, DEFENSIVELY: widening it drops this family into LogOGSim=Verbose.
>
> ONE StartsWith COVERS THE FAMILY: a future sub-tag needs no router edit.

⚠ **Correction C13-2.** The resim-gate family has **eleven** members in the tree:
`.Session`, `.Frame`, `.PushTarget`, `.PushVerdict` and `.SlotMap` are also emitted. §4's table
has ten and omits `[ResimProbe.SlotMap]`, which `CorrectionCache.h` emits. Also, at
`b9f6d81` `Config/DefaultEngine.ini` sets `LogOGSim=Log`, not `Verbose` (C13-1).

### 13.7 The rewind-push probe — the source's own wording, beyond §8

*§8 carries this probe's design. These are the passages from its source that §8 did not already
state.*

> TWO QUESTIONS, ONE FIELD SET, ONE PRODUCTION COMPARATOR. `match` asks whether the
> replay's starting state equals the pushed value; `inert` asks whether the push could
> have changed anything at all. Running both through this one helper is what makes
> "non-inert AND matched" a statement about the engine rather than about which fields
> each question happened to look at.

> One body's verdict line, emitted from the SECOND read point. `pushFrame` and
> `readFrame` are printed side by side and must be equal: a difference means the stash
> outlived the rewind it belongs to and nothing on the line may be read as an answer.

> [netcode-v2 task 10 — PROBE] ONE branch decides whether anything is read or
> formatted; the banner above `pushProbeResimTypeText` says what it measures and why
> the reads are X/R. ⛔ `UE_LOG_ACTIVE`, not a value latched at BeginPlay, so
> `LogOGResimProbe Verbose` typed into the console starts the probe on the next rewind.

> [task 10 rework 1 - PROBE] A stash still pending when a NEW rewind starts was never
> read at its verdict point. Saying so out loud is the instrument's own self-check; the
> alternative is a missing line, which reads exactly like "no rewind happened".

> [task 10 rework 1 - PROBE] The after-push read is now an EAGER-APPLY DISCRIMINATOR
> and nothing else. On public 5.6 it is equal to `beforePush` by construction, so
> `moved=1` would be an engine-behaviour finding in its own right - and it is exactly
> the case in which the verdict read below would be reporting the push rather than the
> replay. It compares all four fields regardless of the wire shape, because both sides
> are LIVE reads of the same particle.

> BodyId lookup goes through the m_physics composite bindings (local-only).
>
> [task 10] `D::name` is the declaration's own body name, the one the factory
> creates the body under, so `body=WeaponAxis` needs no table here to stay true.
> `BodyStateT` is the declaration's ACTUAL wire shape, before the widening
> conversion into `pushBodyState`'s `const PhysicsBodyState&` erases it.

Verified: `createPhysicalObject(D::descriptor(), D::name)` is the factory call in `tryRegister`.
There is no member named `m_physics`; the lookup goes through `getPhysicsComposite()`.

> ⛔ At PostPushData: direct SetX/SetV/SetW is a NO-OP on ResimAsFollower bodies.

⚠ **Correction C13-15.** This sentence justified pushing the rewind state through
`SetTargetStateAtFrame` rather than writing the particle directly. It does not apply to this
tree's bodies. On UE 5.6 `RewindToFrame` re-stamps every dynamic body's resim type on every
rewind, so none is `ResimAsFollower` during a replay. The push it justifies was also measured
never read (task 10's capture: 185 of 185 rewinds `MISMATCH`, `nonInertMatched=0`). The call
stays, because task 10's fix will decide what replaces it. The sentence has no guard, because
it no longer describes a reason.

At `b9f6d81` `Config/DefaultEngine.ini` sets `LogOGResimProbe=Verbose`, so the "shipped
`LogOGResimProbe=Warning`" in the probe banner's volume paragraph (quoted in §8) does not
describe the committed ini (C13-1).

### 13.8 The capacity pin's word rounding — `(usableBits / 32) * 4` ∴D-50

*The `[PacketBudget]` line in `releaseDelayedInputsForStep`.*

The first argument of that line turns `UNetConnection::GetMaxSingleBunchSizeBits()` into
usable **bytes**. It does not divide by 8: it rounds **down to whole 32-bit words** first,
because the channel writes a bunch in words, and then counts four bytes per word. Replace it
with the natural-looking `usableBits / 8` and the number no longer matches what a bunch can
really carry. The literal in `RoundVsPacketBudgetTest.cpp` is meant to be compared with this
printed figure by hand, and it is an **upper bound**: if the logged value is ever smaller than that literal,
the literal must come down (§8, PROBE 6, *the capacity pin*).

The source's banner over that block:

> THE CAPACITY PIN - the packet budget, measured. §8
>
> ⛔ WHY THIS EXISTS: the DERIVED single-bunch capacity does not reproduce. The engine referees.
>
> ⛔ It only moves DOWN, so RoundVsPacketBudgetTest.cpp's literal is an UPPER BOUND.
>
> ONE-SHOT PER SESSION, at Warning: the value is a property of the build, not the connection. §3

### 13.9 What the compiler now enforces instead of a comment

Every row was compiled. The rows marked **seen to fail** were then poisoned: the forbidden edit
was made in a scratch copy of the file, the build was run, and the failure below was
observed. The file was then restored and its hash checked.

| was | now | seen to fail |
|---|---|---|
| "ORDER IS LOAD-BEARING: this MUST precede the `[Resim.` catch-all" and the defensive `[ResimProbe` placement | `static_assert(ogLogRoutesAreAllReachable())` over `kOGLogRoutes`: no prefix may begin with an earlier one, compared case-insensitively as `FString::StartsWith` does | `[Resim.` moved above `[Resim.Input]` ⇒ `C2338`; `[Resim.` widened to `[Resim` ⇒ `C2338`; a lower-case duplicate `[resim.input]` appended ⇒ `C2338` |
| the two collision tables and their "KEEP IN SYNC" | one table, `emplaceBrawlerQueryAdapter`, called from both branches | nothing left to poison: the two copies no longer exist |
| "NOT `HasAuthority()`" at four sites | `#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction` after the includes, `#undef` at the end of the file (it must not leak into the next file of a unity blob) | `HasAuthority()` typed in `BeginPlay` ⇒ `C3861` naming that identifier |
| PROBE A's "COMPARABLE WITH ResimGateProbe … same 120 samples" | `static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples)` | a local shadow of 121 ⇒ `C2338` |
| "NOT ROLE-GATED" at the spawn-point seed | `checkf(m_ringoutSpawnPointsSeeded)` after both role branches | compiled only. A runtime check in the UE layer cannot be exercised from a low-level test |
| the stamp-after-the-fold order and "anyone simplifying `isRoot` away" | `checkf(movementOwnBodyId == parentBodyId)` above `setCharacterBindings` | compiled only, same reason |

**Already enforced, found rather than added.** No tag, no comment:

| the comment said | what already enforces it | seen to fail |
|---|---|---|
| the request is counted after the tick conversion | data dependency: `noteResimRequest` takes the converted tick | hoisted above it ⇒ `C2065` |
| a stale route is skipped, never erased, in the score push | the route table arrives by `const&` | `routes.erase(id)` ⇒ `C2663` |
| no depth is passed to the relay stage | `stageRelayedInput` has no depth parameter; `RelayRedundancyDepthTest.cpp` | not re-poisoned here |
| a client must not push scores | `SetAuthoritativeRingoutScore`'s `checkf` on `HasAuthority()` (the pawn replicates) | not re-poisoned here |
| `isRoot` must not be removed | `static_assert` on `kCharacterCapsuleBody` in `BrawlerMovementSimulation.h` | not re-poisoned here |
| spawn points must not be ordered by `FName` | `LevelSpawnPoint::name` is a `std::string` in an engine-free header | not poisoned here |
| `kNoFreeSlot`'s value is `kMaxSpawnPoints` | `static_assert` at its declaration (the clamp at the call site is still G-67) | not re-poisoned here |

**Tried and NOT convertible.** "NOT `PlayerInput{}`" as
`static_assert(!(getZeroPlayerInput() == PlayerInput{}))`: `C2678`, because `PlayerInput` has no
`operator==`, and `getZeroPlayerInput()` is not `constexpr` in any case. It stays a guard (G-60).

### 13.10 Corrections — claims the `.cpp` carried that were not true

Numbered `C13-n` so they cannot collide with §11's.

| id | the claim | what is true at `b9f6d81` |
|---|---|---|
| C13-1 | the file banner: "`Config/DefaultEngine.ini` sets `LogOGNet=Warning` and a Log line therefore DOES NOT EXIST on a dedicated server" | the ini sets `LogOGNet=Verbose`. It also sets `LogOGSim=Log` (the banner said `Verbose`) and `LogOGResimProbe=Verbose` (the rewind-push probe's banner said the shipped value was `Warning`) |
| C13-2 | the resim-gate family is `.Gate`, `.Chaos`, `.Apply`, `.Landing`, `.Request`, `.Stranded` | eleven members; `.Session`, `.Frame`, `.PushTarget`, `.PushVerdict` and `.SlotMap` are missing from that list, and `.SlotMap` is missing from §4's table too |
| C13-3 | the relay family is six members, and `.Frame` is its one server-side member | eight members; `.Write` and `.Budget` are also server-side |
| C13-4 | `LogOGBrawler` is pinned at `Warning` by the shipped ini | `LogOGBrawler=Verbose` |
| C13-5 | "`GConfig` - the ONLY GConfig use in this codebase" (an include comment) | `MovementSchemeCVar.cpp` also uses `GConfig`, for its stale-ini sweep |
| C13-6 | only `m_toEngine.size()` tells a WorldStatic mapping from none | the adapter decides from a bitmask of explicitly mapped categories, "never from the table's size" |
| C13-7 | the movement sub-sim's "ground/wall probe and its capsule sweeps" | one query: the ground probe |
| C13-8 | §2 "forbids" a write to `StaticData` after construction | §2 forbids a copy or a move, to protect internal references |
| C13-9 | "see the ⛔ banner on `brawlerRingout::SpawnSlotAllocator`" | no banner remains there; the argument is `BrawlerRingoutSimulation-rationale.md` §11a |
| C13-10 | "THE SAME EXPRESSION TWELVE LINES ABOVE" | 26 lines |
| C13-11 | "Per F26 nothing mechanical checks this line" (the score-push gate) | the setter's `checkf` on `HasAuthority()` checks it |
| C13-12 | the core holds "the (0,0,0) guard" | no such check; only the zero-placements fallback |
| C13-13 | removing `isRoot` "silently breaks this line" | a `static_assert` in `BrawlerMovementSimulation.h` fails the build |
| C13-14 | the factory's adopt-root `checkf` means removing the two-source tripwire "drops a duplicate, not the only witness" | the factory check never runs for the two edits the file warned about; replaced by a `checkf` at the stamp |
| C13-15 | direct writes are a no-op on `ResimAsFollower` bodies, as the reason for the target push | no body here is `ResimAsFollower` during a replay on 5.6, and the push is measured never read |
| C13-16 | the file banner: "EVERY SESSION KNOB TAKES THE SAME FOUR STEPS … 4 PROVE an UNCONDITIONAL Warning line" | the relay-delay floor has no unconditional `Warning` proof line in `BeginPlay`: only a `Log` line when the ini overrides it, and a `Warning` when the relay is missing. §3's table makes the same claim |
| C13-17 | the include comment "resimGate:: - the policy kernel. Explicit, not leaned on through SimulationManager.h" | the translation unit names nothing in `resimGate::`; the include has no user here (the same class as §11 C2) |

Two more claims were **true as written but described something that cannot happen**. They are
annotated at their quotes in §13.4 (re-seeding a released slot) and §13.7 (the `ResimAsFollower`
premise, which is also C13-15).

⚠ **Routed, not fixed: a claim in another file about this one.** `OGBrawlerUECharacter.cpp`,
in `SetAuthoritativeRingoutScore`, says this file "warns about it twice and the push uses the
world-level `GetNetMode()` test instead". Neither half is true. The push is gated on
`!runsPrediction()`. The source had **four** `HasAuthority()` warnings, and it now has one
compile-time poison that replaces all four (§13.9).

### 13.11 Small notes carried verbatim

> THE SESSION ROTATION WIDTH. ⛔ The intake clamp stops the proof line below from lying. §3

> Clear the singleton slot before teardown so a later PIE session cannot hit the guard.

> A downward or delay-neutral transition, which drift reaches by advancing, or a first one.

> Routing registration for the `deliver` callback. ⛔ A plain overwrite, ONCE at register. §7 §10

> Line 1 - THE RATIO, client role. p99 and max are why this exists: a mean hides a hitch.

> Same unregister contract for the display. ⛔ A kept ring is a leak keyed on a dead id. §10

> The (old -> new) delta IS the transition signal: the client runs no RTT sampling. §5
>
> ⛔ A fresh connection's first OnRep reports oldTier = 0, the property default, not a tier
> ever run at; `hadAnyTier`, captured above, is what tells it from a genuine prior tier 0.

> ⛔ `runsPrediction()` false covers the server AND standalone, which rides the SERVER tag.

The trailing labels on `PushProbeTally`'s fields, as they stood:

```
int32 bodies             = 0;  // composite bodies the push loop walked
int32 compared           = 0;  // bodies that resolved at BOTH push time and verdict time
int32 unresolved         = 0;  // bodies that failed to resolve at either point
int32 inert              = 0;  // pushed value already equalled the pre-push live state
int32 nonInert           = 0;  // compared - inert: the ONLY testable comparisons
int32 movedAtPush        = 0;  // eager-apply discriminator; never part of the verdict
```

and on four early returns: `// pre-BeginPlay ordering; the relay latches and replays at bind`
(tier and floor), `// an OnRep can fire for an unchanged value; nothing transitioned`, and
`// core manager not constructed yet; no clock to stall`.

---

## Provenance

The two source files carried **317 workspace-only citations** — backlog item numbers, task
numbers, review-note numbers, initiative document names and design section marks — before this
pass. They are meaningless to a reader who does not have that workspace, so they were removed from
the source and their content folded into the sections above. **The facts they carried are here; the
item numbers are not**, deliberately: an item number is provenance, not a guard, and this document
is where provenance belongs. The initiative workspace remains the record of *when* each decision
was taken.
