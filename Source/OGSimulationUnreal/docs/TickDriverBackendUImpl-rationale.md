<!-- SPDX-License-Identifier: MPL-2.0 -->
# `TickDriverBackendUImpl` — rationale

Companion to `Source/OGSimulationUnreal/TickDriverBackendUImpl.h`, a header template since og-tickdriver task 15e,
which moved it here from the game module, where it was a header and a `.cpp` (§12). The header carries no prose: a
two-line docs pointer, code, assertion messages, and one-line tags `⛔G-nn` → an entry in
`TickDriverBackendUImpl-guards.md`.

> **Licence.** This tier is **MPL-2.0**, matching the code it describes. Until og-tickdriver task 15e the class and
> this document were OGBrawlerUnreal's (BUSL-1.1); the user ruled the move to MPL-2.0 on 2026-10-18. §2 to §11 were
> written while the class was the brawler's Jolt host, and the brawler is still its one user: where they name a
> brawler type, the type now comes from the game binding or the engine binding (§12).

* **§1** says why the class exists, what it is, and why the Jolt host is two objects.
* **§2** to **§9** moved here from `SimulationManagerUImpl-rationale.md` (§17, the map of the Jolt host, and §18's
  subsections on the Jolt configuration's world, stepping, render publish and apply, shadow world, render
  interpolation, render correction smoothing and contact counts) in og-simulationscheduler-withjolt task 83, with the
  code they describe. Only their section references, the member and function names the move changed, and the guard
  ids changed.
* **§10** is the class template of og-tickdriver task 15b: its requirement, what it takes from the engine traits and
  the binding, its one instantiation, and what in it is still brawler- or Jolt-specific.
* **§11** is og-tickdriver task 15d: the presenter, the tick driver, the render source and the order in which they
  are built, registered and stopped are og-simulation's `SimulationTickBackend`, which this class holds as
  `m_tickBackend`; this class keeps the Unreal and brawler half. **Names before task 15d (historical):** the nested
  render source BrawlerRenderSourceUImpl, the aliases BrawlerPresenter and BrawlerTickDriver, the members
  m_renderSource, m_presenter and m_tickDriver, and the private doors presenter() and tickDriver() (historical names).
* **§12** is og-tickdriver task 15e: the class moved to OGSimulationUnreal (MPL-2.0) as a header template over three
  parameters, `TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>`, and names no brawler symbol; the
  brawler supplies the game binding and the engine binding and instantiates it in the game module. **Names before
  task 15e (historical):** the template parameter BindingT (now `EngineBindingT`), the alias BrawlerTickBackend (now
  `TickBackend`), the file TickDriverBackendUImpl.cpp, and this document's place in the game module's docs.
* **§13** is og-tickdriver task 15f: the concepts' header `PhysicsEngineTraits.h` moved to og-simulation; this
  header's include line changed and nothing else.
* Since og-tickdriver task 10 the world, the slots, the bind, the shadow world and the contact counts are
  og-simulation-jolt's `JoltTickWorldHost`; §3, §6 and §9 say what this class still supplies.
* Since og-tickdriver task 15a the Jolt engine's own pieces (the adapter types, the log sink and the world-host
  template; `og.Jolt.ContactCounts` too until task 15e, since then the Jolt binding's, §12; and since task 15e
  `Statics` and `splitStatics`) are the engine traits `JoltEngineTraitsUImpl` (`JoltEngineTraitsUImpl-rationale.md`),
  and the brawler's Jolt glue (the category table and its mask, the capsule check, the park snap and the simulatable
  type) is the binding `BrawlerJoltBindingUImpl` (`BrawlerJoltBindingUImpl-rationale.md`). This class calls both;
  §3, §5, §7 and §9 name each call.
* Since og-tickdriver task 15b this class is a class template, named by who schedules the steps: og-simulation's
  `SimulationTickDriver` (§10). Task 15b's form had two parameters, the engine traits and the brawler's binding
  (TickDriverBackendUImpl over EngineTraitsT and BindingT, instantiated over JoltEngineTraitsUImpl and
  BrawlerJoltBindingUImpl; historical); since task 15e it has three, and the Jolt configuration instantiates it as
  `TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>` (§12).
  **Names before task 15b (historical):**
  class JoltBackendUImpl (historical name), in JoltBackendUImpl.h and JoltBackendUImpl.cpp (historical file names);
  this document JoltBackendUImpl-rationale.md and its guards JoltBackendUImpl-guards.md (historical names);
  worlds struct JoltBackendWorldsUImpl and world-host alias JoltTickWorldHostUImpl (historical names);
  getters' namespace joltBackendUImpl and stepping function startJoltStepping (historical names).
  The sections below use today's names, also where they tell what an earlier task did to the class.

---

## §1 Why this class exists

The user asked whether `ASimulationManagerUImpl` could be split so that the Chaos and Jolt backends are more
separate (og-simulationscheduler-withjolt phase F, 2026-10-15). The UImpl is the brawler's per-role simulation
session: the netcode session, the relay listeners, registration and input, the per-frame sends, the latency budget
and log routing. None of that depends on the physics engine. Task 82 moved the Chaos host into its own class
(ChaosBackendUImpl, its historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b); task 83 moved the Jolt host into this class,
and the UImpl lost its last `OG_PHYSICS_BACKEND_CHAOS`.

**What it is.**

* **Plain C++, not reflected; a class template since og-tickdriver task 15b** (§10). It holds a reference to its
  owner, the game binding's `Session` (the brawler's `ASimulationManagerUImpl`; §12), and one to the owner's
  `m_backendWorlds`. The owner declares the configuration's
  backend a friend (`friend physicsBackendUImpl::Backend;`, which in the Jolt configuration names this class's one
  instantiation), so its bodies read the owner's members as the code did when it lived in the owner. Since
  og-tickdriver task 15e they read only the three adapters (`m_physAdapter`, `m_physReaderAdapter`, `m_queryAdapter`),
  `runsPrediction` and the `m_backendWorlds` < `m_backend` row; the game binding reads the rest (the gravity in
  `m_staticData`, `m_storage`, `m_manager` and the net mode; `BrawlerTickDriverGameUImpl-rationale.md`). The bodies
  read `m_staticData` until task 15e, and `m_storage` and `m_manager` until task 15d.
* **Not copyable.** The copy constructor and assignment are deleted; it is a member of its owner and nothing else.
* **The owner calls it through the seam,** the fixed set of entry points in `SimulationManagerUImpl-rationale.md`
  §18, each from exactly one call site and with no `#if`. `PhysicsBackendHostUImpl.h` defines the concept
  `PhysicsBackendHost`, which lists the set, and the owner asserts that its backend satisfies it
  (`PhysicsBackendHostUImpl-rationale.md`). Entry points with no Jolt work are empty inline bodies here:
  `onEndPlayWorld`, and the two latency-window hooks `sampleLatencyWindow_GameThread` and
  `logLatencyWindow_GameThread`, which since og-tickdriver task 18 sit on the empty diagnostics view `editDiagnostics()`
  returns (a by-value `Diagnostics` with no state, header-only like the rest of this class; the game's
  latency diagnostics call it, `DiagnosticsConventions.md` §1). The others call the
  functions that held the Jolt arms before (`startStepping` calls `startDriverStepping`; since og-tickdriver task 9
  `beforePresent_GameThread` and `updateVisualization_GameThread` call og-simulation's presenter's two game-thread
  doors, §5; since og-tickdriver task 10 `buildWorld` imports the statics and calls the world host's `build`, and the
  three visualization doors forward to the world host's, §3), or are the old arms themselves (`onEndPlayBegin`,
  `createBodies_GameThread`, `onRegistered_GameThread`, `onUnregistered_GameThread`).
* **The header has no `#if`.** Only `PhysicsBackendHostUImpl.h` includes it, in the Jolt configuration. Since
  og-tickdriver task 15e it also holds the member definitions, and the class has no `.cpp`: its one explicit
  instantiation is in the brawler's `BrawlerJoltBindingUImpl.cpp`, a whole-file arm (`#if !OG_PHYSICS_BACKEND_CHAOS`)
  like `ChaosTickBackendUImpl.cpp`, so in the Chaos configuration nothing instantiates it (§12). Until task 15e the
  host's own `.cpp` (TickDriverBackendUImpl.cpp, a historical name) was that arm and held the definitions and the
  instantiation (§10). The frame host's `.cpp` was a third such arm until og-tickdriver task 11 moved it to
  OGSimulationUnreal (`UETickDriverHost.cpp`), where it has none (`UETickDriverHost-rationale.md` §7).

**Why the Jolt host is two objects.** Members are destroyed in reverse declaration order, and the Jolt host needs one
part destroyed after the owner's adapters and another destroyed before the owner's manager:

* `TickDriverBackendWorldsUImpl` holds the world state (a struct template over the world host since og-tickdriver
  task 15b; this class names its instance `Worlds`). Since og-tickdriver task 10 it is two members: `engine`, a
  `SimulationEngineUImpl` (OGSimulationUnreal's engine port, `UETickDriverHost.h`: FPlatformTime::Seconds,
  GFrameCounter, `LogOGSimHost`),
  and `host`, og-simulation-jolt's world host `JoltTickWorldHost` instantiated as `TickDriverBackendUImpl::WorldHost` (over
  `SimulatableBrawler`, `UEFMutexLockableUImpl` and that engine type; §3). Since og-tickdriver task 15a the alias is
  the Jolt engine traits' `WorldHost` template over the Jolt binding's `Simulatable`, the same type. The world host holds the runtime lease, the
  contact counter, the step and shadow worlds, the shadow world's adapters, the slot template, the slot table
  (og-simulation's `TickSlotTable`), the world lock, the thread context, the shadow hand-over and its window and the
  contact records (`JoltTickWorldHost-rationale.md` §4). The owner declares the struct as `m_backendWorlds`, where
  the Jolt member block was, above `m_physAdapter`, because the adapters are bound to the step world and the lease
  must outlive both worlds.
* `TickDriverBackendUImpl` (this class) holds everything else, in this order: the frame host, the game binding, the
  render-target table and the tick backend, og-simulation's `SimulationTickBackend` (§11), which holds the render
  source, the presenter (since og-tickdriver task 9 it holds the render snapshots, the corrections, the render
  targets and the render windows) and the tick driver, in that order. The owner declares this class as `m_backend`,
  directly after `m_manager`, because the driver unregisters from the manager's client clock when it is destroyed.

**Why the world host has its own engine.** `JoltTickWorldHost` takes its engine by reference in its constructor, and
`m_backendWorlds` is constructed before `m_backend`, where the frame host's engine lives, so the world host cannot
borrow that one. It gets a second `SimulationEngineUImpl`, declared before it in the same struct. The world host
uses only the engine's clock, frame number, log sink and diagnostics switch, which read the same global state in
every instance; the one per-instance member, the weak owner the frame host sets for the end-of-frame deferral, is
read only by `deferUntil`, which the world host never calls. Neither engine takes a constructor argument: each
reads the step task's priority itself at a dispatch (`UETickDriverHost-rationale.md` §4), and the world host never
dispatches.

og-tickdriver task 15b looked again at whether the world host could borrow the frame host's engine, and kept the
second one. The template changes no member order: `m_backendWorlds` is still constructed before `m_backend` and
destroyed after it, so the frame host's engine does not exist yet when the world host's constructor takes its
reference, and is gone while the world host still holds it. And the frame host, OGSimulationUnreal's
`UETickDriverHost`, owns its engine as a member (`m_engine`), built by its own constructor (which took the two
getters until og-tickdriver task 15c and takes nothing since), so the engine cannot be handed in from the worlds
struct either without a change to that adapter.

On Chaos the owner's two members are `ChaosTickBackendWorldsUImpl` (an empty struct) and `ChaosTickBackendUImpl`, so the
owner's declarations and assertions are the same in both configurations.

| destruction-order row (destroyed right to left) | where it is asserted |
|---|---|
| runtime lease < contact counter < step world | og-simulation-jolt's `JoltTickWorlds`, first `static_assert` (this struct's own until og-tickdriver task 10) |
| runtime lease < shadow world < shadow body adapter < {shadow reader, shadow query} | `JoltTickWorlds`, second `static_assert` (likewise) |
| `engine` < `host` | this class's `compositionContractsHold`, first `static_assert` (since og-tickdriver task 15b; from task 10 until then the worlds struct's own file-scope `static_assert`, which a struct template cannot carry) |
| `m_backendWorlds` < `m_physAdapter` < {`m_physReaderAdapter`, `m_queryAdapter`} < `m_integrationLayer` | the owner's `compositionContractsHold` |
| `m_manager` < `m_backend` | the owner's `compositionContractsHold` |
| `m_backendWorlds` < `m_backend` (the driver holds the world host) | this class's `startDriverStepping`, a `static_assert` (since og-tickdriver task 10) |
| {`m_frameHost`, `m_tickDriverGame`} < `m_tickBackend` | this class's `compositionContractsHold`, second `static_assert` (since og-tickdriver task 15d; from task 8 to 15d the rows "frame host, game binding and presenter < driver" and, from task 9, "frame host and render source < presenter" over this class's own members; the hooks < step driver row is og-simulation's `SimulationTickDriver`'s own) |
| render source < presenter < driver | og-simulation's `SimulationTickBackend`, a `static_assert` in its constructor (since og-tickdriver task 15d; `SimulationTickBackend-rationale.md` §4) |

Since og-tickdriver task 15b the class's rows are checked only where the template is instantiated: a member
function of a class template is compiled when it is used or explicitly instantiated, and `compositionContractsHold`
is reached only through `assertCompositionContracts`, which nothing calls. The explicit instantiation (§10; in the
brawler's `BrawlerJoltBindingUImpl.cpp` since og-tickdriver task 15e) instantiates both, so every row is checked in
every Jolt build of the game module; task 15b saw a reordered member fail the build through that instantiation, and
task 15e saw it again with the header in OGSimulationUnreal (§12). The tick backend's row sits in its constructor, which this class's constructor
instantiates (the session's constructor and the explicit instantiation both reach it); task 15d saw the backend's
presenter and driver, swapped, fail the Editor build there.

Together they are the rows the owner asserted before task 83: runtime < world < adapters < integration layer, the
shadow world's row, the contact counter before the world, and manager < frame host < hooks < driver. Every message
says which row it was.

**What the move changed, and what it did not.** The code moved with only these edits: the owner prefix (`m_owner.`)
on the owner's members and on `runsPrediction` and `GetNetMode`; the worlds prefix (`m_worlds.`) on the members of
`TickDriverBackendWorldsUImpl`; the class qualifier; the step world's after-step observer, which receives and casts this
class instead of the owner; `m_frameHost.begin`, which also takes this class; and in `createBodies_GameThread`, the
parameters in place of `tryRegister`'s locals (`character`, `simulatable`) and `false` in place of
`TryRegisterStatus::Pending`. The order of the calls inside every moved function, the log texts, the console
variables and the threading predicates are unchanged. `tryRegister`'s root-capsule check after the bind reads the
movement declaration's stamped `bindings.parentBodyId`, which the Jolt bind stamps with the slot's root body on every
declaration (task 82 made the check read it).

* **The owner's `HasAuthority` poison comes along.** The owner's `.cpp` defines `HasAuthority` to an undeclared name
  after its includes, so `HasAuthority()` typed there fails to compile (the manager is not replicated, so the call
  is always true; `SimulationManagerUImpl-rationale.md` §13.9). The Jolt code had that protection while it lived in
  the owner's `.cpp`, so this class's `.cpp` repeated the same `#define` after its includes and the `#undef` at its
  end. Since og-tickdriver task 15e the header carries it, around the member definitions (the `#define` after the
  class, the `#undef` at the header's end), under a name that names no game type:
  `HasAuthority_is_not_the_peer_role_in_TickDriverBackendUImpl_use_worldIsAuthority_or_runsPrediction`. The game
  binding, which now reads the session's net mode, carries the original poison in its own header (§12).
* **The nested types moved with the members that use them.** The render target type, the two render windows, the
  shadow state slot, the shadow window and the contact step record were private nested types of the owner, used
  only by Jolt code, so they became private nested types of this class. og-tickdriver task 9 deleted the first three:
  the presenter has its own target and window types. The runtime lease moved to this class's header.
  og-tickdriver task 10 deleted the other three and the lease: the world host has its own
  (`JoltTickWorldHost-rationale.md` §4). The step-driver alias named the manager and the integration executor through `BrawlerCompositionUImpl`
  (task 81), because the owner's `ManagerType` is private; since og-tickdriver task 8 the game binding
  `BrawlerTickDriverGameUImpl` names them for the tick driver.
* **The frame host was a friend of both** (task 83 to og-tickdriver task 8): it called this class's step functions,
  its driver's occupancy door and its world-mutex and step-flag predicates, and the owner's per-frame work. Since
  og-tickdriver task 8 it calls only the tick driver, and neither class names it a friend: the owner's per-frame and
  per-step work is reached through the game binding, which the owner names a friend (§4).

## §2 The Jolt host — overview (og-simulationscheduler-withjolt tasks 53–56 and 18)

Since task 18 the Jolt configuration is the default: `PhysicsBackendChaos` in `OGBrawlerUnreal.Build.cs`
is `0` (`SimulationManagerUImpl-rationale.md` §18). Its host is spread over this class, its owner (the shared
session, which calls it through the seam), the frame host, the game binding and og-simulation's tick driver and step
hooks. This section is the
map; the details are in the sections and documents it points to.

**The shape is Chaos's async mode** (design D1). The same tick groups, the same wait rule (a client's end
of physics waits only for steps dispatched in earlier frames), the same per-frame cadence for the probes
and the reap, and the same roles on the same threads: the authority steps inline on the game thread, a
client on a worker task.

| concern | who | where it is described |
|---|---|---|
| the two tick functions at `TG_StartPhysics` and `TG_EndPhysics`, the engine port and the task-graph executor | the frame host, OGSimulationUnreal's `UETickDriverHost` | `UETickDriverHost-rationale.md` |
| the scheduler, the world-delta time base, the catch-up cap of 60, the batch, the wait rule and the window | og-simulation's tick driver | `SimulationTickDriver-rationale.md` |
| which thread steps: the authority inline; a client one task per frame chained after the previous frame's, or inline under `og.Sim.RunInline` 1; block mode 0 | the tick driver, by this class's stepping mode | `SimulationTickDriver-rationale.md` §4; §3 |
| one step: the world mutex, then the step driver's `runTick` and the four hooks | the tick driver's per-step body, through the world host's lock; og-simulation's step hooks | §4; `TickDriverStepHooks-rationale.md` |
| the per-frame input work before the steps (PROBE A, PROBE 6), the reap after the inline steps, the drain per authority step | the tick driver and its hooks, through the game binding, and the owner | `BrawlerTickDriverGameUImpl-rationale.md`; `SimulationManagerUImpl-rationale.md` §7, §8 |
| the world, its statics, the character slots, the bind and the release | og-simulation-jolt's world host, with this class's static import, configuration and slot glue | §3; `JoltTickWorldHost-rationale.md` |
| the render publish, the interpolation, the correction smoothing and the capsule apply | og-simulation's presenter, through the hooks and this class's two game-thread doors; the actor move through OGSimulationUnreal's render sink | §5, §7 and §8; `TickDriverPresenter-rationale.md`; `UERenderTargetTable-rationale.md` |
| the game-thread shadow world | og-simulation-jolt's world host | §6; `JoltTickWorldHost-rationale.md` §11 |
| the mechanisms between the game thread and the step (J1–J7, the shadow hand-over) | the tick driver, the hooks, the presenter, the world host | `SimulationManagerUImpl-rationale.md` §1 |
| the per-window line `[SimHost.Window]`, the step cost and the thread-ownership asserts | the tick driver, the hooks, og-simulation's thread context | `SimulationTickDriver-rationale.md` §7, §8; `TickDriverStepHooks-rationale.md` §5; `SimulationManagerUImpl-rationale.md` §8; §4 |
| the backend in the build identity and the join check | the build identity | `OGBuildIdentity-rationale.md` §7 |

**One frame on a worker client**, in order:

1. `TG_StartPhysics`, game thread: pump the scheduler with the world's delta time. When steps are due,
   call `onFrameStepsDue_GameThread` with the frame's first upcoming tick (the dispatch counter plus the
   offset the step publishes, J3) and dispatch one task with the batch (J4), chained after the previous
   frame's.
2. The task, for each step: take the world mutex (J2) and run the driver's `runTick`. `beforeTick` drains
   the occupancy commands (J1); `beforeSimulate`, after a resimulation, captures the render correction
   (J7), then takes the latency start time; after the step,
   `afterTick` stamps the step end, publishes the render snapshot (J5), hands the saved state slot to the
   shadow world, and publishes the tick offset (J3).
3. `TG_EndPhysics`, game thread: wait for earlier frames' task, then the owner's `OnPostPhysicsStep`, whose
   first backend call is `beforePresent_GameThread`: restore the
   shadow world from the newest slot, blend the render pose two steps behind the host time, add what
   remains of each correction, move the capsules there and write it into the shadow world, then the shared body (the sends, the timing-relay
   write, the visualization copy with the rendered poses, the ring-out score push, the latency probe).

On the authority step 2 runs inside step 1, on the game thread: its `beforeTick` also releases the
delayed inputs due for that step, and the reap follows the frame's steps. It has no shadow world, and
`TG_EndPhysics` never has anything to wait for.

**How the game-thread visualizations see the physics world** (design D17; the user's ruling OQ13 = (e),
2026-10-08). The block-prediction and target visualizations query a world every frame from the game
thread. On a worker client the step world changes under them, and taking the world mutex for each pass
would make the game thread wait for the running step, which Chaos never did: the design priced that wait
at one step on a normal frame and the replay depth plus one on a resimulation frame, about 1 ms per frame
on desktop and 2–4 ms on Android at its estimates. The ruled option copies Chaos's own model instead: a
second world that only the game thread touches, restored once per frame from the newest saved state
(§6). The visualizations query it with the same adapter
classes, so the component and og-brawler are the same in both configurations; the game thread takes the
world mutex only at `BeginPlay` and around the join-time bind. The alternatives priced with it were a game-thread pose
copy with its own query view, taking the mutex and accepting the wait, and running the visualizations in
the step with a recorded draw list (the user's proposal, kept as a possible later refinement). Which one
is the long-term answer is decided at the decoupling gate, from the shadow world's measured game-thread
cost on Android and from whether step-rate arcs are noticeable on the target displays.

**Lifecycle** (design §9.1). `BeginPlay` builds the Jolt world, its statics and adapters before the role
branches, the shadow world on a worker client (`buildWorld`), and at the end starts the stepping (`startStepping`:
the tick driver with its hooks, step driver and scheduler, then the two tick functions). `EndPlay` first unregisters
the tick functions and waits for every outstanding step (`onEndPlayBegin`, `⛔G-01`), before any teardown.

**Lines.** `[SimHost.Backend]` (both configurations), `[SimHost.Mode]`, `[SimHost.World]`,
`[SimHost.Frame]`, `[SimHost.FirstStep]`, `[SimHost.Slot]`, `[SimHost.Bind]`, `[SimHost.Shadow]`, `[SimHost.RenderTarget]`,
the non-shipping `[SimHost.RenderApply]` and `[SimHost.RenderSync]` checks, `[SimHost.RenderCorrection]`
and `[SimHost.RenderTrace]`,
and the per-window `[SimHost.Window]` (`SimulationManagerUImpl-rationale.md` §8), on `LogOGSimHost`.

## §3 The world (task 53; og-tickdriver task 10)

The first part of the Jolt host builds the physics world and binds characters into it. Task 53 built it without
stepping; task 54 steps it (§4). Since og-tickdriver task 10 the world, the stepping mode, the slots, the bind, the
release and the visualization doors are og-simulation-jolt's `JoltTickWorldHost` (`JoltTickWorldHost-rationale.md`
§5 to §8 in og-simulation-jolt's docs: every step, check and log format there is the transcription of this class's
old code), and this class reaches it as `host` in `m_backendWorlds` and supplies the brawler and Unreal parts. Each item below
says which is which.

* **The world build** (`buildWorld`, before the role branches, the same on every peer). This class:
  1. imports the level's static collision with `UEStaticGeometryImporter` and the brawler's six-row category table
     (`BrawlerJoltBindingUImpl::categoryChannels`, the same rows as the Chaos binding's
     `BrawlerChaosBindingUImpl::categoryChannels`; both were file-local functions of the two hosts until og-tickdriver
     task 15a), and logs its report;
  2. splits the import into the queryable and the PhysicsOnly shapes by the importer's per-shape `physicsOnly`
     flags, with the world host's `splitStatics` (reached through the engine traits' `splitStatics` since
     og-tickdriver task 15e, §12);
  3. fills the world host's configuration: the role, `og.Sim.RunInline` (read once), the movement gravity,
     `og.Jolt.ContactCounts` (read once, through the engine binding's `contactCountsEnabled`,
     `BrawlerJoltBindingUImpl::contactCountsEnabled` since og-tickdriver task 15e; §9), the six mapped
     categories (`BrawlerJoltBindingUImpl::mappedCategories`, the Chaos adapter's mapped set) and the Jolt log sink
     `JoltEngineTraitsUImpl::logEngineMessage`, which maps a `[Warning]` prefix to Warning on `LogOGSimHost`;
  4. calls the tick backend's `build` (§11) with the configuration, the two static parts and the owner's three adapter
     optionals (`m_physAdapter`, `m_physReaderAdapter`, `m_queryAdapter`); it hands them unchanged to the world host's
     `build`, which emplaces them over the step world.

  `build` then fixes the stepping mode and logs `[SimHost.Mode]`, acquires the runtime, folds the slot template from
  `SimulatableBrawler`'s physics composite, and builds the step world, its statics (queryable first, so the static
  bodies get the same Jolt indices on every peer), the three adapters with the access predicate (below) and the
  `[SimHost.World]` line under the world lock; then the shadow world on a worker client (§6) and the
  `[SimHost.Contacts] installed` line. ThirdPersonMap has no PhysicsOnly element, so the second builder builds
  nothing there.

  **The one order change of og-tickdriver task 10.** Before it, `[SimHost.Mode]` and the runtime acquire (with
  og-simulation-jolt's "JoltRuntime: Jolt … initialised" line on `LogOGSimHost`) came before the static import, whose
  two `[StaticImport]` lines are on `LogOGStaticImport`. `build` takes the imported statics as arguments, so the
  import and its two lines now come first and `[SimHost.Mode]` follows them, in the same frame on the same thread.
  The order of the `LogOGSimHost` lines among themselves, every line's text and the world are unchanged.
* **The stepping mode** is fixed in `build` and printed once as
  `[SimHost.Mode] role=<Authority|Client> stepping=<inline|worker> viz=<stepWorld|shadowWorld>`. The authority
  always steps inline on the game thread: its pre-step input release touches UObjects. A client steps on a worker
  task, or inline when `og.Sim.RunInline` is 1. The decision is the role, never the engine's
  ShouldUseThreadingForPerformance, which would put a listen or standalone authority on a worker as Chaos does
  (design D2). The tick driver's threading follows it (§4).
* **The world lock (J2)** is the world host's `UEFMutexLockableUImpl`, a `UE::FMutex` (as this class's
  world mutex was until og-tickdriver task 10; `UEFMutexLockableUImpl-rationale.md`), taken through
  og-simulation's `WorldLockScope`, which also opens a `JoltWorldAccessScope` and sets this role's bit in a
  thread-local "this thread holds J2" mask. The step holds it for each whole tick (the tick driver's per-step body,
  through the world host's `lockForStep`). The game thread holds it only around `tryRegister`'s bind
  (`createBodies_GameThread`, through the world host's `bind`), the world build and the driver's construction;
  never around a visualization pass. `UE::FMutex` is not recursive, so the scope checks the bit before it locks
  (`TickDriverThreadContext-rationale.md` §5).
* **Access predicates.** `JoltSpatialQueryAdapter` asserts every call through the function pointer it is given:
  og-simulation's `TickDriverThreadContext` predicates, chosen by the role and the stepping mode when `build`
  configures the context (`TickDriverThreadContext-rationale.md` §4). A worker client's adapter accepts "this role's
  step is running on this thread, or this thread holds this role's J2"; an inline host's also accepts the game
  thread, because there the game thread runs the steps and queries between them. The "step is running" bit is set
  by the tick driver's `StepScope` around each step.
* **The slot table** (task 66). A character owns one of the world's fixed slots from the first time the manager
  hears of it until it leaves, and it owns the **same** slot on every peer. A slot's bodies have fixed Jolt body ids
  and Jolt orders contact solving by body id, so a character in different slots on two peers can have a contact
  solved in a different order there (design_resim_analysis §6; the N2 paragraph of §4). The table is og-simulation's
  `TickSlotTable` inside the world host (`TickSlotTable-rationale.md`; the world host's `acquire`, its §6). This
  class supplies the brawler's half, in `callWithSlotRequest` (since og-tickdriver task 15e this class computes
  the key and the game binding's `callWithSlotRequest` the rest, with the same statements; §12):
  - the key, the owning actor's address (`slotKeyOf`), and its name in UTF-8 for the lines;
  - the role, the session's `GetNetMode() != NM_Client` (the owner's `HasAuthority` is poisoned where this
    statement is, §1);
  - the pawn's replicated slot, `AOGBrawlerUECharacter::GetPhysicsSlot`, after a `checkf` that the owner is that
    class, because the slot reaches the clients on its replicated `PhysicsSlotValue`;
  - the publish, `SetAuthoritativePhysicsSlot`, which the world host calls on the authority for a new assignment.

  So the authority takes the lowest free slot and writes it to the pawn, which replicates it; a client takes the
  replicated slot, even when a lower one is free here, and never picks one itself; a client with no replicated slot
  yet (`WaitForReplication`), or whose replicated slot is still held here by a character that has left the authority
  but not yet this client (`WaitForRelease`), gets no slot and retries on the next frame; all slots taken is the
  table's `OG_CHECK`. Each owner logs one `[SimHost.Slot] wait=…` line per wait kind and one
  `[SimHost.Slot] assigned …` line, both at `Log`; `lowestFreeLocally` is the slot a first-free rule would have picked on that peer.
  `registerVizVolume`, which the component calls before the character's simulation id is known, takes the slot
  first and returns an empty volume id while the acquire waits, so the component retries before it builds its
  visualization states (`SimmableUpdateComponent-rationale.md` §3); `tryRegister` finds the slot there, and its first
  call returns `Pending` if it does not (`createBodies_GameThread` returns `false`). The slot rides pawn replication
  only; a query hit's key still uses the simulation id, not the slot.
* **The bind** (`tryRegister`'s first call, through `createBodies_GameThread`). After the capsule check (below), this
  class passes the slot request to the tick backend's `acquireAndBind` (§11), which takes the slot from the world host's
  `acquire` and then calls its `bind` with the slot, the simulation id, the simulatable, the owner's static data and
  the tick driver's `noteWorldLockWait` as the wait note (`JoltTickWorldHost-rationale.md` §7). The id is the storage key (`toStorageKey` of the simulation id), which the authority alone assigns and every
  client receives on the pawn (`SimulationManagerUImpl-rationale.md` §15), so it is the same on every peer, as the
  query adapter's hit order requires. The `[SimHost.Bind] bind … body0=…` line is at `Warning`, so it shows at
  `LogOGSimHost`'s committed level (task 66; the lines were at `Log` before and never appeared). Occupancy is not
  applied here: the slot stays parked until the step applies the Occupy command the Ready path pushes (J1, §4).
* **The capsule contract (design D14).** The Chaos factory compared the authored UE capsule with the movement
  descriptor when it adopted the capsule as the root body. The Jolt slot body is built from the descriptor, so the
  bind pass `checkf`s the pawn's unscaled capsule radius and half-height against the descriptor's `CapsuleGeometry`
  instead (`OGBrawlerUECharacter-guards.md` G-01). The check is the binding's
  `BrawlerJoltBindingUImpl::checkCapsuleContract` since og-tickdriver task 15a; `createBodies_GameThread` calls it
  first, where the block stood.
* **Readiness** is unchanged in shape: the second call asks the body adapter's `isBodyResolvable` for every
  declaration's body. For the Jolt body adapter that means "bound": it reads the bind table the first call's
  factory wrote, on this thread, so it needs no lock and is true on the next attempt.
* **The release** (`unregisterFromNewFramework`, last, through `onUnregistered_GameThread`, when the component still
  has an owner): the tick backend's `releaseSlot`, which calls the world host's `release` with the tick driver's
  `pushBodySlotOccupancy` as the occupancy door. It frees the
  slot, pushes the slot's Vacate command (J1), clears the slot's bind-table entries, logs `[SimHost.Bind] release`
  at `Warning` and mirrors the release into the shadow world, on the game thread without J2. The body adapter's
  step-side reads of the bind table (the locked-rotation inertia) never read the entries a release clears; the
  reader's "bound" gate does, and its only callers are the game-thread visualizations, so the release cannot race
  them (task 11 review N3). The bodies keep their stale user data until the next bind, which is harmless: a parked
  body answers no query and collides with nothing. Query volumes are never unregistered, as in the Chaos
  configuration. The release also forgets the owner's logged waits.
* **Visualizations.** `editVizQuery()`, `getVizReader()` and `registerVizVolume` forward, through the tick backend's
  doors of the same names, to the world host's (`JoltTickWorldHost-rationale.md` §8): on an inline host the step world's adapters, which the
  game thread may query between steps, with the slot's root as the ignored root of a registered volume; on a worker
  client the shadow world's (§6). Task 53 returned null on a worker client, and the component skipped its two
  physics-querying passes; task 56 removed that skip, and the accessors return references again, as before task 53.

## §4 The stepping (task 54; og-tickdriver tasks 8 and 10)

Since og-tickdriver task 8 the frame pipeline and the step hooks are og-simulation's `SimulationTickDriver` and
`TickDriverStepHooks` (`SimulationTickDriver-rationale.md`, `TickDriverStepHooks-rationale.md`). The frame host
(OGSimulationUnreal's `UETickDriverHost`, a game-module class until og-tickdriver task 11) is their Unreal engine port
and the two tick functions, and the brawler's game hooks are
`BrawlerTickDriverGameUImpl`; each has its own document. Since og-tickdriver task 10 the driver's world host is
og-simulation-jolt's `JoltTickWorldHost`, held in `m_backendWorlds` (§1, §3). This class's part:

* **Members**, of this class, last and in this order: `m_frameHost` (the engine and the tick functions;
  default-constructed, it reads its own console variables, `UETickDriverHost-rationale.md` §4),
  `m_tickDriverGame` (the game binding over the owner), the render-target table `m_renderTargets`, and
  `m_tickBackend`, og-simulation's `SimulationTickBackend` over the game binding, the world host
  (`TickDriverBackendUImpl::WorldHost`), the binding's simulatable and the frame host's engine (§11). The tick backend
  holds, in this order, the render source, an optional presenter (§5) and an optional `SimulationTickDriver` over the
  game binding, the world host, the presenter and the frame host's engine. The driver owns the hooks and then the
  `SimulationStepDriver` (over the manager, the world host's step world and the hooks), and holds the four by pointer.
  This class's `compositionContractsHold` asserts that the frame host and the game binding are declared before the
  tick backend; the tick backend asserts render source < presenter < driver; a `static_assert` in
  `startDriverStepping` asserts that the owner declares `m_backendWorlds` before `m_backend`, so the world host
  outlives the driver; the owner asserts manager < backend, so the driver goes first and while the manager lives: its
  step driver unregisters its resync callback from the manager's client clock when destroyed.
* **The world host's step doors** (`JoltTickWorldHost-rationale.md` §9). `stepWorld()` is the step world.
  `lockForStep()` returns the world host's `WorldLockScope`: J2, the Jolt access scope and the "J2 held" bit, and no
  step bit. The driver's constructor takes it while it emplaces the hooks and the step driver, as the old
  construction scope did; its per-step body takes it for each step and then opens its own og-simulation `StepScope`
  inside, which sets the only "step is running" bit, read by the access predicates, the hooks' step checks and the
  contact recorder. `afterTick_Step` hands the step's saved state to the shadow world (§6), and `afterRunTick_Step`
  logs that `runTick`'s contact steps when the contact counter is installed (§9). `threadContext()` returns the
  context `build` configured.
  * Until og-tickdriver task 10 an interim world host, a private nested class of this class, forwarded these doors to
    this class's own scopes (a J2 scope and a step scope over this file's own masks, with the J2 scope mirroring its
    bit into og-simulation's mask for the driver's two J2 checks) and to its shadow and contact functions; task 9
    gave it the three present doors. Task 10 deleted it, with this file's masks, predicates and scopes and the mirror,
    so og-simulation's thread context is the only one: the driver's `StepScope`, its two J2 checks and the world
    host's predicates read the same masks.
  * The world host is also the presenter's world host, with the three present doors `TickDriverPresentWorldHost`
    requires: `hasShadowWorld`, `restore_GameThread` (the frame number and the presenter's restore note) and
    `overwriteShadowPoses_GameThread`, the world half of the shadow overwrite (§6, §7).
* **The thread context** (og-tickdriver BL1). The world host's `build` calls its context's `configure` with the role
  bit and the stepping mode before anything else, at world build on the game thread, before any adapter is emplaced,
  so the og-simulation owner mark is set on the game thread from then on: it is what the driver's owner-thread checks
  (the old IsInGameThread checks), its `[SimHost.FirstStep]` `thread=` word and an inline host's access predicate
  read. Task 8 had made the same call from this class's stepping-mode decision; task 10 moved it into `build`.
* **`startDriverStepping`**, at the end of `BeginPlay` (design §9.1 steps 7–9), through the seam's `startStepping`
  (where the Chaos configuration registers its callback): it reads `og.Sim.MaxCatchUpSteps` once and calls the tick
  backend's `start` with `dt`, the role and that cap (§11). `start` hands the world host the manager as the contact
  records' tick source (`bindIntegratedTickSource`, §9), builds the presenter (§5) with the role, `dt` and the world
  host's root declaration index, then the driver with `dt`, the role, the threading (`Inline` when the world host's
  context steps inline, else `FrameBounded`) and the cap (the driver's constructor builds the hooks and the step
  driver under the step-free lock, with `ResimPolicy::OnRequest`; the step driver parks every slot; then the
  scheduler, the window and the `[SimHost.Frame]` line), and returns the driver. This class then calls
  `m_frameHost.begin` with it, which registers the two tick functions.
* **The frame's console variables** are OGSimulationUnreal's since og-tickdriver task 15c (`UETickDriverHost.cpp`;
  `UETickDriverHost-rationale.md` §4): `og.Sim.MaxCatchUpSteps`, `og.Sim.BlockMode`, `og.Sim.RunInline` and
  `TaskGraph.TaskPriorities.OGSimStepTask`, with their names, defaults and help texts. `buildWorld` reads the stepping
  mode through `UETickDriverHost::runInline` and `startDriverStepping` reads the cap through
  `UETickDriverHost::requestedMaxCatchUpSteps`, each once, where this file read the variables itself before; the
  frame host reads the block mode every frame and its engine reads the priority at every dispatch, as they did through
  the getters. From task 11 until task 15c the four objects were this file's, and two free functions declared in the
  header (tickDriverBackendUImpl::stepTaskPriority and tickDriverBackendUImpl::blockMode, deleted by task 15c) were the
  getters the adapter's constructors took.
* **One step**, the driver's per-step body, on the step's thread: the inline check, `lockForStep()` (J2), the
  driver's `StepScope` (the "step is running" bit), the deadline into the hooks, `runTick(physicsStep)`, then
  `afterRunTick_Step`, that tick's contact lines when the contact counter is installed (§9).
* **J1 sites.** `tryRegister`'s Ready path (`onRegistered_GameThread`) pushes an Occupy command after `registerSimulatable` and
  `notifyCharacterRegistered`; the world host's `release` pushes a Vacate command, through the door
  the tick backend's `releaseSlot` passes, before it clears the bind table. Both go through the driver's
  `pushBodySlotOccupancy`, and the J2 wait reaches the window through its `noteWorldLockWait`, all through the tick backend's
  `driver()`, which `OG_CHECK`s that `start` built the driver (a `checkf` with the id until og-tickdriver task 15d). The shadow restore reaches the window through the driver's `noteShadowRestore` too,
  by the restore note the presenter passes (§6).
  A rejoin that rebinds the slot before the step drains the Vacate is safe: J1 is FIFO, so the
  Vacate applies first, and the new Occupy follows only from the new character's own Ready pass.
* **`EndPlay`** begins with `onEndPlayBegin`: the frame host's `unregisterTickFunctions` and then the tick backend's
  `stop()` (`⛔G-01`), which, when the driver was built, calls the driver's `stop()`: it clears the driver's running
  flag and waits for the pending and the blocking step task, so no step runs after it, before any teardown. The owner keeps the call first in `EndPlay`
  (`SimulationManagerUImpl-guards.md` G-85).
* **The per-frame and per-step input work** is `SimulationManagerUImpl-rationale.md` §7's Jolt call pattern;
  PROBE A's tick source is its §8's.

**Why the thread-local flags are keyed per role** (task 53 review N1). A one-process PIE can hold two managers, the
authority's and a client's (`s_instances`), on one game thread. With one process-wide flag, a server step running
inline would satisfy a worker client's predicate on the game thread, and a game-thread bind in one manager would make
the other's adapter accept a call it should refuse. So the masks have one bit per role, fixed by `build`, and each
manager's adapter gets the predicate instantiated for its own bit; the reasoning moved with the masks to
og-simulation (`TickDriverThreadContext-rationale.md` §2). Two client managers in one process are already refused at
`BeginPlay` (`s_instances`).

**Character contact solving and slots** (task 53 review N2, fixed by task 66). Jolt orders contact
constraints by body id, and a character's body ids are its slot's. Until task 66 each peer handed out
slots in its own registration order, so a client that joined late bound its own pawn first and put the
same character in a different slot, with different body ids, from the server; two characters touching
could then be solved in a different order on the two peers. Since task 66 the authority chooses the slot
and every client binds that slot (the slot table, above), so every peer gives a character the same body
ids. Queries were never affected (their hits sort by a peer-stable key). Nothing here claims bitwise
cross-peer determinism of the solver beyond that: the same bodies, statics and inputs are what Jolt's
rollback guide asks for, and prediction correction absorbs what remains.

## §5 The render publish and apply (task 55; og-tickdriver task 9)

The Chaos configuration's end of physics syncs every component to the physics result, interpolated
two steps behind. The Jolt configuration has no such sync, so the Jolt host does it for the one component
that needs it, the capsule root (design D11, §8).

**Since og-tickdriver task 9 the render code is og-simulation's** `TickDriverPresenter`
(`TickDriverPresenter-rationale.md` in og-simulation's docs), and the actor move and the reads of the actor are its
render sink, OGSimulationUnreal's `UERenderTargetTable` (`UERenderTargetTable-rationale.md`). Every log format, order
and condition below is the presenter's transcription of this class's old code. This class supplies:
* **The render source** is the tick backend's `RenderSource` since og-tickdriver task 15d (this class's nested
  BrawlerRenderSourceUImpl before it, historical name), the presenter's view of the session and of the step driver:
  `storage()` is the game binding's `storage()`, the owner's `m_storage`; `integratedTick()` the game binding's
  manager's `currentIntegratedTick`; `PhysicsComposite` the binding's simulatable's physics composite and
  `kMaxSimulatableSlots` og-simulation's slot count; `savesTicks()` and `grantedResims()` are the tick driver's two
  read-only doors. The presenter is built before the driver, so those two reach the driver only when they are
  called: only the step hooks call them, inside a step, after `start` built the driver.
* **The configuration**, in the tick backend's `start`, before the driver: the role (the manager's `runsPrediction`),
  `dt` (the step time the correction smoothing converts with) and the world host's `rootDeclarationIndex()`, which its
  `build` computed. That is before the first registration.
* **The controls**, `PresentControls`, filled every frame by OGSimulationUnreal's
  `UEPresentControls::fromConsoleVariables` (since og-tickdriver task 15c; `UEPresentControls-rationale.md`) from the
  six og.Render console variables, whose objects are in its `.cpp`: the delay, newest-only, the three correction
  settings and the trace switch (non-shipping). `beforePresent_GameThread` passes it the park snap, the binding's
  `BrawlerJoltBindingUImpl::kParkSnap` (this class's kRenderParkSnapCm until og-tickdriver task 15a), so the park snap
  stays the game's. Until task 15c the fill and the objects were this file's (renderPresentControls, a function
  template over the binding since task 15b). `og.Render.ErrorCorrectionMaxCm`
  is converted from centimetres to og-simulation lengths by the table's `lengthFromCentimetres`, and
  `lengthReportScale` is 1.0, so every field whose name ends in Cm prints the same number as before; both are
  identities while the simulation runs in centimetres.
* **The render sink**, `m_renderTargets`. `onRegistered_GameThread` calls the tick backend's `onRegistered`, which
  finds the character's slot, pushes its Occupy command and calls the presenter's `onRegistered` with the slot, and
  then the table's `registerTarget` with the character, its capsule (`GetCapsuleComponent()`) as the expected root and
  the role. The table switches the root's physics simulation off and logs
  `[SimHost.RenderTarget]`; in a non-shipping build it `checkf`s that the capsule is the actor's root, which the
  brawler's constructor makes it, because the table moves the actor and reads its root.
  The brawler's root is its capsule (`OGBrawlerUECharacter.cpp`, `RootComponent = GetCapsuleComponent();`), so the
  table's no-sweep guard (`UERenderTargetTable-guards.md` G-01) protects the capsule here: a swept move would leave
  the capsule, which is the mouse-aim plane point and the camera boom's anchor, away from the simulated pose, and
  the aim fed into the next simulated tick would be measured from the wrong point. Render sync is input-relevant
  for the brawler ("Why it is not cosmetic" below).
* **The two game-thread doors.** `beforePresent_GameThread` calls the tick backend's `present` with the controls and
  the table, and `present` calls the presenter's `present_GameThread` with the tick driver as the clock, the controls,
  the table and the world host (`host` in `m_backendWorlds`); `updateVisualization_GameThread` calls the tick
  backend's `updateVisualization`, which calls the presenter's `updateVisualization_GameThread` with the game
  binding's storage, the owner's.

* **Why it is not cosmetic.** The capsule is the mouse-aim plane point: the input component's
  `updateGameThreadCache` projects the mouse onto the horizontal plane through the capsule's world
  translation (`OGBrawlerInputCollectionComponent.cpp`, the plane point read before the line-plane
  intersection), and the camera boom hangs off the capsule. Without the apply the aim input fed into
  the simulation is measured from wherever the capsule spawned (design F7).
* **The snapshot type.** `RenderSnapshot` is `RenderSnapshotT<kMaxSimulatableSlots * 6>`, declared with
  the other brawler aliases: one body per declaration of the brawler's physics composite in every
  slot. Two `static_assert`s of the presenter hold it: the snapshot's body count equals the slot count times the
  composite's declaration count, and the snapshot is trivially copyable (no heap-owning member).
* **Publish, on the step's thread.** `afterTick` calls the presenter's `publish_Step` with the outcome and the
  step's deadline: take a slot (`beginWrite`), fill it from the render source's storage (`fillRenderSnapshot`),
  `commit`. Once per `runTick`, on every
  role, never in a replay (the driver calls no hook then). The fill runs after the post-step pass, so it
  reads this step's poses from og-simulation's storage, keyed by the peer-stable storage key; it reads
  storage on the step's thread inside the step, as every other step code does, so it adds no crossing
  beyond J5 itself.
* **No allocation after warm-up.** The channel's four slots are built with the presenter, and
  og-simulation's own test pins that the channel allocates nothing after construction. The fill writes
  the slot in place; the storage walk iterates the existing maps; the key sort is an in-place sort over
  the slot's array; the snapshot owns no heap storage (the second `static_assert`).
* **Apply, on the game thread.** The owner's `OnPostPhysicsStep` calls `beforePresent_GameThread`, and through it the
  presenter's `present_GameThread`, before its
  shared body (after the non-shipping hitch sleep and, on a worker client, after the shadow-world restore
  of §6),
  every frame, on every role. The Chaos configuration has no apply: the engine's sync does that job.
  Task 55's apply peeked the newest snapshot; since task 19 it moves the capsule to the render
  interpolation's pose (§7), and `og.Render.NewestSnapshot` 1 restores the newest. For every character in
  the presenter's target table, filled on `tryRegister`'s Ready path (`onRegistered_GameThread`) and erased by
  `unregisterFromNewFramework` (`onUnregistered_GameThread`, which calls the tick backend's `forgetPresenterTarget`,
  the presenter's `onUnregistered`, and the table's `unregisterTarget` before the tick backend's `releaseSlot`),
  it finds the root body by storage key and the root declaration index with a binary search over the
  sorted bodies, and moves the actor there through the table's `applyRootPose`, with teleport and no sweep
  (`UERenderTargetTable-guards.md` G-01; this file's G-04 until og-tickdriver task 9). The rotation is the
  snapshot's only when the body carries one; the brawler's capsule body does not, so the actor keeps its
  own. Then it releases the snapshot.
* **The root declaration.** The world host's `build` takes it from the slot template's one `isRoot` body (an
  `OG_CHECK` demands exactly one) and returns it from `rootDeclarationIndex()`; the presenter's configuration
  carries it. The template and
  the snapshot both enumerate the
  composite in its declaration order, so a template index is a declaration index.
* **On a worker client** the newest committed snapshot may come from this frame's step task, still
  running under block mode 0: a newer pose, never a torn one.
* **The non-shipping check** (`[SimHost.RenderApply]`, `LogOGSimHost`), two comparisons per registered
  character every frame, against the translation the aim plane reads (the table's `readAnchor`, the root's
  translation, which is the capsule's):
  * *held* (the presenter's `checkRenderTargetsHeld`, before the apply): the capsule still sits where the
    last apply put it. Between two applies lie the next frame's pre-physics group, where the aim is
    read, and the engine's own physics groups; anything that moves the capsule there logs a MOVED
    Warning.
  * *applied* (the presenter's `checkRenderApply`, after the apply): the capsule equals the rendered pose
    (the newest snapshot's until task 19, the interpolated one since); otherwise a MISMATCH Warning.

  The held tolerance is 1e-6 cm: the snapshot pose is a float, exact in double, and the engine moves
  the component by a double delta from the old location, so one rounding of that sum is possible.
  Task 55 measured every comparison exact. Since task 19 the applied tolerance is the engine's own
  no-move threshold (§7). A window line every 600 frames gives
  frames, frames with a snapshot, characters, comparisons, missing (a registered character absent from
  the newest snapshot: until the first snapshot published after the character's occupancy push, one or more frames per join on a worker client), exact, out of tolerance, the largest error, held
  comparisons, moved, the largest drift, the path the applied poses travelled (it shows the check
  compares moving capsules) and the snapshot steps; a first line per character gives the spawn-to-pose
  teleport.

## §6 The game-thread shadow world (task 56; og-tickdriver task 10)

On a client the step runs on a worker task, so the game thread may not query the step world: it
changes under the query, and the query adapter's predicate refuses any caller that neither runs the
step nor holds J2. The visualizations still need a world to query every frame. Chaos gives them its
game-thread copy of the scene, refreshed at the end of physics. The Jolt configuration does the same
with a second world that only the game thread touches (design D §2.5, "(e) in detail"; OQ13 = (e)).

Since og-tickdriver task 10 the shadow world, its build, its bind and release mirrors, its bind-table comparison,
its hand-over, its restore and its window are og-simulation-jolt's world host's (`JoltTickWorldHost-rationale.md`
§10 and §11 in og-simulation-jolt's docs, each step and line the transcription of this class's old code). What is
the brawler's:

* **Who.** Worker clients only: the stepping mode decides it at `BeginPlay` and it never changes. The
  authority and a client with `og.Sim.RunInline` 1 step on the game thread, so their visualizations
  query the step world between steps and they build no shadow.
* **Construction** (the end of the world host's `build`): the step world's configuration with the ring depth 0 and
  the 64 KiB temp allocator, instead of the step world's 16 MB (the shadow never steps; og-simulation-jolt's two-world
  tests restored and queried such a world), the same two static builds, its own three adapters, and the shadow
  predicate: the game thread only. The shapes are built twice rather than shared, which needs no og-simulation-jolt
  change (`JoltTickWorldHost-guards.md` G-03). The non-shipping build check and the `[SimHost.Shadow] built` line
  follow. Jolt's own body count is not reachable from this module: Jolt's body manager is not exported from the
  OGSimulationJolt DLL, so the editor build cannot link it, and the check counts the slot bodies plus the bodies the
  two static builders created. A static created outside the two builders would not be counted; the host creates
  none.
* **Bind mirroring.** `tryRegister`'s bind (`createBodies_GameThread`, through the world host's `bind`), after the J2
  scope closes, runs the same `JoltPhysicsFactory` bind over the shadow's adapters (`JoltTickWorldHost-guards.md`
  G-04), on the game thread without J2, because the step never touches the shadow. The declarations' own query
  volumes are not registered in the shadow: they belong to the step's hit detection, and the visualizations use only
  the volume `registerVizVolume` registers. The slot release is mirrored too (G-05 there).
* **Hand-over, on the step's thread** (the world host's `afterTick_Step`, from the hooks' `afterTick`, after the
  render publish and before J3), only when the step saved a state; and **restore, on the game thread** (the world
  host's `restore_GameThread`, first in `OnPostPhysicsStep`'s `beforePresent_GameThread` on a worker client, before
  the render apply; design D §4.2): the presenter's `present_GameThread` calls it with the engine's frame number and
  a restore note, and each restore's time and whether it was refused go through that note to the tick driver's
  `noteShadowRestore`, into `[SimHost.Window]`, as the frame host's note did before og-tickdriver task 8. The channel
  is the world host's `SnapshotChannel<ShadowStateSlot, 3>`, not the design's `SnapshotChannel<JoltStateSlot, 3>`,
  on purpose: its slot wraps the state slot and adds a sequence number, which the restore compares with the last one
  it restored, so it restores each published slot at most once and only a newer one. A refused slot (og-simulation-jolt's
  "refused: the snapshot's body set differs from the live body set" Warning) is not retried. Never in a replay:
  the driver calls no hook then.
* **Timing.** A visualization in frame k reads the state restored at frame k − 1's end of physics, as
  Chaos's queries read the copy its end of physics refreshed. Under block mode 0 the newest slot may
  come from a step task still running: a newer state, never a torn one, because only committed slots are
  handed to the reader. Since task 19 the shadow's slot bodies are then set to the interpolated render
  pose (§7), so the visualizations query the rendered pose.
* **The non-shipping window** (`[SimHost.Shadow] role=Client`, every 600 frames; the world host's §11 lists its
  fields). Its `vizQuery` and `vizReader` counts are how often this class's `editVizQuery()` and `getVizReader()`
  handed out the shadow's or the step's adapters, and its two predicate counts are the shadow predicate's calls and
  the step adapter's predicate calls on the game thread outside J2 (each such call is also an access assert).

## §7 The render interpolation (task 19)

Task 55 moved each capsule to the newest snapshot. At a render rate that is not a multiple of the
60 Hz step (the first Jolt PIE's clients ran at 100 fps), that apply holds the capsule on some frames
and moves it a whole step on others: a 3:2 step/hold judder, which the weapon and guard drawings,
taken from the newest tick, shared. Chaos rendered every body two steps behind, blended between the
two results that bracket that time (the engine's p.AsyncInterpolationMultiplier, default 2, and its
results manager). This configuration now does the same (design D §8).

* **The render time** is the tick driver's `hostNowSeconds()` minus the delay times its
  `stepIntervalSeconds()`: the driver's time base, the one the snapshots' step deadlines come from
  (`SimulationTickDriver-rationale.md` §6; before og-tickdriver task 8 the frame host's two doors). The delay is `og.Render.InterpolationDelaySteps`,
  default 2, read every frame and clamped to [0, 2]. Never sim ticks: a Stall step (the tick does not
  advance) and a Skip step (it jumps) each step the world once and publish a snapshot at their own
  deadline, so the render clock neither stops nor jumps with the sim clock.
* **The core (task 67).** The bracket, the alpha and the blend are og-simulation's engine-free
  `interpolateRenderPoses` (`OGSimulation/RenderInterpolation.h`; its rationale is
  `RenderInterpolation-rationale.md` in og-simulation's docs). The presenter's pose build (this class's
  until og-tickdriver task 9) computes the render time from the controls, passes the snap distance, and calls it
  with J5 and the pose set; it keeps the core's per-frame summary for the window line and the trace. Task 19 had
  the same code in the UImpl; task 67 moved it unchanged, so another game can reuse it.
* **The *next* copy**, the presenter's `m_renderNext`: every frame, in every build including Shipping, the core also
  copies the bracket's *next* snapshot, unblended, through its `nextCopy` argument into this member.
  The correction smoothing reads it (the snapped bodies' straddle, §8), and the
  `[SimHost.RenderTrace]` line reads its root pose. It is one bounded copy of a flat snapshot per frame,
  into a member, so nothing allocates. It used to be taken only for the trace, under
  `og.Render.TraceFrames`, in non-shipping builds.
* **The bracket** follows Chaos's rule: *next* is the oldest published
  snapshot whose deadline is at or after the render time, *prev* the one before it, and alpha is the
  render time's position between their two deadlines. With no snapshot after the render time the
  newest is drawn (alpha 1, no extrapolation, as Chaos); with none before it (the first frames after a
  join) the oldest one reachable is. The walk peeks back 0 first (the channel's read-start view) and
  holds at most two snapshots: when a peeked older snapshot is still at or after the render time, the
  newer one is released. Two `static_assert`s tie this to J5: the core's demands a channel that keeps
  its newest snapshot while two are held (four slots, task 44's sizing rule), and the presenter's keeps
  the delay clamp within what four slots can bracket (the step before the render time is at most the
  delay's whole part plus one behind the newest). The headless runs never reached past back 1.
* **The pose set**, the presenter's `m_renderPoses`: a copy of *next* whose bodies are blended from *prev* with alpha,
  positions linearly and rotations (bodies that carry one) by slerp (the core's `blendRenderPoses`).
  The core snaps above a distance its caller passes; this class passes the binding's
  `BrawlerJoltBindingUImpl::kParkSnap` as the controls' `parkSnap` (half the
  projectile pool's park depth, 500 m): a body whose two poses lie farther apart is a pooled
  projectile being parked or unparked, not motion, so it takes *next*'s pose unblended and counts as a
  snap; every other jump (a respawn) is blended, as Chaos blends it. A body missing from *prev* (a
  character that just joined) takes *next*'s pose.
* **Who reads it**, in `OnPostPhysicsStep`, through the presenter's `present_GameThread` (the shadow restore,
  the apply, the shadow overwrite, in that order) and then the visualization copy:
  * the capsule of every registered character (the apply, through the table), the mouse-aim plane point;
  * on a worker client, the shadow world (the presenter's overwrite, and the world host's
    `overwriteShadowPoses_GameThread` for the world half, which keeps the check below): after this frame's
    restore, every occupied slot body bound in the shadow is set to its pose with the shadow body
    adapter's `setBodyTransform` (no activation; a body without a rotation keeps the restored one). The
    target and block-prediction queries of the next frame then find the bodies where they are drawn, as
    Chaos's queries found its interpolated game-thread copy (design OQ10). An `OG_CHECK` demands that the
    restore ran this frame first: a restore after the overwrite would put the newest tick back;
  * the visualization copy (the presenter's `updateVisualization_GameThread`, called by this class's door of the
    same name): after
    `updateVisualizationAll` it writes the pose into the body states of every declaration except the
    root (the weapon axis, the guard, the projectile pool), so those drawings follow the rendered
    character. The root body keeps the newest tick (`SimmableUpdateComponent-rationale.md` §10, "The
    movement draw and the input-history poll").
  * Inline roles (the authority, and a client under `og.Sim.RunInline`) have no shadow: their
    visualizations query the step world between steps, at the newest tick. A dedicated server draws
    nothing.
* **`og.Render.NewestSnapshot` 1** draws the newest snapshot with no delay and no blend, task 55's
  apply, to measure the difference.
* **Not done here:** Chaos's optional second results channel. The correction smoothing (task 57) adds
  an error term to this pose set after the blend: §8.
* **The capsule check tolerates the engine's no-move threshold.** The engine does not move a component
  whose new location equals the old one within its kinda-small number (1e-4 cm) on every axis (the
  location comparison in the scene component's internal set-world-location-and-rotation). The blended
  idle hover moves less than that on some frames, so the capsule can stay up to 1e-4 cm per axis from
  the pose. The *applied* check therefore compares with that same per-axis tolerance and counts exact
  matches separately; the apply records where the capsule actually ended, so the *held* check still
  demands 1e-6 cm.
* **Lines** (`LogOGSimHost`, non-shipping unless stated):
  * the `[SimHost.RenderApply]` window line also prints `mode` (interp or newest), `delaySteps`,
    `interpolated` (frames blended between two snapshots), `atNewest` (render time past the newest),
    `beyondOldest` (render time before the oldest reachable), `snaps`, `maxNextBack`, and `dispCm`, the
    capsules' per-frame displacement as p50/p99/max;
  * `[SimHost.RenderSync]`, on a worker client every 600 frames: the shadow bodies overwritten, each
    read back through the reader the visualizations use, with exact matches, out of tolerance (1e-3 cm
    or 1e-4 rad: the pose goes from centimetres to Jolt's metres and back through floats), the largest
    error and angle; a SHADOW Warning on the first one out of tolerance;
  * `[SimHost.RenderTarget]`, every build, once per character when it registers: whether the capsule
    simulates physics (the render-target table's registration switches it off; `OGBrawlerUECharacter-rationale.md`
    §1.2) and
    which of the character's components still do (the cosmetic camera-axis sphere);
  * `og.Render.TraceFrames` 1: one `[SimHost.RenderTrace]` line per frame per character (rendered
    position, the *next* step's pose, host and render time, the bracket's steps and kinds, alpha, and
    since task 57 the correction offset `err`), for offline analysis.

## §8 The render correction smoothing (task 57)

A resimulation rewrites ticks the render has drawn or is about to draw. The render interpolation blends
the last published step of the old timeline into the first one of the corrected timeline, so the
whole correction is crossed in one step: the first Jolt PIE showed it as pops on the remote
characters, which are corrected most. Chaos hides a correction by keeping the body where it was drawn
and blending the difference out over p.RenderInterp.ErrorCorrectionDuration (0.3 s), snapping above
p.RenderInterp.MaximumErrorCorrectionBeforeSnapping (250 cm). This configuration does the same with
og-simulation's engine-free `RenderCorrectionSmoother` (`OGSimulation/RenderCorrectionSmoothing.h`;
its rationale, with the Chaos semantics it reproduces and every difference, is
`RenderCorrectionSmoothing-rationale.md` in og-simulation's docs). Since og-tickdriver task 9 the capture point,
the ring and the lines are the presenter's; this class supplies the cvars.

* **Capture, on the step's thread.** The capture itself is og-simulation's engine-free
  `RenderCorrectionCapture` (the presenter's; its call contract and outcomes are §13
  of `RenderCorrectionSmoothing-rationale.md`); the presenter decides when to call it and counts what it
  returns. On a client (the step driver's `savesTicks`, read through the render source and the tick driver's door),
  the presenter's `publish_Step` hands it every
  snapshot it publishes (`noteShownSnapshot`): the pose the render will draw for that tick. The hooks'
  `beforeSimulate` calls the presenter's `captureCorrection_Step` with the step's physics step. `beforeSimulate` runs
  after the
  driver's replay and before the step: the one point where the storage holds the corrected present
  tick, which is the tick of the last published snapshot. It calls `onStepAfterReplay` with the
  step driver's granted-resimulation count (the render source's `grantedResims()`, the tick driver's door), the
  manager's `currentIntegratedTick` and that step, and counts
  the outcome: every outcome but `NoNewResim` is a resimulation (`resims`), `TickMismatch` adds a tick
  mismatch, `NoError` a resimulation below Chaos's thresholds (`withoutError`), and on `Captured` it
  pushes the correction into J7 (`SimulationManagerUImpl-rationale.md` §1), counted as `captured` when the ring took it. The correction's
  carrier step is the step being run, whose snapshot `afterTick` publishes next. The authority never
  resimulates, so its count never moves and it captures nothing.
* **Why there, and not on the game thread.** The game thread never receives the replayed ticks: the
  hooks run once per `runTick`, never during a replay, so only the step's thread ever has both poses of
  the corrected tick. Chaos is shaped the same way: its rewind data caches each body's pose when the
  rewind starts and computes the error after the last replayed step, on the physics thread, and the
  game thread applies it when it moves to the results that carry it.
* **Apply, on the game thread.** The presenter's pose build calls `correctPoses` with J7 right after
  `interpolateRenderPoses`, with the three correction controls and the fixed step (the configuration's
  `stepSeconds`, the step driver's `dt`, which is the physics settings' async fixed step that Chaos converts with
  too). It drains J7 and accumulates each correction on the first frame whose bracket reaches its
  carrier step. The order is a data dependency, not a convention: `correctPoses` takes the bracket's
  frame, which `interpolateRenderPoses` produces. The error is added to the pose set, so everything
  that reads the pose set moves with it: the capsule (the mouse-aim plane point), the shadow
  overwrite on a worker client and the visualization copy (the weapon axis, the guard, the projectile
  pool). The `[SimHost.RenderApply]` and `[SimHost.RenderSync]` checks compare with the pose set, so
  they hold as before.
* **The weapon and the guard snap.** Their declarations (`dAttackRadialSimulation::PhysicsDeclaration`,
  `dAttackGuardSimulation::PhysicsDeclaration`) declare `RenderCorrectionMode::SnapInRootFrame`;
  the movement capsule and the projectiles keep `Smooth`. `correctPoses` therefore keeps no error for
  those two bodies: it draws each at its interpolated corrected pose plus the offset the capsule is
  being drawn with, so the blade angle and the guard facing are the corrected ones from the first
  corrected frame and both stay on the character (§14 of `RenderCorrectionSmoothing-rationale.md` in
  og-simulation's docs). The capsule carries a `LinearBodyState`, so its `RenderBody::hasRotation`
  is 0, which the smoother's root-frame pass checks of every root. The policy is compile-time and
  always on: there is no cvar, so nothing here reads one. The presenter only counts it, in the lines
  below.
* **On the straddle they take the corrected rotation.** `correctPoses` also gets the presenter's `m_renderNext`. On
  the bracket whose *prev* is the last old-timeline step and whose *next* is a correction's carrier
  step, the interpolation would turn the weapon and the guard from the old rotation to the corrected
  one across that step: for one rendered frame or so, a blade between the two. The smoother instead
  gives those two bodies *next*'s rotation on that bracket. Their position stays the blended one plus
  the capsule's offset, so they stay on the character (§15 of `RenderCorrectionSmoothing-rationale.md`
  in og-simulation's docs). Chaos drew the weapon from the newest tick, so a correction popped there
  with no in-between frame; this keeps that, two steps behind like everything else.
* **The cvars**, read every frame: `og.Render.ErrorCorrectionSeconds` (0.3),
  `og.Render.ErrorCorrectionMaxCm` (250) and `og.Render.ErrorCorrectionDesyncSeconds` (0.6), Chaos's
  p.RenderInterp.ErrorCorrectionDuration, MaximumErrorCorrectionBeforeSnapping and
  MaximumErrorCorrectionDesyncTimeBeforeSnapping with their defaults. With the project's step
  (0.016667 s) 0.3 s is 17 steps, the error showing for 16 of them; 0 seconds turns the smoothing off
  (every correction snaps), to compare.
* **Cost.** On a client, each publish copies the snapshot (48 bodies) once. Every step, on every
  peer, builds the correction the capture may write on the step's stack (about 3.5 KB, zeroed).
  Only a resimulation step captures: one storage walk to fill the corrected snapshot, a key lookup per
  body, and a second walk for the speeds. Each frame drains the ring and updates and applies at most one error per body, in
  fixed arrays: nothing allocates.
* **Lines** (`LogOGSimHost`, non-shipping):
  * `[SimHost.RenderCorrection]`, on a client every 600 frames after the `[SimHost.RenderApply]` line:
    `resims` (granted resimulations the capture saw), `captured` (pushed into J7), `withoutError`
    (resimulations below the thresholds), `tickMismatches`, `ringDrops`, then the game thread's
    `corrections` accumulated, `smoothedBodies` and `snappedBodies`, `maxErrorCm`, `maxAppliedCm`,
    `decays` (windows that closed), `decayMeanS` and `decayMaxS` (their length from the accumulation,
    in steps times the step), `activeFrames`, `late`, `pendingDrops`, the settings (`seconds`,
    `durationSteps`, `maxCm`, `desyncS`), then the snapped bodies: `policySnaps` (corrected bodies
    whose error was dropped because they snap, summed over the window), `maxPolicySnapCm` and
    `maxPolicySnapDeg` (the largest dropped position and rotation error) and `rootFramedFrames`
    (frames on which at least one snapped body was moved by its capsule's smoothing offset), and
    `straddleSnaps` (snapped bodies given the carrier step's rotation on a straddle, summed over the
    window). A snapped body is not in `smoothedBodies`, `snappedBodies`, `maxErrorCm` or
    `maxAppliedCm`;
  * a LATE Warning, at most once per window, when a correction reached the game thread after its
    carrier step had been drawn;
  * with `og.Render.TraceFrames` 1, one `[SimHost.RenderCorrection]` event line per frame that
    accumulated a correction, ending with that frame's `policySnaps` and `rootFramed` (the snapped
    bodies moved by their capsule's offset that frame), and the `[SimHost.RenderTrace]` line's `err`,
    the offset the character's root body received that frame.

## §9 The contact counts (task 78; og-tickdriver task 10)

A session log could not tell a divergence caused by contact from one caused by different play: no line
recorded which bodies touched. With `og.Jolt.ContactCounts` 1 the world host installs og-simulation-jolt's
`JoltContactCounter` (a passive Jolt contact listener; what it counts, why it changes no physics and
why its counts are exact on any job system are in `JoltContactCounter-rationale.md` in
og-simulation-jolt's docs) and logs, after every simulated step that touched a contact, one
`[Verbose][SimHost.Contacts]` line. Since og-tickdriver task 10 the counter, the per-step records and the lines are
the world host's (`JoltTickWorldHost-rationale.md` §9 and §12, each the transcription of this class's old code);
this class reads the cvar, passes it in the world host's configuration and hands it the manager as the tick source.
From og-tickdriver task 15a the cvar object and its read were the Jolt engine traits'; since og-tickdriver task 15e
they are the brawler's Jolt binding's (`BrawlerJoltBindingUImpl::contactCountsEnabled`,
`BrawlerJoltBindingUImpl-rationale.md` §6), because the traits moved to a module that cannot see the game's backend
switch (`JoltEngineTraitsUImpl-rationale.md` §3). This class calls the read where it read the cvar.

* **The cvar is read once, when the step world is built** (`buildWorld`, into the configuration `build` takes, at
  `BeginPlay`). Jolt calls the listener from inside a step, and a resimulation is many steps in one `runTick`: a
  listener installed or removed mid-session would count part of a step or part of a replay, and the drain buffer is
  sized at the build. A later console command changes nothing, so the cvar is set at startup through the engine ini
  override (below). The help text says so.
* **The step world only.** The game-thread shadow world never steps, only restores, so it would count nothing, and
  it never gets a listener. One
  `[SimHost.Contacts] installed=<0|1> stepListener=<set|null> shadowListener=<set|null|none> reserved=<records>`
  Warning at the build, and an `OG_CHECK` behind it, hold that.
  With the cvar 0 the line reads `installed=0 stepListener=null`: no listener, no observer, and the only per-step
  cost is one `has_value` check in the world host's `afterRunTick_Step` and one null check in the world's step.
* **The tick** of each record is the manager's `currentIntegratedTick`, the tick that step integrated:
  the tick backend's `start` (§11), from `startDriverStepping`, hands the world host the manager, the game binding's
  `manager()` (`bindIntegratedTickSource`), before the driver exists, so before the first step.
* **The replay flag is the record's place in the batch**, because the hooks fire once per `runTick`, none during
  the replay (the world host's §9). The buffer is reserved for the ring depth plus one; the authority has no ring
  and never replays, so it reserves one.
* **Reading the line** (`LogOGSimHost`, Verbose; only for a step with a contact, and the strings are built only
  when the category is at Verbose):
  * `tick` joins `[DivergenceProbe.Correction]`'s tick; `step` is the scheduler's physics step of the
    `runTick`, the same on a resimulation's replayed lines and its forward line, and the value
    `[Resim.Prepare]` prints as `chaosStep`; `worldStep` orders the lines one by one.
  * `added` counts the manifolds that were not in the contact cache the step started from. On the first
    replayed step that cache is the restored snapshot's, so `added` there is what a restored cache did
    not hold.
  * The six pair counts sum to `manifolds`; `staticStatic` counts unbound non-root dynamic bodies and
    is 0 in this game.
  * `ids` lists each simulation character id that took part, sorted, as
    `id:rootRoot/rootChild/rootStatic/childChild/childStatic:0x<declarations>`, or a dash. The declaration
    mask has bit i for each child body of declaration index i that touched: the brawler's composite
    order, 0 the weapon (`dAttackRadialSimulation`), 1 the guard, 2 to 4 the projectiles; the capsule
    (5) is the root and is not a child. The line prints indices, not names: the counter is
    game-agnostic, and the mapping is this paragraph.
  * `idsDropped` and `declarationsDropped` are the counter's overflow counts; the brawler binds at most
    8 ids with 6 declarations, so both are 0.
  * On ThirdPersonMap a headless session logs almost only while two characters overlap, which happens when two of
    them got the same player start, so a session's line rate depends on its spawn points, not only on the code
    (og-tickdriver task 10: of 59,838 lines in three Jolt sessions, all but 7 were a `rootRoot` contact between two
    ids that had got the same start, and the 7 were single `rootStatic` contacts).
* **Enable it** with the cvar and the category, on each peer that should log:

  ```
  -ini:Engine:[ConsoleVariables]:og.Jolt.ContactCounts=1 -ini:Engine:[Core.Log]:LogOGSimHost=Verbose
  ```

  The game module registers the cvar after the engine applies the ini's console variables; the engine
  keeps such a value in a placeholder and applies it when the cvar is registered, which is before the
  world's `BeginPlay`. The build line shows `installed=1`.
* **The Chaos host has no equivalent.** The cvar, the counter and the line exist only in the Jolt configuration;
  a Chaos build prints no `[SimHost.Contacts]` line, and Chaos collision events are out of scope.

## §10 The template (og-tickdriver task 15b)

*Since og-tickdriver task 15e the class has three parameters, `TickDriverBackendUImpl<GameBindingT, EngineTraitsT,
EngineBindingT>`, and the requirement, the table and the instantiation below changed as §12 says. This section keeps
task 15b's record; `BindingT` here is today's `EngineBindingT`.*

Since og-tickdriver task 15b the class is `TickDriverBackendUImpl<EngineTraitsT, BindingT>`: the host for every engine
that og-simulation's tick driver schedules. Its body is the class as it was, in the same order; only the types it
names come from its parameters.

**The requirement.** The class template requires `TickDriverBackendComposition<EngineTraitsT, BindingT>`, a concept in
its header that is the conjunction of:
1. `PhysicsEngineTraits<EngineTraitsT>` (`PhysicsBackendUImpl-rationale.md` §2);
2. `EngineTraitsT::kHasWorldHost`: the engine is scheduled by the tick driver;
3. `BrawlerEngineBinding<BindingT, EngineTraitsT>` (§3 there), which for such an engine also asks for the capsule check
   and the park snap;
4. `TickDriverWorldHost<typename EngineTraitsT::template WorldHost<typename BindingT::Simulatable>>`: the engine's world
   host, over the binding's simulatable, models og-simulation's driver concept.

The Chaos traits fail item 2 (`ChaosEngineTraitsUImpl-rationale.md` §3), so
`TickDriverBackendUImpl<ChaosEngineTraitsUImpl, BrawlerChaosBindingUImpl>` does not compile: task 15b quoted the compiler's "the associated constraints are not
satisfied", with the concept named.

**What comes from where.**

| what | from |
|---|---|
| the world host type (`WorldHost`) and its configuration type (`WorldHost::Config`) | the traits' `WorldHost` template over the binding's `Simulatable` |
| the worlds struct (`Worlds`, `TickDriverBackendWorldsUImpl<WorldHost>`) | this header, over that world host |
| `createBodies_GameThread`'s simulatable parameter, the tick backend's simulatable (its render source's physics composite) | the binding's `Simulatable` |
| the static import's category table, the mapped categories, the capsule check, the park snap | the binding's `categoryChannels`, `mappedCategories`, `checkCapsuleContract`, `kParkSnap` |
| the engine log sink, the contact-count switch, the visualization types | the traits' `logEngineMessage`, `contactCountsEnabled` (the binding's since og-tickdriver task 15e), `VizQuery`, `VizReader` |

**One instantiation, in the game module.** Until og-tickdriver task 15e the header declared the members, and the
`.cpp` defined them and ended with the explicit instantiation over the Jolt traits and the Jolt binding, inside the
file's `OGSIM_OPTIMIZE_OFF` span like the definitions (§12 says where both are now). The definitions stayed in the `.cpp` because they named
og-simulation-jolt's statics split (and, until og-tickdriver task 15c, the file's console variables). The instantiation is in OGBrawlerUnreal (still, since task 15e), so the driver, its
`StepScope` and the world host's `configure` are instantiated there and nowhere else
(`TickDriverThreadContext-rationale.md` §7 in og-simulation's docs).

**The game binding stays a class template.** `BrawlerTickDriverGameUImpl` is a template over the session because the
session's header includes this one before the session class is defined (`BrawlerTickDriverGameUImpl-rationale.md`
§1). Task 15b did not change that order: the session still holds the backend by value, so it must see the complete
host type first.

**What is still not generic** (the inputs of og-tickdriver tasks 15d and 15e; §11 says what task 15d moved, and §12
what task 15e moved: every item of this list).
* Brawler-only, in the bodies: the `AOGBrawlerUECharacter` cast and its `GetPhysicsSlot` / `SetAuthoritativePhysicsSlot`
  in `callWithSlotRequest`; the character's `GetCapsuleComponent()` (a `UCapsuleComponent`) in `onRegistered_GameThread`;
  `BrawlerTickDriverGameUImpl` over `ASimulationManagerUImpl`; the owner's members (`m_staticData`'s gravity, the
  three adapter optionals, `runsPrediction`, `GetNetMode`; `m_storage` and `m_manager` until og-tickdriver task 15d,
  read since through the game binding by the tick backend); `USimmableUpdateComponent::getStaticData`; and, until task
  15d, `BrawlerCompositionUImpl`'s storage and render snapshot and `kMaxSimulatableSlots`, which the tick backend now
  takes from the game binding and og-simulation.
* Jolt-only, in the `.cpp` only: og-simulation-jolt's `joltTickWorldHost::splitStatics` and its `JoltTickStatics`
  result in `buildWorld` (kept here by og-tickdriver task 15d, lead ruling R6; moving the call into the engine traits
  is task 15e's); and the text of one `checkf` message (`callWithSlotRequest`), which still says "Jolt slot" and is
  unchanged so that every line stays the same. The second such message, `tryRegister`'s, became the tick backend's
  `OG_CHECK` in task 15d (§11). The header names no Jolt symbol. (The console variables, whose help texts say
  "Jolt configuration only", are OGSimulationUnreal's since og-tickdriver task 15c.)
* Read by the bodies but not required by the concepts: the traits' `contactCountsEnabled` and the binding's
  `mappedCategories`. A traits or binding without them fails at the instantiation, inside the body.

## §11 The tick backend (og-tickdriver task 15d)

Since og-tickdriver task 15d the orchestration this class wrote is og-simulation's engine-free
`SimulationTickBackend<GameT, WorldHostT, SimulatableT, EngineT>` (`SimulationTickBackend-rationale.md` in og-simulation's
docs, MPL-2.0 by the user's ruling of 2026-10-18). This class holds it as `m_tickBackend`, over the game binding
(`BrawlerTickDriverGame`), the world host, the binding's `Simulatable` and `SimulationEngineUImpl`, constructed with
the game binding, the worlds struct's `host` and the frame host's engine. Its presenter is
`TickDriverPresenter<RenderSnapshot, SimulationEngineUImpl, RenderSource>` with the game binding's `RenderSnapshot`,
the same type this class's own presenter alias named before.

**What the tick backend does, called from here.**

| this class | the tick backend | what runs, in this order |
|---|---|---|
| `buildWorld`, after the import, the split and the configuration | `build` | the world host's `build` with the same arguments |
| `startDriverStepping`, after the `m_backendWorlds` row and the persistent-level `checkf` | `start` | `bindIntegratedTickSource` with the manager; the presenter; the driver; returns the driver to `m_frameHost.begin` |
| `onEndPlayBegin`, after `unregisterTickFunctions` | `stop` | the driver's `stop()`, when it was built |
| `createBodies_GameThread`, inside `callWithSlotRequest`, after the capsule check | `acquireAndBind` | the world host's `acquire`, then, with a slot, its `bind` with the driver's `noteWorldLockWait` |
| `onRegistered_GameThread`, before the character cast and `registerTarget` | `onRegistered` | the world host's `slotOf`, an `OG_CHECK` that there is a slot, the driver's `pushBodySlotOccupancy(slot, true)`, the presenter's `onRegistered` |
| `onUnregistered_GameThread`, first | `forgetPresenterTarget` | the presenter's `onUnregistered` with the slot (or 0), when the presenter was built |
| `onUnregistered_GameThread`, after `unregisterTarget`, when the component has an owner | `releaseSlot` | the world host's `release` with the driver's `pushBodySlotOccupancy` |
| `beforePresent_GameThread` | `present` | the presenter's `present_GameThread` with the driver as the clock and the world host |
| `updateVisualization_GameThread` | `updateVisualization` | the presenter's `updateVisualization_GameThread` with the game binding's storage |
| the three visualization doors | `editVizQuery`, `getVizReader`, `registerVizVolume` | the world host's doors of the same names |

**What stays here** (the Unreal and brawler half, BUSL-1.1 until og-tickdriver task 15e; §12 says which part of it
became the game binding's and the engine traits'): the static import and its report; the statics split
(og-simulation-jolt's `splitStatics`, R6); the configuration's fields from the console variables, the traits and the
binding (R7: the contact-count switch and the mapped categories stay here and pass through `build` unchanged); the
`AActor` key and name, `GetNetMode` and the character cast and replicated slot in `callWithSlotRequest`; the capsule
check; the render-target table and its `registerTarget`/`unregisterTarget`; the frame host; the controls' fill; the
null-owner condition before `releaseSlot`; and the `m_backendWorlds` row.

**The unregister order is kept exactly** (lead ruling R4): the presenter forget, the table's `unregisterTarget`, then
the release under the null-owner condition. So the tick backend has two calls for it rather than one.

**What changed, and what did not.** Every log line, console variable, thread and order of observable work is as
before. The statement-level differences, each without an observable effect:
* `og.Sim.MaxCatchUpSteps` is read as `start`'s argument, so before the tick-source bind and the presenter instead of
  inside the driver's configuration; a console-variable read changes nothing.
* `dt` reaches `start` as a `double` (the same `static_cast<double>(dt)` the driver's configuration used), and the
  presenter's configuration takes it back as a `float`: a `float` converted to `double` and back is exact.
* The role the presenter's configuration takes is the manager's `runsPrediction()` as before, now read through the
  game binding's `manager()` (the same object). It always equals `!worldIsAuthority`: the session emplaces the
  manager only in `BeginPlay`'s two role branches, with prediction off under `worldIsAuthority` and on otherwise.
* The render source reads the storage and the integrated tick through the game binding (one more pointer between the
  same objects).
* `createBodies_GameThread`'s bind runs inside `callWithSlotRequest`, after the acquire as before; the name's UTF-8
  buffer now outlives the bind.
* The three `checkf`s of the moved doors (`tryRegister`'s "holds no Jolt slot", and the presenter's and the driver's
  "reached … before startDriverStepping built it") are the tick backend's `OG_CHECK`s (R5): fatal either way; in Unreal
  `OG_CHECK` is a `checkf`, now without the id argument (design §2.1, Q13).
* `onEndPlayBegin`'s `if (m_tickDriver.has_value())` is the `if` inside the tick backend's `stop`; G-01's tag sits on
  `m_tickBackend.stop();`, the same wait in the same place.

**The order rows.** The tick backend's row, render source < presenter < driver, is a `static_assert` in its
constructor (§1's table); this class keeps frame host and game binding < tick backend.

**One module.** The tick backend is header-only and is instantiated here, through this class's members and its
explicit instantiation (§10), with the driver and the presenter, so the thread context stays in OGBrawlerUnreal.
Since og-tickdriver task 15e the header is OGSimulationUnreal's and the instantiation is still the game module's
(§12).

## §12 The move to OGSimulationUnreal (og-tickdriver task 15e)

The user approved the move and its licence on 2026-10-18 ("Go ahead with 15, the BUSL→MPL change is approved"), and
chose a module of its own for the Jolt traits (OGSimulationJoltUnreal, `OGSimulationJoltUnreal-rationale.md`) so that
OGSimulationUnreal names no Jolt symbol. Task 15e moved this class here as a header template that names no game type,
and the brawler supplies its bindings and instantiates it.

**The parameters.** `TickDriverBackendUImpl<GameBindingT, EngineTraitsT, EngineBindingT>`:

| parameter | what this class takes from it | the brawler's |
|---|---|---|
| `GameBindingT` | og-simulation's `TickDriverGame` (the manager, the storage, the render snapshot and the hooks, as before), and `Session`, `Component`, `Character`, `StaticData`, `gravity(session)` and `callWithSlotRequest(session, owner, key, slotCall)` | `BrawlerTickDriverGameUImpl<ASimulationManagerUImpl>`, named `BrawlerTickDriverGame` (`BrawlerTickDriverGameUImpl-rationale.md` §5) |
| `EngineTraitsT` | the adapter and visualization types, the world-host template, the log sink, and `Statics` and `splitStatics` (`JoltEngineTraitsUImpl-rationale.md` §5) | `JoltEngineTraitsUImpl`, in OGSimulationJoltUnreal |
| `EngineBindingT` | `Simulatable`, `categoryChannels`, `mappedCategories`, `contactCountsEnabled`, `checkCapsuleContract`, `kParkSnap` | `BrawlerJoltBindingUImpl` (`BrawlerJoltBindingUImpl-rationale.md` §6) |

**The requirement.** `TickDriverBackendComposition<GameBindingT, EngineTraitsT, EngineBindingT>`, in this header, is
`PhysicsEngineTraits<EngineTraitsT>`, `EngineTraitsT::kHasWorldHost`, `PhysicsEngineBinding<EngineBindingT,
EngineTraitsT>` (since task 15f, §13 below, in og-simulation's `PhysicsEngineTraits-rationale.md` §3),
`TickDriverWorldHost` of the world-host instance, and a call of
`EngineBindingT::checkCapsuleContract` with a `const GameBindingT::Character&` and an id. It names no brawler concept:
the brawler asserts its own refinement, `BrawlerEngineBinding`, on its binding in its seam
(`PhysicsBackendUImpl-rationale.md` §3). The Chaos traits still fail the second item, so no driver-scheduled host is
built over Chaos.

**Where each brawler-only statement went** (the inputs §10 listed):

| statement before task 15e | now |
|---|---|
| `callWithSlotRequest`'s cast to `AOGBrawlerUECharacter`, its `checkf`, the name's UTF-8 buffer, `GetNetMode() != NM_Client`, `GetPhysicsSlot` and the `SetAuthoritativePhysicsSlot` publish | the game binding's `callWithSlotRequest`, the same statements in the same order, with the same message; this class computes the key (`slotKeyOf`) first and passes it in |
| `m_owner.m_staticData.m_movementStaticData.gravity` in `buildWorld` | the game binding's `gravity(session)`, the same read |
| the static data's type in `createBodies_GameThread` | `GameBindingT::StaticData`; the call, `owner.getStaticData()`, is unchanged |
| the cast in `onRegistered_GameThread` and the parameter types `USimmableUpdateComponent&`, `AOGBrawlerUECharacter&` | `Character` and `Component`, the game binding's; `GetCapsuleComponent` (Unreal's `ACharacter`) stays |
| the owner's type `ASimulationManagerUImpl`, and the class the `m_backendWorlds` row reads | `Session` |
| the game binding's alias over `ASimulationManagerUImpl` | `GameBindingT` itself, held as `m_tickDriverGame`; the tick backend's alias is `TickBackend` |
| og-simulation-jolt's `joltTickWorldHost::splitStatics` and its result type | the engine traits' `splitStatics` and `Statics`, a forward of the same function (lead ruling L2) |
| the traits' `contactCountsEnabled` | the engine binding's (lead ruling L4: the cvar stays in the game module, under the game's backend switch) |

**What did not change.** The member list, its order and the two order rows of `compositionContractsHold`; the
`m_backendWorlds` row in `startDriverStepping`, now over `Session`; the reads of the owner's three adapter optionals and
`runsPrediction`; every call into the tick backend, the frame host and the render-target table, in the same order. Two
`static_assert` messages named the brawler's session and now name `GameBindingT::Session`; the rest of their text is
unchanged. The `HasAuthority` poison has a game-neutral name (§1), and the game binding carries the original poison.

**One module.** The definitions are in this header, between `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON` as they were
in the `.cpp`. The game module instantiates the class once, explicitly, at the end of `BrawlerJoltBindingUImpl.cpp`
inside that file's `OGSIM_OPTIMIZE_OFF` span, and `PhysicsBackendHostUImpl.h` declares the same instantiation
`extern`, so the session's `.cpp`, which calls the backend, instantiates only the members defined inside the class
(the constructor, the destructor and the empty bodies), as it did when the definitions were in a `.cpp`. Nothing in
OGSimulationUnreal or OGSimulationJoltUnreal instantiates the class: task 15e checked every object of both modules
with `dumpbin` after a positive control on the game's, and saw a member swap in this header fail the game module's
build through the explicit instantiation (C2338, `compositionContractsHold`).

**Equivalence.** Every log line, console variable, thread and order is as before: the statements moved, the types
they name come from the parameters, and `og.Jolt.ContactCounts` registers in the game module of the Jolt
configuration only, as before. Task 15e compared both configurations' console-variable dumps and ran Jolt and Chaos
harness sessions against the session before the task and the B1 baseline.

## §13 The concepts' header moves to og-simulation (og-tickdriver task 15f)

`PhysicsEngineTraits.h` (the concepts `PhysicsEngineTraits` and `PhysicsEngineBinding`, their helpers `EngineLogSink`
and `PhysicsEngineWorldHostTemplate`, and the self-check) was this module's from task 15e and is og-simulation's since
task 15f, next to `TickDriverConcepts.h`: it names only STL. This header includes it as
`OGSimulation/PhysicsEngineTraits.h`; that include line is the only change task 15f made to this header, and the
composition concept (§12) is unchanged. The engine-free narrative of the two concepts is og-simulation's
`PhysicsEngineTraits-rationale.md`; the passages of it that name this project's engines and modules are
`PhysicsBackendUImpl-rationale.md` §2, §3 and §6.

**Who includes the header:** this host (its composition concept requires `PhysicsEngineTraits` and
`PhysicsEngineBinding`), and a game's seam header (the brawler's `PhysicsBackendUImpl.h`, which selects the
configuration's traits and binding and asserts both concepts on them).

**Equivalence.** The header has no run-time code, so task 15f compared the disassembly (dumpbin with -disasm) of
every game-module object of the Jolt Editor build that includes the header, before and after the move; no
OGSimulationUnreal or OGSimulationJoltUnreal object includes it.
