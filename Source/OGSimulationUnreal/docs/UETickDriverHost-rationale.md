<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UETickDriverHost.h` and `.cpp` — rationale

The Unreal engine port of og-simulation's `SimulationTickDriver`: the engine type, the task-graph executor and the two
tick functions that start and end a frame's physics. The source holds the licence, a docs pointer, code, one
`static_assert` and its `check`/`checkf` messages; this file carries the why. It has no prohibition
(`UETickDriverHost-guards.md` has no live entry).

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. What it is

The part of og-tickdriver's engine adapter that needs Unreal (design of og-tickdriver, §2, "What an engine adapter
must supply"). Three pieces:

* **`SimulationEngineUImpl`**, the driver's `EngineT` (it `static_assert`s og-simulation's `TickDriverEngine`):
  * `nowSeconds()` is FPlatformTime::Seconds, `frameNumber()` is GFrameCounter;
  * `log(level, line)` writes the driver's already-formatted line on `LogOGSimHost` with the format `TEXT("%hs")`,
    at Verbose, Log or Warning; `logEnabled(level)` is UE_LOG_ACTIVE on the same category and level;
  * `kHostDiagnostics` is !UE_BUILD_SHIPPING, the switch the engine's own non-shipping blocks use;
  * the executor half (§3), which reads the step task's priority at every dispatch (§4).
* **The two tick functions**, `FSimulationStartStepsTickFunctionUImpl` at `TG_StartPhysics` and
  `FSimulationEndStepsTickFunctionUImpl` at `TG_EndPhysics`, the end function with the start function as its
  prerequisite (§2).
* **`UETickDriverHost`**, the frame host: it owns one engine and the two tick functions, registers them on the
  persistent level (`begin`) and unregisters them (`unregisterTickFunctions`). Its `.cpp` holds the frame's four
  console variables (§4): the end tick function reads the block mode every frame, the engine reads the step task's
  priority at every dispatch, and two static members, `requestedMaxCatchUpSteps` and `runInline`, hand the game the
  two values it reads once.

It knows no driver type and no game type. `begin` is a template over the driver: it registers the two tick functions
with a pointer to the driver and two captureless lambdas that cast it back and call `startFrame` and `endFrame`, so
the `.cpp` holds no template and the header names no game. The two lambdas are instantiated where `begin` is called,
in the game's module (§7).

The host is a plain C++ class. The two tick functions are plain `FTickFunction` subclasses, not reflected structs
(the engine has the same pattern in the pose-search interaction island), so nothing here needs UHT.

**`LogOGSimHost`** is this module's category: `OGSimulationUnreal.h` declares it, exported, and `OGSimulationUnreal.cpp`
defines it. Its name is the one the brawler's host lines have always used, so `Config/DefaultEngine.ini`'s
`LogOGSimHost=Warning` governs it, and the brawler's simulation manager logs its `[SimHost.Backend]` line on it in both
physics configurations.

## 2. The frame

The shape is Chaos's async mode (og-simulationscheduler-withjolt design D1): the same tick groups, the same wait
rule, the same per-frame cadence for the probes and the reap, and the same roles on the same threads.

| Chaos (async, block mode 0) | the frame host and the tick driver |
|---|---|
| the engine's start-physics tick function at `TG_StartPhysics` advances the scene, computes the frame's step count from accumulated world time, and dispatches the steps | `FSimulationStartStepsTickFunctionUImpl` at `TG_StartPhysics` calls the driver's `startFrame` with `static_cast<double>(DeltaTime)`: the driver pumps its scheduler with the accumulated world delta and runs or dispatches the frame's batch |
| client: the steps run as chained task-graph tasks after the previous frame's; dedicated server: inline on the game thread | client: one task per frame, chained after the previous frame's, at the step task's priority (§3); authority (and a client under `og.Sim.RunInline=1`, which the game reads through `runInline`, §4): inline |
| the end-physics tick function at `TG_EndPhysics` waits for the steps dispatched in **earlier** frames, then fires the scene's post-tick delegate | `FSimulationEndStepsTickFunctionUImpl` at `TG_EndPhysics` calls the driver's `endFrame` with its completion event and the block mode, read every frame from `og.Sim.BlockMode` (§4): the driver waits for earlier frames' task, then calls the game's `onPostPhysics` |

Block mode 1 (debug; `og.Sim.BlockMode`, §4) makes the end of physics also wait for this frame's task.

## 3. The executor

The driver's executor half maps one-to-one onto the task graph:

| the driver asks | this engine does |
|---|---|
| `dispatchAfter(previous, work)` | `FFunctionGraphTask::CreateAndDispatchWhenReady(work, TStatId{}, &prerequisites, stepTaskPriority())`, with `previous` as the one prerequisite when it is valid. The driver passes the pending task only while it has not completed, and an empty handle otherwise. The priority is `TaskGraph.TaskPriorities.OGSimStepTask`'s, read by the private static `stepTaskPriority` at every dispatch (§4) |
| `isPending(handle)` | IsValid and not IsComplete |
| `waitFor(handle)` | WaitUntilTaskCompletes on GameThread_Local, after a `checkf` that the event is valid: the driver keeps only events it dispatched |
| `deferUntil(completion, handle, continuation)` | returns `false` when the tick function's completion event is invalid (the engine runs a tick function without one only when it ticks while paused, which these two do not); then the driver waits synchronously. Otherwise it holds the tick group open with DontCompleteUntil on a game-thread task that has `handle` as prerequisite, and returns `true` |

**The deferred task's two checks.** The game-thread task holds the owner through a weak object pointer (set in
`begin`) and does nothing when the owner is gone; the driver's continuation then does nothing when the driver is
stopped. The brawler's host unregisters the tick functions and then calls the driver's `stop()` as the first thing
in `EndPlay`; `stop()` clears the driver's running flag **before** it waits. So a post-physics pass can never run on
an owner that is tearing down, even when the task graph runs the held task inside that wait.

## 4. The frame's console variables (og-tickdriver task 15c)

The `.cpp` holds the frame's four console variables, in an anonymous namespace, with the names, defaults, help texts
and flags they had in the brawler's game module:

| variable | default | read by | when |
|---|---|---|---|
| `og.Sim.MaxCatchUpSteps` | 60 | `UETickDriverHost::requestedMaxCatchUpSteps` | once, when the game builds the driver, into the driver configuration's `requestedMaxCatchUpSteps` |
| `og.Sim.RunInline` | 0 | `UETickDriverHost::runInline` (true when the value is not 0) | once, when the game builds its world, into the world host configuration's `runInline` |
| `og.Sim.BlockMode` | 0 | the end tick function's `ExecuteTick` | every frame, on the game thread, as the argument of `endFrame` |
| `TaskGraph.TaskPriorities.OGSimStepTask` | high thread priority, normal task priority (high when forced to the normal thread priority) | the engine's private static `stepTaskPriority` | at every dispatch, as the last argument of `CreateAndDispatchWhenReady` in `dispatchAfter` |

* **The step task's priority** is read at every dispatch, so a console change of the priority applies to the next
  frame's task, as it always has. A value taken at construction would freeze it (the lead's ruling on BL3,
  og-tickdriver task 11). `dispatchAfter` is a template in the header, instantiated where the driver is; the static
  member it calls is defined in the `.cpp`, beside the variable.
* **The block mode** is read where the end tick function calls `endFrame`, so `endFrame` receives the value the
  variable holds at that moment.
* **The cap and the stepping mode** are read by the game through the two static members, once each at `BeginPlay`
  (the brawler's `TickDriverBackendUImpl::buildWorld` and `startDriverStepping`). The adapter passes neither on: the
  cap reaches the driver's configuration directly, and the driver clamps it and prints it in `[SimHost.Frame]`.

From og-tickdriver task 11 until task 15c the four variables were the game's (the brawler's TickDriverBackendUImpl.cpp (a historical name: the host's `.cpp`, deleted by og-tickdriver task 15e)),
and the engine's and the host's constructors took the priority and the block mode as two getters (StepTaskPriorityFn
and BlockModeFn, function references; the end tick function kept the block-mode getter as a pointer beside `endFrame`).
With the variables here the getters have nothing to forward, and task 15c deleted them: neither constructor takes an
argument, and the end tick function's `check` covers `endFrame` alone, as the frame host's did before task 11.

**They register in both physics configurations.** The module is compiled in both, with no `#if` arm (§7), and a
`TAutoConsoleVariable` or `FAutoConsoleTaskPriority` registers itself with the console manager when the module's
static objects are constructed. So in the Chaos configuration the four exist and nothing reads them: nothing
constructs the engine or the host there. Until task 15c they were in a whole-file Jolt arm of the game module, and the
Chaos configuration did not have them (design of og-tickdriver, Q5, which task 15's re-scope replaced). Three of
the help texts start "Jolt configuration only" (the priority's names "the client's Jolt step task"), and two name the
brawler's simulation manager; the texts are frozen with the names and defaults, and "Jolt configuration only" stays
true of what the variables do.

## 5. Threads

Everything here runs on the game thread: the tick functions, `begin`, `unregisterTickFunctions`, the waits, the
deferral and the four console-variable reads (§4). Only `nowSeconds()`, `logEnabled` and `log` are also called from a step task: the driver's
step hooks read the clock, the first batch logs its `[SimHost.FirstStep]` line from the thread that runs it, and a
world host may log per-step lines (og-simulation-jolt's contact lines test `logEnabled` first). UE_LOG, its activity
test and FPlatformTime::Seconds are thread-safe.

## 6. Lifecycle

* `BeginPlay`, at its end (the brawler's `TickDriverBackendUImpl::startDriverStepping`): the game builds the driver over this
  engine, then calls `begin`, which records the owner for the deferred task's weak pointer and registers the two tick
  functions on the persistent level, the end function with the start function as prerequisite. A second `begin` is a
  `checkf`.
* `EndPlay`, its first statement (the brawler's `TickDriverBackendUImpl::onEndPlayBegin`): `unregisterTickFunctions`
  first, so no new frame starts, and then the driver's `stop()` (`TickDriverBackendUImpl-guards.md` G-01).

The lines the driver prints through this engine (`[SimHost.Frame]`, the catch-up cap Warning, `[SimHost.FirstStep]`
and `[SimHost.Window]`) are formatted by og-simulation's `TickDriverWindow`, byte for byte as the frame host printed
them before og-tickdriver task 8 (`TickDriverWindow-rationale.md`; the UE format-parity test pins the formats).

## 7. Module placement: one copy of the thread masks

This is host code: it names Unreal types and lives in the MPL-2.0 OGSimulationUnreal module, which is its own shared
library in a modular (Editor) build. The four types are exported (`OGSIMULATIONUNREAL_API`) because the game's module
constructs them and calls their members.

og-simulation's tick-driver thread context keeps its masks in header `thread_local` variables, so every shared library
that compiles a use of them gets its own copy (`TickDriverThreadContext-rationale.md` §7). The driver, its step scope
and the world host's lock and predicates must therefore all be compiled in one module, the game's. Nothing here
touches the context: the engine's `.cpp` members and the host's registration are not templates, and the two members
that are (`dispatchAfter`, `deferUntil`) and `begin`'s two lambdas are instantiated where the driver is. og-tickdriver
task 11 checked it on the Editor objects, and task 15c again after the console variables and `UEPresentControls` came
in: none of this module's objects and none of its exports names a `tickDriverThread` symbol, a scope, the driver or
the world host.

The files have no `#if` arm. The module is compiled in both physics configurations, and in the Chaos one nothing
constructs these types. What they do there is register the four console variables at load, and nothing reads them
(§4).

## 8. Provenance

Moved by og-tickdriver task 11 (2026-10-16) from the brawler's game module, where it was the frame host
SimulationFrameHostUImpl (og-simulationscheduler-withjolt task 54 wrote it as the whole Jolt frame pipeline;
og-tickdriver tasks 1 to 8 moved everything engine-free out of it into og-simulation's tick driver, and left the engine
port, the executor and the two tick functions). What changed in the move:
- the frame host is named `UETickDriverHost`, after its file; the engine and the two tick functions keep their names,
  and the tick functions keep their diagnostic strings;
- the step task's priority and the block mode came from two getters instead of the console variables, which moved to
  the brawler's TickDriverBackendUImpl.cpp (a historical name: the host's `.cpp`, deleted by og-tickdriver task 15e) with their names, defaults and help texts; the catch-up cap's reader went
  with them (until task 15c, below);
- the four types are exported, and the host's begin-twice `checkf` names the new class;
- `LogOGSimHost` moved with it from the brawler's simulation manager (§1);
- the `.cpp` lost its whole-file Jolt arm (§7).

The predecessor's guard document had no live entry: its six ids were retired by og-tickdriver task 8, each to the
same-numbered id of og-simulation's `SimulationTickDriver-guards.md`.

og-tickdriver task 15c (2026-10-18) moved the four console variables here from the brawler's
TickDriverBackendUImpl.cpp (a historical name: the host's `.cpp`, deleted by og-tickdriver task 15e), with their names, defaults and help texts, and deleted the two getters (§4). The six
`og.Render.*` variables went to this module's `UEPresentControls.cpp` in the same task (`UEPresentControls-rationale.md`).
