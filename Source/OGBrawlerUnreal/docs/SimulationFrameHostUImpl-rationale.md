<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `SimulationFrameHostUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/SimulationFrameHostUImpl.h` and
`Source/OGBrawlerUnreal/SimulationFrameHostUImpl.cpp` (og-simulationscheduler-withjolt task 54; design D
rev 6, D1–D10, D15, §2.1–§2.4). Prohibitions: `SimulationFrameHostUImpl-guards.md`. The step hooks it
serves are `BrawlerStepHooksUImpl.h` (`BrawlerStepHooksUImpl-rationale.md`); the manager that owns it is
`SimulationManagerUImpl` (`SimulationManagerUImpl-rationale.md` §18).

---

## 1. What it is

The frame host steps the Jolt configuration's simulation in the shape Chaos's async mode gave the Chaos
configuration: the same tick groups, the same wait rule, the same per-frame cadence for the probes and
the reap, and the same roles on the same threads (design D1).

| Chaos (async, block mode 0) | the frame host |
|---|---|
| the engine's start-physics tick function at `TG_StartPhysics` advances the scene, computes the frame's step count from accumulated world time, and dispatches the steps | `FSimulationStartStepsTickFunctionUImpl` at `TG_StartPhysics`: `startSteps_GameThread` pumps the scheduler and runs or dispatches the frame's batch |
| client: the steps run as chained task-graph tasks after the previous frame's; dedicated server: inline on the game thread | client: one task per frame, chained after the previous frame's, at the physics task priority; authority (and a client under `og.Sim.RunInline=1`): inline |
| the end-physics tick function at `TG_EndPhysics` waits for the steps dispatched in **earlier** frames, then fires the scene's post-tick delegate | `FSimulationEndStepsTickFunctionUImpl` at `TG_EndPhysics`, with the start function as prerequisite: `endSteps_GameThread` waits for earlier frames' task, then calls the manager's `OnPostPhysicsStep` |
| `InjectInputs_External`, once per frame with steps: PROBE A, PROBE 6, the drain of the whole batch, the reap | once per frame with steps: `onFrameStepsDue_GameThread` (PROBE A, PROBE 6) before the steps and the reap after the inline steps; the drain moves to each authority step's `beforeTick` (design D3) |

The host is a plain C++ class. The two tick functions are plain `FTickFunction` subclasses, not
reflected structs (the engine has the same pattern in the pose-search interaction island), so nothing
here needs UHT. The `.cpp`'s whole body is one `#if !OG_PHYSICS_BACKEND_CHAOS` arm, and the header has
no arm and is included only from the manager header's Jolt arm and this `.cpp` (design §2.6 rules 1
and 2): UBT compiles every `.cpp`, and in the Chaos configuration this one compiles to nothing.

**Ownership.** The manager holds the host by value (`m_frameHost`), then the hooks (`m_stepHooks`),
then the step driver (`m_stepDriver`), all after `m_manager`; `compositionContractsHold` asserts that
order, so destruction runs driver, hooks, host, manager. The host calls back into the manager
(`onFrameStepsDue_GameThread`, the drain, `OnPostPhysicsStep`, the per-step body `runJoltStep_Step`,
the driver's `noteOccupancy`), which is why it is a class with a `.cpp` that includes the manager
header rather than a header-only template: the manager is incomplete where it declares the host. The
manager names it a friend for those private calls.

## 2. The frame, step by step

**`TG_StartPhysics`, `startSteps_GameThread(DeltaTime)`, game thread.**

1. `m_blockingSteps = m_pendingSteps`: the task dispatched in earlier frames, captured before this
   frame's dispatch, as Chaos captures its pending tasks. Every frame, steps or not.
2. `m_hostTimeSeconds += DeltaTime`, then `numSteps = m_scheduler->pump(m_hostTimeSeconds)`
   (`⛔G-04`, §4). No step due: return.
3. The batch (J4, §5), by value: `first = physicsStepCount() − numSteps`, the count, and each step's
   deadline from the scheduler. The step code never touches the scheduler, which stays a game-thread
   object in M1.
4. `firstUpcomingSimTick = first + J3 offset` (`⛔G-03`, §5).
5. `onFrameStepsDue_GameThread(firstUpcomingSimTick, numSteps)` (`⛔G-02`): PROBE A on both roles, and
   on the authority PROBE 6 and the one-shot `[PacketBudget]` line (manager rationale §8).
6. Inline (`⛔G-01`): `runBatch` on this thread, then, when the manager has a reception coordinator
   (the authority), `reapConnections(firstUpcomingSimTick)` once (`⛔G-06`).
   Worker: `FFunctionGraphTask::CreateAndDispatchWhenReady` with the previous task as prerequisite (when
   it has not completed) at `TaskGraph.TaskPriorities.OGSimStepTask`, a console task priority with the
   same defaults as Chaos's TaskGraph.TaskPriorities.PhysicsTickTask (high-priority task thread,
   normal task priority; high task priority on a normal thread when there are none). It becomes
   `m_pendingSteps`.

`runBatch` calls the manager's `runJoltStep_Step(first + i, deadline[i])` for each step in order. That
function takes the world mutex J2 and sets the step flag (§6), hands the deadline to the hooks
(`currentStepDeadline`, which the render publish in `afterTick` copies into the snapshot), and runs
`driver.runTick`.

**`TG_EndPhysics`, `endSteps_GameThread(MyCompletionGraphEvent)`, game thread.**

1. The task to wait for is `m_blockingSteps`, or `m_pendingSteps` under `og.Sim.BlockMode=1` (debug:
   this frame's task too).
2. Nothing to wait for, or already complete: call `OnPostPhysicsStep()` at once.
3. Otherwise hold the tick group open until it completes and then run `OnPostPhysicsStep()` as a
   game-thread task: `MyCompletionGraphEvent->DontCompleteUntil(...)`, the engine's end-physics
   pattern. The game thread keeps running other game-thread work meanwhile; the next tick group starts
   only after `OnPostPhysicsStep` has run. The task holds the manager through a weak pointer.
   (A tick function run without a completion event, which the engine does only for tick-when-paused
   functions, waits synchronously instead; these two do not tick when paused.)

`OnPostPhysicsStep` runs every frame, with or without steps, as the scene's post-tick delegate did,
so the correction rotation still advances once per game frame. Its body is the shared one (manager
rationale §13.5 and design §4.2): the sends, the timing-relay write, the visualization copy, the
ring-out score push, the latency probe, preceded by the render apply (manager rationale §18, "The
Jolt arm's render publish and apply"), and on a worker client by the shadow-world restore before that
(manager rationale §18, "The Jolt arm's game-thread shadow world").

## 3. One task per frame, and block mode 0

Chaos dispatches two or three chained tasks per step; the host dispatches one per frame that runs the
frame's steps back to back. The steps still run in order, each after the previous frame's, so the
difference is task overhead only.

Block mode 0 means a client's `TG_EndPhysics` waits only for steps dispatched in earlier frames. The
current frame's task normally runs through the during-physics tick group and the rest of the frame, so
`OnPostPhysicsStep`'s sends read live state while it may be stepping. That is today's exposure on
Chaos (design B4; M2's X5) and unchanged here. On an inline host every step of the frame has run
before `TG_EndPhysics`, so there is never anything to wait for.

## 4. Time base and catch-up cap

**Time base (design D6).** The scheduler is pumped with the sum of the tick function's `DeltaTime`,
the world's delta seconds, exactly what Chaos accumulates. A paused world runs no tick functions and
accumulates nothing; slomo and the world's per-frame clamp reach the simulation as they did.
`m_hostTimeSeconds` starts at 0 with the scheduler.

**Catch-up cap (design D5).** `og.Sim.MaxCatchUpSteps`, read once in `begin`, default **60 on both
roles**: Chaos's one-second clamp at 60 Hz, which applies to every role. The scheduler drops the rest
of a longer frame and counts it as lost time. The value is clamped to `[1, kMaxStepsPerFrame]`
(240), the size of the batch's deadline array, with a Warning line when the console value was out of
range. The scheduler's rate bounds are 0.75–1.25 (task 5's note for the dilation port, task 38).

**Lines.** `begin` prints one `[SimHost.Frame] role=<Authority|Client> stepping=<inline|worker> dt=<s>
maxCatchUpSteps=<n> rateScale=[0.75,1.25] timeBase=worldDelta` Warning line on `LogOGSimHost`. The
first batch prints one `[SimHost.FirstStep] role=<…> thread=<game|worker> physicsStep=<n> batch=<n>`
Warning line from the thread that runs it, so a log shows where the steps run.

## 5. J1, J3 and J4

These replace Chaos's implicit marshalling. None is a new crossing between og-simulation structures;
they are listed in the manager rationale's crossing table (§1), not in og-simulation's threading
crossings document.

* **J1, occupancy commands** (game thread → step). `SpscRing<OccupancyCommandUImpl, 64>`. The manager
  pushes `Occupy{slot}` after a character's `registerSimulatable` on `tryRegister`'s Ready path and
  `Vacate{slot}` when it releases the slot (`pushOccupancy_GameThread`); a full ring is a `checkf`
  (a lost command leaves the world stepping the wrong body set). The hooks drain it first thing in
  `beforeTick` into the driver's `noteOccupancy`, so occupancy is never applied from the game thread
  (manager G-81). One producer, the game thread; one consumer, whichever thread runs the step, and the
  steps are serialized by the task chain.
* **J3, the published tick offset** (step → game thread). `std::atomic<int64_t> m_tickOffset{0}`.
  `afterTick` stores `outcome.tick − outcome.physicsStep` (relaxed); the game thread loads it (relaxed)
  and adds its own dispatch counter `first`. It reproduces the Chaos tick mapper's semantics on driver
  data (design §2.4): `TickOutcome::tick` is the tick the step integrated, so the frame's first upcoming
  tick is `first + offset` with **no `+ 1`**, and the series advances by the dispatched step count
  whatever the worker's progress. The one residual difference: the mapper took its offset before a step
  moved the clock, the driver after, so a client Skip or Stall's ±1 reaches PROBE A one frame earlier.
  Before the first `afterTick` the offset is 0 and PROBE A seeds on its first sample, as with the
  mapper's initial 0.
* **J4, the batch** (game thread → task). First step, count and deadlines, copied into the task by
  value.

## 6. Threads, the world mutex, and no deadlock

* The tick functions, the scheduler, `m_pendingSteps` / `m_blockingSteps`, `begin`, the waits and
  `pushOccupancy_GameThread` are game-thread only.
* `runBatch` and the hook doors (`applyOccupancyCommands_Step`, `releaseDelayedInputsForStep`, the two
  latency doors, `publishTickOffset_Step`, `publishRenderSnapshot_Step`, `publishShadowSlot_Step`) run on the step's thread: a worker on a client, the game
  thread inline.
* J2 is held for each whole step (the manager's `runJoltStep_Step`), so the game thread's bind pass
  (manager G-81) waits for at most the rest of one step on a worker client. The game thread never waits
  for a step task while holding J2 (`⛔G-05`): `EndPlay`'s wait and end-of-physics both `checkf` it,
  and no game-thread J2 scope spans a tick group.

## 7. Lifecycle

* `BeginPlay` (manager `startJoltStepping`, after the role branches): the hooks and the driver are
  built under J2 (the driver parks every slot in its constructor), then `begin` emplaces the scheduler
  (`dt`, the cap, 0.75–1.25, start time 0) and registers the two tick functions on the persistent level,
  the end function with the start function as prerequisite.
* `EndPlay` (manager, first statements): `unregisterTickFunctions`, then `waitForOutstandingSteps`
  (manager `⛔G-80`): no step runs after it, before any teardown.
