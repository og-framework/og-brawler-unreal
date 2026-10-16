<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `JoltBackendUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/JoltBackendUImpl.h` and `Source/OGBrawlerUnreal/JoltBackendUImpl.cpp`. Both
source files carry no prose: a two-line docs pointer, code, assertion messages, and one-line tags `⛔G-nn` → an entry
in `JoltBackendUImpl-guards.md`.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says why the class exists, what it is, and why the Jolt host is two objects.
* **§2** to **§9** moved here from `SimulationManagerUImpl-rationale.md` (§17, the map of the Jolt host, and §18's
  subsections on the Jolt configuration's world, stepping, render publish and apply, shadow world, render
  interpolation, render correction smoothing and contact counts) in og-simulationscheduler-withjolt task 83, with the
  code they describe. Only their section references, the member and function names the move changed, and the guard
  ids changed.

---

## §1 Why this class exists

The user asked whether `ASimulationManagerUImpl` could be split so that the Chaos and Jolt backends are more
separate (og-simulationscheduler-withjolt phase F, 2026-10-15). The UImpl is the brawler's per-role simulation
session: the netcode session, the relay listeners, registration and input, the per-frame sends, the latency budget
and log routing. None of that depends on the physics engine. Task 82 moved the Chaos host into `ChaosBackendUImpl`;
task 83 moved the Jolt host into this class, and the UImpl lost its last `OG_PHYSICS_BACKEND_CHAOS`.

**What it is.**

* **Plain C++, not reflected.** It holds a reference to its owner, `ASimulationManagerUImpl`, and one to the owner's
  `m_backendWorlds`. The owner declares it a friend, so its bodies read the owner's members (the adapters,
  `m_storage`, `m_staticData`, `m_manager`) as the code did when it lived in the owner.
* **Not copyable.** The copy constructor and assignment are deleted; it is a member of its owner and nothing else.
* **The owner calls it through the seam,** the fixed set of entry points in `SimulationManagerUImpl-rationale.md`
  §18, each from exactly one call site and with no `#if`. `PhysicsBackendHostUImpl.h` defines the concept
  `PhysicsBackendHost`, which lists the set, and the owner asserts that its backend satisfies it
  (`PhysicsBackendHostUImpl-rationale.md`). Three entry points have no Jolt work and are empty inline bodies here:
  `onEndPlayWorld`, `sampleLatencyWindow_GameThread` and `logLatencyWindow_GameThread`. The others call the
  functions that held the Jolt arms before (`buildWorld` calls `decideJoltSteppingMode` and `buildJoltWorld`,
  `startStepping` calls `startJoltStepping`, `beforePresent_GameThread` calls `syncRenderPoses_GameThread`,
  `updateVisualization_GameThread` calls `updateVisualizationAtRenderPoses_GameThread`), or are the old arms
  themselves (`onEndPlayBegin`, the three visualization doors, `createBodies_GameThread`, `onRegistered_GameThread`,
  `onUnregistered_GameThread`).
* **The header has no `#if`.** Only `PhysicsBackendHostUImpl.h` includes it, in the Jolt configuration. **The `.cpp`
  is a whole-file arm** (`#if !OG_PHYSICS_BACKEND_CHAOS`), like `SimulationFrameHostUImpl.cpp` and
  `ChaosBackendUImpl.cpp`: in the Chaos configuration it compiles to nothing.

**Why the Jolt host is two objects.** Members are destroyed in reverse declaration order, and the Jolt host needs one
part destroyed after the owner's adapters and another destroyed before the owner's manager:

* `JoltBackendWorldsUImpl` holds state only: the runtime lease, the contact counter, the step and shadow worlds, the
  shadow world's adapters, the slot template and table, the world mutex and the two mode bits. The owner declares it
  as `m_backendWorlds`, where the Jolt member block was, above `m_physAdapter`, because the adapters are bound to the
  step world and the lease must outlive both worlds.
* `JoltBackendUImpl` (this class) holds everything else: the render targets, snapshots and windows, the shadow
  hand-over and its window, the contact records, and last the frame host, the step hooks and the step driver. The
  owner declares it as `m_backend`, directly after `m_manager`, because the driver unregisters from the manager's
  client clock when it is destroyed.

On Chaos the owner's two members are `ChaosBackendWorldsUImpl` (an empty struct) and `ChaosBackendUImpl`, so the
owner's declarations and assertions are the same in both configurations.

| destruction-order row (destroyed right to left) | where it is asserted |
|---|---|
| runtime lease < contact counter < step world | `JoltBackendWorldsUImpl`, first `static_assert` |
| runtime lease < shadow world < shadow body adapter < {shadow reader, shadow query} | `JoltBackendWorldsUImpl`, second `static_assert` |
| `m_backendWorlds` < `m_physAdapter` < {`m_physReaderAdapter`, `m_queryAdapter`} < `m_integrationLayer` | the owner's `compositionContractsHold` |
| `m_manager` < `m_backend` | the owner's `compositionContractsHold` |
| `m_frameHost` < `m_stepHooks` < `m_stepDriver` | this class's `compositionContractsHold` |

Together they are the rows the owner asserted before task 83: runtime < world < adapters < integration layer, the
shadow world's row, the contact counter before the world, and manager < frame host < hooks < driver. Every message
says which row it was.

**What the move changed, and what it did not.** The code moved with only these edits: the owner prefix (`m_owner.`)
on the owner's members and on `runsPrediction` and `GetNetMode`; the worlds prefix (`m_worlds.`) on the members of
`JoltBackendWorldsUImpl`; the class qualifier; the step world's after-step observer, which receives and casts this
class instead of the owner; `m_frameHost.begin`, which also takes this class; and in `createBodies_GameThread`, the
parameters in place of `tryRegister`'s locals (`character`, `simulatable`) and `false` in place of
`TryRegisterStatus::Pending`. The order of the calls inside every moved function, the log texts, the console
variables and the threading predicates are unchanged. `tryRegister`'s root-capsule check after the bind reads the
movement declaration's stamped `bindings.parentBodyId`, which the Jolt bind stamps with the slot's root body on every
declaration (task 82 made the check read it).

* **The owner's `HasAuthority` poison comes along.** The owner's `.cpp` defines `HasAuthority` to an undeclared name
  after its includes, so `HasAuthority()` typed there fails to compile (the manager is not replicated, so the call
  is always true; `SimulationManagerUImpl-rationale.md` §13.9). The Jolt code had that protection while it lived in
  the owner's `.cpp`, so this class's `.cpp` repeats the same `#define` after its includes and the `#undef` at its
  end.
* **The nested types moved with the members that use them.** `RenderTargetUImpl`, the two render windows,
  `ShadowStateSlotUImpl`, `ShadowWindowUImpl` and `ContactStepUImpl` were private nested types of the owner, used
  only by Jolt code, so they are private nested types of this class. `JoltRuntimeLeaseUImpl` moved to this class's
  header. `BrawlerStepDriver` names the manager and the integration executor through `BrawlerCompositionUImpl`
  (task 81), because the owner's `ManagerType` is private.
* **The frame host is a friend of both.** It calls this class's step functions, its driver's occupancy door and its
  world-mutex and step-flag predicates through a second pointer that `begin` now takes. It keeps calling the owner
  for the owner's work: `releaseDelayedInputsForStep` and `onFrameStepsDue_GameThread` (both private, hence the
  owner's friendship), the latency stamps, the reception coordinator and `OnPostPhysicsStep`. Its end-of-physics
  task still holds the owner through a weak pointer and reaches itself as `m_backend.m_frameHost` of a live owner.

## §2 The Jolt host — overview (og-simulationscheduler-withjolt tasks 53–56 and 18)

Since task 18 the Jolt configuration is the default: `PhysicsBackendChaos` in `OGBrawlerUnreal.Build.cs`
is `0` (`SimulationManagerUImpl-rationale.md` §18). Its host is spread over this class, its owner (the shared
session, which calls it through the seam), the frame host and the step hooks. This section is the
map; the details are in the sections and documents it points to.

**The shape is Chaos's async mode** (design D1). The same tick groups, the same wait rule (a client's end
of physics waits only for steps dispatched in earlier frames), the same per-frame cadence for the probes
and the reap, and the same roles on the same threads: the authority steps inline on the game thread, a
client on a worker task.

| concern | who | where it is described |
|---|---|---|
| the two tick functions at `TG_StartPhysics` and `TG_EndPhysics`, the scheduler, the world-delta time base, the catch-up cap of 60 | the frame host | `SimulationFrameHostUImpl-rationale.md` §1–§4 |
| which thread steps: the authority inline; a client one task per frame chained after the previous frame's, or inline under `og.Sim.RunInline` 1; block mode 0 | the frame host, by this class's stepping mode | frame-host rationale §2–§3; §3 |
| one step: the world mutex, then the step driver's `runTick` and the four hooks | `runJoltStep_Step`, the step hooks | §4; `BrawlerStepHooksUImpl-rationale.md` |
| the per-frame input work before the steps (PROBE A, PROBE 6), the reap after the inline steps, the drain per authority step | the frame host, the hooks and the owner | `SimulationManagerUImpl-rationale.md` §7, §8 |
| the world, its statics, the character slots, the bind and the release | this class | §3 |
| the render publish, the interpolation, the correction smoothing and the capsule apply | the hooks and this class | §5, §7 and §8 |
| the game-thread shadow world | this class | §6 |
| the mechanisms between the game thread and the step (J1–J6, the shadow hand-over) | the frame host, the hooks, this class | `SimulationManagerUImpl-rationale.md` §1 |
| the per-window line `[SimHost.Window]`, the step cost and the thread-ownership asserts | the frame host, the hooks, this class | frame-host rationale §8–§9; hooks rationale §3; `SimulationManagerUImpl-rationale.md` §8; §4 |
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
the hooks, the driver, the scheduler, the two tick functions). `EndPlay` first unregisters the tick functions and
waits for every outstanding step (`onEndPlayBegin`, `⛔G-01`), before any teardown.

**Lines.** `[SimHost.Backend]` (both configurations), `[SimHost.Mode]`, `[SimHost.World]`,
`[SimHost.Frame]`, `[SimHost.FirstStep]`, `[SimHost.Slot]`, `[SimHost.Bind]`, `[SimHost.Shadow]`, `[SimHost.RenderTarget]`,
the non-shipping `[SimHost.RenderApply]` and `[SimHost.RenderSync]` checks, `[SimHost.RenderCorrection]`
and `[SimHost.RenderTrace]`,
and the per-window `[SimHost.Window]` (`SimulationManagerUImpl-rationale.md` §8), on `LogOGSimHost`.

## §3 The world (task 53)

The first part of the Jolt host builds the physics world and binds characters into it. Task 53 built
it without stepping; task 54 steps it (§4). Everything below is plain C++ in this class's two files (until
task 83, in the Jolt arms of `ASimulationManagerUImpl`'s two files).

* **Members**, of `JoltBackendWorldsUImpl`, in declaration order: `m_joltRuntime` (a `JoltRuntimeLeaseUImpl`, which
  acquires the reference-counted `JoltRuntime` and releases it when destroyed: PIE has two worlds in one process),
  `m_joltWorld`, the slot template, the slot table, the world mutex `m_joltWorldMutex` and the stepping
  mode. The owner declares the struct as `m_backendWorlds`, above its adapter members. The struct's own
  `static_assert` keeps runtime < world, and the owner's `compositionContractsHold` keeps the worlds < body
  adapter < reader and query adapter < integration layer, because members are destroyed in reverse
  and each of those holds a reference into the one before it (§1).
* **The stepping mode** is fixed at `BeginPlay` by `decideJoltSteppingMode` and printed once as
  `[SimHost.Mode] role=<Authority|Client> stepping=<inline|worker> viz=<stepWorld|shadowWorld>`. The authority
  always steps inline on the game thread: its pre-step input release touches UObjects. A client steps
  on a worker task, or inline when `og.Sim.RunInline` is 1 (read once). The decision is the role,
  never the engine's ShouldUseThreadingForPerformance, which would put a listen or standalone authority on
  a worker as Chaos does (design D2). Task 54 dispatches by it.
* **`buildJoltWorld`** runs before the role branches, in this order, the same on every peer:
  1. acquire the runtime;
  2. import the level's static collision with `UEStaticGeometryImporter` and the brawler's six-row
     category table (`brawlerCategoryChannels`, the same rows as the Chaos host's
     `emplaceBrawlerQueryAdapter`, in `ChaosBackendUImpl.cpp` since task 82; the duplicate goes with it in
     task 21), and log its
     report;
  3. split the import into queryable and PhysicsOnly shapes by the importer's per-shape
     `physicsOnly` flags;
  4. construct `m_joltWorld` from the slot template (one body per physics declaration of
     `SimulatableBrawler`, in declaration order, so the factory's template-order bind matches the
     composite), `kMaxSimulatableSlots` slots, the static layer keys of both parts, the movement
     gravity, and a rollback ring of `rollbackWindowHardCap` plus 2 ticks on a client and none on the
     authority (which never resimulates);
  5. build the queryable statics with one `JoltStaticWorldBuilder`, then the PhysicsOnly statics with
     a second one, always both and always in that order, so the static bodies get the same Jolt
     indices on every peer (the static part of a query hit's sort key is the body index);
  6. construct the body adapter, the reader over its bind table, and the query adapter with the six
     mapped categories (the Chaos adapter's mapped set) and an access predicate (below);
  7. exclude the PhysicsOnly bodies from queries, as Chaos's queries never see them.

  Steps 4 to 7 run under the world mutex. Nothing is contended at `BeginPlay`; it is there because
  the query adapter's predicate demands it on a worker client. One `[SimHost.World]` line reports the
  slot count, bodies per slot, ring depth and both static counts. ThirdPersonMap has no PhysicsOnly
  element, so the second builder builds nothing there.
* **The world mutex (J2)** is a UE::FMutex, taken through a scope class that also opens a
  `JoltWorldAccessScope` and sets this manager's bit in a thread-local "this thread holds J2" mask. The
  step holds it for each whole tick (`runJoltStep_Step`, task 54). The game thread holds it only around
  `tryRegister`'s bind pass (`createBodies_GameThread`, `⛔G-02`), the world build above and the driver's
  construction; never
  around a visualization pass. UE::FMutex is not recursive, so the scope `checkf`s the bit before it
  locks.
* **Access predicates.** `JoltSpatialQueryAdapter` asserts every call through the function pointer it
  is given. A worker client's adapter gets "this manager's step is running on this thread, or this
  thread holds this manager's J2". An inline host's also accepts "on the game thread", because there the
  game thread runs the steps and queries between them. The "step is running" bit is set by the step
  scope in `runJoltStep_Step`. Both flags are keyed per manager (§4).
* **The slot table** (task 66). A character owns one of the world's fixed slots from the first time the
  manager hears of it until it leaves, and it owns the **same** slot on every peer. A slot's bodies have
  fixed Jolt body ids and Jolt orders contact solving by body id, so a character in different slots on two
  peers can have a contact solved in a different order there (design_resim_analysis §6; the N2 paragraph
  below). `tryAcquireJoltSlot` keys the table by the owning actor and returns the owner's slot if it has
  one. Otherwise it asks og-simulation's `decideBodySlot` (`BodySlotAssignment.h`), with the pawn's
  replicated slot (`AOGBrawlerUECharacter::GetPhysicsSlot`) and the slots taken here (`⛔G-05`):
  - the authority takes the lowest free slot, as before (so the server's slots are unchanged), and writes
    it to the pawn's `PhysicsSlotValue` with `SetAuthoritativePhysicsSlot`, which replicates it;
  - a client takes the replicated slot, even when a lower one is free here. It never picks one itself;
  - a client with no replicated slot yet (`WaitForReplication`), or whose replicated slot is still held
    here by a character that has left the authority but not yet this client (`WaitForRelease`), gets no
    slot. The acquire returns empty and the caller retries on the next frame. The release of the departed
    character's slot (below) ends a `WaitForRelease`;
  - all slots taken is `Full`, today's `checkf`.

  Each owner logs one wait line per wait kind, not per attempt,
  `[SimHost.Slot] wait=<WaitForReplication|WaitForRelease> owner=<name> slot=<slot|-1> heldBy=<name|->`,
  and one line when it gets its slot,
  `[SimHost.Slot] assigned role=<Authority|Client> owner=<name> slot=<slot> lowestFreeLocally=<slot|-1>`,
  both at `Log`. `lowestFreeLocally` is the slot a first-free rule would have picked on that peer.
  `registerVizVolume`, which the component calls before the character's simulation id is known, takes
  the slot first and returns an empty volume id while the acquire waits, so the component retries before
  it builds its visualization states (`SimmableUpdateComponent-rationale.md` §3); `tryRegister` finds the
  slot there, and its first call returns `Pending` if it does not (`createBodies_GameThread` returns `false`). The slot rides pawn replication only;
  a query hit's key still uses the simulation id, not the slot.
* **The bind** (`tryRegister`'s first call, through `createBodies_GameThread`, `⛔G-02`). Under J2,
  `JoltPhysicsFactory` binds the slot's
  template bodies in declaration order. Its `simulatableId` is the storage key (`toStorageKey` of the simulation id),
  which the authority alone assigns and every client receives on the pawn (`SimulationManagerUImpl-rationale.md`
  §15), so it is the same on
  every peer, as the query adapter's hit order requires. The factory's shape registrar is the query
  adapter's `registerShape`, so child bodies map to their root and their shape enables register. Each
  declaration's query volumes are registered with the slot's root body as the ignored root. Then one
  `[SimHost.Bind] bind id=<id> slot=<slot> body0=<index> boundBodies=<n> slotsInUse=<k>` line at
  `Warning`, so it shows at `LogOGSimHost`'s committed level: `body0` is the Jolt body index of the slot's
  first template body (`JoltWorld::slotBodyId`), so two peers' lines show the same body ids, not only
  the same slot (task 66; the lines were at `Log` before and never appeared). Occupancy
  is not applied here: the slot stays parked until the step applies the Occupy command the Ready
  path pushes (J1, task 54).
* **The capsule contract (design D14).** The Chaos factory compared the authored UE capsule with the
  movement descriptor when it adopted the capsule as the root body. The Jolt slot body is built from
  the descriptor, so the bind pass `checkf`s the pawn's unscaled capsule radius and half-height against
  the descriptor's `CapsuleGeometry` instead (`OGBrawlerUECharacter-guards.md` G-01).
* **Readiness** is unchanged in shape: the second call asks the body adapter's `isBodyResolvable` for every
  declaration's body. For the Jolt body adapter that means "bound": it reads the bind table the first
  call's factory wrote, on this thread, so it needs no lock and is true on the next attempt.
* **The release** (`unregisterFromNewFramework`, last, through `onUnregistered_GameThread`): `releaseJoltSlot`
  pushes the slot's Vacate
  command (J1, task 54), clears the slot's bind-table entries and frees the slot, on the game thread
  without J2. The body adapter's step-side
  reads of the bind table (the locked-rotation inertia) never read the entries a release clears; the
  reader's "bound" gate does, and its only callers are the game-thread visualizations, so the
  release cannot race them (task 11 review N3). A reader with a step-thread caller would need the
  release under J2. The bodies keep their stale user data until the next bind, which is harmless: a
  parked body answers no query and collides with nothing. Query volumes are never unregistered, as in
  the Chaos configuration. One `[SimHost.Bind] release id=<id> slot=<slot> boundBodies=<n> slotsInUse=<k>`
  line at `Warning` (task 66, as the bind line). The release also forgets the owner's logged waits.
* **Visualizations.** On an inline host `editVizQuery()` and `getVizReader()` return the step world's
  adapters, which the game thread may query between steps, and `registerVizVolume` registers in the
  same adapter with the slot's root as the ignored root. On a worker client they return the shadow
  world's adapters, and the volume registers there (the shadow world, below). Task 53 returned null on
  a worker client, and the component skipped its two physics-querying passes; task 56 removed that skip,
  and the accessors return references again, as before task 53.

## §4 The stepping (task 54)

The frame host (`SimulationFrameHostUImpl`) and the step hooks (`BrawlerStepHooksUImpl`) have their
own documents. This class's part:

* **Members**, of this class, last and in this order: `m_frameHost`, `m_stepHooks`, `m_stepDriver` (a
  `SimulationStepDriver` over the manager, `m_joltWorld` and the hooks). The owner
  declares this class as `m_backend`, after `m_manager`. This class's `compositionContractsHold` asserts
  frame host < hooks < driver and the owner's asserts manager < backend, so together they keep manager <
  frame host < hooks < driver: the driver unregisters its
  resync callback from the manager's client clock when destroyed, and holds the hooks, which hold the
  frame host. The frame host is a friend of this class and of the owner: it calls this class's private
  per-step functions below and the driver, and the owner's private per-frame functions
  (`onFrameStepsDue_GameThread`, `releaseDelayedInputsForStep`).
* **`startJoltStepping`**, at the end of `BeginPlay` (design §9.1 steps 7–9), through the seam's
  `startStepping` (where the Chaos configuration registers its callback): under J2 it constructs the hooks and the driver (`dt`, `ResimPolicy::OnRequest`;
  its constructor parks every slot), then `m_frameHost.begin` builds the scheduler and registers the two
  tick functions.
* **`runJoltStep_Step(physicsStep, deadline)`**, called by the frame host for each step of a batch on
  the step's thread: J2 scope, step scope (the "step is running" bit), the deadline into the hooks, then
  `m_stepDriver->runTick(physicsStep)`, and, when the contact counter is installed, that tick's contact
  lines (the contact counts, below). It is one of the step driver's two doors (the other is the
  frame host's `applyOccupancyCommands_Step`), and it first `checkf`s that the frame host's batch is
  running on this thread and, in the inline mode, that this is the game thread (frame-host rationale
  §9). `isJoltStepRunningOnThisThread` reads the "step is running" bit for the frame host's and the
  hooks' step-context asserts.
* **J1 sites.** `tryRegister`'s Ready path (`onRegistered_GameThread`) pushes an Occupy command after `registerSimulatable` and
  `notifyCharacterRegistered`; `releaseJoltSlot` pushes a Vacate command before it clears the bind table.
  A rejoin that rebinds the slot before the step drains the Vacate is safe: J1 is FIFO, so the
  Vacate applies first, and the new Occupy follows only from the new character's own Ready pass.
* **`EndPlay`** begins with `onEndPlayBegin`: the frame host's `unregisterTickFunctions` and then
  `waitForOutstandingSteps` (`⛔G-01`): no step runs after it, before any teardown. The owner keeps the call
  first in `EndPlay` (`SimulationManagerUImpl-guards.md` G-85).
* **The per-frame and per-step input work** is `SimulationManagerUImpl-rationale.md` §7's Jolt call pattern;
  PROBE A's tick source is its §8's.

**Why the thread-local flags are keyed per manager** (task 53 review N1). A one-process PIE can hold
two managers, the authority's and a client's (`s_instances`), on one game thread. With one process-wide
flag, a server step running inline would satisfy a worker client's predicate on the game thread, and a
game-thread bind in one manager would make the other's adapter accept a call it should refuse. So the
two flags are bit masks with one bit per manager slot (`kJoltAuthorityRoleBit`, `kJoltClientRoleBit`,
fixed by `decideJoltSteppingMode`), and each manager's adapter gets the predicate instantiated for its
own bit (`joltWorldAccessCheckFor`). The J2 scope's "not already held" `checkf` tests the manager's own
bit, so a game thread that ever held both managers' mutexes at once would not trip it; that nesting
cannot deadlock, because a step only ever takes its own manager's mutex. Two client managers in one
process are already refused at `BeginPlay` (`s_instances`).

**Character contact solving and slots** (task 53 review N2, fixed by task 66). Jolt orders contact
constraints by body id, and a character's body ids are its slot's. Until task 66 each peer handed out
slots in its own registration order, so a client that joined late bound its own pawn first and put the
same character in a different slot, with different body ids, from the server; two characters touching
could then be solved in a different order on the two peers. Since task 66 the authority chooses the slot
and every client binds that slot (the slot table, above), so every peer gives a character the same body
ids. Queries were never affected (their hits sort by a peer-stable key). Nothing here claims bitwise
cross-peer determinism of the solver beyond that: the same bodies, statics and inputs are what Jolt's
rollback guide asks for, and prediction correction absorbs what remains.

## §5 The render publish and apply (task 55)

The Chaos configuration's end of physics syncs every component to the physics result, interpolated
two steps behind. The Jolt configuration has no such sync, so this class does it for the one component
that needs it, the capsule root (design D11, §8).

* **Why it is not cosmetic.** The capsule is the mouse-aim plane point: the input component's
  `updateGameThreadCache` projects the mouse onto the horizontal plane through the capsule's world
  translation (`OGBrawlerInputCollectionComponent.cpp`, the plane point read before the line-plane
  intersection), and the camera boom hangs off the capsule. Without the apply the aim input fed into
  the simulation is measured from wherever the capsule spawned (design F7).
* **The snapshot type.** `RenderSnapshot` is `RenderSnapshotT<kMaxSimulatableSlots * 6>`, declared with
  the other brawler aliases: one body per declaration of the brawler's physics composite in every
  slot. Two `static_assert`s in `publishRenderSnapshot_Step` hold it: the 6 equals the composite's
  declaration count, and the snapshot is trivially copyable (no heap-owning member).
* **Publish, on the step's thread.** `afterTick` calls the frame host's `publishRenderSnapshot_Step`,
  which calls this class's `publishRenderSnapshot_Step(outcome, currentStepDeadline)`: take a slot
  (`beginWrite`), fill it from `m_storage` (`fillRenderSnapshot`), `commit`. Once per `runTick`, on every
  role, never in a replay (the driver calls no hook then). The fill runs after the post-step pass, so it
  reads this step's poses from og-simulation's storage, keyed by the peer-stable storage key; it reads
  storage on the step's thread inside the step, as every other step code does, so it adds no crossing
  beyond J5 itself.
* **No allocation after warm-up.** The channel's four slots are built with the manager, and
  og-simulation's own test pins that the channel allocates nothing after construction. The fill writes
  the slot in place; the storage walk iterates the existing maps; the key sort is an in-place sort over
  the slot's array; the snapshot owns no heap storage (the second `static_assert`).
* **Apply, on the game thread.** The owner's `OnPostPhysicsStep` calls `applyRenderSnapshot_GameThread`, through
  `beforePresent_GameThread` and `syncRenderPoses_GameThread`, before its
  shared body (after the non-shipping hitch sleep and, on a worker client, after the shadow-world restore
  of §6),
  every frame, on every role. The Chaos configuration has no apply: the engine's sync does that job.
  Task 55's apply peeked the newest snapshot; since task 19 it moves the capsule to the render
  interpolation's pose (§7), and `og.Render.NewestSnapshot` 1 restores the newest. For every character in
  `m_renderTargetsById`, filled on `tryRegister`'s Ready path (`onRegistered_GameThread`) and erased by
  `unregisterFromNewFramework` (`onUnregistered_GameThread`),
  it finds the root body by storage key and `m_joltRootDeclarationIndex` with a binary search over the
  sorted bodies, and moves the actor there with teleport and no sweep (`⛔G-04`). The rotation is the
  snapshot's only when the body carries one; the brawler's capsule body does not, so the actor keeps its
  own. Then it releases the snapshot.
* **The root declaration.** `buildJoltWorld` takes `m_joltRootDeclarationIndex` from the slot template's
  one `isRoot` body (a `checkf` demands exactly one). The template and the snapshot both enumerate the
  composite in its declaration order, so a template index is a declaration index.
* **On a worker client** the newest committed snapshot may come from this frame's step task, still
  running under block mode 0: a newer pose, never a torn one.
* **The non-shipping check** (`[SimHost.RenderApply]`, `LogOGSimHost`), two comparisons per registered
  character every frame, against the translation the aim plane reads:
  * *held* (`checkRenderTargetsHeld_GameThread`, before the apply): the capsule still sits where the
    last apply put it. Between two applies lie the next frame's pre-physics group, where the aim is
    read, and the engine's own physics groups; anything that moves the capsule there logs a MOVED
    Warning.
  * *applied* (`checkRenderApply_GameThread`, after the apply): the capsule equals the rendered pose
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

## §6 The game-thread shadow world (task 56)

On a client the step runs on a worker task, so the game thread may not query the step world: it
changes under the query, and the query adapter's predicate refuses any caller that neither runs the
step nor holds J2. The visualizations still need a world to query every frame. Chaos gives them its
game-thread copy of the scene, refreshed at the end of physics. The Jolt configuration does the same
with a second world that only the game thread touches (design D §2.5, "(e) in detail"; OQ13 = (e)).

* **Who.** Worker clients only: the stepping mode decides it at `BeginPlay` and it never changes. The
  authority and a client with `og.Sim.RunInline` 1 step on the game thread, so their visualizations
  query the step world between steps and they build no shadow.
* **Construction** (`buildShadowWorld`, at the end of `buildJoltWorld`, after the step world's J2 scope).
  `m_shadowWorld` takes the step world's `JoltWorldConfig` with two fields changed: `ringDepthTicks` 0
  (the shadow never saves a tick) and `tempAllocatorBytes` `kShadowTempAllocatorBytes`, 64 KiB instead
  of 16 MB (the shadow never steps; og-simulation-jolt's two-world tests restored and queried such a
  world). The same two `JoltStaticWorldBuilder` calls run in the same order, from the same two static
  descriptions, so the statics get the same body ids. The shadow has its own body adapter, reader and
  query adapter (`m_shadowBodyAdapter`, `m_shadowReader`, `m_shadowQuery`), declared after
  `m_shadowWorld` (`JoltBackendWorldsUImpl`'s second `static_assert` keeps the order), and the PhysicsOnly statics are
  excluded from its queries as from the step's. The query adapter's predicate is
  `joltShadowAccessOnGameThread`: on the game thread. The shapes are built twice rather than shared,
  which needs no og-simulation-jolt change. `⛔G-03`.
* **The BeginPlay check** (non-shipping `checkf`, and one `[SimHost.Shadow] built` line on every
  build). The restore checks only the slot bodies: the save filter skips statics, so a shadow whose
  statics differ would restore without complaint. The check compares the two worlds' body counts
  (slot bodies plus the bodies the two static builders created) and the two lists of static body ids.
  Jolt's own body count is not reachable from this module: Jolt's body manager is not exported from
  the OGSimulationJolt DLL, so the editor build cannot link it. A static created outside the two
  builders would not be counted; the host creates none.
* **Bind mirroring** (`bindShadowSlot`). The step world's bind writes three groups of things no state
  slot carries: the body adapter's bind table, the query adapter's shape registrations, and, through the
  factory's body defaults, each body's friction, restitution, user data and motion properties. The query adapter
  drops hits on bodies without user data, so a shadow without them would answer nothing. So
  `tryRegister`'s bind (`createBodies_GameThread`), after the J2 scope closes, runs the same `JoltPhysicsFactory` bind (the whole
  `createPhysicalObject`, body defaults included) over the shadow's adapters: the same slot template,
  slot, storage key and options, in declaration order, with the shadow's `registerShape` as the
  registrar. It runs on the game thread without J2, because the step never touches the shadow. The
  declarations' own query volumes are not registered in the shadow: they belong to the step's hit
  detection, and the visualizations use only the volume `registerVizVolume` registers. The slot release
  mirrors the step's: `releaseShadowSlot` clears the shadow bind table's entries for the slot.
* **The bind-table comparison** (`compareShadowBindTables_GameThread`, non-shipping), after every bind
  and every release: for every slot body, the two worlds' body ids, bindings and locked-rotation inertia
  must agree, and after a bind also the body and shape ids the two factories returned. It logs
  `[SimHost.Shadow] bindCompare after=<bind|release> id slot equal=1 boundBodies compares mismatches`
  at Log, or a Warning with equal=0 and the count of differing bodies.
* **Hand-over, on the step's thread** (`publishShadowSlot_Step`, from `afterTick` through the frame host,
  after the render publish and before J3). Only on a worker client, and only when the step saved a
  slot: `stepAllocatesFrontierSlot` of the step's kind, the same predicate the driver uses (a Stall step
  saves none, and the shadow keeps the previous slot). It copies the step world's ring slot for the
  step's tick into a pooled `ShadowStateSlotUImpl` of `m_shadowSlots` (the bytes into storage the slot's
  constructor sized to `ringSlotBytes`, the body ids into a vector reserved for every slot body),
  stamps a sequence number and commits it. The channel is `SnapshotChannel<ShadowStateSlotUImpl, 3>`,
  not the design's `SnapshotChannel<JoltStateSlot, 3>`, on purpose: `ShadowStateSlotUImpl` wraps the
  state slot and adds that sequence number, which the restore compares with the last one it restored, so
  it restores each published slot at most once and only a newer one. `buildShadowWorld` `checkf`s that the step world's ring slot
  size is the one the pooled slots were sized for. Never in a replay: the driver calls no hook then.
* **Restore, on the game thread** (`restoreShadowWorld_GameThread`, first in `OnPostPhysicsStep`'s
  `beforePresent_GameThread` on a worker client, before the render apply; design D §4.2). It peeks the newest slot and, when its
  sequence is newer than the last one restored, calls `restoreFromSnapshot`: the body-set check, Jolt's
  state restore, then the slot's occupancy and shape enables, without the undo save `restoreTick` takes.
  A slot whose body set differs from the shadow's is refused with og-simulation-jolt's Warning
  ("refused: the snapshot's body set differs from the live body set") and the shadow keeps its
  state; a refused slot is not retried. A frame with no newer slot (a faster display, or a frame with
  no step) restores nothing.
* **Timing.** A visualization in frame k reads the state restored at frame k − 1's end of physics, as
  Chaos's queries read the copy its end of physics refreshed. Under block mode 0 the newest slot may
  come from a step task still running: a newer state, never a torn one, because only committed slots are
  handed to the reader. Since task 19 the shadow's slot bodies are then set to the interpolated render
  pose (§7), so the visualizations query the rendered pose.
* **The non-shipping window** (`logShadowWindow_GameThread`, `[SimHost.Shadow] role=Client`, every 600
  frames): frames, restores, frames with nothing newer, refusals, slots published and slots missing
  (a tick the step world's ring did not hold), how many restores left the shadow's state hash equal to
  the saved tick's (`liveStateHash`) and how many did not, the occupied slots of the last restore, the
  last restored tick, the slowest restore, how often `editVizQuery()` and `getVizReader()` handed out
  the shadow's or the step's adapters, how often the shadow's predicate ran, how often the step
  adapter's predicate ran on the game thread outside J2 (each such call is also an access assert), the
  bind comparisons and mismatches so far, and the channel's drops.

## §7 The render interpolation (task 19)

Task 55 moved each capsule to the newest snapshot. At a render rate that is not a multiple of the
60 Hz step (the first Jolt PIE's clients ran at 100 fps), that apply holds the capsule on some frames
and moves it a whole step on others: a 3:2 step/hold judder, which the weapon and guard drawings,
taken from the newest tick, shared. Chaos rendered every body two steps behind, blended between the
two results that bracket that time (the engine's p.AsyncInterpolationMultiplier, default 2, and its
results manager). This configuration now does the same (design D §8).

* **The render time** is `hostNowSeconds_GameThread()` minus the delay times
  `stepIntervalSeconds_GameThread()`: the frame host's time base, the one the snapshots' step
  deadlines come from (frame-host rationale §4). The delay is `og.Render.InterpolationDelaySteps`,
  default 2, read every frame and clamped to [0, 2]. Never sim ticks: a Stall step (the tick does not
  advance) and a Skip step (it jumps) each step the world once and publish a snapshot at their own
  deadline, so the render clock neither stops nor jumps with the sim clock.
* **The core (task 67).** The bracket, the alpha and the blend are og-simulation's engine-free
  `interpolateRenderPoses` (`OGSimulation/RenderInterpolation.h`; its rationale is
  `RenderInterpolation-rationale.md` in og-simulation's docs). `buildRenderPoses_GameThread` reads the
  cvars, computes the render time, passes the snap distance, and calls it with J5 and the pose set; it
  keeps the core's per-frame summary (`m_renderFrame`) for the window line and the trace. Task 19 had
  the same code in the UImpl; task 67 moved it unchanged, so another game can reuse it.
* **The *next* copy**, `m_renderNext`: every frame, in every build including Shipping, the core also
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
  its newest snapshot while two are held (four slots, task 44's sizing rule), and this class's keeps
  the delay clamp within what four slots can bracket (the step before the render time is at most the
  delay's whole part plus one behind the newest). The headless runs never reached past back 1.
* **The pose set**, `m_renderPoses`: a copy of *next* whose bodies are blended from *prev* with alpha,
  positions linearly and rotations (bodies that carry one) by slerp (the core's `blendRenderPoses`).
  The core snaps above a distance its caller passes; this class passes `kRenderParkSnapCm` (half the
  projectile pool's park depth, 500 m): a body whose two poses lie farther apart is a pooled
  projectile being parked or unparked, not motion, so it takes *next*'s pose unblended and counts as a
  snap; every other jump (a respawn) is blended, as Chaos blends it. A body missing from *prev* (a
  character that just joined) takes *next*'s pose.
* **Who reads it**, in `OnPostPhysicsStep`, through `syncRenderPoses_GameThread` (the shadow restore,
  the apply, the shadow overwrite, in that order) and then the visualization copy:
  * the capsule of every registered character (the apply, `⛔G-04`), the mouse-aim plane point;
  * on a worker client, the shadow world (`overwriteShadowPoses_GameThread`): after this frame's
    restore, every occupied slot body bound in the shadow is set to its pose with the shadow body
    adapter's `setBodyTransform` (no activation; a body without a rotation keeps the restored one). The
    target and block-prediction queries of the next frame then find the bodies where they are drawn, as
    Chaos's queries found its interpolated game-thread copy (design OQ10). A `checkf` demands that the
    restore ran this frame first: a restore after the overwrite would put the newest tick back;
  * the visualization copy (`updateVisualizationAtRenderPoses_GameThread`): after
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
    simulates physics (the registration switches it off; `OGBrawlerUECharacter-rationale.md` §1.2) and
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
`RenderCorrectionSmoothing-rationale.md` in og-simulation's docs). This class supplies the cvars, the
capture point, the ring and the lines.

* **Capture, on the step's thread.** The capture itself is og-simulation's engine-free
  `RenderCorrectionCapture` (`m_renderCorrectionCapture_Step`; its call contract and outcomes are §13
  of `RenderCorrectionSmoothing-rationale.md`); this class decides when to call it and counts what it
  returns. On a client (the step driver's `savesTicks`), `publishRenderSnapshot_Step` hands it every
  snapshot it publishes (`noteShownSnapshot`): the pose the render will draw for that tick. The hooks'
  `beforeSimulate` calls the frame host's `captureRenderCorrection_Step`, which calls this class's
  `captureRenderCorrection_Step` with the step's physics step. `beforeSimulate` runs after the
  driver's replay and before the step: the one point where the storage holds the corrected present
  tick, which is the tick of the last published snapshot. It calls `onStepAfterReplay` with the
  driver's granted-resimulation count, the manager's `currentIntegratedTick` and that step, and counts
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
* **Apply, on the game thread.** `buildRenderPoses_GameThread` calls `correctPoses` with J7 right after
  `interpolateRenderPoses`, with the three cvars and the fixed step (`m_renderCorrectionStepSeconds`,
  the step driver's `dt`, which is the physics settings' async fixed step that Chaos converts with
  too). It drains J7 and accumulates each correction on the first frame whose bracket reaches its
  carrier step. The order is a data dependency, not a convention: `correctPoses` takes the bracket's
  frame, which `interpolateRenderPoses` produces. The error is added to `m_renderPoses`, so everything
  that reads the pose set moves with it: the capsule (the mouse-aim plane point, `⛔G-04`), the shadow
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
  always on: there is no cvar, so nothing here reads one. This class only counts it, in the lines
  below.
* **On the straddle they take the corrected rotation.** `correctPoses` also gets `m_renderNext`. On
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

## §9 The contact counts (task 78)

A session log could not tell a divergence caused by contact from one caused by different play: no line
recorded which bodies touched. With `og.Jolt.ContactCounts` 1 this class installs og-simulation-jolt's
`JoltContactCounter` (a passive Jolt contact listener; what it counts, why it changes no physics and
why its counts are exact on any job system are in `JoltContactCounter-rationale.md` in
og-simulation-jolt's docs) and logs, after every simulated step that touched a contact, one
`[SimHost.Contacts]` line.

* **The cvar is read once, when the step world is built** (`buildJoltWorld`, right after the world is
  constructed, at `BeginPlay`). Jolt calls the listener from inside a step, and a resimulation is many
  steps in one `runTick`: a listener installed or removed mid-session would count part of a step or
  part of a replay, and the drain buffer is sized there. A later console command changes nothing, so
  the cvar is set at startup through the engine ini override (below). The help text says so.
* **The step world only.** `m_joltContactCounter` becomes the step world's contact listener, and
  `onJoltStepWorldStepped_Step` its after-step observer (`JoltWorld-rationale.md` §14 in
  og-simulation-jolt's docs). The game-thread shadow world never steps, only restores, so it would count
  nothing, and it never gets a listener. `m_joltContactCounter` is declared before `m_joltWorld`, and
  `JoltBackendWorldsUImpl`'s first `static_assert` keeps it there: the world holds a raw pointer to its listener
  until it is destroyed.
* **Logged once at world build** (Warning):

  ```
  [SimHost.Contacts] installed=<0|1> stepListener=<set|null> shadowListener=<set|null|none> reserved=<records>
  ```

  A `checkf` behind it requires the step world's listener to be set exactly when the counter was installed, and the shadow world's to be null. With
  the cvar 0 the line reads `installed=0 stepListener=null`: no listener, no observer, and the only
  per-step cost is one `has_value` check in `runJoltStep_Step` and one null check in the world's step.
* **One record per world step.** The observer runs on the step's thread after each world step, forward
  and replayed. `recordContactStep_Step` drains the counter into `m_contactSteps_Step` with the
  manager's `currentIntegratedTick` (the tick that step integrated) and the world's
  `physicsStepCount`. The buffer is reserved at world build for the ring depth plus one (a resimulation
  replays at most the ticks the ring holds, then one forward step); the authority has no ring and never
  replays, so it reserves one. A `checkf` fires before a push past the reservation.
* **The replay flag is the record's place in the batch.** The step driver runs a resimulation's replayed
  steps before the forward step inside one `runTick`, and its hooks fire once per `runTick`, none
  during the replay, so no hook can mark a replayed step. After `runTick` returns,
  `logContactSteps_Step` checks that it drained exactly `replayedTicks` + 1 records (the driver's
  `TickOutcome`) and logs every record but the last as replayed. The buffer is cleared after each
  `runTick`, logged or not.
* **The line** (`LogOGSimHost`, Verbose; only for a step with a contact, and the strings are built only
  when the category is at Verbose):

  ```
  [Verbose][SimHost.Contacts] tick=%u step=%llu worldStep=%llu replay=%d manifolds=%u added=%u rootRoot=%u rootChild=%u rootStatic=%u childChild=%u childStatic=%u staticStatic=%u ids=%s idsDropped=%u declarationsDropped=%u
  ```

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
* **Enable it** with the cvar and the category, on each peer that should log:

  ```
  -ini:Engine:[ConsoleVariables]:og.Jolt.ContactCounts=1 -ini:Engine:[Core.Log]:LogOGSimHost=Verbose
  ```

  The game module registers the cvar after the engine applies the ini's console variables; the engine
  keeps such a value in a placeholder and applies it when the cvar is registered, which is before the
  world's `BeginPlay`. The build line shows `installed=1`.
* **The Chaos host has no equivalent.** The cvar, the counter and the line exist only in this class;
  a Chaos build prints no `[SimHost.Contacts]` line, and Chaos collision events are out of scope.
