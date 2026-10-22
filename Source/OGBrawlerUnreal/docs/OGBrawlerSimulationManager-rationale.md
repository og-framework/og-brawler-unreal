<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: ASimulationManagerUImpl -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to AOGBrawlerSimulationManager (this actor); kept in the history sections §0, §4 "History", §11 and §12, and named in their markers -->
<!-- lint-external-ref: ASimulationManagerUImpl::readMovementStaticDataCVars -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to AOGBrawlerSimulationManager::readMovementStaticDataCVars; kept in the §12 history -->
<!-- lint-external-ref: SimulationManagerUImpl.h -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to OGBrawlerSimulationManager.h; kept in the history sections §0, §4 "History", §11 and §12 -->
<!-- lint-external-ref: SimulationManagerUImpl.cpp -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to OGBrawlerSimulationManager.cpp; kept in the §11 history -->
<!-- lint-external-ref: SimulationManagerUImpl-guards.md -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to OGBrawlerSimulationManager-guards.md (ids unchanged); kept in the §12 history -->
<!-- lint-external-ref: getRingoutVizState -- retired name: became AOGBrawlerSimulationManager::getVizSlice (og-ue-simulationcomponentandmanagersplit task 22); historical mention in the carried header comments (§12) -->
<!-- lint-external-ref: m_movementStaticDataCVars -- retired member: folded into BrawlerSessionGameUImpl::makeStaticData by og-ue-simulationcomponentandmanagersplit task 24a; historical mention in §0, §11 and the carried header comments (§12) -->
# `AOGBrawlerSimulationManager` — rationale

Companion to `Source/OGBrawlerUnreal/OGBrawlerSimulationManager.h` and
`Source/OGBrawlerUnreal/OGBrawlerSimulationManager.cpp`. This file carries the reasoning, the
provenance and the worked derivations.

**Both source files carry no prose.** Each holds a two-line docs pointer, code, assertions whose
messages say what they replaced, and one-line tags: `⛔G-nn` → an entry in
`OGBrawlerSimulationManager-guards.md`, `∴D-nn` → the heading in this file that ends with that id. The
header's ids are below 50; the `.cpp`'s start at `G-50` / `D-50`, so the two files' ids cannot
collide.

* **The header's** pre-conversion comment text is carried here verbatim: the orientation banner
  in §0, everything else in §12 (og-netcode-v2-field-defects task 11).
* **The `.cpp`'s** comments were mostly already stated in §1–§11, which were written from them.
  What those sections did not carry is quoted in §13, with the compile-time checks that replaced
  six of its prose fences (§13.9) and the claims it made that were not true (§13.10)
  (og-netcode-v2-field-defects task 12).
* **§14** recorded a later code change: how the first replayed step hands the restored body state to
  the engine (og-netcode-v2-field-defects task 10's fix). It moved to `ChaosTickBackendUImpl-rationale.md` §5
  with the Chaos host (og-simulationscheduler-withjolt task 82), as did §8's rewind-push probe and §9's
  derivation of the `+ 1`.
* **§16**, the latency-budget probe's host side (og-simulationscheduler-withjolt task 1), moved into the latency
  class's rationale with the code (og-tickdriver task 18). Since og-ue-simulationcomponentandmanagersplit task 19a
  the code is OGSimulationUnreal's `UELatencyBudgetDiagnostics` and its game-neutral part is that template's
  rationale; §16 holds the brawler's wiring again.
* **§17**, the map of the Jolt host, moved to `TickDriverBackendUImpl-rationale.md` §2 with the Jolt host
  (og-simulationscheduler-withjolt task 83). The frame host and the game's tick-driver binding have their own
  documents, OGSimulationUnreal's `UETickDriverHost-rationale.md` (since og-tickdriver task 11; a game-module
  document before) and `BrawlerTickDriverGameUImpl-rationale.md`; since
  og-tickdriver task 8 the frame pipeline and the step hooks are og-simulation's `SimulationTickDriver` and
  `TickDriverStepHooks`.
* **§18** is the backend switch: the compile-time choice between the Chaos host and the Jolt host
  (og-simulationscheduler-withjolt task 51) and the flip of the default to Jolt (task 18). Since task 82 it
  holds the seam, the fixed set of entry points through which this class calls its backend host; since
  task 83 both hosts are classes of their own and this class has no arm. Its subsections on the Jolt
  configuration's world (task 53), stepping (task 54), render publish and apply (task 55), game-thread shadow
  world (task 56), render interpolation, correction smoothing and contact counts moved to
  `TickDriverBackendUImpl-rationale.md` §3–§9 (task 83).

<!-- ================= DECLARED LINT ESCAPES =================================
     Every token below is CORRECT and cannot resolve. None is here to silence a
     name that is wrong: the two wrong names this lint found were FIXED, not
     escaped (§11 F-32-5 and F-32-6).
     ========================================================================= -->
<!-- lint-external-ref: bUseLoggingInShipping -- Unreal Engine target-rules flag (Shipping logging); this project does not set it -->
<!-- lint-external-ref: FNoLoggingCategory -- Unreal Engine type (LogMacros.h), outside every scan root -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
<!-- lint-external-ref: pushBodyState -- retired name: the Chaos rollback-push lambda deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
<!-- lint-external-ref: UDataStreamChannel::WriteData -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::FindValidResimFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::SetTargetStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FChaosScene -- Unreal Engine type, outside every scan root -->
<!-- lint-external-ref: seedRingoutSpawnPointsFromLevel -- retired name: became BrawlerRosterUImpl::seedSpawnPointsFromLevel (og-ue-simulationcomponentandmanagersplit task 23); historical mention in §11 and the carried header comments (§12) -->
<!-- lint-external-ref: void seedRingoutSpawnPointsFromLevel(UWorld& world); -- the retired declaration, quoted in the §12 heading of its carried comment block (code moved to BrawlerRosterUImpl.h, task 23) -->
<!-- lint-external-ref: editQueryAdapter -- RETIRED in task 51 (§18): the public query-adapter accessor, replaced by editVizQuery. It must NOT resolve -->
<!-- lint-external-ref: getPhysicsBodyReaderAdapter -- RETIRED in task 51 (§18): the public reader accessor, replaced by getVizReader; §12 quotes its old declaration verbatim. It must NOT resolve -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: AddWeakLambda -- Unreal Engine delegate method, outside every scan root -->
<!-- lint-external-ref: AddUObject -- Unreal Engine delegate method, outside every scan root -->
<!-- lint-external-ref: FChaosScene::GetNetworkDeltaTimeScale -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root -->
<!-- lint-external-ref: NoLogging -- an Unreal log-verbosity enumerator, written as an ini value -->
<!-- lint-external-ref: notReady -- a field NAME inside one probe's log format string, not a declared identifier -->
<!-- lint-external-ref: .Build.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: .Target.cs -- a FILE-EXTENSION pattern naming a class of build files, not one file -->
<!-- lint-external-ref: routeInboundHits -- ABSENCE FENCE (§5): the retired adapter-side routing shim. It must NOT resolve -->
<!-- lint-external-ref: m_replicatedInputSyncedBuffer -- ABSENCE FENCE (§6): the retired second write of the discharged dual-write fence. It must NOT resolve -->
<!-- lint-external-ref: getLatestInput -- ABSENCE FENCE (§6): the deleted correction-input read. It must NOT resolve -->
<!-- lint-external-ref: sampleAndDeriveConnectionTier -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->
<!-- lint-external-ref: tryEnqueueDelayedRemoteInput -- ABSENCE FENCE (§7): relocated to the RPC boundary and deleted here. It must NOT resolve -->
<!-- lint-external-ref: USimmableUpdateComponent -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to UOGBrawlerSimulatableComponent; kept in the history sections §11 and §12 (history keeps the old name) -->
<!-- lint-external-ref: SimmableUpdateComponent.cpp -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to OGBrawlerSimulatableComponent.cpp; kept in the history of §4 ("History: what tasks 80 and 12 checked and measured") and in §12 -->
<!-- lint-external-ref: SimmableUpdateComponent-rationale.md -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to OGBrawlerSimulatableComponent-rationale.md (ids unchanged); kept in the §11 history (C3) -->
<!-- lint-external-ref: SimmableUpdateComponent-guards.md -- RENAMED by og-ue-simulationcomponentandmanagersplit task 21a to OGBrawlerSimulatableComponent-guards.md (ids unchanged); kept in the §11 history (C3) -->

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. It is deliberately not
> in the `og-simulation` docs tier, which is MPL-2.0 and travels with a different repository.

**Read §0 first** — the header's orientation block, carried below verbatim. It states the shape —
two role instances, the thread table, the construction order, the one delay formula, the four
session knobs — in about a hundred lines. Everything below assumes it.

---

## §0 Orientation — the header's banner, carried

*Names here are as they stood then: task 30 renamed the actor `ASimulationManagerUImpl` to `AOGBrawlerSimulationManager`, its files to `OGBrawlerSimulationManager.h` and `OGBrawlerSimulationManager.cpp`, and this docs pair to `OGBrawlerSimulationManager-rationale.md` and `OGBrawlerSimulationManager-guards.md` (ids unchanged).*

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
  The list above also omits `m_movementStaticDataCVars`, which `m_staticData` reads (§2). *Code moved to
  `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 24a): the row is the session's, without the cvar member, which became
  `BrawlerSessionGameUImpl::makeStaticData` (§2).*
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

**Jolt-configuration annotations (og-simulationscheduler-withjolt task 18).** The block describes the
Chaos host. Since task 18 the default build is the Jolt configuration (§18; `TickDriverBackendUImpl-rationale.md` §2), and there:

* *"binds them to Chaos"* — the manager binds every character to a Jolt world it owns; Chaos is the
  gameplay physics only in the Chaos configuration, until task 21.
* THREADS, the GAME row's *"InjectInputs_External -> releaseDelayedInputsForStep"* — that is the Chaos
  configuration's per-frame entry. In the Jolt configuration the frame host's `TG_StartPhysics` tick
  function, through the tick driver, runs the per-frame half (`onFrameStepsDue_GameThread`, then, on the authority, the reap after
  the inline steps), and the drain runs once per step from the authority's `beforeTick` (§7; `TickDriverBackendUImpl-rationale.md` §2).
* THREADS, the PHYSICS row's *"FSimulationManagerAsyncCallback's five _Internal hooks"* — the Chaos
  configuration's. In the Jolt configuration the physics side is the step: one worker task per frame on
  a client, inline on the game thread on the authority, each step running the step driver and the four
  step hooks under the world mutex (`TickDriverBackendUImpl-rationale.md` §2).
* CROSSING — the count of three is unchanged in both configurations. The Jolt host adds its own
  game-thread-to-step mechanisms (J1–J6 and the shadow hand-over), which are the Jolt host's boundary and
  not og-simulation crossings (§1).

**Code moved (og-ue-simulationcomponentandmanagersplit task 22).** `m_inputHistory`, `pollInputHistory`,
`getInputHistoryRows`, `pollInputHistoryLanes` and `getInputHistoryLanes`, named in THREADS, CROSSING and NARROW
PASSTHROUGHS above, are members of `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h`), which this class
holds as `m_inputHistoryFeed`. The feed reads this class through a `const` reference. The read crossing is argued in
`InputHistoryDisplay-rationale.md` §8.6, and the CROSSING count of three is unchanged.

⛔ **The rules this block states are enforced where they are typed, not here.** *"⛔ DO NOT WIDEN
ONE INTO AN ACCESSOR"*, *"⛔ ONE CATEGORY PER PROBE FAMILY"* and *"⛔ NOTHING ELSE CROSSES"* have
no single site in the header — the wrong edit could be a new member anywhere in the class, or a
router arm in the `.cpp` — so they carry no tag (§1, §4). *"⛔ both go through one recompute"* does
have a site: G-05 of `UESimulationSession-guards.md` on `recomputeAndPublishEffectiveInputDelay`, a member of the session
since og-ue-simulationcomponentandmanagersplit task 25 (this file's G-16 until then).

---

## §1 Threads, and why every entry point here is narrow

`AOGBrawlerSimulationManager` straddles two threads and owns the boundary between them.

| | runs on | what it is |
|---|---|---|
| `BeginPlay` / `EndPlay` | GAME | the composition root; since og-ue-simulationcomponentandmanagersplit task 27a `EndPlay` is `m_session.endPlay()`, then `Super` (the session's teardown, `UESimulationSession-rationale.md` §10, in OGSimulationUnreal's docs), and since task 27b `BeginPlay` is `Super`, then `m_session.beginPlay()` (the session's start-up, its §11) |
| `onConnectionTierReceived` / `Replayed`, `onRelayDelayFloorReceived` / `Replayed`, `onInputRelayHostReady`, `onTimingInfoReceived` | GAME | the relay listeners: members of the session `m_session` since task 25 (`UESimulationSession-rationale.md` §9, in OGSimulationUnreal's docs). *R0, task 25: the row said "the four replication listeners"; there are six entry points over three interfaces, and the timing callback was not listed* |
| `deliverRemoteInput`, `relayRemoteInput`, `noteDelayedInputComponent` | GAME | the reception sinks, driven from the RPC receipt path, and the route's registration: the session's since og-ue-simulationcomponentandmanagersplit task 26a (`UESimulationSession-rationale.md` §9.7, in OGSimulationUnreal's docs); this class keeps the registration's `SimCharacterId` wrapper |
| `InjectInputs_External` → `onFrameStepsDue_GameThread`, `releaseDelayedInputsForStep`, `reapConnections` (Chaos configuration, §18; a `ChaosTickBackendUImpl` member since task 82) | GAME | PROBE A and PROBE 6 once per physics frame, then the drain and the reap (§7; the probes' function is the session's since task 26b, the drain since task 26a) |
| `onPostPhysicsStep` (the session's since task 27a, `UESimulationSession-rationale.md` §10; this class's `OnPostPhysicsStep` before) | GAME | the post-physics pass: the shadow-world restore (Jolt configuration, worker client, `TickDriverBackendUImpl-rationale.md` §6), the render interpolation and apply and the shadow overwrite (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §5, §7), visualization snapshot (with the rendered poses in the Jolt configuration), timing buffer, score push (the brawler's hook `BrawlerSessionGameUImpl::afterVisualization` since task 27a), latency probe; bound to the scene's post-tick delegate in the Chaos configuration, called by the frame host's `TG_EndPhysics` tick function in the Jolt configuration |
| the frame host's two tick functions → the tick driver → `onFrameStepsDue_GameThread`, the reap (authority), `onPostPhysicsStep`, through `BrawlerTickDriverGameUImpl` (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §4) | GAME | the per-frame work at `TG_StartPhysics` and `TG_EndPhysics` (`UETickDriverHost-rationale.md`; `SimulationTickDriver-rationale.md` §3) |
| the tick driver's per-step body (`SimulationTickDriver`) → `SimulationStepDriver::runTick` and the four step hooks (`TickDriverStepHooks`), including the authority's per-step drain `releaseDelayedInputsForStep` (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §4) | PHYSICS: a worker task on a client, the game thread inline on the authority | one simulation step under the world mutex; everything the core `SimulationManager` runs beneath it |
| `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`, `OnPostSolve_Internal`, `ProcessInputs_Internal`, `TriggerRewindIfNeeded_Internal`, `FirstPreResimStep_Internal`, `ApplyCorrections_Internal` | PHYSICS | the six Chaos hook overrides (Chaos configuration, §18; in `ChaosTickBackendUImpl.cpp` since task 82) — `ProcessInputs_Internal` and `ApplyCorrections_Internal` are empty (§11 C5) — and everything the core `SimulationManager` runs beneath them |
| `pollInputHistory`, `pollInputHistoryLanes` (members of `BrawlerInputHistoryFeedUImpl`, this class's `m_inputHistoryFeed`, since task 22) | GAME | the input-history display's render-rate feed, driven from `UOGBrawlerSimulatableComponent::TickComponent`; it reads the session `m_session` through a `const` reference since task 24b, this class before (`InputHistoryDisplay-rationale.md` §8.6) |

**There are three crossings, and they are not alike.** One is a write of one scalar; the other
two are reads — of a whole capture by the input-history feed (argued in `InputHistoryDisplay-rationale.md` §8.6 since
task 22, which moved the feed out of this class), and of the ring-out score table below — and each is an
accepted tear rather than a synchronized access. *(This sentence said "two crossings" until task
11; the ring-out push added the third in ring-out task 5 — §11 C16.)*

**The write crossing carries one scalar**, the client's effective input delay. Its writer,
`publishClientEffectiveInputDelayTicks`, is the session's since og-ue-simulationcomponentandmanagersplit task 25, and the
argument moved with it, verified (it also names the resimulation's load), to `UESimulationSession-rationale.md` §9.3 (OGSimulationUnreal). The
crossing is still one of the three this table counts.

**Everything else on this class is game-thread-only and has no internal synchronization:**
`m_inputHistoryFeed` (its store; `m_inputHistory` until task 22).
The reception coordinator, PROBE 5 and the delayed-input map were on this list until og-ue-simulationcomponentandmanagersplit
task 26a moved them into the session, and the two frame probes (`m_frameHealthProbe`, `m_connectionBudgetProbe`) until
task 26b; they are game-thread-only there too (`UESimulationSession-rationale.md` §9.7, §9.9).
`m_manager->editResimGateProbe()` is the mirror case on the
other side: physics-thread-only, and its correctness rests on nothing else touching it.

### The Jolt host's mechanisms J1–J6 and the shadow hand-over (og-simulationscheduler-withjolt tasks 54–56 and 52)

The Jolt configuration replaces Chaos's implicit marshalling (its game-thread body API, its
scene-query copy, the tick mapper's atomic, its marshalled input struct, its end-of-physics body
sync) with five host mechanisms
(design D §2.4), and Chaos's game-thread copy of the physics scene with a shadow world on a worker client (design D §2.5, "(e) in detail"). A sixth, J6, replaces nothing: it carries the step's measured cost to the game thread for `[SimHost.Window]` (§8). A seventh, J7, replaces Chaos's hand-over of a resimulation's error with its results (task 57). Until og-tickdriver task 8 none was a crossing between og-simulation structures; since task 8, J1, J3 and J6 are mailboxes of og-simulation's `SimulationTickDriver` and are listed in og-simulation's `ThreadingCrossings.md` ("The tick driver's mailboxes"); since og-tickdriver task 9, J5 and J7 are og-simulation's render presenter's (`TickDriverPresenter`) and are listed there too; and since og-tickdriver task 10, J2 and the shadow hand-over are og-simulation-jolt's world host's (`JoltTickWorldHost`, which the Jolt host `TickDriverBackendUImpl` holds in `m_backendWorlds`), and J2 is listed there too.

| | what | direction | mechanism | why it is safe |
|---|---|---|---|---|
| **J1** | occupancy commands `{slot, occupied}` | GT → step | the tick driver's `SpscRing<BodySlotOccupancyCommand, 64>` (the frame host's until og-tickdriver task 8), through its `pushBodySlotOccupancy`: Occupy pushed on `tryRegister`'s Ready path after `notifyCharacterRegistered`, Vacate pushed by the world host's `release` (`JoltTickWorldHost`, since og-tickdriver task 10) through the door `onUnregistered_GameThread` passes; drained first in `beforeTick` into the driver's `noteBodySlotOccupancy` | one producer (the game thread) and one consumer (the step, serialized by the task chain); occupancy is applied only by the driver, on its thread, so the occupancy timeline records what the world stepped with (`JoltTickWorldHost-guards.md` G-02, which succeeds `TickDriverBackendUImpl-guards.md` G-02). A full ring is a `checkf` |
| **J2** | the world mutex, the world host's `UEFMutexLockableUImpl` (`JoltTickWorldHost`; the Jolt host's own member until og-tickdriver task 10) | GT ↔ step | a UE::FMutex held for each whole step (by the tick driver's per-step lock through the world host's `lockForStep`; task 8's interim world host until og-tickdriver task 10) and by the game thread around `tryRegister`'s bind pass, the world build and the driver's construction; the bind's wait for it is timed into `[SimHost.Window]` (§8) | the bind needs synchronous ids, so it locks instead of queuing (design D7); the game thread never waits for a step task while holding it (`SimulationTickDriver-guards.md` G-05, which succeeds frame-host G-05); the query adapter's access predicate checks it (`TickDriverBackendUImpl-rationale.md` §3; `ThreadingCrossings.md` J2) |
| **J3** | the published tick offset | step → GT | the tick driver's `std::atomic<int64_t>` (the frame host's until og-tickdriver task 8), stored relaxed in `afterTick` as `tick − physicsStep`, loaded relaxed at `TG_StartPhysics` and added to the dispatch counter | the mapper's own semantics (a lone relaxed offset, read on the game thread); no `+ 1` (§9, `SimulationTickDriver-guards.md` G-03, which succeeds frame-host G-03) |
| **J4** | the frame's step batch | GT → task | first step, count and deadlines copied into the task by value | the step code never touches the game-thread-owned scheduler |
| **J5** | the render snapshot | step → GT | og-simulation's presenter's `SnapshotChannel<Snapshot, 4>` (`TickDriverPresenter`; the Jolt host's own channel until og-tickdriver task 9): `afterTick` calls the presenter's `publish_Step`, which fills a pooled slot with `fillRenderSnapshot` and commits it once per step; the presenter's `present_GameThread`, called from `OnPostPhysicsStep` through the backend's `beforePresent_GameThread`, peeks back from the newest, moves every registered actor to the bracketed pose through its render sink and releases the pair (`TickDriverBackendUImpl-rationale.md` §5; `ThreadingCrossings.md` J5) | one writer (the step) and one reader (the game thread) on a lock-free channel that hands the reader only committed slots and never recycles a held one; since task 19 the reader holds at most the two snapshots that bracket the render time, and four slots still always leave the writer a free one (a full channel fails the presenter's check in `publish_Step`) |
| **J7** | a resimulation's render correction (client only) | step → GT | og-simulation's presenter's `SpscRing<Correction, 16>` (`TickDriverPresenter`; the Jolt host's own ring until og-tickdriver task 9): `beforeSimulate` calls the presenter's `captureCorrection_Step`, which pushes one correction after a resimulation, before the same step's `afterTick` commits the snapshot it rides; the presenter's pose build in `present_GameThread` drains it through `correctPoses` after the bracket is chosen (`TickDriverBackendUImpl-rationale.md` §8; `ThreadingCrossings.md` J7) | one producer (the step) and one consumer (the game thread) on the same lock-free ring as J1. The push precedes the carrier snapshot's commit, so a game thread that sees the carrier step sees its correction. A full ring drops the correction and counts it (`ringDrops`); the four capture counters beside it are relaxed atomics, read only for the window line |
| **J6** | step-cost samples | step → GT | the tick driver's `SpscRing<StepCostSample, 256>` (the frame host's until og-tickdriver task 8): `afterTick` pushes one sample per step (the time before the normal step, the normal step, the replay depth, whether a resimulation was refused); the tick driver drains it on the game thread after every `OnPostPhysicsStep` into `[SimHost.Window]` (§8) | one producer (the step) and one consumer (the game thread) on the same lock-free ring as J1; a full ring drops the sample and counts it. Diagnostic only: nothing decides on it |
| **shadow hand-over** | the newest saved state slot (worker client only) | step → GT | the world host's `m_shadowSlots` (`JoltTickWorldHost`; the Jolt host's own until og-tickdriver task 10), a `SnapshotChannel<ShadowStateSlot, 3>` whose three slots are pre-reserved to the ring's slot size when the manager is built: `afterTick` calls the world host's `afterTick_Step`, which copies the step world's ring slot for the step's tick into a pooled slot and commits it, only when the step saved one (`stepAllocatesFrontierSlot`; a Stall step saves none); the world host's `restore_GameThread` peeks the newest at the top of `OnPostPhysicsStep` and, when it is newer than the last one restored, restores the shadow world from it (`TickDriverBackendUImpl-rationale.md` §6) | the same channel argument as J5: one writer, one reader holding one slot, so three slots always leave the writer a free one (a full channel is an `OG_CHECK`). The copy is a byte copy into reserved storage and the body-id list fits its reserved capacity, so nothing allocates after construction. The shadow world itself is game-thread-only: built at `BeginPlay`, bound and released by `tryRegister` and the slot release, restored and queried on the game thread; the step never touches it, and the visualizations never touch the step world |

### The read crossing — `pollInputHistory`: moved to the feed (task 22)

Code moved to `InputHistoryVisualizationUImpl.cpp` (og-ue-simulationcomponentandmanagersplit task 22). `pollInputHistory`
and `pollInputHistoryLanes` are members of `BrawlerInputHistoryFeedUImpl`, which reads `getLocalInputCache`, the
reconciliation's two diagnostic seams, the tier consumer, the `TimeConfig` and the effective-delay atomic through a
`const` reference: to this class until task 24b, to the session since (the tier consumer and the effective delay
through the session's const `owner()` in task 24b, as the session's own const reads since task 25). The tear argument, the second and third feeds and the
presence test moved with the code, verified and corrected, to `InputHistoryDisplay-rationale.md` §8.6. The door the first
read goes through, `getLocalInputCache`, is the session's since task 24b (`UESimulationSession-rationale.md` §8, in
OGSimulationUnreal's docs), and it still returns a pointer to `const`.

### The second read crossing — the ring-out score push, and why its tear is the same one

*(ring-out initiative task 5, 2026-09-13)*

`AOGBrawlerSimulationManager::OnPostPhysicsStep` now copies each character's ring-out score out of
`brawlerRingout::ScoreSystem` — reached through `m_systemsExec`, on the **game thread** — and
onto that character's replicated `RingoutScore` property. `brawlerRingout::ScoreSystem`'s
`postIntegrate` writes that table on the **physics thread**, beneath `onGameSimulation`. The two
are not same-thread, so this is a crossing and the CROSSING table (§0, formerly the header's
banner) gains a bullet.

*Since og-ue-simulationcomponentandmanagersplit task 23 the walk itself is `BrawlerRosterUImpl::pushRingoutScores`, a
`static` member that `OnPostPhysicsStep` calls, under the same gate, with the same two `const` references. The argument
below is unchanged; the `checkf` that states its precondition moved with the walk (`BrawlerRosterUImpl-rationale.md` §3).
Since task 27a the gate and the call are the brawler's hook `BrawlerSessionGameUImpl::afterVisualization`, which the
session's `onPostPhysicsStep` calls at the same position, on the game thread, with the session; the hook reads the same
two members through the session's const reads (`BrawlerSessionGameUImpl-rationale.md` §5).*

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
  from the registration sequences, `tryRegister` and `unregisterFromNewFramework` (this class's until
  og-ue-simulationcomponentandmanagersplit task 28, the session's since, `UESimulationSession-rationale.md` §12) — through
  `notifyCharacterRegistered` / `notifyCharacterUnregistered` — and this class's own thread roster records both as game-thread
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
`getLocalInputCache`, `noteResimRequest` and
`noteResimGrant` are one-purpose entry points. Each of the objects behind
them is otherwise driven **exclusively** by the core `SimulationManager`'s tick loop. Handing
game-thread `UObject` code a general mutable handle to one of them would invite exactly the
cross-thread reach the entry point exists to bound. **Do not widen one into an accessor.**

⚠ **This list has now gone stale twice**, both times because a new member matching the pattern
exactly was added without extending the sentence. The rule it states is about the *class* of
member, not about these six names: **every public member of `AOGBrawlerSimulationManager` that
forwards to one core-owned object for one purpose belongs here**, and the enumeration is a
reading aid. Replacing the list outright with that sentence would end the drift, at the cost of
the six anchors `doc_anchor_lint.ps1` currently resolves through it — a trade worth making only
if the lead wants the anchors spent elsewhere.

*(Until og-ue-simulationcomponentandmanagersplit task 22 the list had ten names: it also named `pollInputHistory`,
`getInputHistoryRows`, `pollInputHistoryLanes` and `getInputHistoryLanes`. Task 22 moved them to
`BrawlerInputHistoryFeedUImpl`, which holds a `const` reference to this class pinned by a `static_assert`, so the
compiler, not this list, keeps them from writing production state. Their paragraphs, which followed here, moved with
them to `InputHistoryDisplay-rationale.md` §8.6.)*

*(Since og-ue-simulationcomponentandmanagersplit task 24b two of the names, `getLastRelayedInput` and `getLocalInputCache`,
are members of the session, `m_session`, with the composition reads they sit beside; the rule binds them there, and the
session's rationale states it for them (`UESimulationSession-rationale.md` §8). Since task 25 two more,
`requestInputDelayIncreaseStall` and `publishClientEffectiveInputDelayTicks`, are the session's, with the transport
(`UESimulationSession-rationale.md` §9.3). The other names here stay the Chaos host's.)*

**In the Chaos configuration only (§18):** `noteResimRequest` and `noteResimGrant` have no callers
but the Chaos hooks, and neither have the four plain forwards beside them (`onGameSimulation`,
`onPostGameSimulation`, `onCheckIsSimilar`, `prepareResimulation`), so all six sat in the Chaos arm.
Since task 82 they are members of `ChaosTickBackendUImpl`, the Chaos host, which reaches the manager through its
owner (`ChaosTickBackendUImpl-rationale.md` §1). They are still one-purpose forwards, and the rule above binds them
there. The Jolt host feeds the resim-gate probe from the step driver instead (task 18).

*The paragraph on `requestInputDelayIncreaseStall`'s `runsPrediction()` guard moved with the member to `UESimulationSession-rationale.md` §9.3
(task 25); the guard is `UESimulationSession-guards.md` G-03.*

### The latency-budget probe adds no crossing (og-simulationscheduler-withjolt task 1)

The probe is game-thread-owned. Its physics-thread stamps reach it through a single-producer
mailbox (synchronized), the one game-to-physics value it needs is an atomic, and every other read
its stamp sites make is same-thread with its writer. The CROSSING count is unchanged; the full
argument, read by read, is the *Threads* table of `UELatencyBudgetDiagnostics-rationale.md` §6 (OGSimulationUnreal)
for the template's reads and §16's *Threads* paragraph for the brawler's.

### One acknowledged wart: moved to the session (task 26a)

`getServerReceptionTick()` is the session's since og-ue-simulationcomponentandmanagersplit task 26a, with the reception
coordinator. Its unsynchronized read of the physics-thread server clock, and why it was left, moved with it to
`UESimulationSession-rationale.md` §9.7 ("Threads, and one acknowledged wart").

---

## §2 Construction, ownership and teardown

**Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 24a).** The composition (the latency member, the backend's worlds, the three
adapter slots, the storage, the static data, the three peers, the systems executor, the integration layer, the manager
and the backend) is the member `m_session`, a `UESimulationSession<BrawlerSessionTraitsUImpl>`. Its game-neutral reasoning
moved with it: the member order, the four `compositionContractsHold()` rows and the reorder hazard are
`UESimulationSession-rationale.md` §2, and the backend's two members and the destruction order are its §3, in
OGSimulationUnreal's docs. This section keeps the brawler's part.

### What this class holds, and in which order

The private data members, top to bottom: `m_roster`; `m_inputHistoryFeed` (the two frame probes that sat between them
until task 26b are the session's, `UESimulationSession-rationale.md` §9.9, and so, since task 26a, are the reception
coordinator, PROBE 5 and the delayed-input map, its §9.7); `m_game`, the brawler's phase hooks
(`BrawlerSessionGameUImpl`; since task 27b it holds a reference to `m_roster`, and since task 28 one to `m_inputHistoryFeed`,
both declared above it); and `m_session`, constructed with `*this` and `m_game`. The pending registrations, the last member
until task 28, are the session's since (`UESimulationSession-rationale.md` §2, §12.2). The static instance table is the session's since task 25 (`UESimulationSession-rationale.md` §9.2).

* **The order is today's for every member whose construction or destruction does something.** The roster, the two frame
  probes and the feed are declared before the session, so they are destroyed after it, as they were destroyed after the
  composition before task 24a (the reception state, which was declared here too, is destroyed inside the session since
  task 26a, its coordinator before the manager); the pending registrations are declared after it, so they are destroyed before it, as
  before. The roster must outlive the session: the unregistration that will run in the session's teardown calls its
  forgets (`BrawlerRosterUImpl-rationale.md`).
* **The feed is bound to the session before the session is constructed,** `m_inputHistoryFeed{ std::as_const(m_session) }`,
  and reads it only after: the binding forms a reference to the member object `m_session`, which is well-defined while this
  class is being constructed; what would not be is a reference into one of the session's members before the session's
  constructor began ([class.cdtor]/1; the initiative's review, finding m-13), and the feed holds none
  (`UESimulationSession-rationale.md` §8).
* **The timing callback and the timing relay pointer** sat between the worlds and the adapters until task 24a, before
  the session from task 24a, and since task 25 are the session's members `m_onTimingInfoReceivedCallback` and
  `m_timingRelay`, after its backend, with the tier consumer and the published-delay mark. Neither construction nor
  destruction of any of them is observable: the callback is a `std::function` that nothing in the tree sets (its setter
  is og-simulation's owner concept), so it is empty; the relay is a raw pointer; the tier consumer and the mark are
  emplaced and written only after `BeginPlay` builds the manager, and the consumer is reset in the teardown (this
  class's `EndPlay` until task 27a, the session's `endPlay` since; `UESimulationSession-rationale.md` §9, §10).
* **The start-up builds the session's state** (the actor's `BeginPlay` until task 27b; since then the session's `beginPlay`,
  which `BeginPlay` calls after `Super`): the adapters, then per role branch the integration layer, the systems executor
  (the brawler's hook `emplaceSystems`) and the manager, then the tier consumer and, on the authority, the reception
  coordinator. The order and why it holds moved to `UESimulationSession-rationale.md` §11, in OGSimulationUnreal's docs;
  the brawler's six start-up hooks are `BrawlerSessionGameUImpl-rationale.md` §6. The teardown unwinds the last two before
  the manager (`UESimulationSession-guards.md` G-21).

### The feed's four bridges: deleted (task 24b)

Task 24a, which moved the composition into the session, left the feed's file unedited, so this class declared four
private `const` references named as the feed read them, bound to the session's members after `m_session`, and the feed
compiled unchanged. Task 24b deleted the four, with the three aliases that typed them, when the feed came to hold a
`const` reference to the session and read it through the session's const members (`UESimulationSession-rationale.md`
§8). No reader of them is left (task 24b's notes record the grep). The feed is no longer this class's friend: what it
read of this class in task 24b, the tier consumer and the effective input delay, it read through public `const`
members, by way of the session's const `owner()`; since task 25 they are the session's (`UESimulationSession-rationale.md` §9).

### The systems executor's adapters: the brawler's verification (was G-75's)

The systems executor's guard moved to the session (`UESimulationSession-guards.md` G-01, in OGSimulationUnreal's docs):
the hit detector, `BrawlerCompositionUImpl::HitDetectionSystem`, is emplaced in each role branch of the start-up from
the integration layer's two adapters, and never from the reader: since task 27b by the brawler's hook `emplaceSystems`,
which the session's `beginPlay` hands `*m_physAdapter` and `*m_queryAdapter` (`BrawlerSessionGameUImpl-rationale.md` §6). The brawler's measurement of why the reader is wrong stayed here:

**Verified 2026-09-23 (Chaos configuration).** `ChaosPhysicsBodyAdapter::getBodyTransform` reads
`proxy->GetPhysicsThreadAPI()`; `ChaosPhysicsBodyReaderAdapter::getBodyTransform` reads
`proxy->GetGameThreadAPI()` (its own banner: "Reads GT-interpolated state"). Both satisfy
`PhysicsBodyReaderAdapter`, so the wrong one compiles. The detector runs on the physics thread, in
`firePreIntegrate` of the next tick (og-netcode-v2-field-defects task 20; `firePostIntegrate` before
it), and reads the TARGET's guard transform that the target's integrate wrote on the physics-thread
particle, as the physics step left it.

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

**Since og-ue-simulationcomponentandmanagersplit task 24a** the member is the session's, initialized as `m_staticData{ Game::makeStaticData() }`.
`BrawlerSessionGameUImpl::makeStaticData()` reads the movement console variables once, through this class's private static
`readMovementStaticDataCVars()` (this class befriends the hook's class), and returns the static data as a prvalue, so it is
still constructed in place, never copied or moved. It replaced the member `m_movementStaticDataCVars`, which held the cvar
result between the read and the construction; nothing else read that member. The hook is defined in
`OGBrawlerSimulationManager.cpp`, inside its `OGSIM_OPTIMIZE_OFF` region, so the read and the construction keep the
optimization state they had in this class's constructor; the cvar read is in `MovementSchemeCVar.cpp`, unchanged.

### The alias chain

Since task 81 the chain is defined in `struct BrawlerCompositionUImpl` (`BrawlerCompositionUImpl.h`; why it exists, the
member table, the single source of truth and the bind wrapper are in `BrawlerCompositionUImpl-rationale.md`). Since
og-ue-simulationcomponentandmanagersplit task 24a the session names the composition through its traits (`BrawlerSessionTraitsUImpl::Composition` is
`BrawlerCompositionUImpl`), and this class kept only the two re-exports its own code still named, `BrawlerHitDetectionSystem` and
`ManagerType` (the two role branches' emplaces in `BeginPlay`), until task 27b moved the start-up into the session: the
session names its own manager type, and the hit detector's alias is the hook class's (`BrawlerSessionGameUImpl`), so this
class keeps none. The others were deleted with the members they typed:
by task 24a with the composition, and by task 24b the three that typed the feed's bridges (the storage, reconciliation
and input-resolution aliases). The
row that `ManagerType` is the composition's manager moved to the session, which compares its own private manager type with
the one the tick-driver binding names.

`getTimeConfigPtr()` is the session's since task 24b, with its reasoning and its assertion (it returns a **pointer, not
a reference**, so the state before the manager is built is representable): `UESimulationSession-rationale.md` §8.

---

## §3 The session knobs: moved to the session (task 26b)

**Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26b).** The three ini intakes, the
authority apply and the resim-policy apply with its proof line are the session's: `readSessionKnobs`,
`applySessionKnobs` and `applyResimTriggerPolicy`. The start-up calls them at the positions of the statements they hold
(the session's `beginPlay` since task 27b, the actor's `BeginPlay` before):
the intakes after the latency member's begin and before the backend builds its world, the apply in the authority branch
after the neutral input, the policy after both role branches. The argument (the four steps, the floor, the rotation
width, the resim policy and the retired fourth knob) moved with them, verified and game-neutral, to
`UESimulationSession-rationale.md` §9.8, in OGSimulationUnreal's docs, with its four guards (G-57, G-56, G-58 and G-63
here → the session's G-14 to G-17; `OGBrawlerSimulationManager-guards.md` §R). R0 corrections made in that move are listed
there, marked "task 26b".

What stays here is the brawler's own configuration, and the history (§11 C1, C17, C18).

**The brawler's knobs, as shipped** (`Config/DefaultEngine.ini`, its `[OGNetcode]` block): `RelayDelayFloorTicks=6`, so
the authority's start-up prints `[RelayDelayFloor] session floor = 6 ticks (ini override)` and its advisory line;
`CorrectionRotationK` is commented out, so the proof line says `compiled default`; `ResimTriggerPolicy=OnDisagreement`,
so the proof line says `OnDisagreement (ini override)`. The same file runs `LogOGNet` at `Warning`, the level the proof
lines are written for.

**The timing relay is spawned first.** The floor's publish needs the session's timing relay to exist when `BeginPlay`
runs. The brawler's composition root, `USimulationManagerSubsystem::OnWorldBeginPlay`, spawns the relay before this actor
on every non-client world, which is why the failure line (`no timing relay at manager BeginPlay`) does not appear in a
healthy run.

---

## §4 Log categories and `RouteOGMessage`

**Code moved to `UESimulationLogRoutes.h` (og-ue-simulationcomponentandmanagersplit task 19).** The eight routed
`LogOG*` categories, the route table `kOGLogRoutes` with this module's 14 rows (now `kOGAdapterLogRoutes`), its five
`static_assert`s, `ogLogRouteFor`, `OG_LOG_ROUTED` and the router `RouteOGMessage` moved to the og-simulation-ue
plugin's `OGSimulationUnreal` module (`UESimulationLogRoutes.h` and `.cpp`, MPL-2.0). Their maintained text moved with
them, to `UESimulationLogRoutes-rationale.md`: the router (§2), the categories and the three probe families (§3), the
order (§4), the tag families (§5), the levels and the rates that set them (§6), `OG_LOG_ROUTED` (§7), the two blocks
(§8), the checks (§9), and the router's per-line notes (§10; this document's §13.6 until task 19). R0 corrections made
in that move are listed in its sections, marked "until task 19".

What stays in this module, and in this section:

* `LogOGBrawler`, declared in `OGBrawlerSimulationManager.h` and defined in its `.cpp`. It is the game-rule category: the
  `ogblog` sink this module installs on both role branches (since task 27b the hook `installGameLoggers`, defined in this
  class's `.cpp`; `BeginPlay`'s two lambdas before) logs to it, and so does `MovementSchemeCVar.cpp`. The router
  never logs to it.
* The router's callers: both role branches of the start-up hand `RouteOGMessage` to the core's components (`setLogger`)
  and to `simlog::setGlobal` (the session's `beginPlay` since task 27b, this class's `BeginPlay` before). The frame probes (PROBE A, whose server line is `[RelayProbe.Frame]`, and PROBE 6) log through it
  from the session since task 26b (this module's `.cpp` before), as does the session's PROBE 5 (since task 26a); this
  module instantiates the session.
* The brawler's setting of the field-diff gate (below). The gate itself, `bindCorrectionFieldDiffGate`, and its guard
  (G-50 here) are the session's since task 27b.
* Which rows this module emits, its direct log sites, the two latency categories, and the record of what
  og-simulationscheduler-withjolt task 80 and og-tickdriver task 12 checked and measured (below).

### The field-diff cost gate: moved to the session (task 27b)

**Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 27b).** `bindCorrectionFieldDiffGate`
is the session's private static member, called at the same two positions (the end of each role branch of the start-up,
after the `setGlobal` sink installs), and its guard G-50 is the session's G-22. The argument (og-simulation ships the walk
closed; what the predicate guarantees; why it is a live predicate and declares `-> bool`; the one category it must share
with `RouteOGMessage`; the caveat that it is not yet confirmed in PIE) moved with it, verified and game-neutral, to
`UESimulationSession-rationale.md` §11.4, in OGSimulationUnreal's docs.

What stays here is the brawler's setting: `Config/DefaultEngine.ini` sets `LogOGDivergenceProbe=Verbose` (§13.10, C13-1),
so outside Shipping the brawler's sessions walk the fields of every disagreeing correction and its correction lines carry
the `field=` tail.

### The two latency categories

| category | tags |
|---|---|
| `LogOGLatencyBudget` | `[LatencyBudget]`, `[LatencyBudget.Window]` (logged with `UE_LOG` directly, not routed; since task 19a declared, exported, in OGSimulationUnreal's `UELatencyBudgetDiagnostics.h` and defined in its `.cpp`, both under `!UE_BUILD_SHIPPING`; that template's rationale §3 and §7) |
| `LogOGChaosDilation` | `[ChaosDilation]` (logged with `UE_LOG` directly, not routed; since task 19a declared in `ChaosTickBackendUImpl.h` and defined in its `.cpp`, gated by `OG_LATENCY_BUDGET_DIAGNOSTICS`, so it exists only in the Chaos configuration; §16) |

They are not routed and have no row; the rule that gives each probe family its own category is
`UESimulationLogRoutes-rationale.md` §3.

### This module's rows, by emitter (og-tickdriver task 12)

**Which rows are og-simulation's** was decided by the emitters: every string literal in a non-comment line
under `Plugins/` and `Source/`, with a leading `[Verbose]` or `[Warning]` token removed as `RouteOGMessage`
removes it, assigned to the row that routed it.
- og-simulation only (25 rows, now in `kSimulationLogRoutes`): every row not listed below. Their emitting
  files are listed in og-simulation's `SimulationLogRoutes-rationale.md` §5.
- Emitted by og-simulation **and** this module (kept here): `[ResimProbe` (also `ChaosTickBackendUImpl.cpp`, this
  module's `.cpp`, and since task 26b the session header's PROBE A client line, instantiated only in this module),
  `[ServerReceive]` (also `OGBrawlerSimulatableComponent.cpp`) and `[RelayProbe` (also the session header's PROBE A server
  line and PROBE 6 lines, this module's `.cpp` until task 26b, and its PROBE 5 lines, this module's `.cpp` until task
  26a; the session is instantiated only in this module).
- Emitted by this module only (kept here): `[ResimCheck.TriggerRewind]` (`ChaosTickBackendUImpl.cpp`),
  `[ClientPrediction]` (`OGBrawlerInputCollectionComponent.cpp`), `[ReceiveCorrectionState]`
  (`OGBrawlerSimulatableComponent.cpp`), `[ReleaseBatch]` (this module's `.cpp` until task 26a; since then the session header's, instantiated only in this
  module), `SimulationManager:` (this
  module's `.cpp`), `tryRegister:` (`BrawlerJoltBindingUImpl.cpp`'s capsule `checkf`, the Jolt host's `.cpp` until og-tickdriver task
  15a, and this module's `.cpp`) and `NewFramework:`
  (`OGBrawlerSimulatableComponent.cpp` and this module's `.cpp`).
- No emitter (kept here; deleting them was out of scope): `[SendRemoteInputToClients]`,
  `[ReceiveCorrectionInput]`, `[InjectCorrectionInput]`, `[DrainOutOfOrder]`.

### This module's direct log sites (og-simulationscheduler-withjolt task 80)

**Direct log sites read the same table: `OG_LOG_ROUTED`.** Six per-tick or per-packet lines in this
module are logged with `UE_LOG` directly rather than through `RouteOGMessage`. They now use
`OG_LOG_ROUTED(Category, Format, ...)` (`UESimulationLogRoutes.h` since task 19), which looks up the row whose
prefix begins `Format` at compile time, fails the build if there is none, and expands to
`UE_LOG(Category, Verbose, ...)` or `UE_LOG(Category, Log, ...)` from that row's `plainLevel`. So the
level is set in one place for routed and direct lines alike. The sites:

| row | site | file |
|---|---|---|
| `[ReleaseBatch]` | `releaseDelayedInputsForStep`, the session's since og-ue-simulationcomponentandmanagersplit task 26a | `UESimulationSession.h` (OGSimulationUnreal; `OGBrawlerSimulationManager.cpp` before) |
| `[ResimCheck.TriggerRewind]` | `TriggerRewindIfNeeded_Internal`, the two `rewind=0` lines (Chaos only) | `ChaosTickBackendUImpl.cpp` |
| `[ServerReceive]` | `receiveRemoteMoveBundle` (the RPC tail since og-ue-simulationcomponentandmanagersplit task 20a), the no-coordinator path | `UESimulatableOwnerBinding.h` (since task 21; was `OGBrawlerSimulatableComponent.cpp`) |
| `[ReceiveCorrectionState]` | `onCorrectionStateAccepted`, called from `USimulatableNetSyncComponent::OnRep_CorrectionState` since task 20a | `UESimulatableOwnerBinding.h` (since task 21; was `OGBrawlerSimulatableComponent.cpp`) |
| `[ClientPrediction]` | `buildPlayerInput` | `OGBrawlerInputCollectionComponent.cpp` |

The category stays at the site. Nothing checks that it is the row's category; all six agree today.

**Direct sites that stay at a fixed `Log`:**

* the `rewind=1` line of `TriggerRewindIfNeeded_Internal` (Chaos only). It is a resim request, a
  rare event, but it shares the `[ResimCheck.TriggerRewind]` tag with the per-step `rewind=0` lines,
  and the text that tells them apart is at the end of the line, where no prefix row can reach it.
* `[RelayDelayFloor]` and `[ConnectionTier]` (session and tier events, at most about 0.1 per second;
  `[ConnectionTier]` also in `OGBrawlerSimulatableComponent.cpp`). They have no row and need none. Their sites are the
  session header's since tasks 25 and 26b (the floor's ini line moved with `applySessionKnobs`).

### History: what tasks 80 and 12 checked and measured

*Names here are as they stood then: task 21a renamed the brawler component `USimmableUpdateComponent` to `UOGBrawlerSimulatableComponent`, in `OGBrawlerSimulatableComponent.h` and `OGBrawlerSimulatableComponent.cpp` (docs `OGBrawlerSimulatableComponent-rationale.md` and `OGBrawlerSimulatableComponent-guards.md`, ids unchanged).*

*Names here are as they stood then: task 30 renamed the actor `ASimulationManagerUImpl` to `AOGBrawlerSimulationManager`, its files to `OGBrawlerSimulationManager.h` and `OGBrawlerSimulationManager.cpp`, and this docs pair to `OGBrawlerSimulationManager-rationale.md` and `OGBrawlerSimulationManager-guards.md` (ids unchanged).*

*Code moved to `UESimulationLogRoutes.h` (task 19). The records below are those tasks', kept here as they were: "the
header" is `SimulationManagerUImpl.h` of that time, and "this module's block" is today's `kOGAdapterLogRoutes`. Task 19
did not re-run them; its own checks are in `UESimulationLogRoutes-rationale.md` §9.*

#### og-simulationscheduler-withjolt task 80

**What the compiler checks.** Each arm below was compiled out of tree: a copy of the header with the
same line count, a scratch translation unit that includes it, and UBT's own compiler response file
for `SimmableUpdateComponent.cpp` (same flags, same shared PCH). The tree was not modified.

| check | poison | result |
|---|---|---|
| `ogLogRoutesAreAllReachable` (unchanged, now in the header) | `[ReceiveLocalInput] DISCARD` row moved below `[ReceiveLocalInput]` | `C2338`, "a route's prefix BEGINS WITH an earlier route's prefix" |
| `ogLogRoutePlainLevelsAreLogOrVerbose` | the `[Park]` row's `plainLevel` set to `Warning` | `C2338`, "a row's plainLevel is the level of a line that carries no [Verbose] or [Warning] token" |
| `OG_LOG_ROUTED`'s row lookup | `OG_LOG_ROUTED(LogOGNet, TEXT("[NoSuchTag] x=%d"), x)` | `C2338`, "the format's leading tag has no kOGLogRoutes row" |
| control | the unpoisoned header, ten `static_assert`s on `ogLogRouteFor` (for example `[ServerReceive]` is `Verbose`, `[ReceiveLocalInput] DISCARD` is `Log`, `[RelayDelayFloor]` has no row) and two `OG_LOG_ROUTED` uses | compiles, 0 errors |

**Measured.** Headless sessions on the editor binary (Jolt, Development): a `-game -server` with the
committed ini plus the level under test, and two idle `-nullrhi` clients on loopback, about 95 s
each. `[SimHost.Window]` p99 in µs, the range over the steady 10 s windows after both clients joined:

| run | `preUs` p99 | `stepUs` p99 | `postPhysicsUs` p99 | lost steps |
|---|---|---|---|---|
| before, `Log`, visible console | 2895–3835 | 379–2915 | 1529–3702 | 0 |
| after, `Log`, visible console | 19–23 | 134–160 | 139–176 | 0 |
| after, `Log`, no console | 17–26 | 149–228 | 141–185 | 0 |
| before, `Warning`, visible console | 13–14 | 106–118 | 88–108 | 0 |
| after, `Verbose`, no console | 137–157 | 197–215 | 190–215 | 0 |

`LogOGNet` / `LogOGSimTick` lines at `Log` and `Verbose` in the same sessions:

| run | server | client 1 |
|---|---|---|
| before, `Log` | 91 513 at `Log`: the 8 per-tick tags plus `[RelayDelayFloor]` ×2 and `[ConnectionTier]` ×1 | 54 006 at `Log`: the 9 per-tick tags plus `[ConnectionTier]` ×2 and `[RelayDelayFloor]` ×1 |
| after, `Log` | 3 at `Log`: `[RelayDelayFloor]` ×2, `[ConnectionTier]` ×1 | 3 at `Log`: `[ConnectionTier]` ×2, `[RelayDelayFloor]` ×1 |
| after, `Verbose` | the same 3 at `Log`, plus 91 453 at `Verbose` (the 8 per-tick tags) | the same 3 at `Log`, plus 53 725 at `Verbose` (the 9 per-tick tags and `[Resim.Input]`) |

The server's per-tick tags are `[ServerReceive]`, `[CollectInput]`, `[Park]`, `[ReceiveLocalInput]`,
`[Release]`, `[ReleaseBatch]`, `[AuthoritySimulation]` and `[SendCorrectionStateToClients]`; the
client's are `[ResimCheck.Check]`, `[CollectInput]`, `[PostPrediction]`, `[SendLocalInputToServer]`,
`[PredictionSimulation]`, `[ResimCheck.IsSimilar]`, `[ClientPrediction]`, `[ReceiveCorrectionState]`
and `[InjectCorrectionState]`. The `LogOGSim` lines (resim lifecycle, resyncs) are at `Log` in all
three runs.

At the committed `Warning`, the set of category, level and tag triples in the before and after
sessions is identical on the server; on the clients it differs by one run-dependent
`[InputRelay]` warning that one client hit in each run.

#### og-tickdriver task 12

**The one-time proof, not shipped.** A scratch translation unit froze the 39 rows of the single table
verbatim from the pre-task header, included the new header, and asserted that each frozen row's prefix
resolves through `ogLogRouteFor` (the new table's first match) to a row with the same prefix, route and
`plainLevel`, and that each frozen row appears in the new table exactly once. It compiled with 0 errors. The
same TU with the frozen `[Park]` row's level flipped failed (`C2338`, "a frozen row resolves to a different
(route, plainLevel) in the new table"), so the check is not vacuous. It also compiled two `OG_LOG_ROUTED`
uses, one of an og-simulation row (`[Park]`) and one of this module's (`[ReleaseBatch]`).

**What the compiler checks, seen failing.** As before, each arm compiled a copy of the header with the
same line count from a scratch translation unit, with UBT's own compiler response file for
`SimmableUpdateComponent.cpp`; the tree was not modified. The unpoisoned copy compiled with 0 errors.

| check | poison | result |
|---|---|---|
| `ogLogRouteBlocksAreDisjoint` | this module's `[ReleaseBatch]` row's prefix changed to `[Resim` (og-simulation's `[Resim.Input]` and `[Resim.` begin with it) | `C2338`, "a prefix in the og-simulation block and a prefix in this module's block BEGIN WITH one another"; no other assert fired |
| `ogLogRouteBlocksAreDisjoint`, ignoring case | `NewFramework:` changed to `[time` (og-simulation's `[TimeResync.` begins with it, ignoring case) | the same `C2338` |
| `ogLogRoutesAreAllReachable`, this module's block | `NewFramework:` changed to `tryRegister: x` | `C2338`, "a route's prefix BEGINS WITH an earlier route's prefix" |
| `ogLogRoutePlainLevelsAreLogOrVerbose` | `[ReleaseBatch]` at `Warning` | `C2338`, "a row's plainLevel is the level of a line that carries no [Verbose] or [Warning] token" |
| `ogLogRoutesAreAllMapped` | the `SimulationLogRoute::Net` case of `ogLogRouteOf` deleted | `C2338`, "an og-simulation row's SimulationLogRoute has no OGLogRoute in ogLogRouteOf" |

The first arm is the case the new check exists for: the reachability check alone would accept it, because
`[Resim` sits after the rows it overlaps.

**Measured.** Headless harness sessions (90 s, a server and two clients, one leaving and a new one joining)
on the Jolt and the Chaos Editor, before and after, with every routed category at `Log`, then at `Verbose`
(`-LogCmds`). In all 16 process pairs, no tag present on both sides changed its category or its level. The
per-tick tags agree within 3 % at the same steps per window (up to +3.0 % on Jolt at `Verbose`, a uniform
+1 % of it from the after run's processes living longer); the low-rate resim tags vary as they do
between two runs of one binary.

---

## §5 The client's effective input delay

**Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 25).** The effective input delay,
its one recompute, the tier consumer, the server-owned tier, the preserved tier-0 quirk and its two consequences, and why
replayed values never stall are the session's, and the text moved with the code, made game-neutral and verified (two
corrections: the published-delay mark is kept instead of reading back the input-resolution peer's atomic, not "the net
sync"; the tier-0 baseline is the reception coordinator's map) to `UESimulationSession-rationale.md` §9.4, in OGSimulationUnreal's docs. Its
guards moved too: G-16, G-61, G-62 and G-65 are `UESimulationSession-guards.md` G-05 to G-08. The two subsections below are
the brawler's and stay here.

### The zero input

*The call moved to the session's `beginPlay` and the value to the brawler's hook (og-ue-simulationcomponentandmanagersplit
task 27b).* Why the neutral input is set on the authority too, and before every registration, is the session's
(`UESimulationSession-rationale.md` §11.3, guard `UESimulationSession-guards.md` G-23, this file's G-59 until then). The
brawler's value, `getZeroPlayerInput()`, is the hook `neutralInput()`, and why it is not `PlayerInput{}` is
`BrawlerSessionGameUImpl-rationale.md` §6 (guard `BrawlerSessionGameUImpl-guards.md` G-01, this file's G-60 until then).

### Inbound-hit routing

Cross-character inbound-hit routing runs **inside** `m_manager->onGameSimulation`, via
`brawlerHitRouting::System::preIntegrate` (since og-netcode-v2-field-defects task 20; it was
`postIntegrate`), fired by the systems executor before `integrateAll` on every tick — including each
resim replay tick, whose first step now re-routes what the restored tick produced — so the routed
`HitFlinch` flags stay deterministic. There is no adapter-side routing wrapper any more: the former
`routeInboundHits()` shim and its map were removed once the routing system owned the whole pass, and
no reset-order hazard remains because the single routing pass *is* the system's `preIntegrate`.

---

## §6 The relay ring

### The host boundary: moved to the session (task 25)

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 25). `onInputRelayHostReady` is the
session's, and it attaches the relay host to the character's `USimulatableNetSyncComponent` (the brawler component's
binding forwarded there before). The bridge argument (no map, idempotent) moved with it to `UESimulationSession-rationale.md` §9.5.

### The relay tap: moved to the session (task 26a)

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26a). `relayRemoteInput` is the
session's, beside the other reception sink. Its argument (stage, do not write; no depth at the site; the unchecked staging
outcome; which ring) moved with it, verified, to `UESimulationSession-rationale.md` §9.7, in OGSimulationUnreal's docs
(its R0: the tap's no-host fallback is the sibling's staging ring, `m_detachedRelayStagingRing`). The brawler's history
stays here:

**The dual-write fence is discharged.** The relay tap originally wrote only the ring, leaving
`SimulationNetSync::sendCorrectionAll`'s input write into `m_replicatedInputSyncedBuffer` running
beside it so no reader had to move and be moved back. The readers were then switched and that
second write — and the whole correction-input channel with it — was removed. This tap is now the
only path by which a character's input reaches other clients. The viz call site reads it through
`SimulationInputResolution::getLastRelayedInput`, whose nullopt contract was modelled on the
retired `editReconciliation().getLatestInput(...)` so `BrawlerVisualizationInputSource.h`'s
cold-source skip did not have to be re-specified.

---

## §7 The reception coordinator and the transport adapter

**Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26a).** The reception coordinator
and its emplace, its three accessors, the delayed-input map and its registration, the two sinks, the per-step release,
and the reap and the forget are the session's. The argument (the coordinator and the transport adapter, the two sinks,
the map, the drain) moved with them, verified and game-neutral, to `UESimulationSession-rationale.md` §9.7, in
OGSimulationUnreal's docs. This class keeps `noteDelayedInputComponent` as a wrapper that turns the pawn's
`SimCharacterId` into the storage key (`toStorageKey`, §15) and passes it to the session's (*task 29:* the wrapper has
no caller since: the component's registration skeleton, `UESimulatableOwnerBinding`, calls the session's with the key), and it calls the session at
the positions of the statements it held: `beginReceptionCoordinator(pctmloggerServer)` on the start-up's authority branch
(the actor's `BeginPlay` until task 27b, as `m_session.beginReceptionCoordinator`; the session's `beginPlay` since), and
`forgetReceptionOwner(id)` in `unregisterFromNewFramework` (this class's, as `m_session.forgetReceptionOwner`, until task
28; the session's own since, `UESimulationSession-rationale.md` §12.3). What stays here is the
history below and the brawler's two hosts' call patterns.

`sampleAndDeriveConnectionTier` and `tryEnqueueDelayedRemoteInput` are **gone**; their
engine-primitive acquisition moved up to the RPC boundary (the component binding's `receiveRemoteMoveBundle`), which now forwards straight into
`ServerReceptionCoordinator::noteRttSample` (once per bundle) and
`ServerReceptionCoordinator::receiveInputBundle` (the whole per-slot loop). Nothing of
the RPC per-slot path remains manager-side.

### The hosts' call patterns (the brawler's Chaos and Jolt configurations)

The three pieces of a frame's reception work (the session's `onFrameStepsDue_GameThread`, this class's until task 26b;
the session's drain `releaseDelayedInputsForStep`; and the coordinator's reap; `UESimulationSession-rationale.md` §9.7 and
§9.9) are called by the brawler's two hosts as follows.

**The Chaos configuration's call order is the order of the code before the split.**
`InjectInputs_External` (a `ChaosTickBackendUImpl` member since task 82) runs on the game thread once per
physics frame, before the frame's steps are handed to the solver, and calls: the mapper's
`firstUpcomingSimTick` (`ChaosTickBackendUImpl-rationale.md` §4, `ChaosTickBackendUImpl-guards.md` G-07); then
`onFrameStepsDue_GameThread(first, NumSteps)`; then, when a coordinator exists, the drain with
`(first, NumSteps)` and `reapConnections(first)`. So this configuration releases the frame's whole
batch up front, before its first step: one `[ReleaseBatch]` per frame, with `numSteps` equal to the
frame's step count. Every `[Release]` line that follows lies in `[first, first + numSteps)` of the
nearest `[ReleaseBatch]` before it. That containment is the within-run check of the pattern (§18).

The drain hands over only the game-thread-safe upcoming sim tick (`ChaosTickBackendUImpl-rationale.md` §4) and the per-id callback. The
reap was relocated here off the former arrival-gated RTT sample path: it runs once per physics frame
regardless of traffic — a documented benign cadence change, since an idle server now reaps — and the
coordinator gates it on the dwell boundary internally. Its tick comes from the mapper, **not** the
physics-thread-written server clock.

PROBE A runs on both roles and a pure client never has a coordinator, so every coordinator test sits
below it.

**The Jolt configuration's call pattern (og-simulationscheduler-withjolt task 54).** The same three
pieces, at the three places design D3 puts them (`SimulationTickDriver-rationale.md` §3 since og-tickdriver
task 8; `BrawlerTickDriverGameUImpl-rationale.md` §2):

* `onFrameStepsDue_GameThread(first + offset, n)` once per frame at `TG_StartPhysics`, only when the
  scheduler made `n ≥ 1` steps due: the condition and the arguments under which Chaos called
  `InjectInputs_External`.
* The drain **per simulation tick**, from the authority step's `beforeTick`, as
  `releaseDelayedInputsForStep(authorityTick, 1)`: one `[ReleaseBatch]` with `numSteps=1` per step,
  for the tick that step will simulate (`TickDriverStepHooks-guards.md` G-01 and `BrawlerTickDriverGameUImpl-guards.md`
  G-01, the successors of the retired G-01 of the brawler's former step hooks).
* `reapConnections(first + offset)` **once per frame, after the frame's inline steps**, with the value
  the probe call received. It stays per frame because it is the only feed of the coordinator's
  `noteServerTick`, the receipt gate's reference: per step, the reference would sit at the frame's last
  tick, not its first, when the next frame's receipts are judged, and the gate's window would shift by
  the frame's step count (`SimulationTickDriver-guards.md` G-06, which succeeds frame-host G-06). On the authority the value is exactly the server clock plus
  one, because every earlier step has completed inline and published its offset.

No receipt can land between inline steps (receipts happen in the engine's receive dispatch, before the
tick groups), so the queue a step drains holds what the batch would have held. The two patterns
release the same `(id, captureTick, releaseTick)` set and differ only in which step pops an input in a
multi-step frame with a per-character gap (design D4): the batch hands the next input to the gap tick;
per step, the gap tick underruns and the rest apply on time. The comparison is therefore by
distribution, never line by line (§18).

---

## §8 The probes

All of them are purely diagnostic: nothing reads them but the log lines they feed. **Volume
convention: per-window summaries at `Warning`, per-event detail at `Verbose`, and nothing
per-tick or per-write at any verbosity.** The three frame probes are the session's: PROBE 5 since task 26a, PROBE A and
PROBE 6 since task 26b (`UESimulationSession-rationale.md` §9.7, §9.9). This section keeps the brawler's part of PROBE A
and PROBE 6, and the brawler's own probes below them.

### PROBE A — frame health: moved to the session (task 26b); its tick source in the brawler's two hosts

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26b): `m_frameHealthProbe` and its
call in `onFrameStepsDue_GameThread`, with the window-length assertion. The argument (what it settled, why the client
needed it, why it lives beside the host's call, why this hook, why the client's tick is safe, the per-role tags, the
window comparability) moved, verified and game-neutral, to `UESimulationSession-rationale.md` §9.9.

**The brawler's tick source, per configuration.** The probe is measured on the game thread from the tick its host hands
it, and the only tick source legal to read there is the host's own. In the Chaos configuration that is the
`ChaosTickMapper`'s atomic offset, which the brawler's Chaos host owns (a `ChaosTickBackendUImpl` member since task 82):
`InjectInputs_External` calls `onFrameStepsDue_GameThread` with the mapper's tick (§7, §18). In the Jolt configuration
the tick driver calls it at `TG_StartPhysics` with its dispatch counter plus the published offset J3 (§1), which is the
mapper's value except that a client Skip or Stall's ±1 reaches the probe one frame earlier
(`BrawlerTickDriverGameUImpl-rationale.md` §4; the frame host until og-tickdriver task 8); the line formats are unchanged
(og-simulationscheduler-withjolt task 54). The `+ 1` in the Chaos host's derivation is proved under an authority-only
assumption (`ChaosTickBackendUImpl-rationale.md` §4); the session's §9.9 says why the probe's deltas are safe on a client
anyway.

### PROBE 5 — relay writes per frame: moved to the session (task 26a)

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26a): `m_relayWriteProbe` (with its
guard, G-17 here → `UESimulationSession-guards.md` G-12), its write in the relay sink and its forget are the session's, and
the probe's argument moved, verified, to `UESimulationSession-rationale.md` §9.7, in OGSimulationUnreal's docs (its R0:
the owner id on its lines is the peer-stable storage key, §15, not a per-process number).

### PROBE 6 — per-connection send budget: moved to the session (task 26b); the brawler's packet-budget literal

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26b): `m_connectionBudgetProbe`, its
call below the coordinator test in `onFrameStepsDue_GameThread`, and the one-shot `[PacketBudget]` line. The argument
(why arithmetic was not enough, the read before the flush, the cumulative counters, the allowance's denominator, the
capacity pin and its word rounding) moved, verified and game-neutral, to `UESimulationSession-rationale.md` §9.9, with
the guards on the coordinator test and the counters (G-70 and G-72 here → the session's G-18 and G-19) and the
derivation of the word rounding (§13.8's D-50 → the session's D-01). Two R0 corrections made there: `notReady` counts
`QueuedBits` at or above zero, an approximation of the engine's own not-ready test; and the `[PacketBudget]` line is one
shot per process, not per session.

**The brawler's literal.** The capacity pin is checked against the brawler's packet-budget literal, pinned in
`Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp`. It is an **upper
bound**: if the `usableSingleBunchBytes` the line logs is ever smaller than that literal, the literal is optimistic and
must be lowered there.

### The resim-gate feeds (`noteResimRequest` / `noteResimGrant`), client only

*Chaos configuration only (§18): both feeds are called from the Chaos hooks, and since task 82 both are
`ChaosTickBackendUImpl` members. In the Jolt configuration
the step driver feeds the same probe (`SimulationStepDriver::resimulateIfRequested`: a request when the
gate names an anchor, a grant when the world still holds that tick), so `[ResimProbe.Gate]`,
`[ResimProbe.Chaos]` and `[ResimProbe.Apply]` keep their formats with a sim-tick frame domain: a
refusal is a ring miss, a clamp cannot happen (task 54).*

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

*Since og-ue-simulationcomponentandmanagersplit task 27b the startup proof line below, and the field-diff predicate the
next paragraphs name, are the session's statements in its `beginPlay`'s client branch, unchanged
(`UESimulationSession-rationale.md` §11).*

**The startup proof line** (`[ResimProbe.Session]`) is the fourth step of that family's
category path — declaration, router branch, ini block, then this — and the one that makes the
other three checkable from a log instead of from the source. It reports the **effective runtime
verbosity**, not a constant: `verboseDetail` says whether the per-event half will emit at all,
which is the single most likely thing to be silently off when somebody goes looking, and
`windowSamples` states the denominator every per-window line is divided by, taken from the constant
rather than retyped. **Its own absence is also information:** no `[ResimProbe.Session]` line in a
client log means either the category was set to `NoLogging` or the branch never ran, and both are
worth knowing before reading a zero off any counter.

**The statement is compiled only `#if !NO_LOGGING`** (og-brawler-shrink-install task 9,
2026-09-26). Its arguments call `LogOGResimProbe.GetVerbosity()` and `.IsSuppressed(...)`. In a
build with `NO_LOGGING` (Shipping without `bUseLoggingInShipping`) every log category is an
`FNoLoggingCategory`, which has neither member. `UE_LOG` still type-checks its arguments there, so
without the `#if` the Shipping client does not compile (`error C2039` ×2). With logging on, the
preprocessor keeps the statement byte-for-byte, so Development and Debug behave as before. In a
`NO_LOGGING` build the line could never print anyway.

For the same reason, the field-diff predicate in `bindCorrectionFieldDiffGate` declares its return
type as `-> bool` (same task). Under `NO_LOGGING`, `UE_LOG_ACTIVE(...)` expands to `(0)`, so the lambda
returned `int`. `std::function<bool()>` then converts that `int` to `bool` inside the standard
library, and warning C4800 is an error in this build (`error C4800` at `functional(810)`). The
Shipping client first reported it once the two C2039 errors above were gone. With the return type
declared, the conversion is a constant `0` in the lambda's own `return`, like the
`const bool pushProbeActive = UE_LOG_ACTIVE(...)` initialisation in the same file, which compiles
cleanly. With logging on, `UE_LOG_ACTIVE` already yields a `bool`, so nothing changes. The
predicate still reads the category live (guard G-50).

### The rewind-push probe (`[ResimProbe.PushTarget]` / `[ResimProbe.PushVerdict]`), client only

Moved to `ChaosTickBackendUImpl-rationale.md` §3 (og-simulationscheduler-withjolt task 82), with the code it describes.

### `[SimHost.Window]` — the Jolt host's window line, both roles (og-simulationscheduler-withjolt task 52)

Not a probe of this class: og-simulation's tick driver owns the line and its layout since og-tickdriver task 8
(`SimulationTickDriver-rationale.md` §7, `TickDriverWindow-rationale.md`; the frame host before). It belongs to this section's convention all the same:
one `Warning` summary per 10-second window per role on `LogOGSimHost`, which the ini pins at `Warning`,
so it prints in normal play, and nothing per tick. Jolt configuration only. It reports the game
thread's frame time and the host's slices of it, the steps run, the scheduler's lost steps and lost
time, the J1 and J6 drops, the game thread's waits for the world mutex, the step's measured cost from
J6 (the normal step, and the replay or the input release before it), and the shadow world's restore
cost. It is the line gate 22's "lost steps per role" and the hitch check read.

The Jolt host (`TickDriverBackendUImpl` since task 83) feeds it two game-thread samples:
* **The world mutex wait.** `tryRegister`'s bind takes the time just before its J2 scope and hands the
  wait to the tick driver's `noteWorldLockWait` once it holds the mutex. That bind is the game thread's only
  J2 scope while the host steps (the shadow-world ruling, `TickDriverBackendUImpl-rationale.md` §2); the `BeginPlay` scopes run before the
  first step and are not timed. On a worker client a wait therefore shows only in a window with a
  join; on the authority the step is inline and the wait is never contended.
* **The shadow restore.** The world host's `restore_GameThread` hands each restore's duration and whether it
  was refused, through the presenter's restore note, to the tick driver's `noteShadowRestore`, on a worker client.

---

## §9 Tick alignment — the `+ 1`

The Chaos configuration's derivation of the `+ 1` moved to `ChaosTickBackendUImpl-rationale.md` §4 (og-simulationscheduler-withjolt task 82).

**That derivation is the Chaos configuration's** and retires with it (task 21). The Jolt configuration
has no mapper and no `+ 1`: the step driver hands each authority step `serverTick + 1` and checks after
the step that it simulated exactly that tick (`SimulationStepDriver-rationale.md` §7), and the per-frame
tick is the tick driver's dispatch counter plus the offset `afterTick` publishes, `tick − physicsStep`,
in which the `+ 1` is already contained (`SimulationTickDriver-rationale.md` §5;
og-simulationscheduler-withjolt task 54).

---

## §10 Registration and unregistration — moved to `UESimulationSession` (task 28)

*Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 28): `tryRegister` and
`unregisterFromNewFramework`, after their first statements, are the session's own sequences, with the pending record
(`PendingRegistration` and its map) and `TryRegisterStatus`; this class keeps the two `SimCharacterId` wrappers (the
`SimCharacterId::None` check, `toStorageKey`, §15), which call them. The brawler statements are the four registration
hooks of `BrawlerSessionGameUImpl`, at their statements' positions, and their bodies call the roster
(`BrawlerRosterUImpl-rationale.md` §2 and §4) and the input-history feed.*

*This section moved two ways (manager design §4.3; R0 on every claim). The generic contract — the two phases, the
resolvability gate, the systems executor's notify, the unregistration's order (its items 1 to 3) — is
`UESimulationSession-rationale.md` §12, in OGSimulationUnreal's docs, game-neutral. The brawler's part — the capsule body
id, the movement sub-simulation, the shared attach parent, the notify's brawler dependants and the roster and display
forgets (item 4) — is `BrawlerSessionGameUImpl-rationale.md` §7, with three corrections task 28 found: the collision
category needs one row per configuration's table, not one per role branch of `BeginPlay`; the notify runs on both roles,
not only the authority's; and the first-call pass's body work is the backend's `createBodies_GameThread`, not
`tryRegister`'s own. The pre-diet cap's subsection moved to the roster in task 23 (below).*

### ∴D-01 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): the pre-diet cap moved to `BrawlerRosterUImpl`

**Was:** the derivation tag on `kPreDietCharacterCap`'s declaration in `OGBrawlerSimulationManager.h`, which pointed at
this subsection, *The pre-diet character cap*: where 4 came from, why the emission is once per over-cap character at
`Warning`, why the denominator is a set, what lifting the cap means, and the coupling to
`brawlerRingout::kMaxSpawnPoints`.

**Why it retired.** Its subject moved. Task 23 moved the constant, both of its `static_assert`s and the cap check into
`BrawlerRosterUImpl`, and the subsection moved with them, verbatim, with an R0 note. **Successor:**
`BrawlerRosterUImpl-rationale.md` §4, *The pre-diet character cap*, the heading that carries the roster's D-01 tag.

---

## §11 Corrections — claims these files carried that were not true

*Names here are as they stood then: task 21a renamed the brawler component `USimmableUpdateComponent` to `UOGBrawlerSimulatableComponent`, in `OGBrawlerSimulatableComponent.h` and `OGBrawlerSimulatableComponent.cpp` (docs `OGBrawlerSimulatableComponent-rationale.md` and `OGBrawlerSimulatableComponent-guards.md`, ids unchanged).*

*Names here are as they stood then: task 30 renamed the actor `ASimulationManagerUImpl` to `AOGBrawlerSimulationManager`, its files to `OGBrawlerSimulationManager.h` and `OGBrawlerSimulationManager.cpp`, and this docs pair to `OGBrawlerSimulationManager-rationale.md` and `OGBrawlerSimulationManager-guards.md` (ids unchanged).*

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
  held `m_detachedRelayRing`, the *no-host read fallback* (since og-ue-simulationcomponentandmanagersplit
  task 20b the sibling `USimulatableNetSyncComponent` holds it). A server write with no host lands in
  `m_detachedRelayStagingRing` instead (`SimulatableNetSyncComponent-rationale.md` §10,
  `UESimulatableOwnerBinding-rationale.md` §6 since og-ue-simulationcomponentandmanagersplit task 21,
  `SimmableUpdateComponent-rationale.md` §12; the
  header comment this line used to quote was removed in task 25's conversion and was itself false,
  F-25-5);
- the registration is **`DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing,
  COND_SkipOwner)`**. The old `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` stood
  on the component and was replaced; its absence is fenced by G-09 of
  `SimulatableNetSyncComponent-guards.md` (G-33 of `SimmableUpdateComponent-guards.md` until
  og-ue-simulationcomponentandmanagersplit task 20a moved the registrations to that component).

A third clause was stale in a third way: it said this *writes* the ring, where the write path has
since become **staging** (`UESimulatableOwnerBinding-rationale.md` §6, the staging write).

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
  teleport seed; ⚠ but that "respawn" leaves the body below the kill plane and it dies again on
  the next tick. This bullet undersold it; that is the death loop T18 fixed for characters 5-8
  (§10);
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

*Code moved to `UESimulationLogRoutes.h` (task 19); the maintained tables are `UESimulationLogRoutes-rationale.md` §3
and §5.*

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

### C20 — §9 named an empty hook as the drain's caller

§9 said the drain was reached through the pre-tick hook, then `InjectInputs_External`. The pre-tick
hook this class bound to the scene's pre-tick delegate had an empty body and called nothing.
`InjectInputs_External` is bound to the rewind callback's `InjectInputsExternal` delegate, which
Chaos broadcasts from the solver when it pushes the frame's steps. Task 51 deleted the empty hook
and its binding, and §9 then named the real caller. That text is `ChaosTickBackendUImpl-rationale.md` §4 since task 82.

*Found by og-simulationscheduler-withjolt task 51.*

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
*Names here are as they stood then: task 21a renamed the brawler component `USimmableUpdateComponent` to `UOGBrawlerSimulatableComponent`, in `OGBrawlerSimulatableComponent.h` and `OGBrawlerSimulatableComponent.cpp` (docs `OGBrawlerSimulatableComponent-rationale.md` and `OGBrawlerSimulatableComponent-guards.md`, ids unchanged).*

*Names here are as they stood then: task 30 renamed the actor `ASimulationManagerUImpl` to `AOGBrawlerSimulationManager`, its files to `OGBrawlerSimulationManager.h` and `OGBrawlerSimulationManager.cpp`, and this docs pair to `OGBrawlerSimulationManager-rationale.md` and `OGBrawlerSimulationManager-guards.md` (ids unchanged).*

*Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 24a): the composition members below (the adapters, the storage, the cvar
result and the static data, the peers, the systems executor, the integration layer, the manager and the backend) and the
construction-order and destruction-order assertions their "Where it went" lines name are the session's; the cvar member
became `BrawlerSessionGameUImpl::makeStaticData`. The entries are kept as they were (§2 has the current shape).*


*Task 11 removed every comment from `SimulationManagerUImpl.h` except the licence line. The banner
is §0. **Everything else is here, verbatim, in file order** — 99 comment runs, 387 lines — each
with where its content went. Where a numbered section above covers the same ground, that section
is the maintained text and the block below is the record of what the header said at `b9f6d81`.
The R0 annotations point at §11.*

*Nothing below is a guard. A guard's live text is its entry in `SimulationManagerUImpl-guards.md`.*

*Code moved (og-ue-simulationcomponentandmanagersplit task 22): the blocks above `pollInputHistory`,
`getRingoutVizState` (now `getVizSlice`), `pollInputHistoryLanes`, `getRelayReadReadout`, `gatherNearestCandidates`
and `m_inputHistory` describe code that is `BrawlerInputHistoryFeedUImpl`'s since task 22 (`getVizSlice` stays on this
class). Their pointers to §1's read crossing and to G-05 … G-13 now land in `InputHistoryDisplay-rationale.md` §8.6 and
in `InputHistoryVisualizationUImpl-guards.md` G-09 … G-17.*

*Code moved (og-ue-simulationcomponentandmanagersplit task 23): the blocks above `kPreDietCharacterCap`,
`m_authorityRegisteredIds`, `m_spawnSlots`, `seedRingoutSpawnPointsFromLevel` (now `BrawlerRosterUImpl::seedSpawnPointsFromLevel`)
and `m_ringoutSpawnPointsSeeded` describe code that is `BrawlerRosterUImpl`'s since task 23. Their pointers to `∴D-01`
and to G-14 now land in `BrawlerRosterUImpl-rationale.md` §4 and in `BrawlerRosterUImpl-guards.md` G-01.*

*Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 24b): the blocks above the clock
accessors, `onPostSimulationGameThread`, `getLastRelayedInput`, `getLocalInputCache`, `isLocallyControlledOnThisPeer`,
`getRingoutVizState` (then `getVizSlice`), `getRelayedReadObservations`, `getRelayedInputArrivals` and `getTimeConfigPtr`
describe members that are the session's since task 24b. Their reasoning is `UESimulationSession-rationale.md` §8, and
the pointer to G-04 now lands in `UESimulationSession-guards.md` G-02.*

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

*The next eight blocks: code moved to `UESimulationLogRoutes.h` (task 19); the declarations are exported there,
and their maintained text is `UESimulationLogRoutes-rationale.md` §3. `LogOGBrawler`'s, the ninth, stays in this
module.*

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

**Where it went:** Lines 272-273 → **`⛔G-02`**. The rest is rationale — §8, *The rewind-push probe*. Task 82 moved both with the Chaos host: they are `ChaosTickBackendUImpl-guards.md` G-01 and `ChaosTickBackendUImpl-rationale.md` §3.

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

**Where it went:** G-18, a tag on the alias chain. Task 81 moved the chain to `BrawlerCompositionUImpl.h` and retired G-18; its successor is `BrawlerCompositionUImpl-guards.md` G-01.

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

**Where it went:** Rationale — §10, *The capsule body id*. *(Code moved to `UESimulationSession.h`, task 28: the record is
the session's; the paragraph is `BrawlerSessionGameUImpl-rationale.md` §7.1.)*

---

## §13 `OGBrawlerSimulationManager.cpp` — what its comments carried that §1–§12 did not

<!-- ================= DECLARED LINT ESCAPES — §13 ==========================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: RewindToFrame -- Chaos engine method, outside every scan root -->

The implementation file was converted to guard tags (`OGBrawlerSimulationManager-guards.md`, ids
**G-50 onward**) and one derivation tag (**D-50**, §13.8; retired by task 26b, which moved its expression into the
session, where it is the session's D-01). Most of what its comments said was
already in §1–§11, which were written from those same comments (§12 carries the header's). This section holds the
rest: every comment whose content appeared nowhere above. Each is quoted as it shipped, so
this is a move and not a rewrite. Where verification found a quoted sentence false, the
correction sits beneath the quote and the quote is left intact. The corrections are collected
in §13.10.

### 13.1 The ring-out respawn points come from the level — moved to `BrawlerRosterUImpl-rationale.md` §1 (task 23)

*Code moved to `BrawlerRosterUImpl.cpp` (og-ue-simulationcomponentandmanagersplit task 23): the seed is
`BrawlerRosterUImpl::seedSpawnPointsFromLevel` and the seeded check is `BrawlerRosterUImpl::checkSeeded`, both called
from `BeginPlay` at the positions this section described (since task 27b from the brawler's hooks `beforeWorldBuild` and
`afterSessionConfigured`, which the session's `beginPlay` calls there). The quoted comments, corrections C13-4, C13-8 and C13-12, and
task 23's R0 notes are `BrawlerRosterUImpl-rationale.md` §1 (ruling Q-B: §13.1 moves with task 23).*

### 13.2 The gravity cross-check — moved to `BrawlerSessionGameUImpl-rationale.md` §6.1 (task 27b)

*Code moved to `OGBrawlerSimulationManager.cpp`'s hook `BrawlerSessionGameUImpl::beforeWorldBuild` (og-ue-simulationcomponentandmanagersplit
task 27b), the first statements of the brawler's start-up hook, unchanged. The quoted comment and its verification are
`BrawlerSessionGameUImpl-rationale.md` §6.1 (ruling Q-B: §13.2 moves to the hooks' doc).*

### 13.3 The collision-category table

*Now one table per configuration, in the brawler's engine bindings: `BrawlerChaosBindingUImpl::categoryChannels`, which the Chaos host's `buildWorld` hands the query adapter, and `BrawlerJoltBindingUImpl::categoryChannels`, which the Jolt host's `buildWorld` hands the static importer (og-tickdriver task 15a). Before that it was one file-local function, emplaceBrawlerQueryAdapter, called from both role branches; og-simulationscheduler-withjolt task 82 moved it into `ChaosTickBackendUImpl.cpp`, called once from `buildWorld`, and task 83 gave the Jolt configuration its own table, brawlerCategoryChannels in the Jolt host's `.cpp` (TickDriverBackendUImpl.cpp, a historical name: og-tickdriver task 15e deleted it).*

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

### 13.4 Registration's first-call pass — moved to `BrawlerSessionGameUImpl-rationale.md` §7.3 and `UESimulationSession-rationale.md` §12 (task 28)

*Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 28): the cast, the body creation,
the DO_CHECK walk and the resolvability gate are the session's `tryRegister`; the brawler's root body is the hook
`BrawlerSessionGameUImpl::rootBodyId`. The generic part of this subsection (the cast's role, generic creation and
hand-written collision, the gate's order-first read) is `UESimulationSession-rationale.md` §12.1, game-neutral; the quoted
comments, verbatim, with this subsection's note on the tables, are `BrawlerSessionGameUImpl-rationale.md` §7.3 (ruling
Q-B: §13.4's generic part moves with task 28). The brawler part moved to `BrawlerRosterUImpl-rationale.md` §2 in task 23.*

### 13.5 The ring-out score push — moved to `BrawlerRosterUImpl-rationale.md` §3 (task 23)

*Code moved to `BrawlerRosterUImpl.cpp` (og-ue-simulationcomponentandmanagersplit task 23): the walk is
`BrawlerRosterUImpl::pushRingoutScores`, called from `OnPostPhysicsStep` at the position this section described (since
task 27a from the brawler's hook `BrawlerSessionGameUImpl::afterVisualization`, which the session's `onPostPhysicsStep`
calls there). The
quoted comments, corrections C13-10 and C13-11, and task 23's R0 notes are `BrawlerRosterUImpl-rationale.md` §3
(ruling Q-B: §13.5 moves with task 23). The cross-thread argument stays in §1, the second read crossing.*

### 13.6 The router — moved to `UESimulationLogRoutes-rationale.md` §10 (task 19)

*Code moved to `UESimulationLogRoutes.h` (og-ue-simulationcomponentandmanagersplit task 19). The per-line notes that
stood beside the router's old branches, and corrections C13-2 and C13-3 restated against the tree of task 19, are
`UESimulationLogRoutes-rationale.md` §10 (ruling Q-B: §13.6 moves with task 19).*

### 13.7 The rewind-push probe — the source's own wording, beyond §8

*`ChaosTickBackendUImpl-rationale.md` §3 carries this probe's design (§8's text, moved there by task 82). These are the
passages from its source that it did not already state. Chaos configuration only: the probe and its helpers are
the Chaos host's (`ChaosTickBackendUImpl.cpp` since task 82) and retire with it in task 21 (§18).*

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

⚠ **Outdated by task 10's fix (2026-09-25), not wrong when written.** The push is now a direct write
onto the particle (`ChaosTickBackendUImpl-rationale.md` §5), so the after-push read differs from `beforePush` on every non-inert push, and
`moved=1` is the expected reading. The verdict still comes from read 3, one hook later.

> BodyId lookup goes through the m_physics composite bindings (local-only).
>
> [task 10] `D::name` is the declaration's own body name, the one the factory
> creates the body under, so `body=WeaponAxis` needs no table here to stay true.
> `BodyStateT` is the declaration's ACTUAL wire shape, before the widening
> conversion into `pushBodyState`'s `const PhysicsBodyState&` erases it.

Verified: `createPhysicalObject(D::descriptor(), D::name)` is the factory call of the first-call bind. *R0, task 28:* it is
in the self-scheduled host's `ChaosTickBackendUImpl::createBodies_GameThread` (since og-simulationscheduler-withjolt task 82),
which `tryRegister`'s first call reaches through the backend; it was never `tryRegister`'s own statement after that task.
There is no member named `m_physics`; the lookup goes through `getPhysicsComposite()`.

> ⛔ At PostPushData: direct SetX/SetV/SetW is a NO-OP on ResimAsFollower bodies.

⚠ **Correction C13-15.** This sentence justified pushing the rewind state through
`FRewindData::SetTargetStateAtFrame` rather than writing the particle directly. It does not apply to this
tree's bodies. On UE 5.6 `RewindToFrame` re-stamps every dynamic body's resim type on every
rewind, so none is `ResimAsFollower` during a replay. The push it justifies was also measured
never read (task 10's capture: 185 of 185 rewinds `MISMATCH`, `nonInertMatched=0`). ~~The call
stays, because task 10's fix will decide what replaces it.~~ Task 10's fix (2026-09-25) removed the
call and writes the particle directly (`ChaosTickBackendUImpl-rationale.md` §5, `ChaosTickBackendUImpl-guards.md` G-08). The sentence has no guard, because it no
longer describes a reason.

At `b9f6d81` `Config/DefaultEngine.ini` sets `LogOGResimProbe=Verbose`, so the "shipped
`LogOGResimProbe=Warning`" in the probe banner's volume paragraph (quoted in §8) does not
describe the committed ini (C13-1).

### 13.8 The capacity pin's word rounding — moved to `UESimulationSession-rationale.md` §9.9 (task 26b)

Code moved to `UESimulationSession.h` (og-ue-simulationcomponentandmanagersplit task 26b): the `[PacketBudget]` line, with
`onFrameStepsDue_GameThread`. Its derivation moved with it, verified against the engine, to
`UESimulationSession-rationale.md` §9.9, *The capacity pin's word rounding*, with the source's banner over the block.

#### ∴D-50 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): the `[PacketBudget]` word rounding moved to `UESimulationSession`

**Was:** the derivation tag on the first argument of the `[PacketBudget]` line in `onFrameStepsDue_GameThread`
(`OGBrawlerSimulationManager.cpp`), `(usableBits / 32) * 4`, which pointed at this subsection: the usable bytes are rounded
down to whole 32-bit words before four bytes per word are counted, because the channel writes a bunch in words, so the
natural-looking `usableBits / 8` would print a number a bunch cannot carry; and the brawler's literal compared with it
by hand is an upper bound.

**Why it retired.** Its subject moved. Task 26b moved `onFrameStepsDue_GameThread`, with PROBE A, the coordinator test,
the `[PacketBudget]` line and PROBE 6, into OGSimulationUnreal's `UESimulationSession`, and the subsection moved with it,
with an R0 line (the engine's `UDataStreamChannel::WriteData` rounds exactly so). **Successor:**
`UESimulationSession-rationale.md` §9.9, *The capacity pin's word rounding*, the heading that carries the session's D-01
tag. The brawler's literal stays in §8 above.

### 13.9 What the compiler now enforces instead of a comment

Every row was compiled. The rows marked **seen to fail** were then poisoned: the forbidden edit
was made in a scratch copy of the file, the build was run, and the failure below was
observed. The file was then restored and its hash checked.

| was | now | seen to fail |
|---|---|---|
| "ORDER IS LOAD-BEARING: this MUST precede the `[Resim.` catch-all" and the defensive `[ResimProbe` placement | `static_assert(ogLogRoutesAreAllReachable())` over `kOGLogRoutes`: no prefix may begin with an earlier one, compared case-insensitively as `FString::StartsWith` does | `[Resim.` moved above `[Resim.Input]` ⇒ `C2338`; `[Resim.` widened to `[Resim` ⇒ `C2338`; a lower-case duplicate `[resim.input]` appended ⇒ `C2338` |
| the two collision tables and their "KEEP IN SYNC" | one table, emplaceBrawlerQueryAdapter, called from both branches (since og-tickdriver task 15a the Chaos binding's `BrawlerChaosBindingUImpl::categoryChannels`) | nothing left to poison: the two copies no longer exist |
| "NOT `HasAuthority()`" at four sites | `#define HasAuthority HasAuthority_is_constant_true_on_AOGBrawlerSimulationManager_use_worldIsAuthority_or_runsPrediction` after the includes, `#undef` at the end of the file (it must not leak into the next file of a unity blob) | `HasAuthority()` typed in `BeginPlay` ⇒ `C3861` naming that identifier |
| PROBE A's "COMPARABLE WITH ResimGateProbe … same 120 samples" | `static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples)` (in the session's `onFrameStepsDue_GameThread` since task 26b) | a local shadow of 121 ⇒ `C2338` |
| "NOT ROLE-GATED" at the spawn-point seed | `checkf(m_ringoutSpawnPointsSeeded)` after both role branches (in `BrawlerRosterUImpl::checkSeeded` since task 23) | compiled only. A runtime check in the UE layer cannot be exercised from a low-level test |
| the stamp-after-the-fold order and "anyone simplifying `isRoot` away" | `checkf(movementOwnBodyId == parentBodyId)` above `setCharacterBindings` (in `BrawlerRosterUImpl::onBodiesCreated` since task 23) | compiled only, same reason |

**Already enforced, found rather than added.** No tag, no comment:

| the comment said | what already enforces it | seen to fail |
|---|---|---|
| the request is counted after the tick conversion | data dependency: `noteResimRequest` takes the converted tick | hoisted above it ⇒ `C2065` |
| a stale route is skipped, never erased, in the score push | the route table arrives by `const&` | `routes.erase(id)` ⇒ `C2663` |
| no depth is passed to the relay stage | `stageRelayedInput` has no depth parameter; `RelayRedundancyDepthTest.cpp` | not re-poisoned here |
| a client must not push scores | `SetAuthoritativeRingoutScore`'s `checkf` on `HasAuthority()` (the pawn replicates) | not re-poisoned here |
| `isRoot` must not be removed | `static_assert` on `kCharacterCapsuleBody` in `BrawlerMovementSimulation.h` | not re-poisoned here |
| spawn points must not be ordered by `FName` | `LevelSpawnPoint::name` is a `std::string` in an engine-free header | not poisoned here |
| `kNoFreeSlot`'s value is `kMaxSpawnPoints` | `static_assert` at its declaration (the clamp at the call site is `BrawlerRosterUImpl-guards.md` G-03 since task 23, G-67 here before) | not re-poisoned here |

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

*C13-2 and C13-3 are restated, re-verified at task 19, in `UESimulationLogRoutes-rationale.md` §10, with the code
they describe (task 19).*

Two more claims were **true as written but described something that cannot happen**. They are
annotated at their quotes in §13.4 (re-seeding a released slot) and §13.7 (the `ResimAsFollower`
premise, which is also C13-15).

✅ **Discharged by netcode task 25:** the comment was removed in the conversion of `OGBrawlerUECharacter.cpp`,
the correction lives in `OGBrawlerUECharacter-rationale.md` §7 (C-6), and the `checkf` message
now names the `!runsPrediction()` gate. The original routing note follows.

⚠ **Routed, not fixed: a claim in another file about this one.** `OGBrawlerUECharacter.cpp`,
in `SetAuthoritativeRingoutScore`, says this file "warns about it twice and the push uses the
world-level `GetNetMode()` test instead". Neither half is true. The push is gated on
`!runsPrediction()`. The source had **four** `HasAuthority()` warnings, and it now has one
compile-time poison that replaces all four (§13.9).

### 13.11 Small notes carried verbatim

> THE SESSION ROTATION WIDTH. ⛔ The intake clamp stops the proof line below from lying. §3

> Clear the singleton slot before teardown so a later PIE session cannot hit the guard.

> A downward or delay-neutral transition, which drift reaches by advancing, or a first one.

*The routing registration's note ("A plain overwrite, ONCE at register") moved with the registration to
`UESimulationSession-rationale.md` §9.7 (task 26a).*

> Line 1 - THE RATIO, client role. p99 and max are why this exists: a mean hides a hitch.

*The display's unregister note ("Same unregister contract for the display") moved with the unregistration's display forget
to `BrawlerSessionGameUImpl-rationale.md` §7.4 (task 28).*

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

## §14 The first replayed step — the restored body state is written, not targeted

Moved to `ChaosTickBackendUImpl-rationale.md` §5 (og-simulationscheduler-withjolt task 82), with the code it describes.

---

## §15 The simulation character id — assigned here, replicated on the pawn (task 25)

**What changed.** Until task 25 every id in this file was the registering component's
`GetUniqueID()`, a per-process number: the same character was 42357 on a client and 39920 on the
server. Since task 25 the id is a `SimCharacterId` (og-brawler, `SimCharacterId.h`). The
authority's manager assigns it from its roster's `m_simCharacterIds` (`BrawlerRosterUImpl`, since
og-ue-simulationcomponentandmanagersplit task 23), the pawn replicates it, and every
peer registers under the same number. `tryRegister`, `noteDelayedInputComponent` and
`unregisterFromNewFramework` take a `SimCharacterId`, so an engine object id passed to them is a
compile error. *R0, task 29:* the component's registration no longer goes through the first two: the binding's
skeleton passes the session the storage key its game's hook answered (`ready(toStorageKey(simId))`, the brawler's
`resolveRegistrationKey`), so on that path the typed id ends inside the hook; `unregisterFromNewFramework` still takes
the `SimCharacterId` from the component's `EndPlay`. Inside, `toStorageKey` turns it into the `unsigned int` key every container already
used. No container changed type. There is no wire change, because the id rides the pawn's
replication, not the correction composite.

**`allocateSimCharacterId`** (`BrawlerRosterUImpl::allocateSimCharacterId` since task 23; the component calls it
through `roster()` and passes the manager's `GetNetMode()`) is the only call that issues one. It `checkf`s that
world-level net mode, so a client world's manager can never assign. It returns `SimCharacterId::None` once the
allocator is exhausted, after logging a `LogOGMgmt` Error and failing an `OG_CHECK`. That check is
fatal in Development and compiled out in Shipping, where the character simply does not register
(guard G-02 of `BrawlerRosterUImpl-guards.md`, this file's G-78 until task 23; the permanence rule itself is G-01 of
`SimCharacterId-guards.md`).

**Why the counter is a member and not a process static** (user ruling, 2026-09-26). The ids must
be unique for as long as the storage and wire that use them live, and that is exactly this
manager's lifetime. Only one authority manager exists at a time (`instanceFor(true)`). The PIE
server is the editor process, so a process static would carry the count across every PIE session
and refuse registrations after roughly 60-120 runs. A new manager (a new PIE run or a map load)
restarts at 1. The tree has no seamless travel, so no pawn outlives the manager that numbered it.

**PIE's two managers in one process.** The authority manager and a client world's manager each
have their own storage. Only the authority's ever calls `allocateSimCharacterId`: the component
allocates only on its authority path, and the `checkf` enforces it. The client instance's
allocator is never used. The same id appearing in both storages is correct, since that is the
peer-stable property.

**Couch co-op.** Several pawns share one connection. Each pawn is its own actor, so each gets its
own id at its own registration. The property is a plain `DOREPLIFETIME` (not `COND_OwnerOnly`),
so every client sees every pawn's id, the siblings on its own connection included. The tier and
input paths keyed by ROOT connection are unaffected.

**A rejoin** is a new pawn, so it gets a new component with id 0, a new allocation and a new id.
The departed pawn's id was unregistered at its EndPlay and is never handed out again.

## §16 The latency-budget probe — the brawler's wiring (og-simulationscheduler-withjolt task 1)

The probe's host is OGSimulationUnreal's template `UELatencyBudgetDiagnostics` (`UELatencyBudgetDiagnostics.h`, since
og-ue-simulationcomponentandmanagersplit task 19a). Its rationale, `UELatencyBudgetDiagnostics-rationale.md` in
OGSimulationUnreal's docs, is the class, its gate, its const access, the fence test, the template's own stamp points and
threads, the `H3` hook and the `[LatencyBudget]` lines. This section is the brawler's side: this class's member and call
sites, the brawler's other stamp sites, their threads and errors, and the Chaos host's `[ChaosDilation]` line. The
probe is a measurement of the Chaos build, per hop, that the scheduler migration was compared against. **It changes no
gameplay behaviour, timing or netcode: every addition is a timestamp and a counter.**

### Ownership and call sites

This class holds one member, `m_latencyBudgetDiagnostics`, of type
`UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>` (the traits are `BrawlerSessionTraitsUImpl-rationale.md`),
initialized with `{ std::as_const(*this) }` (the template's rationale §4 says why: with `{ *this }`, MSVC compiled this
class's later initializers `m_reconciliation{ m_storage }`, `m_inputResolution`, `m_netSync` and `m_backend` as though
`this` were const, C2665), and the `get`/`edit` pair
`getLatencyBudgetDiagnostics()` / `editLatencyBudgetDiagnostics()`. It befriended the template
(`template <typename TraitsT> friend class UELatencyBudgetDiagnostics;`), which reads `m_manager`, `m_storage`,
`m_inputResolution` and `m_delayedInputComponentsById` through a const reference. Since og-ue-simulationcomponentandmanagersplit
task 26a all four are the session's (the map was this class's until then), so none of the template's reads needed this
class's friend line any more, and task 27b deleted it. Its call sites, each at the point the
probe's code had before og-tickdriver task 18 moved it out of this class:

* `onTimingInfoReceived`: `noteWireSample`, the first statement (the client's `WIRE` sample), the session's own call
  since task 25 (`UESimulationSession-rationale.md` §9);
* the start-up: `begin`, which creates the probe with the role's hop mask (`kServerHops` on the authority world,
  `kClientHops` on a client world) and its two mailboxes, and binds the post-tick flush. That is before the backend starts
  stepping (`startStepping`, at the end of the start-up), and nothing resets them before the actor is destroyed, so a step
  can never reach a null or freed mailbox (this class's `BeginPlay` until task 27b, the session's `beginPlay` since, its
  own statement, `UESimulationSession-rationale.md` §11);
* the teardown: `end`, inside the world test, before `onEndPlayWorld` (in this class's `EndPlay` until task 27a, in the
  session's `endPlay` since, `UESimulationSession-rationale.md` §10);
* `tryRegister`: `wrapInputProvider`, on the client branch (this class's until task 28, the session's own statement since,
  `UESimulationSession-rationale.md` §12.1);
* `unregisterFromNewFramework`: `forgetCharacter` (likewise, §12.3);
* the last statement of the post-physics pass: `tick_GameThread`, with the backend's `editDiagnostics()` view (this
  class's `OnPostPhysicsStep` until task 27a, the session's `onPostPhysicsStep` since, after the brawler's hook).

No production local, decision or replicated value moved into the template. The two neighbours that feed it have their
own fenced blocks: the Chaos host's `[ChaosDilation]` window (`ChaosTickBackendUImpl-guards.md` G-11) and the input
collection component's input-sample time (an old-style fence comment; that file is unconverted).

PIE in one process has two manager instances and two probes; the `role=` field tells their lines apart. The component's
game-thread sites reach the role's instance through `UELatencyBudgetDiagnostics::withInstance`
(`UESimulatableOwnerBinding-rationale.md` §4 since task 21), which finds this member through `instanceFor`.

### Threads — the brawler's reads add no crossing

The template's own reads are same-thread with their writers (its rationale §6). The brawler's: the correction buffer's
tick (written by `sendCorrectionAll` inside `onPostSimulationGameThread`), `m_delayedInputComponentsById`, the scene's
network delta-time scale (written by the engine's time-dilation client RPC) and the local player controller's physics
tick offset (written by the engine's timestamp-setup client RPC) are all read on the game thread, where they are written.
The one value that crosses from the game thread to the step thread, the input-sample time, is a `std::atomic<double>` on
the input collection component (`BrawlerSessionGameUImpl-rationale.md` §2). **This class's CROSSING count stays three**
(§1), and this paragraph is the argument for it, made rather than assumed.

On a dedicated server the step thread *is* the game thread (Chaos steps inline; the Jolt configuration steps the
authority inline too).

### Why the capture stamp is a wrapper in `tryRegister`, not a line in the provider lambda

`UOGBrawlerSimulatableComponent`'s provider lambda may not capture the manager (its guard G-05). The capture stamp therefore
lives in the wrapper `wrapInputProvider` returns, which `tryRegister` (the session's since task 28) applies to a client's
provider before handing it to the core. The wrapper calls the component's lambda, then posts `InputSampled` and `Captured`
for `step.getTick()`. It captures `this`, the diagnostics object, a member of the session (of this class until task 24a),
which owns the core container the callable is stored in,
so it adds no lifetime edge to another actor, and the input sample source `BrawlerSessionGameUImpl::inputSampleSource`
returned at registration: the same input-collection pointer the inner lambda already holds. **An empty provider is
returned unchanged**: provider presence is the core's local-versus-remote identity test, and a `checkf` states that the
wrapper never changes it (the template's rationale §3).

### What the brawler has at 0 (Shipping, and the fence build)

The template's members are no-ops (its rationale §3). On the brawler's side: the Chaos host's `[ChaosDilation]` window
members and `LogOGChaosDilation` do not exist and its view's four members are inline no-ops; the input collection
component's input-sample time and its view do not exist, so `BrawlerSessionGameUImpl::inputSampledSeconds` is not
declared either. **What still runs at 0, deliberately:** the Chaos async callback still reads `FPlatformTime::Seconds`
before `onGameSimulation`, because the step-start time must be taken before the step and it is a production local beside
production statements; and the authority's relay-host flush callback is still bound (the sibling's `attachInputRelayHost`
binds `makeRingFlushedCallback`'s result), with a body that is a single `withInstance` call, a no-op at 0.

**The fence test** (the template's rationale §5) forces the macro in this module's `OGBrawlerUnreal.Build.cs` through a
`PublicDefinitions` entry. The harness's two Chaos client checks for the latency lines (`[ChaosDilation]` present with
`samples>0`, `[LatencyBudget.Window]` present) fail there by design and are judged as expected-absent (og-tickdriver
task 18's lead ruling).

### The hop table: stamp sites and known measurement error

Times are `FPlatformTime::Seconds` (one monotonic clock per process). "GT" is the game thread and "PT" the step's
thread (a task-graph worker on a client, the game thread on a dedicated server).

| hop | start: file · function (thread) | end: file · function (thread) | known error |
|---|---|---|---|
| `H1` | `OGBrawlerInputCollectionComponent.cpp` · `updateGameThreadCache`, first statement after the owner check (GT) | `UELatencyBudgetDiagnostics.h` · the wrapper from `wrapInputProvider`, after the provider returns (PT) | Starts at the frame's input refresh, not at the device event (`BrawlerSessionGameUImpl-rationale.md` §2): the true input age is **under**-stated by up to one game frame. The sample time is a relaxed atomic, unordered with the caches the capture reads, so a capture racing a refresh can pair a new time with the previous frame's values: an error of at most one frame, rare. |
| `H2` | the same wrapper, `Captured` (PT) | `UESimulatableOwnerBinding.h` · `sendLocalInputToAuthority` (the component's binding base since task 21), after the bundle's send (`sendInputBundleToAuthority`, the RPC since task 20a), once per capture tick in the bundle, first send wins (GT) | Microseconds. Redundant re-sends of the same capture are duplicates and do not move the hop. |
| `H3` (client) | `sendLocalInputToAuthority` (GT) | `UELatencyBudgetDiagnostics.h` · `onPostTickFlush`, bound to `UWorld::OnPostTickFlush` (GT) | See the template's *`H3` hook*. |
| `H3` (server) | `UESimulatableOwnerBinding.h` · the relay host's flush callback from `makeRingFlushedCallback`, which the sibling's `attachInputRelayHost` binds (task 20b), fired from `ASimulationInputRelay::PreReplication` after a flush published entries (GT, inside the net driver's flush) | `onPostTickFlush` (GT) | Measures the rest of that flush after this relay's pre-update. The same send caveats as the client row. |
| `H3c` | `UELatencyBudgetDiagnostics.h` · `tick_GameThread`, when a character's correction-buffer tick changed since the last frame (GT, end of the session's `onPostPhysicsStep`, this class's `OnPostPhysicsStep` until task 27a) | `onPostTickFlush` (GT) | A property leaves only when the replication system polls and sends it; a correction written but not sent at this flush still reads as left. A **lower** bound. The start is late by the frame's visualization work (the backend's `updateVisualization_GameThread`). |
| `H4` | `UESimulatableOwnerBinding.h` · `receiveRemoteMoveBundle`, which the sibling's RPC calls after its version fence (task 20a), once per capture tick in the bundle, first arrival wins (GT, inside the net driver's receive dispatch) | `UELatencyBudgetDiagnostics.h` · `stampAfterStep_Internal`, for the capture tick `getLastUsedCaptureTick` reports after the step, stamped with that step's start time (PT) | The datagram reached the socket before the frame's receive dispatch: up to one server frame **under**-stated. An underrun step reports the sentinel and stamps nothing. |
| `H4r` | as `H4` | the relay flush callback (as `H3` server) | The hand-off is the flush into the replicated ring, not the staging write at receipt: staging is synchronous with receipt and would read zero. |
| `H5` | `stampStepEnd_Internal`, at each step's end (below), per character, for `currentIntegratedTick` (PT; inline on a dedicated server) | as `H3c` start | `noEnd` counts the ticks a character's state was not written: the correction rotation (`CorrectionRotationK`) skips a character on purpose, so on a rotation of 2 about half the ticks have no end. |
| `H6` | `UESimulatableOwnerBinding.h` · `noteRelayedRingArrived`, which the sibling's `onRelayedInputRingArrived` calls after its owner-skip check (task 20b), once per capture tick in the ring, first arrival wins (GT, from the ring's OnRep) | `stampAfterStep_Internal`: the relayed-read observation for the step's tick names the applied capture tick, stamped with the step's start time (PT) | The bytes arrived up to one client frame before the OnRep. Only reads that applied a relayed capture stamp an end; a miss stamps nothing and the capture finalizes as `noEnd`. |
| `H6c` | `UESimulatableOwnerBinding.h` · `onCorrectionStateAccepted`, called from the sibling's `OnRep_CorrectionState` (task 20a), for the peeked tick (GT) | `stampAfterStep_Internal`: a pending stamp at the start of the first client step after receipt (PT) | Measures when the correction is available to the next step, not when a resimulation applies it. |
| `H7` | `stampStepEnd_Internal` (PT) | `tick_GameThread`, after the visualization pass: the newest pending tick per character only (GT) | Not the photon. The engine interpolates the render poses behind the simulation on the game thread, and the render thread and GPU add one or two frames more. A step that ends between the mailbox drain and this stamp is stamped one frame late. On a dedicated server it measures the per-frame visualization pass, which draws nothing. |
| `WIRE` | client: `onTimingInfoReceived`, the raw round trip the timing relay hands `NetworkTimeEstimator::updateRTT`, halved (GT) | — | Half of a round trip assumes symmetric paths. Server: `receiveRemoteMoveBundle`, `readRoundTripMs` of the root connection, halved, once per bundle (GT). The two roles' values come from different estimators. |

**H1 and H2 are measured for a remote client's own characters only.** A listen host's local players have no client
provider, so the host role has no capture path to stamp.

### The step-side stamp sites per configuration (og-simulationscheduler-withjolt task 54)

In the Jolt configuration "PT" is the step's thread (the tick driver's task on a client, the game thread inline on the
authority), every game-thread site is unchanged, and the step-side sites are the game binding's step hooks
(`BrawlerTickDriverGameUImpl-rationale.md`):

| stamp | Chaos configuration (`ChaosTickBackendUImpl.cpp`, through the host's `editDiagnostics()`) | Jolt configuration (`BrawlerTickDriverGameUImpl.h`) |
|---|---|---|
| step-start time for `H4`, `H6`, `H6c` and the clock counts | `OnPreSimulate_Internal`, immediately before `onGameSimulation` | og-simulation's `TickDriverStepHooks` `beforeSimulate`, after any replay and immediately before the normal step (`TickDriverStepHooks-guards.md` G-03); on a client early only by the Jolt scratch save |
| `stampAfterStep_Internal(start)` | `OnPreSimulate_Internal`, after `onGameSimulation`, when not resimulating | `onStepStarted`, from `beforePhysics`, after `onGameSimulation`, before the physics step |
| `stampStepEnd_Internal` (`H5`, `H7` starts) | `OnPostSolve_Internal`, when not resimulating | `onStepEnded`, from `afterTick`, after the post-step pass and the ring save: microseconds later |

Both configurations take the start time at the same instant, so gate 22 compared them like for like. `[ChaosDilation]` is
the Chaos configuration's only.

### The `[ChaosDilation]` line (client only)

Per window, from `tick_GameThread`, in the Chaos configuration only. The Chaos host samples and logs it, through its
`editDiagnostics()` view's `sampleLatencyWindow_GameThread` and `logLatencyWindow_GameThread`
(`ChaosTickBackendUImpl-rationale.md` §2), under its own category `LogOGChaosDilation`, which `ChaosTickBackendUImpl.h`
declares and its `.cpp` defines (both gated with the probe; since task 19a, before that beside the probe's category):

* `min` / `mean` / `max` / `samples`: the physics scene's network delta-time scale, sampled once per game frame from
  the world's physics scene (`GetPhysicsScene`, an `FChaosScene`; `FChaosScene::GetNetworkDeltaTimeScale`, beside the
  setter the engine's time-dilation client RPC calls). It is the engine's physics time dilation.
* `resets` / `offset`: how many times the first local player controller's network physics tick offset changed in the
  window, and its current value. The engine's timestamp-setup client RPC writes that offset. ⚠ A reset to the **same**
  value is invisible to this poll, and only the first local player controller is read.
* `skips` / `stalls` / `hardResyncs`: the window's increase in `ClientPredictionClock`'s Skip, Stall and HardResync
  counts, read on the step thread and carried by the template's clock mailbox (`clockCountsInWindow`). A client that
  joins mid-session shows one hard resync in its first window.

```
[ChaosDilation] role=Client min=0.9762 mean=0.9998 max=1.0237 samples=1000 resets=0 offset=537 skips=0 stalls=0 hardResyncs=0
```

All three latency lines are `Warning`. `Config/DefaultEngine.ini` pins `LogOGLatencyBudget=Warning`; `LogOGChaosDilation`
has no ini line and runs at its declared default (`Log`), which prints its `Warning` line.

### Verified, and what was not

A headless run (one `-server` process and two `-game -nullrhi` clients on the ThirdPerson map, 2026-10-06) printed every
enabled hop on both roles, with `stale=0` and `mailboxDropped=0`. Representative p50s: client `H6` 90 ms and server `H4`
149 ms (the relay-delay floor and the input delay, as expected), `H5`'s `noEnd` about half its ticks (the rotation), and
the dilation scale moving between 0.976 and 1.024. A headless client is smoother than a rendering one, so these numbers
are wiring evidence, not a baseline.

### History

* **og-simulationscheduler-withjolt task 1** wrote the probe's host side in this class and this section.
* **og-tickdriver task 18** moved the probe, its mailboxes, its stamp functions and its window logging out of this class
  into a game-module class, LatencyBudgetDiagnosticsUImpl (historical name), compiled out of Shipping, and moved this
  section into that class's rationale (§6 there). It applied three user rulings: diagnostics are grouped, fenced and
  fence-tested (2026-08-29); the grouped class ("Group it", 2026-10-19); and "gate it behind `#if !UE_BUILD_SHIPPING`"
  (2026-10-19), named once as the macro so the fence build can force it. Its POISON (a non-const owner reference) was seen
  failing, and the provider-presence `checkf`, which before it covered the wrapping path only, moved above the gate. The
  names it changed:

  | before task 18 (historical names) | since task 19a |
  |---|---|
  | stampLatency | `stamp` |
  | noteLatencyWireSample | `noteWireSample` |
  | postLatencyEvent_Internal | `post_Internal` |
  | stampLatencyAfterStep_Internal | `stampAfterStep_Internal` |
  | stampLatencyStepEnd_Internal | `stampStepEnd_Internal` |
  | createLatencyBudget, and the post-tick-flush registration beside it in `BeginPlay` | `begin` |
  | the post-tick-flush removal in `EndPlay` | `end` |
  | the two latency statements of `unregisterFromNewFramework` | `forgetCharacter` |
  | wrapInputProviderWithLatencyStamp | `wrapInputProvider` |
  | onLatencyPostTickFlush | `onPostTickFlush` |
  | tickLatencyBudget_GameThread | `tick_GameThread` |
  | LatencyMailbox, LatencyClockMailbox, LatencyClockCounts | `Mailbox`, `ClockMailbox`, `ClockCounts` |
  | m_latencyProbe, m_latencyMailbox, m_latencyClockMailbox | `m_probe`, `m_mailbox`, `m_clockMailbox` |
  | m_latencyClockPosted_Physics, m_latencyClockLatest, m_latencyClockAtWindowStart | `m_clockPosted_Physics`, `m_clockLatest`, `m_clockAtWindowStart` |
  | m_latencyMailboxDropped, m_latencyLastHandedCorrectionTick, m_latencyPostTickFlushHandle | `m_mailboxDropped`, `m_lastHandedCorrectionTick`, `m_postTickFlushHandle` |
  | the backend hosts' sampleLatencyWindow_GameThread and logLatencyWindow_GameThread | the same names on each host's `editDiagnostics()` view |
  | the Chaos host's stampLatencyAfterStep_Internal and stampLatencyStepEnd_Internal forwarders | the same names on `ChaosTickBackendUImpl::Diagnostics` |
  | the input collection component's getInputSampledSeconds | `inputSampledSeconds`, on the view `UOGBrawlerInputCollectionComponent::getDiagnostics` returns |

  Two mechanical differences, both equivalent: the post-tick-flush binding was an `AddUObject` binding of this class's
  own handler and became an `AddWeakLambda` binding on this class that calls `onPostTickFlush` (the same multicast
  delegate, bound at the same point of `BeginPlay`, weakly bound to the same object); the `[ChaosDilation]` clock fields
  were the Chaos host's direct subtraction of this class's two members and became `clockCountsInWindow()` and
  `restartClockWindow()`.
* **og-ue-simulationcomponentandmanagersplit task 19a**: code moved to `UELatencyBudgetDiagnostics.h` (OGSimulationUnreal,
  MPL-2.0), a template over `BrawlerSessionTraitsUImpl`, with the input-sample read through the two hooks of
  `BrawlerSessionGameUImpl`. Its game-neutral narrative moved to the template's rationale; this brawler part came back to
  this section, and the old class's docs pair was retired (its one guard id's successor is the template's G-01).

---

## §17 The Jolt host — overview (og-simulationscheduler-withjolt tasks 53–56 and 18)

Moved to `TickDriverBackendUImpl-rationale.md` §2 (og-simulationscheduler-withjolt task 83), with the code it describes.

## §18 The backend switch — `OG_PHYSICS_BACKEND_CHAOS` (og-simulationscheduler-withjolt task 51)

The initiative replaces Chaos with Jolt as the gameplay physics engine. Until gate 22 compares the
two, both hosts stay selectable at compile time, and task 21 removes the switch after the gate (the
user's ruling D21, 2026-10-07: compile time rather than run time, because Chaos is retired soon).

### The macro and the traits header

* **One macro**, `OG_PHYSICS_BACKEND_CHAOS`: `1` builds the Chaos host, `0` the Jolt host. It comes
  from one constant, `PhysicsBackendChaos` in `OGBrawlerUnreal.Build.cs`, added as a
  **`PublicDefinitions`** entry, so every module that includes this header sees the same value. A
  private definition would let a future includer compile the other arm, an ODR violation. It is not
  an environment variable, because nothing established that UBT re-reads a changed environment
  variable before it reuses its makefile; an edited `OGBrawlerUnreal.Build.cs` is always re-read. Task 51 saw both: a
  flipped constant, and a deleted definition, each recompiled the module and stopped at the traits
  header's matching error. A switch changes a definition every file of the module compiles with, so
  make it with the editor closed rather than through Live Coding.
* **The constant is `0`, the Jolt configuration, since task 18.** Task 51 built the switch with the
  constant at `1` and a Jolt branch that was only an `#error`; tasks 53 to 56 built the Jolt host in
  that branch (below) with the constant still `1`, and task 18 flipped it. Set it to `1` to build the
  Chaos configuration, for the comparison playtests until task 21.
* **`PhysicsBackendUImpl.h`** stops the build when the macro is undefined. Since og-tickdriver task 15a it is
  seam A of the backend (`PhysicsBackendUImpl-rationale.md`): the concepts `PhysicsEngineTraits` and
  `BrawlerEngineBinding`, and one selector that names the configuration's engine traits and the brawler's binding
  for that engine. Since og-tickdriver task 15e `PhysicsEngineTraits` is not defined there: the header includes it,
  OGSimulationUnreal's in task 15e and og-simulation's `PhysicsEngineTraits.h` since task 15f, and asserts it on the
  selected traits:

| alias | Chaos configuration | Jolt configuration |
|---|---|---|
| `EngineTraits` | `ChaosEngineTraitsUImpl` | `JoltEngineTraitsUImpl` |
| `EngineBinding` | `BrawlerChaosBindingUImpl` | `BrawlerJoltBindingUImpl` |

  The engine traits hold the six adapter types (`BodyAdapter`, `ReaderAdapter`, `QueryAdapter`, `Factory`,
  `VizQuery`, `VizReader`), the backend name `kName` (`chaos` or `jolt`) and `computeDeterminismFingerprint`, which
  runs og-simulation-jolt's determinism fingerprint in the Jolt configuration and returns none in the Chaos one.
  The session's adapter members (`m_physAdapter`, `m_physReaderAdapter`, `m_queryAdapter`; this class's until
  og-ue-simulationcomponentandmanagersplit task 24a), this class's two visualization doors and its `[SimHost.Backend]` line name them through
  `EngineTraits` (the session through `BrawlerSessionTraitsUImpl::EngineTraits`). Until task 15a the header held the
  adapter aliases, the name and the fingerprint itself, per configuration, in namespace `physicsBackendUImpl`; task 15a
  kept six of those names for the files outside it, and task 15b switched those files (`BrawlerCompositionUImpl`'s
  `IntegrationExecFor` and `HitDetectionSystem`, `OGBrawlerSimulationManagerConceptTest.cpp`, the build identity and
  `PhysicsBackendHostUImpl.h`) to `EngineTraits` and deleted the names (`PhysicsBackendUImpl-rationale.md` §5).

### What the arms hold (tasks 82 and 83)

Since og-simulationscheduler-withjolt task 82 the Chaos host is a class of its own, `ChaosTickBackendUImpl`
(`ChaosTickBackendUImpl.h` and `.cpp`; its reasoning is `ChaosTickBackendUImpl-rationale.md`). Since task 83 the Jolt
host is too, `TickDriverBackendUImpl` with its state struct `TickDriverBackendWorldsUImpl` (`TickDriverBackendUImpl.h` and,
until og-tickdriver task 15e moved the host to OGSimulationUnreal as a header template, a `.cpp`;
`TickDriverBackendUImpl-rationale.md`). Until og-tickdriver task 15b the two had historical names:
ChaosBackendUImpl with ChaosBackendWorldsUImpl (historical names), and
JoltBackendUImpl with JoltBackendWorldsUImpl (historical names). Task 15b named them by who schedules the steps and
made the Jolt one a template over the engine traits and the binding. This class's two files have no arm left:

* **Header:** the include of `PhysicsBackendHostUImpl.h`, which includes the configuration's backend header and
  names its two types (`PhysicsBackendHostUImpl-rationale.md`); the members `m_backendWorlds`, above the adapters,
  and `m_backend`, directly after `m_manager`, which are the session's since og-ue-simulationcomponentandmanagersplit task 24a (typed by
  `BrawlerSessionTraitsUImpl::BackendWorlds` and `Backend`, which this header's selector defines;
  `UESimulationSession-rationale.md` §3); and `friend physicsBackendUImpl::Backend;`, which befriended the
  configuration's one host (since og-tickdriver task 15b; it replaced one `friend class` line per host), and the
  class-template friend `BrawlerTickDriverGameUImpl` (which replaced the frame host's friend line in og-tickdriver
  task 8), in both configurations, until og-ue-simulationcomponentandmanagersplit task 27b deleted both: since task 24a the session befriends the two hosts
  through its traits, and by task 27b neither of this class's lines covered a call.
* **`.cpp`:** at each seam call site (the table below), one unconditional call on the session's backend,
  `m_session.m_backend.` (`m_backend.` until og-ue-simulationcomponentandmanagersplit task 24a).

`ChaosTickBackendUImpl` holds what the Chaos arm held before task 82: the Chaos engine includes; `FSimulationState2`,
`FSimulationInput2`, `FRewindPushProbeStashedBody` and `FSimulationManagerAsyncCallback`, whose definitions moved
with them; the six passthroughs from `onGameSimulation` to `noteResimGrant` (§1); `InjectInputs_External` with
`editChaosTickMapper` / `getChaosTickMapper`; `m_asyncCallback`, the two delegate handles and `m_chaosTickMapper`;
the `[ChaosDilation]` window; the category-table helper (emplaceBrawlerQueryAdapter, whose rows moved into the
Chaos binding's `BrawlerChaosBindingUImpl::categoryChannels` in og-tickdriver task 15a), `writeRestoredBodyState` and
the push-probe helpers.
Since task 53 the arm had also held three pieces that task 51 left shared: the role branches' adapter
construction in `BeginPlay` (with the solver lookup above the branches), `tryRegister`'s first-call bind
(`ChaosPhysicsFactory` and the volume registration), and `registerVizVolume`'s query parameters. They moved too.
The callback and its input hold a `ChaosTickBackendUImpl*` instead of this class. One member stayed here,
the clock counts at the window start (m_latencyClockAtWindowStart, historical name), for a reason
`ChaosTickBackendUImpl-rationale.md` §2 gives, until og-tickdriver task 18 moved it into the latency class
(`m_clockAtWindowStart`; the class is `UELatencyBudgetDiagnostics` since task 19a).

The guards on that code moved with it. This class's G-02, G-51 to G-55, G-71, G-76 and G-77 are retired
(`OGBrawlerSimulationManager-guards.md` §R) and are `ChaosTickBackendUImpl-guards.md` G-01 to G-09.

`TickDriverBackendUImpl` holds what the Jolt arms held before task 83: the Jolt includes, console variables and
anonymous helpers; the stepping mode, the world build, the slot table, the bind and the release; the frame host,
the hooks and the step driver (since og-tickdriver task 8 the frame host, the game binding and the tick driver); the render publish, interpolation, correction smoothing and apply; the shadow
world's hand-over, restore and overwrite; and the contact counts. Its world state is `TickDriverBackendWorldsUImpl`,
which this class holds as `m_backendWorlds`. The guards moved too: this class's G-80 to G-84 are retired
(`OGBrawlerSimulationManager-guards.md` §R) and are `TickDriverBackendUImpl-guards.md` G-01 to G-05. Since og-tickdriver tasks 9
and 10 the render half is og-simulation's presenter, and the stepping mode, the world build, the slots, the bind, the
release, the shadow world and the contact counts are og-simulation-jolt's world host `JoltTickWorldHost`, held in
`TickDriverBackendWorldsUImpl`; of those guards only G-01 is live (`TickDriverBackendUImpl-guards.md` §R names the successors).

### The seam (task 82)

This class calls its backend through one fixed set of entry points. Each is called from exactly one place, the
site where the arm sat, with no `#if`. Task 82 implemented the entry points that have a Chaos body; task 83 moved
the Jolt column into its own host and gave every entry point a body on both hosts: an empty inline body where the
column says "empty". The set is checked at compile time: the concept `PhysicsBackendHost`
(`PhysicsBackendHostUImpl.h`) lists every entry point with its signature, and `BrawlerSessionTraitsUImpl.h` asserts
it on the backend the session holds, `BrawlerSessionTraitsUImpl::Backend<UESimulationSession<BrawlerSessionTraitsUImpl>>`
(this class's `compositionContractsHold()` did until og-ue-simulationcomponentandmanagersplit task 24a), as the selector does after it picks the host
(since og-tickdriver task 15b).

| entry point | call site in this class | Chaos body (`ChaosTickBackendUImpl`) | Jolt body (`TickDriverBackendUImpl`) |
|---|---|---|---|
| `buildWorld` | the start-up (the session's `beginPlay` since task 27b), after the ini reads, before the role branches | the solver checks, then the adapter emplacement (both moved, `ChaosTickBackendUImpl-rationale.md` §2) | the static import, then the world host's `build` |
| `startStepping` | the end of the start-up (the session's `beginPlay` since task 27b) | the post-tick delegate, the async callback, the `InjectInputsExternal` delegate | `startDriverStepping` |
| `onEndPlayBegin` | the first statement of the teardown, and it stays first (this class's `EndPlay` and its `G-85` until task 27a; the session's `endPlay` and `UESimulationSession-guards.md` G-20 since) | empty | unregister the tick functions and wait for outstanding steps (`TickDriverBackendUImpl-guards.md` G-01) |
| `onEndPlayWorld` | the teardown (the session's `endPlay` since task 27a), inside the world test, after the latency diagnostics' `end` | remove both delegates, free the callback | empty |
| `beforePresent_GameThread` | the post-physics pass (the session's `onPostPhysicsStep` since task 27a), before the post-simulation pass | empty | the presenter's `present_GameThread` (og-simulation's `TickDriverPresenter`) |
| `updateVisualization_GameThread` | the post-physics pass (the session's `onPostPhysicsStep` since task 27a), after the timing buffer | `updateVisualizationAll` | the presenter's `updateVisualization_GameThread` |
| `editVizQuery`, `getVizReader`, `registerVizVolume` | the three public doors of the same names, which keep their signatures and forward (the session's since og-ue-simulationcomponentandmanagersplit task 24b) | the query adapter, the reader, the volume with the owner ignored | the step world or the shadow world, by stepping mode |
| `createBodies_GameThread` | `tryRegister`'s first pass; `false` returns `TryRegisterStatus::Pending` | the `ChaosPhysicsFactory` bind; returns `true` | the slot acquire and the Jolt bind; `false` where it waits for a slot |
| `onRegistered_GameThread` | `tryRegister`'s Ready path | empty | the occupancy push and the render target |
| `onUnregistered_GameThread` | `unregisterFromNewFramework` | empty | the render target and the slot release |
| `editDiagnostics().sampleLatencyWindow_GameThread` | `UELatencyBudgetDiagnostics::tick_GameThread`, before the window closes (since og-tickdriver task 18; this class's own latency tick before) | the dilation scale and the tick-offset poll | empty |
| `editDiagnostics().logLatencyWindow_GameThread` | `UELatencyBudgetDiagnostics::tick_GameThread`, after `[LatencyBudget.Window]` | the `[ChaosDilation]` line and its reset | empty |

The root-capsule check after `tryRegister`'s bind reads the movement declaration's stamped
`bindings.parentBodyId`, because `createBodies_GameThread` returns only whether the bind happened
(`ChaosTickBackendUImpl-rationale.md` §2). The Jolt bind stamps the slot's root body there on every declaration, so
the value is the one the check compared before (`TickDriverBackendUImpl-rationale.md` §1).

### The rules

1. **Nothing reflected sits inside an arm.** UHT rejects `UPROPERTY`, `UFUNCTION` and the other
   reflection markers inside a preprocessor block it does not know ("must not be inside preprocessor
   blocks"; the UHT header parser, lines 631-636 of UhtHeaderFileParser.cs, UE 5.6), and
   `OG_PHYSICS_BACKEND_CHAOS` is such a block. Plain C++ inside it is fine. This header's only
   reflected declarations are the `UCLASS` and its `GENERATED_BODY`, both outside every arm; since task 83
   this header has no arm at all. A reflected declaration that one backend needs is declared in both
   configurations.
2. **Arms live only in `OGBrawlerUnreal.Build.cs` (the constant), the seam-A selector `PhysicsBackendUImpl.h`,
   the host selector `PhysicsBackendHostUImpl.h`, `ChaosTickBackendUImpl.cpp` and, since og-tickdriver task 15a,
   `BrawlerJoltBindingUImpl.cpp` and `BrawlerChaosBindingUImpl.cpp`.** The last three are whole-file arms; the
   engine-traits and binding headers have no `#if` (`PhysicsBackendUImpl-rationale.md` §4). The Jolt host's `.cpp`
   and the Jolt traits' `.cpp` were two more arms until og-tickdriver task 15e, which made the host a header template
   in OGSimulationUnreal (its instantiation is in `BrawlerJoltBindingUImpl.cpp`) and moved the traits to
   OGSimulationJoltUnreal, which has no arm. The frame host's `.cpp` was a third until og-tickdriver task 11 moved it to OGSimulationUnreal,
   where it has no arm (`UETickDriverHost-rationale.md` §7). The two backend headers have no `#if`, and only the
   selector includes them, each in its own configuration (task 83, which replaced task 82's list; this class's
   two files had arms until then). `UOGBrawlerSimulatableComponent` and
   `AOGBrawlerUECharacter` have none. The component reaches the visualization adapters through three members
   that exist in both configurations (below).
3. **Both adapter sets compile in both configurations** once the Jolt arm exists (task 18). The
   switch selects wiring, never whether an adapter compiles.
4. **The Chaos arm holds the code as it was,** with only the edits the shared changes below force. No
   new feature lands in it. Task 82 moved it into `ChaosTickBackendUImpl` with the owner prefix and the pointer
   types changed, the call order inside each piece kept, and two pieces of `BeginPlay` moved into one
   `buildWorld` call (`ChaosTickBackendUImpl-rationale.md` §2).
5. **No comment marks an arm.** This section is the description.

### The shared changes

* `OnPostPhysicsStep` takes no argument (nor does the session's `onPostPhysicsStep`, which it became in task 27a). The
  Chaos configuration binds the scene's post-tick delegate to a weak lambda that drops the scene and calls it. The `[ChaosDilation]` block reads the scene
  from `GetWorld()` instead (§16, "The `[ChaosDilation]` line"). The two empty physics-scene hooks (pre-tick and step) and their
  bindings are deleted (§11 C20).
* **The split** of the frame's game-thread work into `onFrameStepsDue_GameThread`, the drain and the
  reap (§7).
* **`[ReleaseBatch] first=<tick> numSteps=<n>`**, one line per drain call, on `LogOGNet` at `Verbose`
  (its `kOGLogRoutes` row, `UESimulationLogRoutes-rationale.md` §6), after the drain's coordinator test and before it drains. The committed
  `LogOGNet=Warning` hides it, and so does `LogOGNet=Log`. In the Chaos configuration there is one per physics frame, with `n` equal to the frame's step
  count; the Jolt host will print one per step with `n = 1`. Every `[Release]` line's `releaseTick`
  lies in `[first, first + n)` of the nearest `[ReleaseBatch]` before it. `[Release]` prints
  absolute ticks, which differ between runs, so the two backends are compared by distributions,
  never by lines (gate 22).
* **`og.Sim.TestGameThreadHitchMs`** (not in Shipping): *code moved to `UESimulationSession.cpp` and the session's
  `onPostPhysicsStep` (og-ue-simulationcomponentandmanagersplit task 27a); `UESimulationSession-rationale.md` §10, in
  OGSimulationUnreal's docs, describes it.* The brawler's use: on its inline-stepping server, 250 ms gives a frame of
  about 15 steps, which shows the release pattern in one `[ReleaseBatch]` line.
* **`editVizQuery()`, `getVizReader()` and `registerVizVolume(descriptor, owner)`** replace
  `editQueryAdapter()` and `getPhysicsBodyReaderAdapter()`, and the unused body-adapter accessor is
  deleted, so no code outside this class can name a body adapter. The first two return references to
  a world every peer's game thread may query (`OGBrawlerSimulatableComponent-rationale.md` §10; task 53 made
  them pointers, null on a worker client, until task 56 gave that client its shadow world). In the Chaos configuration
  they always return the Chaos query adapter and reader; `registerVizVolume` builds the query
  parameters the component used to build itself (simple collision, the owner ignored) and registers
  the volume on the query adapter. It returns an optional volume id: always set in the Chaos
  configuration, empty in the Jolt configuration while the owner's slot acquire waits (task 66, the slot
  table, `TickDriverBackendUImpl-rationale.md` §3). They are handles, an exception to §1's narrow-passthrough rule that
  they inherit from the accessors they replace: the two visualizers are templates over the adapter
  types and call them directly, on the game thread. The Jolt configuration's answers are in
  `TickDriverBackendUImpl-rationale.md` §3. Since og-ue-simulationcomponentandmanagersplit task 24b the three doors are the
  session's (`UESimulationSession-rationale.md` §8), and the component reaches them through this class's `session()`.
* **The backend token.** The build identity builds its token from `EngineTraits::kName` (kBackendName until og-tickdriver task 15b) and the fingerprint:
  `chaos`, or `jolt:<fingerprint>`. The server refuses a login whose backend differs from its own, with a
  reason that names both, and admits a different Jolt fingerprint with a Warning (`OGBuildIdentity-rationale.md` §7).
* **`LogOGSimHost`** and its one-shot `Warning` line at the start-up (since og-ue-simulationcomponentandmanagersplit task 27b
  the session's statement in `beginPlay`, with the fingerprint from the brawler's hook `backendFingerprint()`;
  `UESimulationSession-rationale.md` §11.6):
  `[SimHost.Backend] backend=chaos fingerprint=none` in the Chaos configuration, and
  `[SimHost.Backend] backend=jolt fingerprint=<16 hex digits>` in the Jolt one, so every log names the
  configuration it came from. The fingerprint is the build identity's (`backendFingerprint`), computed
  once per process. `Config/DefaultEngine.ini` pins the category at `Warning`, beside
  `LogOGLatencyBudget`. Since og-tickdriver task 11 the category is declared, exported, in OGSimulationUnreal's
  `OGSimulationUnreal.h` and defined in its `.cpp`, under the same name, so that ini row still governs it; this
  class's header includes that header where its own declaration of the category was, so every file that saw the
  category through it still does, in both configurations. In the Chaos configuration it is the category's only `Warning` line; the Jolt
  host adds the lines `TickDriverBackendUImpl-rationale.md` §2 lists, its window line `[SimHost.Window]` among them (§8).
* **`dt`** is the engine physics setting AsyncFixedTimeStepSize in both configurations, `checkf`-ed at the start-up to
  be one tick of `UEBrawlerNetConfig::tickFrequencyHz` (since task 27b by the session's `beginPlay`, through its traits'
  `NetConfig`, which is `UEBrawlerNetConfig`; `UESimulationSession-rationale.md` §11.5). It is the number Chaos steps
  with, and the Chaos arm `checkf`s that its solver's async dt is the same value, which it was before
  the manager stopped asking the solver (design D13).

### The Jolt arm's world (task 53)

Moved to `TickDriverBackendUImpl-rationale.md` §3 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's stepping (task 54)

Moved to `TickDriverBackendUImpl-rationale.md` §4 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's render publish and apply (task 55)

Moved to `TickDriverBackendUImpl-rationale.md` §5 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's game-thread shadow world (task 56)

Moved to `TickDriverBackendUImpl-rationale.md` §6 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's render interpolation (task 19)

Moved to `TickDriverBackendUImpl-rationale.md` §7 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's render correction smoothing (task 57)

Moved to `TickDriverBackendUImpl-rationale.md` §8 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The Jolt arm's contact counts (task 78)

Moved to `TickDriverBackendUImpl-rationale.md` §9 (og-simulationscheduler-withjolt task 83), with the code it describes.

### The build rule until task 21

Every task that edits a file with an arm (the traits header, the selector, `ChaosTickBackendUImpl.cpp` and
`BrawlerJoltBindingUImpl.cpp`, which instantiates the Jolt host since og-tickdriver task 15e; the host's own `.cpp`
until then), or a file the selector includes or that includes it (this class's
two files, both backend headers), builds both configurations: the Jolt configuration on the task's full target matrix, and the Chaos configuration
on the editor, the targets gate 22 runs it on (the packaged dedicated server and the Android client)
and both test targets. To build the Chaos configuration, set the constant to `1`, build, set it back to
`0`, and make sure the restored `OGBrawlerUnreal.Build.cs` has a new modification time: a restore that
keeps the old time (a copy that preserves it) lets UBT reuse the other configuration's makefile, and the
module silently keeps the other arm.

### Task 21

After gate 22, task 21 deletes the Chaos arms, the macro, the constant and the traits header's Chaos
branch, and retires the guards listed above. Since task 83 that is deleting `ChaosTickBackendUImpl.h` and `.cpp` with
their two documents, the Chaos branches of the traits header and the selector, and the config flip.

---

## Provenance

The two source files carried **317 workspace-only citations** — backlog item numbers, task
numbers, review-note numbers, initiative document names and design section marks — before this
pass. They are meaningless to a reader who does not have that workspace, so they were removed from
the source and their content folded into the sections above. **The facts they carried are here; the
item numbers are not**, deliberately: an item number is provenance, not a guard, and this document
is where provenance belongs. The initiative workspace remains the record of *when* each decision
was taken.
