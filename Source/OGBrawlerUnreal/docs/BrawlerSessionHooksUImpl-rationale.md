<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerTickDriverGameUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionHooksUImpl.h` (og-tickdriver task 8, 2026-10-16).
Prohibitions: `BrawlerSessionHooksUImpl-guards.md`.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is

The brawler's `TickDriverGame` for og-simulation's `SimulationTickDriver`: the seven game hooks the driver and its
step hooks call, each forwarded to the session, plus the manager and the storage the driver's step driver runs on.
Since og-input-latency task P5-15 it is also a `TickDriverSampleGame`: an eighth hook, `sampleInput()`, publishes each
registered character's input sample (§8). Since og-input-latency task P5-30 it is also a `TickDriverReceiveGame`: a ninth
hook, `receiveForStep()`, is a client's og-sim-net drain at each step's start (§9). Since og-input-latency task
P5-36 it is also a `TickDriverDisplayGame`: a tenth hook, `publishForDisplay()`, is a client's display publish after each
step batch, on the step's thread (§11).
The session is `UESimulationSession<BrawlerSessionTraitsUImpl>` since og-ue-simulationcomponentandmanagersplit task 24a (`AOGBrawlerSimulationManager`
before); the hooks that still forward to the actor reach it through the session's `owner()` (§6). It owns nothing and adds
no logic of its own beyond `sampleInput`'s walk over the registered components (§8). The one decision it made, the reap's null check, is the session's own since og-input-latency task P5-5
(§2, §7).

**Why a class template over the session.** `TickDriverBackendUImpl.h` names the driver type, and the session's header
includes `TickDriverBackendUImpl.h` (and, since og-tickdriver task 15e, this header beside it) before the session class
is defined. A plain class there could declare its hooks but
not define them, because every body calls a member of the still-incomplete session. Over `SessionT`, the class is
complete with the session incomplete (it holds a reference), the concept check needs only the declarations, and
each body is instantiated where it is first used, where the session is complete: in the host's `.cpp` until
og-tickdriver task 15e (TickDriverBackendUImpl.cpp, a historical name), and since then in `BrawlerJoltBindingUImpl.cpp`,
which instantiates the host. The only instantiation is `BrawlerTickDriverGameUImpl<UESimulationSession<BrawlerSessionTraitsUImpl>>` since task 24a
(over `AOGBrawlerSimulationManager` before), named `BrawlerTickDriverGame` at the end of this header since task 15e (the
host's own alias before). og-tickdriver task 15b made the host a template and kept this one a template: the session
still holds the host by value, so its header still includes the host's before the session class is defined
(`TickDriverBackendUImpl-rationale.md` §10).

**Friendship.** The bodies reach the session's private members (`m_manager`, `m_storage`, `m_staticData`, and since
og-ue-simulationcomponentandmanagersplit task 26b `onFrameStepsDue_Step`), so the session names the binding a
friend (`friend typename TraitsT::template TickDriverGame<UESimulationSession>;`). Until task 26b they also reached the
actor's private `onFrameStepsDue_GameThread` through `owner()` (and `releaseDelayedInputsForStep` too, until task 26a
moved the release and the coordinator into the session), which is why the actor has a friend line for the template, a
declaration-only line in its header; since task 26b the binding's remaining `owner()` calls (`OnPostPhysicsStep`,
`GetNetMode`) are public, so that line no longer covers a call (since task 27a only `GetNetMode` goes through `owner()`:
`onPostPhysics` calls the session's `onPostPhysicsStep`, as its friend); the frame host, which made these calls before og-tickdriver task 8, needs no
friendship any more and lost its line.

## §2 The hooks

| hook (og-simulation) | when, which thread | the binding calls |
|---|---|---|
| `manager()`, `storage()` | the driver's constructor (the step driver runs on the manager) | `*m_session.m_manager`, `m_session.m_storage` |
| `beforePump(hostNow, rate)` | on a client once per frame, on the game thread, before the pump; on the authority at every runner wake, on the runner | the session's `beforePump_Owner(hostNow, rate)`, both arguments forwarded (since og-time-dilation task 8, §12): on a client the clock-dilation stage (`UESimulationSession-rationale.md` §22, OGSimulationUnreal); on the authority its refused-rate line, the runner requests, then og-sim-net's server `drain()` (§10) |
| `idleUntil(deadline)` (`TickDriverIdleGame`, since og-input-latency task P5-19) | only on the authority, on the runner, between wakes | the session's `idleUntil_Runner(deadline)`: og-sim-net's `SimNetIdle` over the server endpoint, or `Unavailable` with no endpoint (§10) |
| `onFrameStepsDue(first, n)` | once per frame or wake with steps, before the steps (`SimulationTickDriver-guards.md` G-02): on a client on the game thread; on the authority once per stepping wake, on the runner | the session's `onFrameStepsDue_Step(first, n)` (the actor's, through `owner()`, until task 26b): PROBE A on both roles (`UESimulationSession-rationale.md` §9.9, OGSimulationUnreal); until og-session-runtime task 14 also PROBE 6 and the one-shot `[PacketBudget]` line on the authority |
| `afterInlineBatch(first)` | after each inline batch (`SimulationTickDriver-guards.md` G-06): on the authority once per stepping wake, on the runner; never on a client, whose batches run on a worker (`FrameBounded`) | the session's `afterAuthorityBatch(first)` (since og-input-latency task P5-5, §7): with og-sim-net's server, the core's post-simulation send, the clock's publish, then og-sim-net's `afterInlineBatch` (the corrections, the clock, the reap and the window), the correction stamps and the presentation slices (§10). Before P5-5 the binding tested the reception coordinator itself, through `getReceptionCoordinator()` (the actor's, through `owner()`, until task 26a) |
| `releaseInputsForAuthorityTick(tick)` | each authority step's `beforeTick`, on the runner (`TickDriverStepHooks-guards.md` G-01) | the session's `noteAuthorityStepTime_Step()` (since og-input-latency task P5-37: the step's time for its `[Release]` and `[AuthoritySimulation]` lines, `UESimulationSession-rationale.md` §19.7, OGSimulationUnreal), then `releaseDelayedInputsForStep(tick, 1)` (`⛔G-01`; the actor's, through `owner()`, until task 26a): one `[ReleaseBatch]` with `numSteps=1` per step |
| `receiveForStep()` (`TickDriverReceiveGame`, since og-input-latency task P5-30) | each client step's `beforeTick`, on the step's thread, before the resimulation check and the collect (`TickDriverStepHooks-guards.md` G-04); never on the authority | the session's `receiveForClientStep_Step()`: on a bound client the queued unregistrations and the og-sim-net drain, otherwise nothing (`UESimulationSession-rationale.md` §18.2, OGSimulationUnreal; §9) |
| `onStepStarted(t1)` | each step's `beforePhysics`, on the step's thread | `editLatencyBudgetDiagnostics().stampAfterStep_Internal(t1)` (a no-op in Shipping; `UELatencyBudgetDiagnostics-rationale.md` §3, OGSimulationUnreal), then the session's `afterClientCapture_Step()`: on a bound client the send at capture, otherwise nothing (`UESimulationSession-rationale.md` §18.2, OGSimulationUnreal; og-input-latency task P5-17). Until og-input-latency task P5-30 the drain followed the send here (`onClientStepStarted_Step()`), and fed the next step |
| `onStepEnded()` | each step's `afterTick`, before the render and shadow publishes | `editLatencyBudgetDiagnostics().stampStepEnd_Internal()`, then the session's `afterStep_Step()`: on a server, the server tick to the SimClock server (§10) |
| `publishForDisplay()` (`TickDriverDisplayGame`, since og-input-latency task P5-36) | once per client batch, after its last step, on the step's thread, under the world lock; never on the authority | the session's `publishDisplaySnapshot_Step()`: the client's display snapshot fill (`UESimulationSession-rationale.md` §17.2, OGSimulationUnreal; §11) |
| `onPostPhysics()` | every frame at `TG_EndPhysics`, on the game thread, after a client's wait for its step | the session's `onPostPhysicsStep()` (since task 27a; the actor's `OnPostPhysicsStep()`, through `owner()`, before) |
| `sampleInput()` (`TickDriverSampleGame`, since og-input-latency task P5-15) | once per frame, first in `startFrame` (`TG_StartPhysics`, game thread), before `beforePump` and the pump; never in `FreeRunning` | each registered component's owner, cast to the brawler character, `getInputCollection()->publishSample()` (§8) |

## §3 Why the brawler's hooks sit where they do

* **The per-frame pair and the drain** (og-simulationscheduler-withjolt design D3, D4). The former Chaos
  configuration (removed by og-session-runtime tasks 3–6) ran PROBE A, PROBE 6, the drain of the whole batch and the reap once per physics frame in
  `InjectInputs_External`. Here PROBE A and the reap run once per authority batch (PROBE 6 until og-session-runtime
  task 14), and the release is per authority step, immediately before the tick it releases for. No receipt lands
  between the steps of a batch (the runner services og-net only between batches), so the queue contents are those of
  the former Chaos batch; the difference is at consumption, in a multi-step batch with a per-character gap tick, where
  the Chaos batch let a later input fill the gap early. The details are `OGBrawlerSimulationManager-rationale.md` §7.
* **The reap's null check.** Before og-tickdriver task 8 the frame host made the call only when
  `getReceptionCoordinator()` returned one; the driver then called `afterInlineBatch` on every inline frame with
  steps, an inline client's included, so the check moved here unchanged. og-input-latency task P5-5 moved it on into the
  session's `afterAuthorityBatch` (§7), which now tests og-sim-net's server instead.
* **The latency stamps** (design D9, D18; og-simulationscheduler-withjolt task 49). The Chaos configuration took
  the step-start time immediately before `onGameSimulation` and stamped after it. og-simulation's
  `beforeSimulate` takes `t1` at that instant on both roles (`TickDriverStepHooks-guards.md` G-03); on a client it is
  early only by the Jolt scratch save, microseconds. The stamps read what `onGameSimulation` produced, so they are
  posted from `beforePhysics`; the step-end stamps go in `afterTick`, after the post-step pass and the ring save
  (`OGBrawlerSimulationManager-rationale.md` §16, "The step-side stamp sites per configuration").
* **`onPostPhysics` every frame.** `onPostPhysicsStep` (the actor's `OnPostPhysicsStep` until task 27a) runs with or without steps, as the scene's post-tick delegate
  did. Until og-session-runtime task 14 the in-frame server's pass advanced the correction rotation there, once per game
  frame; on the authority's runner it advances after each batch (`afterInlineBatch`). Its first backend call is the Jolt host's
  `beforePresent_GameThread` (`TickDriverBackendUImpl-rationale.md` §5, §6).

## §4 The tick the per-frame hooks receive

`first` is the batch's first physics step plus the offset J3 the step hooks publish, `tick − physicsStep`, with no
`+ 1` (`SimulationTickDriver-guards.md` G-03). It reproduces the semantics of the former Chaos tick mapper (removed by
og-session-runtime task 4) on driver data:
`TickOutcome::tick` is the tick the step integrated, so the sum is the frame's first upcoming tick, and the series
advances by the dispatched step count whatever a worker client's progress. The one residual difference: the mapper
took its offset before a step moved the clock, the driver after, so a client Skip or Stall's ±1 reaches PROBE A one
frame earlier. Before the first `afterTick` the offset is 0 and PROBE A seeds on its first sample, as with the
mapper's initial 0. On the authority `first` is exactly the server clock plus one, because every earlier step has
completed inline and published its offset.

## §5 The game binding of the tick-driver host (og-tickdriver task 15e)

Since og-tickdriver task 15e the tick-driver host is OGSimulationUnreal's `TickDriverBackendUImpl<GameBindingT,
EngineTraitsT, EngineBindingT>`, which names no brawler type (`TickDriverBackendUImpl-rationale.md` §12). This class is
its `GameBindingT`: besides the seven hooks (eight since og-input-latency task P5-15, §8) and the two doors above, it supplies every game type and statement the
host used to name (lead ruling L1).

| member | what it is | was, in the host |
|---|---|---|
| `Session` | `SessionT`, the brawler's `UESimulationSession<BrawlerSessionTraitsUImpl>` (`AOGBrawlerSimulationManager` until task 24a) | the owner's type |
| `Component` | `UOGBrawlerSimulatableComponent` | the component parameters of the three registration entry points |
| `Character` | `AOGBrawlerUECharacter` | the character parameter, and the cast in `onRegistered_GameThread` |
| `StaticData` | `simulatableBrawler::StaticData` | the static data's type in `createBodies_GameThread` |
| `gravity(session)` | the session's movement gravity, `m_staticData.m_movementStaticData.gravity` | the same read in `buildWorld` |
| `callWithSlotRequest(session, owner, key, slotCall)` | the cast and `checkf` of the owner, its name in UTF-8, the role from the actor's net mode (`session.owner().GetNetMode()` since task 24a), the replicated slot and the slot publish, handed to `slotCall` with the host's key; since og-input-latency task P5-5 the slot and the publish are the session's `physicsSlotFor` and `recordAuthoritativeSlot` (§7) | the body of the host's `callWithSlotRequest`, the same statements and message in the same order |

* **The character type is a defaulted template parameter of `callWithSlotRequest`** (`CharacterT = Character`). The
  character is incomplete in this header, which the session's header includes, and a non-dependent cast or member call
  on it would be compiled where the template is defined. Over `CharacterT` they are compiled where the function is
  instantiated, in `BrawlerJoltBindingUImpl.cpp`, where the character is complete; the only argument it is ever
  given is the default.
* **`gravity` and `callWithSlotRequest` read the session's private members** (`m_staticData`) and the actor's net mode;
  the session's friend line for this binding covers the first, and the net mode is public.
* **The `HasAuthority` poison.** The role is the net mode's (`GetNetMode() != NM_Client`), never the session's
  `HasAuthority()`, which is always true (`OGBrawlerSimulationManager-rationale.md` §13.9). The header defines
  `HasAuthority` to the session's poison name after its includes and undefines it at its end, as the host's `.cpp`
  did around this statement before (`TickDriverBackendUImpl-rationale.md` §1).
* **`BrawlerTickDriverGame`** is the alias of the one instantiation, after the class; the seam
  (`PhysicsBackendHostUImpl.h`) and the host's explicit instantiation name the host over it.

## §6 Over the session (og-ue-simulationcomponentandmanagersplit task 24a)

Since task 24a the session the binding is instantiated over is OGSimulationUnreal's `UESimulationSession<BrawlerSessionTraitsUImpl>`,
which the actor holds as `m_session` (`UESimulationSession-rationale.md`, in OGSimulationUnreal's docs). The binding names
it only through `SessionT`, so its text changed in five statements and the alias:

* `manager()`, `storage()`, `gravity()` and the two latency stamps read the session, with the same spellings: the members
  moved into the session and the session befriends the binding, and `editLatencyBudgetDiagnostics()` is the session's.
* `onFrameStepsDue`, the reap's coordinator lookup, `releaseInputsForAuthorityTick` and `onPostPhysics` call members that
  are still the actor's, so they go through the session's `owner()` (the interim path, until tasks 26a to 27a move them);
  the coordinator's type was named through the session's owner type. Since og-ue-simulationcomponentandmanagersplit
  task 26a the binding names it as the session's (`UESimulationSession::ReceptionCoordinator`) and calls the reap's
  coordinator lookup and the release on the session directly, not through `owner()`.
* `callWithSlotRequest` takes the role from `session.owner().GetNetMode()`: the session is not an actor.

The calls are inline forwards. Task 24a read the disassembly of the Jolt Editor's per-step release and step stamps: the
release and the step-start stamp are direct calls, and the step-end stamp is inlined, as before the task (its notes).

---

## §7 The session seams (og-input-latency task P5-5)

og-input-latency task P5-5 moved two of the binding's statements into the session, so that the switch to og-net (its task
P5-7) changes the session and not this binding (`UESimulationSession-rationale.md` §15, in OGSimulationUnreal's docs):

* **`afterInlineBatch`** calls the session's `afterAuthorityBatch(first)`, which holds the coordinator test and the reap that
  were here.
* **`callWithSlotRequest`** keeps its `checkf` of the cast, with the same message, and hands `slotCall` the session's
  `physicsSlotFor(owner, key)` and a setter that calls `recordAuthoritativeSlot(owner, key, slot)`. At P5-5 both read and
  wrote the character's `PhysicsSlotValue`, as the binding's `GetPhysicsSlot()` and `SetAuthoritativePhysicsSlot()` calls did; the
  setter captures the session, the owner and the key where it captured the character. The `checkf` tests the cast's result
  directly, since nothing else here uses the character. The defaulted `CharacterT` stays for that cast (§5).

**Since og-input-latency task P5-7** the doors no longer touch the character's slot property: on the authority they read and
write the session's slot map, keyed by the pawn actor, and on a client `physicsSlotFor` reads the character's og-sim-net roster
entry for the storage key this binding now passes as a third argument, `toStorageKey(character->GetSimCharacterId())` (0 while
the id has not replicated, which the session answers with "no slot yet") (`UESimulationSession-rationale.md` §16.4). This
binding is the one place that may read the brawler character's id for the session: the session is game-neutral. The `checkf` keeps its role and gets a true reason: only the brawler character's
pawns register (the session builds the bodies from its root capsule), and the slot reaches the clients on that character's
roster entry. Its message says so, and names the property the slot rode until P5-7.

Both doors are the session's private members, reached through its friend line for this binding (§1).

---

## §8 The input sample (og-input-latency task P5-15)

**What `sampleInput()` does.** It walks the session's registered components (`getRegisteredComponentsById()`, the
session's public, game-thread map), casts each live one's owner to the brawler character and calls its input collection
component's `publishSample()`. That copies the sample the component staged at the end of `updateGameThreadCache` into the
component's `TripleBuffer<BrawlerInputSample>` (og-simulation's `Mailbox.h`) and publishes it. The client's input provider,
`buildPlayerInput`, runs on the step's thread and reads only the latest published sample, never a field the game thread
writes. The sample and its reasoning are in the component's header (an old-convention file, which keeps its comments).

**Why here, first in `startFrame`.** og-simulation's `SimulationTickDriver` calls the hook first in `startFrame`, in
`Inline` and `FrameBounded`, before the clock read, `beforePump` and the pump (`SimulationTickDriver-rationale.md` §6, in
og-simulation's docs); `FreeRunning` never calls it (an `OG_CHECK` there). The host's start tick function runs it on the
game thread at `TG_StartPhysics`. The character's `Tick`, which stages the sample, runs earlier in the frame, at the actor
default `TG_PrePhysics`, after its controller's tick (the engine makes a possessed pawn's tick depend on its controller's), where Enhanced Input dispatches
the frame's events. So every step the frame dispatches reads values sampled in the same frame, before the pump, as it did
when it read the fields directly. What changed is a step batch that outlives its frame (`FrameBounded` waits at end of
physics for the earlier frames' steps, and for its own only in a single-step frame): it used to read the next frame's fields while the
game thread wrote them; it now reads whole samples, the newest one published when it reads.

**Every registered component, not only the local ones.** Only a client's autonomous proxies have an input provider
(`UESimulatableOwnerBinding-guards.md` G-09, in OGSimulationUnreal's docs), so only their samples are read. Publishing
the others' too costs a copy of a few dozen bytes per character per frame and keeps a second "is this local" rule out of
the binding, where it could drift from the provider's. An authority never runs the hook: every authority session steps
in `FreeRunning` (og-session-runtime task 14), which never calls it.

**The template parameter.** `sampleInput` is a function template over `CharacterT = Character`, as `callWithSlotRequest`
is (§5): the character and the component are incomplete in this header, and over `CharacterT` the cast and the
`getInputCollection()` call are compiled where the hook is instantiated (`BrawlerJoltBindingUImpl.cpp`). The concept
`TickDriverSampleGame` only needs the declaration, a `void` call with no arguments, which the default supplies.

**The `static_assert`.** The driver calls the hook under `if constexpr (TickDriverSampleGame<GameT>)`, so a binding that
lost the hook, or changed its return type, would still compile and simply never publish: every client input would then be
neutral. The `static_assert` after the `BrawlerTickDriverGame` alias makes that a compile error.

**The former Chaos configuration** (removed by og-session-runtime tasks 3–6) had no tick driver, so nothing called this
hook there; the input collection component published at the end of `updateGameThreadCache` instead, under
`OG_PHYSICS_BACKEND_CHAOS` (its Chaos steps started after the character's `Tick`, at the physics frame's start). Task 3
removed that arm with the macro.

## §9 The receive (og-input-latency task P5-30)

**What `receiveForStep()` does.** It forwards to the session's `receiveForClientStep_Step()`, which on a bound client
takes the game thread's queued unregistrations and drains og-sim-net (`UESimulationSession-rationale.md` §18.2,
OGSimulationUnreal). og-simulation calls it at the start of each client step, before the step's resimulation check and its
input collect (`TickDriverConcepts-rationale.md` §10, OGSimulation), so a correction, a relay ring or a clock sample the drain
delivers is used by the step that drained it. An unbound client and the server return at once (the hook never runs on an
authority step). Until og-session-runtime task 12 a frame-locked client returned at once too: its game thread drained at the
frame point.

**The send stays at capture.** `onStepStarted` keeps the send (`afterClientCapture_Step()`): it runs after the step's
collect, so it sends the input the step just captured. Only the drain moved: in `onStepStarted` it ran after the collect
and fed the next step (og-input-latency task P5-17's finding F2: H6c one tick, N6 up).

**The `static_assert`.** The step hooks call the hook under `if constexpr (TickDriverReceiveGame<...>)`, so a binding that
lost it would compile and a client would never drain once its session binds (the lanes' game-thread drain stops then,
`UESimNetLanes-guards.md` G-02, OGSimulationUnreal). The `static_assert` after the alias, beside `sampleInput`'s, makes that a compile error.

## §10 The authority loop's hooks (og-input-latency task P5-19)

On a dedicated server, the only world that runs an authority session since og-session-runtime task 14, og-simulation's
driver runs the authority in `FreeRunning` on the
session's runner thread (`UESimulationSession-rationale.md` §19, OGSimulationUnreal), and calls this binding there:
- **`idleUntil`** is new: `SimulationTickBackend::runAuthorityLoop` requires `TickDriverIdleGame`, and a `static_assert` below the
  class keeps the hook, because without it the loop, which every dedicated server runs, does not build (its message says so
  since og-session-runtime task 14, which retired the knob it named). It forwards to the session, whose `SimNetIdle` services
  og-net's server endpoint until the deadline. On clients nothing calls it.
- **`beforePump`** forwards on both roles, at every runner wake on the authority; a client runs the dilation stage there
  instead (§12). On the authority it is og-sim-net's drain point (P2-19: `beforePump` → `drain()`), after the session's
  runner requests (registration and unregistration, `UESimulationSession-rationale.md` §19.4).
- **`afterInlineBatch`** keeps its one call; the session's `afterAuthorityBatch` does the runner's sends and reap.
- **`onStepEnded`** also publishes the server tick to og-sim-net's SimClock server after every authority step (`afterStep_Step`;
  P2-20 F2); on a client it does nothing.
- **`sampleInput`** is not called in `FreeRunning` (og-simulation's `OG_CHECK`), and **`onPostPhysics`** still runs on the game
  thread, from the frame thread's `endFrame`.

## §11 The display publish (og-input-latency task P5-36)

**What `publishForDisplay()` does.** It forwards to the session's `publishDisplaySnapshot_Step()`, which forwards to
og-sim-session's `SessionDisplay::publishAfterBatch` (since og-session-runtime task 25); that fills the client's display
snapshot from the storage, the input resolution and the reconciliation and publishes it to the game thread
(`UESimulationSession-rationale.md` §17.2, OGSimulationUnreal; `SessionDisplay-rationale.md` §3, og-sim-session). og-simulation calls it once after each client batch, on the
thread that ran the batch, where no step runs (`TickDriverConcepts-rationale.md` §11, OGSimulation). Until P5-36 the session
filled the snapshot inside `onPostPhysicsStep`, on the game thread, while the frame's own batch could still be stepping on a
worker: under the frame-locked client's `EarlierFrames`, and under the immediate client's `ThisFramesSingleStep` in a frame
of two or more steps. `onPostPhysics` now only acquires what was published.

**The `static_assert`.** The driver calls the hook under `if constexpr (TickDriverDisplayGame<...>)`, so a binding that lost
it would compile and a client would publish no display snapshot: the post-physics pass only acquires one. The `static_assert` after the alias makes that a compile error.

## §12 The rate hook's arguments (og-time-dilation task 8)

`beforePump(hostNowSeconds, rate)` forwards both of its arguments to the session's `beforePump_Owner`. Until this task it
dropped them, because nothing in the brawler read the host time or changed the rate. Now a predicting client's session runs
og-simulation's clock-dilation stage there (`clockDilation::beforeClientPump`), which reads the door (`rate`: its tick offset,
step count, next deadline, interval and scale) and places the prediction tick with `hostNowSeconds`. Where the stage runs, in which modes, and
why `Shadow` changes no rate are the session's (`UESimulationSession-rationale.md` §22, OGSimulationUnreal). The binding still
decides nothing: one call, on whatever thread the driver calls it on (the game thread on a client, the runner on the authority,
§10).
