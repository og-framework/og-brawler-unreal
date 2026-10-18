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
* **§14** recorded a later code change: how the first replayed step hands the restored body state to
  the engine (og-netcode-v2-field-defects task 10's fix). It moved to `ChaosTickBackendUImpl-rationale.md` §5
  with the Chaos host (og-simulationscheduler-withjolt task 82), as did §8's rewind-push probe and §9's
  derivation of the `+ 1`.
* **§16** is the latency-budget probe's host side: its stamp sites, threads and measurement errors
  (og-simulationscheduler-withjolt task 1).
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
<!-- lint-external-ref: AActor::Owner -- Unreal Engine type, outside every scan root; the engine is not vendored into this repository -->
<!-- lint-external-ref: bUseLoggingInShipping -- Unreal Engine target-rules flag (Shipping logging); this project does not set it -->
<!-- lint-external-ref: FNoLoggingCategory -- Unreal Engine type (LogMacros.h), outside every scan root -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
<!-- lint-external-ref: pushBodyState -- retired name: the Chaos rollback-push lambda deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
<!-- lint-external-ref: UNetConnection::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::Tick -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UDataStreamChannel::WriteData -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: GameEngine::GetMaxTickRate -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::FindValidResimFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::SetTargetStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: TickFlush -- Unreal Engine method, outside every scan root -->
<!-- lint-external-ref: UWorld::OnPostTickFlush -- Unreal Engine method, outside every scan root (§16) -->
<!-- lint-external-ref: FChaosScene::GetNetworkDeltaTimeScale -- Unreal Engine method, outside every scan root (§16) -->
<!-- lint-external-ref: FChaosScene -- Unreal Engine type, outside every scan root (§16) -->
<!-- lint-external-ref: OnPhysScenePreTick -- Unreal Engine delegate (ChaosScene.h), outside every scan root; this module stopped binding it in task 51 (§13.1 quotes the old source) -->
<!-- lint-external-ref: OnPhysSceneStep -- Unreal Engine delegate (ChaosScene.h), outside every scan root; this module stopped binding it in task 51 (§13.1 quotes the old source) -->
<!-- lint-external-ref: editQueryAdapter -- RETIRED in task 51 (§18): the public query-adapter accessor, replaced by editVizQuery. It must NOT resolve -->
<!-- lint-external-ref: getPhysicsBodyReaderAdapter -- RETIRED in task 51 (§18): the public reader accessor, replaced by getVizReader; §12 quotes its old declaration verbatim. It must NOT resolve -->
<!-- lint-external-ref: FPlatformTime::Seconds -- Unreal Engine function, outside every scan root (§16) -->
<!-- lint-external-ref: SendTo -- Unreal Engine socket method, outside every scan root (§16) -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root (§16) -->
<!-- lint-external-ref: net.IpConnectionUseSendTasks=1 -- Unreal Engine console variable and value, outside every scan root (§16) -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: StatPeriod -- Unreal Engine stat window, outside every scan root -->
<!-- lint-external-ref: DesiredTickRate -- Unreal Engine net-driver field, outside every scan root -->
<!-- lint-external-ref: MaxNetTickRate -- Unreal Engine net-driver setting, outside every scan root -->
<!-- lint-external-ref: BaseEngine.ini -- Unreal Engine's own config file, outside every scan root -->
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
| `InjectInputs_External` → `onFrameStepsDue_GameThread`, `releaseDelayedInputsForStep`, `reapConnections` (Chaos configuration, §18; a `ChaosTickBackendUImpl` member since task 82) | GAME | PROBE A and PROBE 6 once per physics frame, then the drain and the reap (§7) |
| `OnPostPhysicsStep` | GAME | the post-physics pass: the shadow-world restore (Jolt configuration, worker client, `TickDriverBackendUImpl-rationale.md` §6), the render interpolation and apply and the shadow overwrite (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §5, §7), visualization snapshot (with the rendered poses in the Jolt configuration), timing buffer, score push, latency probe; bound to the scene's post-tick delegate in the Chaos configuration, called by the frame host's `TG_EndPhysics` tick function in the Jolt configuration |
| the frame host's two tick functions → the tick driver → `onFrameStepsDue_GameThread`, the reap (authority), `OnPostPhysicsStep`, through `BrawlerTickDriverGameUImpl` (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §4) | GAME | the per-frame work at `TG_StartPhysics` and `TG_EndPhysics` (`UETickDriverHost-rationale.md`; `SimulationTickDriver-rationale.md` §3) |
| the tick driver's per-step body (`SimulationTickDriver`) → `SimulationStepDriver::runTick` and the four step hooks (`TickDriverStepHooks`), including the authority's per-step drain `releaseDelayedInputsForStep` (Jolt configuration, `TickDriverBackendUImpl-rationale.md` §4) | PHYSICS: a worker task on a client, the game thread inline on the authority | one simulation step under the world mutex; everything the core `SimulationManager` runs beneath it |
| `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`, `OnPostSolve_Internal`, `ProcessInputs_Internal`, `TriggerRewindIfNeeded_Internal`, `FirstPreResimStep_Internal`, `ApplyCorrections_Internal` | PHYSICS | the six Chaos hook overrides (Chaos configuration, §18; in `ChaosTickBackendUImpl.cpp` since task 82) — `ProcessInputs_Internal` and `ApplyCorrections_Internal` are empty (§11 C5) — and everything the core `SimulationManager` runs beneath them |
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
`static_assert` at the poll pins `simulatableBrawler::PlayerInput` trivially copyable (it owns
no memory). A member that owned an
allocation would turn this tear into a crash, and that assert is what stops one landing quietly.
*(Until og-syncedInput-rework task 3 there were two asserts: the input was a six-slice composite,
which fails `is_trivially_copyable` through `std::tuple`'s implementation alone, so the composite
was pinned trivially destructible and the machine sub-input the display read trivially copyable.
Since task 3 `simulatableBrawler::PlayerInput` is the flat `SyncedPlayerInput`, and one
trivially-copyable assert, whose message names the two it replaced, covers both.)*

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

**In the Chaos configuration only (§18):** `noteResimRequest` and `noteResimGrant` have no callers
but the Chaos hooks, and neither have the four plain forwards beside them (`onGameSimulation`,
`onPostGameSimulation`, `onCheckIsSimilar`, `prepareResimulation`), so all six sat in the Chaos arm.
Since task 82 they are members of `ChaosTickBackendUImpl`, the Chaos host, which reaches the manager through its
owner (`ChaosTickBackendUImpl-rationale.md` §1). They are still one-purpose forwards, and the rule above binds them
there. The Jolt host feeds the resim-gate probe from the step driver instead (task 18).

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

### The latency-budget probe adds no crossing (og-simulationscheduler-withjolt task 1)

The probe is game-thread-owned. Its physics-thread stamps reach it through a single-producer
mailbox (synchronized), the one game-to-physics value it needs is an atomic, and every other read
its stamp sites make is same-thread with its writer. The CROSSING count is unchanged; the full
argument, read by read, is §16's *Threads* table.

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

### The backend's two members and the destruction order (og-simulationscheduler-withjolt task 83)

This class holds its physics backend in two members, chosen by `PhysicsBackendHostUImpl.h`:
`physicsBackendUImpl::BackendWorlds m_backendWorlds` and
`physicsBackendUImpl::Backend m_backend{ *this, m_backendWorlds }`. In the Jolt configuration they are
`TickDriverBackendWorldsUImpl` over the host's world host (since og-tickdriver task 10 an engine port and og-simulation-jolt's world host
`JoltTickWorldHost`, which holds the runtime lease, the contact counter, the step and shadow worlds, the shadow
world's adapters, the slot table, the world mutex and the thread context) and `TickDriverBackendUImpl` (the frame host,
the game binding, the render-target table and, since og-tickdriver task 15d, og-simulation's `SimulationTickBackend`,
which holds the render source, the presenter and the tick driver, which owns the hooks and the step driver),
instantiated as `TickDriverBackendUImpl<JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>`
since og-tickdriver task 15b (as `TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl,
BrawlerJoltBindingUImpl>`, OGSimulationUnreal's header template, since og-tickdriver task 15e); in the Chaos configuration an
empty `ChaosTickBackendWorldsUImpl` and `ChaosTickBackendUImpl`. Members are destroyed in reverse declaration order,
and the two positions are what that order needs. Declared, top to bottom:

```
m_backendWorlds → m_physAdapter → m_physReaderAdapter, m_queryAdapter → … → m_integrationLayer → m_manager → m_backend
```

So `m_backend` is destroyed first (on Jolt the tick backend, which destroys the tick driver, with its step driver and
hooks, then the presenter and the render source; then the render-target table, the game binding and the frame host,
while the manager still lives), then the manager, the integration layer and the adapters, and last the
worlds (on Jolt the world host, with the shadow world's adapters, the two worlds, the contact counter and then the
runtime lease, and then its engine). `compositionContractsHold()` asserts `m_backendWorlds < m_physAdapter < {m_physReaderAdapter,
m_queryAdapter} < m_integrationLayer` and `m_manager < m_backend`, the same in both configurations. The
rows inside the two Jolt objects are their own assertions (`TickDriverBackendUImpl-rationale.md` §1); together
they are the rows this class asserted before task 83.

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

Since task 81 the chain is defined in `struct BrawlerCompositionUImpl`
(`BrawlerCompositionUImpl.h`; why it exists, the member table, the single source of truth and the
bind wrapper are in `BrawlerCompositionUImpl-rationale.md`). This class keeps every old name
(`BrawlerSimulatables`, `BrawlerStorage`, ..., `ManagerType`) as a one-line, class-scoped
re-export of the struct's member, so its member declarations are typed exactly as before.
`compositionContractsHold()` asserts that `ManagerType` is `BrawlerCompositionUImpl::Manager`: a
backend host outside this class (task 83) names the manager through the struct, because
`ManagerType` is private. `IntegrationLayerType` is still this class's own alias; the Jolt-only
step-driver alias moved to `TickDriverBackendUImpl` with the step driver (task 83), where it named the struct's
members; since og-tickdriver task 8 the game binding `BrawlerTickDriverGameUImpl` names them for the tick driver.

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

`[ResimProbe` is the first row of this module's block, so it sits after the `[Resim.` catch-all,
which is in og-simulation's block (*Two blocks*, below). The two cannot collide: `[Resim.` requires
the dot and `[ResimProbe` has a `P` there. The one edit that would make them collide, widening the
catch-all to `[Resim`, is now a compile error, not a silent reroute: in og-simulation it fails
`allReachable` (the `[ResimCheck.` rows after it begin with `[Resim`) and
`ogLogRouteBlocksAreDisjoint` (`[ResimProbe` begins with it), and a `[Resim` row in this module's
block fails `ogLogRouteBlocksAreDisjoint`. *(Until og-tickdriver task 12 the single table put
`[ResimProbe` ahead of `[Resim.`, defensively, against that edit.)*

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
| `LogOGLatencyBudget` | `[LatencyBudget]`, `[LatencyBudget.Window]` (logged with `UE_LOG` directly, not routed; §16) |
| `LogOGChaosDilation` | `[ChaosDilation]` (logged with `UE_LOG` directly, not routed; §16) |

`[RelayProbe.Miss]` says *why* each miss missed — an in-span coverage hole, asking above the
newest arrival, or below the oldest; `[RelayProbe.Delta]` is the signed probe-tick-to-newest
distribution; `[RelayProbe.Stale]` is the longest consecutive fallback run. `[ResimProbe.Chaos]`
is the request/grant/refusal ledger and the requested-vs-granted pin; `[ResimProbe.Apply]` is the
apply edge and the replay span; `[ResimProbe.Landing]` is the frontier-landing split;
`[ResimProbe.Stranded]` is the per-event Verbose companion to it.

### Levels — the `plainLevel` column (og-simulationscheduler-withjolt task 80)

**Every row of `kOGLogRoutes` carries a third field, `plainLevel`: the level of a routed line that
carries no `[Verbose]` or `[Warning]` token.** `RouteOGMessage` starts every line at `Log`, sets
`Verbose` or `Warning` from a leading token, and only then, on the matching row, replaces a
remaining `Log` with the row's `plainLevel`. A token therefore always wins: the per-window
`[Warning]` summaries and the per-event `[Verbose]` probe detail are logged exactly as their
emitters mark them, whatever the row says. That is also why G-50's predicate is untouched: every
`[DivergenceProbe` line carries a token, so the row's `plainLevel` never applies to one.

The table lives in `SimulationManagerUImpl.h`, not in this `.cpp`, so that log sites in other files
can read a row at compile time (below). `RouteOGMessage` itself stays in this file. Since og-tickdriver
task 12 the rows og-simulation alone emits come from og-simulation and the header concatenates them with
its own (*Two blocks*, below); the levels and the order within each block are the ones listed here.

**Why.** With a visible `-log` console, every log line costs a synchronous console write on the
thread that logs it (the research note on the gate-22 dedicated-server frame rate: about 1.4 ms per
line at gate 22, 12–20 ms step outliers, and a 3.2 s join hitch when one unbounded receive loop logged
761 packets). At `LogOGNet=Log` and `LogOGSimTick=Log` every tick and every received packet wrote a
line. Now `Log` carries only rare events, and the per-tick lines need `Verbose`.

**The rule.** A row whose token-less lines arrive per tick, per packet or per relay (about 10 lines
per second per peer or more in normal play) is `Verbose`. A row for rare events (gaps, drops, delay
shifts, stats windows, resyncs, resim prepare and finish, join and leave) is `Log`. A `static_assert`
(`ogLogRoutePlainLevelsAreLogOrVerbose`) admits only those two values: a line that must reach
`Warning` says so with its own token.

**The table, with the rates that classified it.** Rates are token-less lines per second in the
gate-22 Jolt packaged final run (`LogOGNet=Log`, `LogOGSimTick=Log`; server 234 s with up to three
clients, client 1 191 s, client 3 76 s). "token" means every line of that row carries `[Verbose]`
or `[Warning]`, so `plainLevel` does not apply to it. Before this task every row behaved as `Log`.

| row | category | rate (server / client) | `plainLevel` |
|---|---|---|---|
| `[Resim.Input]` | `LogOGSimTick` | token (`[Verbose]`) | `Verbose` |
| `[ResimProbe` | `LogOGResimProbe` | token | `Log` |
| `[TimeResync.` | `LogOGSim` | – / ≤ 0.7 (join) | `Log` |
| `[Resim.` | `LogOGSim` | – / ≤ 5.0 (`[Resim.Post]`) | `Log` |
| `[ResimCheck.Divergence]` | `LogOGSim` | – / ≤ 1.0 | `Log` |
| `[ResimCheck.PrepareRestore]` | `LogOGSim` | – / ≤ 2.8 | `Log` |
| `[ResimCheck.Check]` | `LogOGSimTick` | – / 142–172 | `Verbose` |
| `[ResimCheck.IsSimilar]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[ResimCheck.TriggerRewind]` | `LogOGSimTick` | Chaos only: one per physics step | `Verbose` |
| `[AuthoritySimulation]` | `LogOGSimTick` | 60 / – | `Verbose` |
| `[ClientPrediction]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[PredictionSimulation]` | `LogOGSimTick` | – / 57–59 | `Verbose` |
| `[PostPrediction]` | `LogOGSimTick` | – / 142–172 | `Verbose` |
| `[CollectInput]` | `LogOGSimTick` | 116 / 142–172 | `Verbose` |
| `[ServerReceive]` | `LogOGNet` | 573 / – | `Verbose` |
| `[ReceiveLocalInput] DISCARD` | `LogOGNet` | 0 (rare: a too-far-future input) | `Log` |
| `[ReceiveLocalInput]` | `LogOGNet` | 116 / – | `Verbose` |
| `[SendCorrectionStateToClients]` | `LogOGNet` | 49 / – | `Verbose` |
| `[SendRemoteInputToClients]` | `LogOGNet` | no emitter | `Log` |
| `[SendLocalInputToServer]` | `LogOGNet` | – / 94–99 | `Verbose` |
| `[ReceiveCorrectionState]` | `LogOGNet` | – / 57–59 | `Verbose` |
| `[ReceiveCorrectionInput]` | `LogOGNet` | no emitter | `Log` |
| `[InjectCorrectionState]` | `LogOGNet` | – / 57–59 | `Verbose` |
| `[InjectCorrectionInput]` | `LogOGNet` | no emitter | `Log` |
| `[DrainOutOfOrder]` | `LogOGNet` | no emitter | `Log` |
| `[InputGap]`, `[InputDrop]`, `[DelayShift]`, `[InputStats]`, `[InputDomain]` | `LogOGNet` | token (`[Warning]`) | `Log` |
| `[Park]` | `LogOGNet` | 116 / – | `Verbose` |
| `[Release]` | `LogOGNet` | 116 / – | `Verbose` |
| `[ReleaseBatch]` | `LogOGNet` | 60 / – | `Verbose` |
| `[RelaySkip]` | `LogOGNet` | token | `Log` |
| `[RelayProbe` | `LogOGRelayProbe` | token | `Log` |
| `[DivergenceProbe` | `LogOGDivergenceProbe` | token | `Log` |
| `SimulationManager:`, `tryRegister:`, `NewFramework:` | `LogOGMgmt` | ≤ 0.04 | `Log` |

Notes on the rows:

* **`[ReceiveLocalInput] DISCARD` is its own row, ahead of `[ReceiveLocalInput]`.** The core emits
  both from one callback (`SimulationNetSync.h`); the first is per input, the second is a rare
  anomaly with no token. The more specific row keeps the anomaly at `Log`. The reachability
  `static_assert` keeps it ahead: below `[ReceiveLocalInput]` it could never be taken.
* **`[ReleaseBatch]` is a row although `RouteOGMessage` never sees it.** It is logged directly, by
  `releaseDelayedInputsForStep`, and its row exists so that site reads its level from the table.
* **`[Resim.Input]` and the token rows.** Their `plainLevel` has no effect today. `[Resim.Input]` is
  `Verbose` because it is a per-replayed-tick line; the token rows are `Log` so a future token-less
  line there is not hidden by accident.
* **Four rows have no emitter in the tree:** `[SendRemoteInputToClients]` (its write is retired in
  `SimulationNetSync.h`), `[ReceiveCorrectionInput]`, `[InjectCorrectionInput]` and
  `[DrainOutOfOrder]`. With no rate to classify them they stay `Log`. They are candidates for
  deletion; this task did not delete them.
* **The `LogOGSim` rows stay `Log`**, and the committed `Config/DefaultEngine.ini` sets
  `LogOGSim=Log`, so the committed output is unchanged. Resim lifecycle lines are at most about 5 per
  second on a client.

**Direct log sites read the same table: `OG_LOG_ROUTED`.** Six per-tick or per-packet lines in this
module are logged with `UE_LOG` directly rather than through `RouteOGMessage`. They now use
`OG_LOG_ROUTED(Category, Format, ...)` (`SimulationManagerUImpl.h`), which looks up the row whose
prefix begins `Format` at compile time, fails the build if there is none, and expands to
`UE_LOG(Category, Verbose, ...)` or `UE_LOG(Category, Log, ...)` from that row's `plainLevel`. So the
level is set in one place for routed and direct lines alike. The sites:

| row | site | file |
|---|---|---|
| `[ReleaseBatch]` | `releaseDelayedInputsForStep` | `SimulationManagerUImpl.cpp` |
| `[ResimCheck.TriggerRewind]` | `TriggerRewindIfNeeded_Internal`, the two `rewind=0` lines (Chaos only) | `ChaosTickBackendUImpl.cpp` |
| `[ServerReceive]` | `ServerReceiveRemoteMove_Implementation`, the no-coordinator path | `SimmableUpdateComponent.cpp` |
| `[ReceiveCorrectionState]` | `OnRep_CorrectionState` | `SimmableUpdateComponent.cpp` |
| `[ClientPrediction]` | `buildPlayerInput` | `OGBrawlerInputCollectionComponent.cpp` |

The category stays at the site. Nothing checks that it is the row's category; all six agree today.

**Direct sites that stay at a fixed `Log`:**

* the `rewind=1` line of `TriggerRewindIfNeeded_Internal` (Chaos only). It is a resim request, a
  rare event, but it shares the `[ResimCheck.TriggerRewind]` tag with the per-step `rewind=0` lines,
  and the text that tells them apart is at the end of the line, where no prefix row can reach it.
* `[RelayDelayFloor]` and `[ConnectionTier]` (session and tier events, at most about 0.1 per second;
  `[ConnectionTier]` also in `SimmableUpdateComponent.cpp`). They have no row and need none.

**What an operator types now.** Per-tick and per-packet diagnostics need
`LogOGNet=Verbose` and `LogOGSimTick=Verbose` (on the command line,
`-ini:Engine:[Core.Log]:LogOGNet=Verbose`). At `Log` those categories carry only the rare lines. At
the committed `Warning` nothing changed. Lines read at `Verbose` are prefixed `Verbose: ` after the
category, and the capture extractors match on the tag anywhere in the line.

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

### Two blocks: og-simulation's rows first, then this module's (og-tickdriver task 12)

**`kOGLogRoutes` is now the concatenation of two tables.** The rows whose tags only og-simulation emits
moved into og-simulation as engine-free data: `kSimulationLogRoutes` in its `SimulationLogRoutes.h`, with
plain `char` prefixes, a `SimulationLogRoute` and a `HostLogLevel`. The rest stay in this header, in
`kOGGameLogRoutes`. `ogConcatenateLogRoutes` builds `kOGLogRoutes` at compile time: og-simulation's 25 rows
first, then this module's 14, each block in the order the single table had. `RouteOGMessage`,
`OG_LOG_ROUTED` and `ogLogRouteFor` read `kOGLogRoutes` as before; none of them changed, and nor did the
`.cpp`.

**Which rows are og-simulation's** was decided by the emitters: every string literal in a non-comment line
under `Plugins/` and `Source/`, with a leading `[Verbose]` or `[Warning]` token removed as `RouteOGMessage`
removes it, assigned to the row that routed it.
- og-simulation only (25 rows, now in `kSimulationLogRoutes`): every row not listed below. Their emitting
  files are listed in og-simulation's `SimulationLogRoutes-rationale.md` §5.
- Emitted by og-simulation **and** this module (kept here): `[ResimProbe` (also `ChaosTickBackendUImpl.cpp` and
  this module's `.cpp`), `[ServerReceive]` (also `SimmableUpdateComponent.cpp`) and `[RelayProbe` (also this
  module's `.cpp`, the PROBE 5 and 6 lines).
- Emitted by this module only (kept here): `[ResimCheck.TriggerRewind]` (`ChaosTickBackendUImpl.cpp`),
  `[ClientPrediction]` (`OGBrawlerInputCollectionComponent.cpp`), `[ReceiveCorrectionState]`
  (`SimmableUpdateComponent.cpp`), `[ReleaseBatch]` (this module's `.cpp`), `SimulationManager:` (this
  module's `.cpp`), `tryRegister:` (`BrawlerJoltBindingUImpl.cpp`'s capsule `checkf`, the Jolt host's `.cpp` until og-tickdriver task
  15a, and this module's `.cpp`) and `NewFramework:`
  (`SimmableUpdateComponent.cpp` and this module's `.cpp`).
- No emitter (kept here; deleting them was out of scope): `[SendRemoteInputToClients]`,
  `[ReceiveCorrectionInput]`, `[InjectCorrectionInput]`, `[DrainOutOfOrder]`.

**Why the reorder changes no route.** The single table interleaved the two kinds of row, so the
concatenation cannot equal it element for element. It is equal as a router instead. A line can match two
rows only if one row's prefix begins with the other's, because both begin the line. So if no prefix in one
block begins with a prefix in the other, a line matches rows of one block at most, and its first match is
the same whichever block comes first; within each block the order is the old one. The header checks that
at compile time:
- `ogLogRouteBlocksAreDisjoint`: no og-simulation prefix and no prefix of this module begin with one
  another, compared ignoring case as `FString::StartsWith` compares. New, and permanent: a later row added
  to either block is held to it.
- `ogLogRoutesAreAllReachable` and `ogLogRoutePlainLevelsAreLogOrVerbose`, unchanged, now run over the
  concatenation. og-simulation asserts its own block the same way.
- `ogLogRoutesAreAllMapped`: every og-simulation row's route has an `OGLogRoute` through `ogLogRouteOf`; an
  unmapped one would be `kOGLogRouteUnmapped`, which `RouteOGMessage`'s switch has no case for.
- `ogSimulationLogRoutePrefixesArePrintableAscii`: each og-simulation prefix is widened to `TCHAR` one `char`
  at a time (`kOGSimulationLogRoutePrefixes`, sized by `ogLongestSimulationLogRoutePrefix`), which is exact for
  ASCII only.

The two mappings live in one place each: `ogLogRouteOf` (`SimulationLogRoute` to `OGLogRoute`) and
`ogLogVerbosityOf` (`HostLogLevel` to `ELogVerbosity`).

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
`brawlerHitRouting::System::preIntegrate` (since og-netcode-v2-field-defects task 20; it was
`postIntegrate`), fired by the systems executor before `integrateAll` on every tick — including each
resim replay tick, whose first step now re-routes what the restored tick produced — so the routed
`HitFlinch` flags stay deterministic. There is no adapter-side routing wrapper any more: the former
`routeInboundHits()` shim and its map were removed once the routing system owned the whole pass, and
no reset-order hazard remains because the single routing pass *is* the system's `preIntegrate`.

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
`unregisterFromNewFramework`. The id is the pawn's replicated `SimCharacterId`, keyed through
`toStorageKey` (§15); before task 25 it was the component's per-process `GetUniqueID()`.

The per-id `deliver` callback answers "is this owner still alive", so the coordinator can drop a
stale claim — mirroring the retired drain's own `target.Get()==nullptr` prune, and routes a live delivery through `deliverRemoteInput`. **The liveness check stays
on the adapter side** because the drain's prune contract is a `bool` return while the sink method
itself returns `void` per the concept.

### The drain and the reap

The frame's game-thread work is three pieces (split by og-simulationscheduler-withjolt task 51, §18):

* `onFrameStepsDue_GameThread(firstUpcomingSimTick, numSteps)` holds PROBE A, then the coordinator
  test (`⛔G-70`), then PROBE 6 and `[PacketBudget]` (§8). Nothing in it releases input.
* `releaseDelayedInputsForStep(firstTick, numSteps)` is the drain only. It tests the coordinator
  itself, prints one `[ReleaseBatch] first=<tick> numSteps=<n>` line on `LogOGNet` at `Verbose` (the
  category and level of the coordinator's `[Release]` lines; its `kOGLogRoutes` row, §4), and calls
  `ServerReceptionCoordinator::releaseDelayedInputs` with the per-id callback.
* The reap, `ServerReceptionCoordinator::reapConnections`, is called by the host.

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
to read there is the `ChaosTickMapper`'s atomic offset — which this actor's Chaos host owns (a
`ChaosTickBackendUImpl` member since task 82) and
`SimulationNetSync` has no access to. Since task 51 the probe sits in `onFrameStepsDue_GameThread`,
which the Chaos configuration's `InjectInputs_External` calls with the mapper's tick (§7, §18). In the
Jolt configuration the tick driver calls it at `TG_StartPhysics` with its dispatch counter plus the
published offset J3 (§1), which is the mapper's value except that a client Skip or Stall's ±1 reaches
the probe one frame earlier (`BrawlerTickDriverGameUImpl-rationale.md` §4; the frame host until og-tickdriver
task 8); the line formats are
unchanged (og-simulationscheduler-withjolt task 54).

**Why this hook and not `OnPostPhysicsStep`, on either role:** that hook is game-thread on both
roles too, but it receives no physics step number (it was handed only an `FChaosScene` until task 51,
and now takes no argument), and therefore has no route to a sim tick through the only source safe on
that thread. Sampling there would have
required either reading the physics-thread-written clock or inventing a second counter, both ruled
out. This hook already has both numbers: the step, and Chaos's own sub-step count.

**Why reusing `firstUpcomingSimTick` is safe on the client** even though the `+1` derivation (`ChaosTickBackendUImpl-rationale.md` §4)
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

*Site: `onFrameStepsDue_GameThread`, below its coordinator test, once per physics frame (§7).*

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

**Where 4 came from, and what the same arithmetic gives now.** The binding constraint is the
**input guarantee**: all the remote characters' relay rings must fit one packet by themselves,
because a ring that gets scheduled out under redundancy 0 loses its whole staged burst with no
recovery path. In the test's own terms that is `44*SumE + 11*(N-1) <= 943 B`: 44 B is the relay
ring's entry stride (capture tick 4 + dA 1 + the 39 B `simulatableBrawler::SyncedPlayerInput`), 11 B
is each ring object's fixed cost (u16 length prefix, 2 B codec header, 7 B batch framing), and
943 B is the 952 B bunch less 9 B of per-packet overhead. Modelling a join as "the joiner's ring at
the stage cap (8 entries), everyone else at the measured average (1.132)", the largest N whose
join still fits is **11** (N = 11 needs 919.272 B, 32.728 B or 0.744 of an entry to spare); N = 12
needs 980.080 B and does **not** fit. The same arithmetic is asserted from inside the suite by
`Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp`'s pre-diet table. The
other half of the pre-diet configuration is `TimeConfig::correctionRotationK` (§3).

⚠ **R0, og-syncedInput-rework task 3 (2026-10-03).** This paragraph used to derive 4: it priced
the entry at `81*SumE` (the stride was 82 B by then) and concluded *"N = 4 clears that bound with
about nine tenths of one entry to spare and N = 5 does **not**"*. That was true of the six-slice
77 B input composite. Task 3 replaced it with the flat 39 B `SyncedPlayerInput` (stride 82 -> 44 B),
and the bound moved from 4 to 11. **`kPreDietCharacterCap` is retained at 4 anyway**: the constant
and its check are unchanged, and lifting the cap is unscheduled — og-netcode-v2-input-relay item
40, which owned it, was closed on 2026-10-03 without doing so. Until someone lifts it, 4 is a
tested-size statement rather than the packet-budget boundary.

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

**Lifting the cap means deleting both the constant and its check**, and their **absence** afterwards
is the "cap lifted" statement: there is no flag to flip and no value to raise, which is deliberate,
because a cap you can quietly widen is not a cap. (This was written as the wire diet's job,
og-netcode-v2-input-relay item 40; that item closed on 2026-10-03, so nobody owns it today.)

⛔ **The cap is coupled to `brawlerRingout::kMaxSpawnPoints`, and since task 11 the header asserts
it:** `static_assert(kPreDietCharacterCap <= kMaxSpawnPoints)`. The cap only WARNS — it allocates
nothing.
- The ring-out spawn table has exactly `kMaxSpawnPoints` entries. A character that registers while the table is full gets `SpawnSlotAllocator::kNoFreeSlot`.
- It then respawns with **no teleport seed**, logging one `[Warning][Ringout.spawnSlot]` per respawn.
- Its body is still below the kill plane, so it dies again on the next tick, every respawn delay: a death loop that feeds everyone else's score (T10 spike flag 1).
- A cap above the table would stop warning about exactly the characters that cannot be placed.

**Since og-brawler-uploadtosteam T18 (2026-09-29) the table is 8 and the cap stays 4.**
- The cap was the packet-budget number derived above (it is 11 since og-syncedInput-rework task 3, and the cap was kept at 4); the table is sized by a playtest ruling: 8 characters, and nobody refused for numbers.
- Characters 5-8 therefore get real slots and trigger the `[PreDietCap]` warning, which is the truthful "above the tested size" signal.
- The assertion still holds (4 ≤ 8) and still forbids the one dangerous direction: the cap above the table.
- The 9th concurrent character is back in the loop above. The engine allows up to 16 per server. *(The header's prose said
raising the cap alone "SILENTLY STOPS RESPAWNING EVERY CHARACTER PAST THE 4th". Both halves were
wrong — §11 C7.)*

⚠ **This heading carries `∴D-01`**, the derivation tag on `kPreDietCharacterCap`'s declaration.
Whoever deletes the constant deletes that tag's site, so they must retire `D-01` in
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
  holds `m_detachedRelayRing`, the *no-host read fallback*. A server write with no host lands in
  `m_detachedRelayStagingRing` instead (`SimmableUpdateComponent-rationale.md` §6 and §12; the
  header comment this line used to quote was removed in task 25's conversion and was itself false,
  F-25-5);
- the registration is **`DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing,
  COND_SkipOwner)`**. The old `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` stood
  on the component and was replaced; its absence is fenced by G-33 of
  `SimmableUpdateComponent-guards.md`.

A third clause was stale in a third way: it said this *writes* the ring, where the write path has
since become **staging** (`SimmableUpdateComponent-rationale.md` §6, "Staging, not writing").

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

Since this conversion there is **one** table per configuration (§13.3; since og-tickdriver task 15a
`BrawlerChaosBindingUImpl::categoryChannels` and `BrawlerJoltBindingUImpl::categoryChannels`), so a new
category needs one row in each, not one per role branch.

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
(ids are authority-assigned `SimCharacterId`s and never reused, §15). A re-seed per call would therefore just return the same
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

*Call site by configuration: `OnPostPhysicsStep` is called by the scene's post-tick delegate in the
Chaos configuration and by the frame host's `TG_EndPhysics` tick function, through the tick driver, in the Jolt one, at the same
point of the frame (after the wait for earlier frames' steps) and every frame. On the authority every
step has run inline before it, so the push reads a score table no step is writing; the argument below
is unchanged (task 54).*

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

Verified: `createPhysicalObject(D::descriptor(), D::name)` is the factory call in `tryRegister`.
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

### 13.8 The capacity pin's word rounding — `(usableBits / 32) * 4` ∴D-50

*The `[PacketBudget]` line in `onFrameStepsDue_GameThread` (in `releaseDelayedInputsForStep` until
task 51's split, §7).*

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
| the two collision tables and their "KEEP IN SYNC" | one table, emplaceBrawlerQueryAdapter, called from both branches (since og-tickdriver task 15a the Chaos binding's `BrawlerChaosBindingUImpl::categoryChannels`) | nothing left to poison: the two copies no longer exist |
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

## §14 The first replayed step — the restored body state is written, not targeted

Moved to `ChaosTickBackendUImpl-rationale.md` §5 (og-simulationscheduler-withjolt task 82), with the code it describes.

---

## §15 The simulation character id — assigned here, replicated on the pawn (task 25)

**What changed.** Until task 25 every id in this file was the registering component's
`GetUniqueID()`, a per-process number: the same character was 42357 on a client and 39920 on the
server. Since task 25 the id is a `SimCharacterId` (og-brawler, `SimCharacterId.h`). The
authority's manager assigns it from `m_simCharacterIds`, the pawn replicates it, and every
peer registers under the same number. `tryRegister`, `noteDelayedInputComponent` and
`unregisterFromNewFramework` take a `SimCharacterId`, so an engine object id passed to them is a
compile error. Inside, `toStorageKey` turns it into the `unsigned int` key every container already
used. No container changed type. There is no wire change, because the id rides the pawn's
replication, not the correction composite.

**`allocateSimCharacterId`** is the only call that issues one. It `checkf`s the world-level net
mode, so a client world's manager can never assign. It returns `SimCharacterId::None` once the
allocator is exhausted, after logging a `LogOGMgmt` Error and failing an `OG_CHECK`. That check is
fatal in Development and compiled out in Shipping, where the character simply does not register
(guard G-78; the permanence rule itself is G-01 of `SimCharacterId-guards.md`).

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

## §16 The latency-budget probe — stamp sites, threads and errors (og-simulationscheduler-withjolt task 1)

**What it is.** A measurement of today's Chaos build, per hop, that the scheduler migration is
compared against. The engine-free instrument is `latencyBudget::LatencyBudgetProbe`
(`OGSimulation/LatencyBudgetProbe.h`; its own rationale explains keys, joins, counters, windows and
percentiles). This section is the host side: where each hop is stamped, on which thread, and how far
each stamp can sit from the event it names. **It changes no gameplay behaviour, timing or netcode:
every addition is a timestamp and a counter.**

### Ownership

Each manager instance owns one probe, created in `BeginPlay` by `createLatencyBudget` with the role's
hop mask (`kServerHops` on the authority world, `kClientHops` on a client world), plus two
single-producer mailboxes: `m_latencyMailbox` (stamps) and `m_latencyClockMailbox` (the prediction
clock's event counts). All three are created before the solver callback is registered and are never
reset before the actor is destroyed, so a physics step can never reach a null or freed mailbox. PIE
in one process has two instances and two probes; the `role=` field tells their lines apart.

### Threads — the probe adds no unsynchronized crossing

The probe itself is **game-thread-owned** and has no internal lock. Everything reaches it one of
three ways:

| path | used by | synchronization |
|---|---|---|
| `stampLatency` / `noteLatencyWireSample` | every game-thread site | same thread as the owner; both `checkf` that they run on the game thread |
| `postLatencyEvent_Internal` → `m_latencyMailbox` → drained in `tickLatencyBudget_GameThread` | every physics-thread site | the mailbox's release/acquire pair (`LatencyBudgetProbe-guards.md` G-01) |
| `m_latencyClockMailbox` | the client clock's skip/stall/hard-resync counts | the same mailbox type |

Every physics-thread read a stamp makes is **same-thread with its writer**: `m_storage`'s per-step
walk (the core walks it on that thread every step), `getLastUsedCaptureTick` (written by
`collectInputAll` on that thread), the relayed-read observation ring (noted by `collectInputAll` on
that thread), `currentIntegratedTick` and the client clock's diagnostics (both advanced by
`onGameSimulation` on that thread). Every game-thread read is same-thread as well: the correction
buffer's tick (written by `sendCorrectionAll` inside `onPostSimulationGameThread`),
`m_delayedInputComponentsById`, the scene's network delta-time scale (written by the engine's
time-dilation client RPC) and the local player controller's physics tick offset (written by the
engine's timestamp-setup client RPC). The one value that crosses from the game thread to the physics
thread, the input-sample time, is a `std::atomic<double>` on the input collection component.
**The §1 CROSSING count stays three**, and this paragraph is the argument for it, made rather than
assumed.

On a dedicated server the physics thread *is* the game thread (Chaos steps inline), so the mailbox
is pushed and drained by one thread. That is a legal degenerate case of single-producer,
single-consumer.

### Why the capture stamp is a wrapper in `tryRegister`, not a line in the provider lambda

`USimmableUpdateComponent`'s provider lambda may not capture the manager (its guard G-05). The
capture stamp therefore lives in `wrapInputProviderWithLatencyStamp`, which `tryRegister` applies to
a client's provider before handing it to the core. The wrapper calls the component's lambda, then
posts `InputSampled` and `Captured` for `step.getTick()`. It captures `this`, the manager that owns
the core container the callable is stored in, so it adds no lifetime edge to another actor, and the
same input-collection pointer the inner lambda already holds. **An empty provider is returned
unchanged**: provider presence is the core's local-versus-remote identity test, and a `checkf`
states that the wrapper never changes it.

### The hop table: stamp sites and known measurement error

Times are `FPlatformTime::Seconds` (one monotonic clock per process). "GT" is the game thread and
"PT" the physics thread (a task-graph worker on a client, the game thread on a dedicated server).

| hop | start: file · function (thread) | end: file · function (thread) | known error |
|---|---|---|---|
| `H1` | `OGBrawlerInputCollectionComponent.cpp` · `updateGameThreadCache`, first statement after the owner check (GT) | `SimulationManagerUImpl.cpp` · the wrapper from `wrapInputProviderWithLatencyStamp`, after the provider returns (PT) | Starts at the frame's input refresh, not at the device event: Enhanced Input dispatched the frame's events earlier in the same frame, and the OS queued them up to one frame before that, so the true input age is **under**-stated by up to one game frame. The sample time is a relaxed atomic, unordered with the caches the capture reads, so a capture racing a refresh can pair a new time with the previous frame's values: an error of at most one frame, rare. |
| `H2` | the same wrapper, `Captured` (PT) | `SimmableUpdateComponent.cpp` · `sendLocalInputToAuthority`, after `ServerReceiveRemoteMove(bundle)`, once per capture tick in the bundle, first send wins (GT) | Microseconds. Redundant re-sends of the same capture are duplicates and do not move the hop. |
| `H3` (client) | `sendLocalInputToAuthority` (GT) | `SimulationManagerUImpl.cpp` · `onLatencyPostTickFlush`, bound to `UWorld::OnPostTickFlush` (GT) | See *the `H3` hook* below. |
| `H3` (server) | `SimmableUpdateComponent.cpp` · the relay host's flush callback bound in `attachInputRelayHost`, fired from `ASimulationInputRelay::PreReplication` after a flush published entries (GT, inside the net driver's flush) | `onLatencyPostTickFlush` (GT) | Measures the rest of that flush after this relay's pre-update. The same send caveats as the client row. |
| `H3c` | `SimulationManagerUImpl.cpp` · `tickLatencyBudget_GameThread`, when a character's correction-buffer tick changed since the last frame (GT, end of `OnPostPhysicsStep`) | `onLatencyPostTickFlush` (GT) | A property leaves only when the replication system polls and sends it; a correction written but not sent at this flush still reads as left. A **lower** bound. The start is late by the frame's `updateVisualizationAll` work. |
| `H4` | `SimmableUpdateComponent.cpp` · `ServerReceiveRemoteMove_Implementation`, after the version fence, once per capture tick in the bundle, first arrival wins (GT, inside the net driver's receive dispatch) | `SimulationManagerUImpl.cpp` · `stampLatencyAfterStep_Internal`, for the capture tick `getLastUsedCaptureTick` reports after the step, stamped with that step's start time (PT) | The datagram reached the socket before the frame's receive dispatch: up to one server frame **under**-stated. An underrun step reports the sentinel and stamps nothing. |
| `H4r` | as `H4` | the relay flush callback (as `H3` server) | The hand-off is the flush into the replicated ring, not the staging write at receipt: staging is synchronous with receipt and would read zero. |
| `H5` | `stampLatencyStepEnd_Internal`, from `OnPostSolve_Internal`, per character, for `currentIntegratedTick` (PT; inline on a dedicated server) | as `H3c` start | `noEnd` counts the ticks a character's state was not written: the correction rotation (`CorrectionRotationK`) skips a character on purpose, so on a rotation of 2 about half the ticks have no end. |
| `H6` | `SimmableUpdateComponent.cpp` · `onRelayedInputRingArrived`, once per capture tick in the ring, first arrival wins (GT, from the ring's OnRep) | `stampLatencyAfterStep_Internal`: the relayed-read observation for the step's tick names the applied capture tick, stamped with the step's start time (PT) | The bytes arrived up to one client frame before the OnRep. Only reads that applied a relayed capture stamp an end; a miss stamps nothing and the capture finalizes as `noEnd`. |
| `H6c` | `SimmableUpdateComponent.cpp` · `OnRep_CorrectionState`, for the peeked tick (GT) | `stampLatencyAfterStep_Internal`: a pending stamp at the start of the first client step after receipt (PT) | Measures when the correction is available to the next step, not when a resimulation applies it. |
| `H7` | `stampLatencyStepEnd_Internal` (PT) | `tickLatencyBudget_GameThread`, after `updateVisualizationAll`: the newest pending tick per character only (GT) | Not the photon. The engine interpolates the physics proxies roughly two steps behind on the game thread, and the render thread and GPU add one or two frames more. A step that ends between the mailbox drain and this stamp is stamped one frame late. On a dedicated server it measures the per-frame visualization pass, which draws nothing. |
| `WIRE` | client: `onTimingInfoReceived`, the raw round trip the timing relay hands `NetworkTimeEstimator::updateRTT`, halved (GT) | — | Half of a round trip assumes symmetric paths. Server: `ServerReceiveRemoteMove_Implementation`, `readRoundTripMs` of the root connection, halved, once per bundle (GT). The two roles' values come from different estimators. |

**H1 and H2 are measured for a remote client's own characters only.** A listen host's local players
have no client provider, so the host role has no capture path to stamp.

### The Jolt configuration's stamp sites (og-simulationscheduler-withjolt task 54)

The hop table above names the Chaos configuration's step-side sites. In the Jolt configuration "PT" is
the step's thread (the tick driver's task on a client, the game thread inline on the authority), every
game-thread site is unchanged, and the step-side sites move to the step hooks (design D9, D18):

| stamp | Chaos configuration | Jolt configuration |
|---|---|---|
| step-start time for `H4`, `H6`, `H6c` and the clock counts | `OnPreSimulate_Internal`, immediately before `onGameSimulation` | og-simulation's `TickDriverStepHooks` `beforeSimulate`, after any replay and immediately before the normal step (`TickDriverStepHooks-guards.md` G-03, the successor of the step hooks' retired G-03); on a client early only by the Jolt scratch save |
| `stampLatencyAfterStep_Internal(start)` | `OnPreSimulate_Internal`, after `onGameSimulation` | `beforePhysics`, after `onGameSimulation`, before the physics step |
| `stampLatencyStepEnd_Internal` (`H5`, `H7` starts) | `OnPostSolve_Internal` | `afterTick`, after the post-step pass and the ring save: microseconds later |

Both configurations take the start time at the same instant, so gate 22 compares them like for like.
`[ChaosDilation]` is the Chaos configuration's only (§18).

### The `H3` hook

`UWorld::OnPostTickFlush` is broadcast by the world's tick immediately after the net drivers'
`TickFlush`, and it is the earliest engine hook after it. The actual `SendTo` runs inside
`TickFlush` (the connection's flush of its send buffer), so the stamp is late by whatever
`TickFlush` did after this connection's send: sub-millisecond with one connection, longer on a busy
server. It is early, or simply wrong, whenever the message did not leave at this flush:

* a client rendering faster than its `MaxNetTickRate` (120) skips a connection tick, so the packet
  leaves at the next frame;
* a saturated connection or the bandwidth limit defers the bunch;
* `net.IpConnectionUseSendTasks=1` (default 0) moves the `SendTo` onto a task after the flush;
* a replicated property (the correction, the relay ring) is sent only when the replication system
  polls it, so a write can wait a poll period.

In each case `H3` reads as though the message left this frame. Task 2's baseline should read `H3` as
a lower bound.

### The `[ChaosDilation]` line (client only)

Per window, from `tickLatencyBudget_GameThread`, in the Chaos configuration only (§18). Since task 82 the
Chaos host samples and logs it, through `sampleLatencyWindow_GameThread` and `logLatencyWindow_GameThread`:

* `min` / `mean` / `max` / `samples`: the physics scene's network delta-time scale, sampled once per
  game frame from the world's physics scene (`GetPhysicsScene`, an `FChaosScene`;
  `FChaosScene::GetNetworkDeltaTimeScale`, beside the setter the engine's time-dilation client RPC
  calls). It is the engine's physics time dilation, active today. Until task 51 the same scene
  arrived as `OnPostPhysicsStep`'s argument: the scene's post-tick delegate passes the scene that
  broadcasts it, which is the world's one physics scene.
* `resets` / `offset`: how many times the first local player controller's network physics tick
  offset changed in the window, and its current value. The engine's timestamp-setup client RPC
  writes that offset. ⚠ A reset to the **same** value is invisible to this poll, and only the first
  local player controller is read.
* `skips` / `stalls` / `hardResyncs`: the window's increase in `ClientPredictionClock`'s Skip, Stall
  and HardResync counts, read on the physics thread and carried by `m_latencyClockMailbox`. A client
  that joins mid-session shows one hard resync in its first window.

### Log lines and categories

```
[LatencyBudget] role=Client hop=H2 p50=0.131 p95=0.273 p99=0.308 max=0.397 n=601 unit=ms noStart=0 noEnd=0 outOfOrder=0 overflow=0
[LatencyBudget.Window] role=Client seconds=10.01 stale=0 duplicate=2402 laneOverflow=0 mailboxDropped=0
[ChaosDilation] role=Client min=0.9762 mean=0.9998 max=1.0237 samples=1000 resets=0 offset=537 skips=0 stalls=0 hardResyncs=0
```

One `[LatencyBudget]` line per enabled hop and one `[LatencyBudget.Window]` line per window, both
under **`LogOGLatencyBudget`**; the `[ChaosDilation]` line under its own **`LogOGChaosDilation`**
(one category per probe family, §4). All three are `Warning`. They are logged with `UE_LOG` directly
from this class (the `[ChaosDilation]` line from its Chaos host since task 82), never as core SIMLOG strings, so `RouteOGMessage` has no arm for them and its route
table is unchanged. `Config/DefaultEngine.ini` pins `LogOGLatencyBudget=Warning`;
`LogOGChaosDilation` has no ini line and runs at its declared default (`Log`), which prints its
`Warning` line. `duplicate` counts the copies first-stamp-wins discarded (an input re-sent in
several redundant bundles, a capture carried by more than one relay round), so a large number there
is the expected state, not a fault.

### Verified, and what was not

A headless run (one `-server` process and two `-game -nullrhi` clients on the ThirdPerson map, 2026-10-06)
printed every enabled hop on both roles, with `stale=0` and `mailboxDropped=0`. Representative
p50s: client `H6` 90 ms and server `H4` 149 ms (the relay-delay floor and the input delay, as
expected), `H5`'s `noEnd` about half its ticks (the rotation), and the dilation scale moving between
0.976 and 1.024. A headless client is smoother than a rendering one, so these numbers are wiring
evidence, not a baseline; the baseline is task 2's.

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
  This class's adapter members (`m_physAdapter`, `m_physReaderAdapter`, `m_queryAdapter`), its two visualization
  doors and its `[SimHost.Backend]` line name them through `EngineTraits`. Until task 15a the header held the
  adapter aliases, the name and the fingerprint itself, per configuration, in namespace `physicsBackendUImpl`; task 15a
  kept six of those names for the files outside it, and task 15b switched those files (`BrawlerCompositionUImpl`'s
  `IntegrationExecFor` and `HitDetectionSystem`, `SimulationManagerUImplConceptTest.cpp`, the build identity and
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
  and `m_backend`, directly after `m_manager` (§2); and `friend physicsBackendUImpl::Backend;`, which befriends the
  configuration's one host (since og-tickdriver task 15b; it replaced one `friend class` line per host), and the
  class-template friend `BrawlerTickDriverGameUImpl` (which replaced the frame host's friend line in og-tickdriver
  task 8), in both configurations.
* **`.cpp`:** at each seam call site (the table below), one unconditional `m_backend.` call.

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
`m_latencyClockAtWindowStart`, for a reason `ChaosTickBackendUImpl-rationale.md` §2 gives.

The guards on that code moved with it. This class's G-02, G-51 to G-55, G-71, G-76 and G-77 are retired
(`SimulationManagerUImpl-guards.md` §R) and are `ChaosTickBackendUImpl-guards.md` G-01 to G-09.

`TickDriverBackendUImpl` holds what the Jolt arms held before task 83: the Jolt includes, console variables and
anonymous helpers; the stepping mode, the world build, the slot table, the bind and the release; the frame host,
the hooks and the step driver (since og-tickdriver task 8 the frame host, the game binding and the tick driver); the render publish, interpolation, correction smoothing and apply; the shadow
world's hand-over, restore and overwrite; and the contact counts. Its world state is `TickDriverBackendWorldsUImpl`,
which this class holds as `m_backendWorlds`. The guards moved too: this class's G-80 to G-84 are retired
(`SimulationManagerUImpl-guards.md` §R) and are `TickDriverBackendUImpl-guards.md` G-01 to G-05. Since og-tickdriver tasks 9
and 10 the render half is og-simulation's presenter, and the stepping mode, the world build, the slots, the bind, the
release, the shadow world and the contact counts are og-simulation-jolt's world host `JoltTickWorldHost`, held in
`TickDriverBackendWorldsUImpl`; of those guards only G-01 is live (`TickDriverBackendUImpl-guards.md` §R names the successors).

### The seam (task 82)

This class calls its backend through one fixed set of entry points. Each is called from exactly one place, the
site where the arm sat, with no `#if`. Task 82 implemented the entry points that have a Chaos body; task 83 moved
the Jolt column into its own host and gave every entry point a body on both hosts: an empty inline body where the
column says "empty". The set is checked at compile time: the concept `PhysicsBackendHost`
(`PhysicsBackendHostUImpl.h`) lists every entry point with its signature, and `compositionContractsHold()`
asserts `PhysicsBackendHost<physicsBackendUImpl::Backend>`, as the selector does after it picks the host (since
og-tickdriver task 15b).

| entry point | call site in this class | Chaos body (`ChaosTickBackendUImpl`) | Jolt body (`TickDriverBackendUImpl`) |
|---|---|---|---|
| `buildWorld` | `BeginPlay`, after the ini reads, before the role branches | the solver checks, then the adapter emplacement (both moved, `ChaosTickBackendUImpl-rationale.md` §2) | the static import, then the world host's `build` |
| `startStepping` | the end of `BeginPlay` | the post-tick delegate, the async callback, the `InjectInputsExternal` delegate | `startDriverStepping` |
| `onEndPlayBegin` | the first statement of `EndPlay`, and it stays first (`⛔G-85`) | empty | unregister the tick functions and wait for outstanding steps (`TickDriverBackendUImpl-guards.md` G-01) |
| `onEndPlayWorld` | `EndPlay`, inside the world test, after the latency handle | remove both delegates, free the callback | empty |
| `beforePresent_GameThread` | `OnPostPhysicsStep`, before the post-simulation pass | empty | the presenter's `present_GameThread` (og-simulation's `TickDriverPresenter`) |
| `updateVisualization_GameThread` | `OnPostPhysicsStep`, after the timing buffer | `updateVisualizationAll` | the presenter's `updateVisualization_GameThread` |
| `editVizQuery`, `getVizReader`, `registerVizVolume` | the three public doors of the same names, which keep their signatures and forward | the query adapter, the reader, the volume with the owner ignored | the step world or the shadow world, by stepping mode |
| `createBodies_GameThread` | `tryRegister`'s first pass; `false` returns `TryRegisterStatus::Pending` | the `ChaosPhysicsFactory` bind; returns `true` | the slot acquire and the Jolt bind; `false` where it waits for a slot |
| `onRegistered_GameThread` | `tryRegister`'s Ready path | empty | the occupancy push and the render target |
| `onUnregistered_GameThread` | `unregisterFromNewFramework` | empty | the render target and the slot release |
| `sampleLatencyWindow_GameThread` | `tickLatencyBudget_GameThread`, before the window closes | the dilation scale and the tick-offset poll | empty |
| `logLatencyWindow_GameThread` | `tickLatencyBudget_GameThread`, after `[LatencyBudget.Window]` | the `[ChaosDilation]` line and its reset | empty |

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
   two files had arms until then). `USimmableUpdateComponent` and
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

* `OnPostPhysicsStep` takes no argument. The Chaos configuration binds the scene's post-tick delegate
  to a weak lambda that drops the scene and calls it. The `[ChaosDilation]` block reads the scene
  from `GetWorld()` instead (§16). The two empty physics-scene hooks (pre-tick and step) and their
  bindings are deleted (§11 C20).
* **The split** of the frame's game-thread work into `onFrameStepsDue_GameThread`, the drain and the
  reap (§7).
* **`[ReleaseBatch] first=<tick> numSteps=<n>`**, one line per drain call, on `LogOGNet` at `Verbose`
  (its `kOGLogRoutes` row, §4), after the drain's coordinator test and before it drains. The committed
  `LogOGNet=Warning` hides it, and so does `LogOGNet=Log`. In the Chaos configuration there is one per physics frame, with `n` equal to the frame's step
  count; the Jolt host will print one per step with `n = 1`. Every `[Release]` line's `releaseTick`
  lies in `[first, first + n)` of the nearest `[ReleaseBatch]` before it. `[Release]` prints
  absolute ticks, which differ between runs, so the two backends are compared by distributions,
  never by lines (gate 22).
* **`og.Sim.TestGameThreadHitchMs`** (not in Shipping): when it is above 0, the next
  `OnPostPhysicsStep` sleeps the game thread that many milliseconds, once, and sets the variable back
  to 0 at console priority (`ECVF_SetByConsole`), so a value set from the console or `-ExecCmds` is
  reset. It forces a catch-up frame: on an inline-stepping server, 250 ms gives a frame of about 15
  steps, which shows the release pattern in one `[ReleaseBatch]` line. The sleep sits at the top of
  the shared `OnPostPhysicsStep`, the one hitch site for both hosts.
* **`editVizQuery()`, `getVizReader()` and `registerVizVolume(descriptor, owner)`** replace
  `editQueryAdapter()` and `getPhysicsBodyReaderAdapter()`, and the unused body-adapter accessor is
  deleted, so no code outside this class can name a body adapter. The first two return references to
  a world every peer's game thread may query (`SimmableUpdateComponent-rationale.md` §10; task 53 made
  them pointers, null on a worker client, until task 56 gave that client its shadow world). In the Chaos configuration
  they always return the Chaos query adapter and reader; `registerVizVolume` builds the query
  parameters the component used to build itself (simple collision, the owner ignored) and registers
  the volume on the query adapter. It returns an optional volume id: always set in the Chaos
  configuration, empty in the Jolt configuration while the owner's slot acquire waits (task 66, the slot
  table, `TickDriverBackendUImpl-rationale.md` §3). They are handles, an exception to §1's narrow-passthrough rule that
  they inherit from the accessors they replace: the two visualizers are templates over the adapter
  types and call them directly, on the game thread. The Jolt configuration's answers are in
  `TickDriverBackendUImpl-rationale.md` §3.
* **The backend token.** The build identity builds its token from `EngineTraits::kName` (kBackendName until og-tickdriver task 15b) and the fingerprint:
  `chaos`, or `jolt:<fingerprint>`. The server refuses a login whose backend differs from its own, with a
  reason that names both, and admits a different Jolt fingerprint with a Warning (`OGBuildIdentity-rationale.md` §7).
* **`LogOGSimHost`** and its one-shot `Warning` line at `BeginPlay`:
  `[SimHost.Backend] backend=chaos fingerprint=none` in the Chaos configuration, and
  `[SimHost.Backend] backend=jolt fingerprint=<16 hex digits>` in the Jolt one, so every log names the
  configuration it came from. The fingerprint is the build identity's (`backendFingerprint`), computed
  once per process. `Config/DefaultEngine.ini` pins the category at `Warning`, beside
  `LogOGLatencyBudget`. Since og-tickdriver task 11 the category is declared, exported, in OGSimulationUnreal's
  `OGSimulationUnreal.h` and defined in its `.cpp`, under the same name, so that ini row still governs it; this
  class's header includes that header where its own declaration of the category was, so every file that saw the
  category through it still does, in both configurations. In the Chaos configuration it is the category's only `Warning` line; the Jolt
  host adds the lines `TickDriverBackendUImpl-rationale.md` §2 lists, its window line `[SimHost.Window]` among them (§8).
* **`dt`** is the engine physics setting AsyncFixedTimeStepSize in both configurations, `checkf`-ed at
  `BeginPlay` to be one tick of `UEBrawlerNetConfig::tickFrequencyHz`. It is the number Chaos steps
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
