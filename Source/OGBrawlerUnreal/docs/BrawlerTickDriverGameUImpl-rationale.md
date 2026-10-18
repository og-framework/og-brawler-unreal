<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerTickDriverGameUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h` (og-tickdriver task 8, 2026-10-16).
Prohibitions: `BrawlerTickDriverGameUImpl-guards.md`.

**If this file and the header disagree, the header is authoritative and this file is stale.**

---

## §1 What it is

The brawler's `TickDriverGame` for og-simulation's `SimulationTickDriver`: the seven game hooks the driver and its
step hooks call, each forwarded to the session `ASimulationManagerUImpl`, plus the manager and the storage the
driver's step driver runs on. It owns nothing and adds no logic of its own; the one decision it makes is the
reap's null check (§2).

**Why a class template over the session.** `TickDriverBackendUImpl.h` names the driver type, and the session's header
includes `TickDriverBackendUImpl.h` (and, since og-tickdriver task 15e, this header beside it) before the session class
is defined. A plain class there could declare its hooks but
not define them, because every body calls a member of the still-incomplete session. Over `SessionT`, the class is
complete with the session incomplete (it holds a reference), the concept check needs only the declarations, and
each body is instantiated where it is first used, where the session is complete: in the host's `.cpp` until
og-tickdriver task 15e (TickDriverBackendUImpl.cpp, a historical name), and since then in `BrawlerJoltBindingUImpl.cpp`,
which instantiates the host. The only instantiation is `BrawlerTickDriverGameUImpl<ASimulationManagerUImpl>`, named
`BrawlerTickDriverGame` at the end of this header since task 15e (the host's own alias before). og-tickdriver task 15b made the host a template and kept this one a template: the session
still holds the host by value, so its header still includes the host's before the session class is defined
(`TickDriverBackendUImpl-rationale.md` §10).

**Friendship.** The bodies reach the session's private members (`m_manager`, `m_storage`,
`onFrameStepsDue_GameThread`, `releaseDelayedInputsForStep`), so the session names the template a friend, a
declaration-only line in its header; the frame host, which made these calls before og-tickdriver task 8, needs no
friendship any more and lost its line.

## §2 The hooks

| hook (og-simulation) | when, which thread | the binding calls |
|---|---|---|
| `manager()`, `storage()` | the driver's constructor (the step driver runs on the manager) | `*m_session.m_manager`, `m_session.m_storage` |
| `beforePump(hostNow, rate)` | once per frame, on the game thread, before the pump | nothing: the brawler changes no rate in the MVP |
| `onFrameStepsDue(first, n)` | once per frame with steps, game thread, before the steps (`SimulationTickDriver-guards.md` G-02) | `onFrameStepsDue_GameThread(first, n)`: PROBE A on both roles, and on the authority PROBE 6 and the one-shot `[PacketBudget]` line (`SimulationManagerUImpl-rationale.md` §8) |
| `afterInlineBatch(first)` | once per frame, after the inline steps (`SimulationTickDriver-guards.md` G-06) | the reception coordinator's `reapConnections(first)`, when the session has a coordinator: only the authority does, so an inline client reaps nothing |
| `releaseInputsForAuthorityTick(tick)` | each authority step's `beforeTick`, on the game thread (`TickDriverStepHooks-guards.md` G-01) | `releaseDelayedInputsForStep(tick, 1)` (`⛔G-01`): one `[ReleaseBatch]` with `numSteps=1` per step |
| `onStepStarted(t1)` | each step's `beforePhysics`, on the step's thread | `stampLatencyAfterStep_Internal(t1)` |
| `onStepEnded()` | each step's `afterTick`, before the render and shadow publishes | `stampLatencyStepEnd_Internal()` |
| `onPostPhysics()` | every frame at `TG_EndPhysics`, after the wait | `OnPostPhysicsStep()` |

## §3 Why the brawler's hooks sit where they do

* **The per-frame pair and the drain** (og-simulationscheduler-withjolt design D3, D4). The Chaos configuration
  ran PROBE A, PROBE 6, the drain of the whole batch and the reap once per physics frame in
  `InjectInputs_External`. Here the probes and the reap stay per frame, and the drain is per authority step,
  immediately before the tick it releases for. No receipt lands between inline steps (receipts happen in the
  engine's receive dispatch), so the queue contents are those of Chaos's batch; the difference is at consumption,
  in a multi-step frame with a per-character gap tick, where the batch let a later input fill the gap early.
  The details are `SimulationManagerUImpl-rationale.md` §7.
* **The reap's null check.** Before og-tickdriver task 8 the frame host made the call only when
  `getReceptionCoordinator()` returned one; the driver now calls `afterInlineBatch` on every inline frame with
  steps, an inline client's included, so the check moved here unchanged.
* **The latency stamps** (design D9, D18; og-simulationscheduler-withjolt task 49). The Chaos configuration took
  the step-start time immediately before `onGameSimulation` and stamped after it. og-simulation's
  `beforeSimulate` takes `t1` at that instant on both roles (`TickDriverStepHooks-guards.md` G-03); on a client it is
  early only by the Jolt scratch save, microseconds. The stamps read what `onGameSimulation` produced, so they are
  posted from `beforePhysics`; the step-end stamps go in `afterTick`, after the post-step pass and the ring save
  (`SimulationManagerUImpl-rationale.md`, "The Jolt configuration's stamp sites").
* **`onPostPhysics` every frame.** `OnPostPhysicsStep` runs with or without steps, as the scene's post-tick delegate
  did, so the correction rotation still advances once per game frame. Its first backend call is the Jolt host's
  `beforePresent_GameThread` (`TickDriverBackendUImpl-rationale.md` §5, §6).

## §4 The tick the per-frame hooks receive

`first` is the batch's first physics step plus the offset J3 the step hooks publish, `tick − physicsStep`, with no
`+ 1` (`SimulationTickDriver-guards.md` G-03). It reproduces the Chaos tick mapper's semantics on driver data:
`TickOutcome::tick` is the tick the step integrated, so the sum is the frame's first upcoming tick, and the series
advances by the dispatched step count whatever a worker client's progress. The one residual difference: the mapper
took its offset before a step moved the clock, the driver after, so a client Skip or Stall's ±1 reaches PROBE A one
frame earlier. Before the first `afterTick` the offset is 0 and PROBE A seeds on its first sample, as with the
mapper's initial 0. On the authority `first` is exactly the server clock plus one, because every earlier step has
completed inline and published its offset.

## §5 The game binding of the tick-driver host (og-tickdriver task 15e)

Since og-tickdriver task 15e the tick-driver host is OGSimulationUnreal's `TickDriverBackendUImpl<GameBindingT,
EngineTraitsT, EngineBindingT>`, which names no brawler type (`TickDriverBackendUImpl-rationale.md` §12). This class is
its `GameBindingT`: besides the seven hooks and the two doors above, it supplies every game type and statement the
host used to name (lead ruling L1).

| member | what it is | was, in the host |
|---|---|---|
| `Session` | `SessionT`, the brawler's `ASimulationManagerUImpl` | the owner's type |
| `Component` | `USimmableUpdateComponent` | the component parameters of the three registration entry points |
| `Character` | `AOGBrawlerUECharacter` | the character parameter, and the cast in `onRegistered_GameThread` |
| `StaticData` | `simulatableBrawler::StaticData` | the static data's type in `createBodies_GameThread` |
| `gravity(session)` | the session's movement gravity, `m_staticData.m_movementStaticData.gravity` | the same read in `buildWorld` |
| `callWithSlotRequest(session, owner, key, slotCall)` | the cast and `checkf` of the owner, its name in UTF-8, the role from the net mode, the replicated slot and the slot publish, handed to `slotCall` with the host's key | the body of the host's `callWithSlotRequest`, the same statements and message in the same order |

* **The character type is a defaulted template parameter of `callWithSlotRequest`** (`CharacterT = Character`). The
  character is incomplete in this header, which the session's header includes, and a non-dependent cast or member call
  on it would be compiled where the template is defined. Over `CharacterT` they are compiled where the function is
  instantiated, in `BrawlerJoltBindingUImpl.cpp`, where the character is complete; the only argument it is ever
  given is the default.
* **`gravity` and `callWithSlotRequest` read the session's private members** (`m_staticData`) and its net mode; the
  session's friend line for this template already covers them.
* **The `HasAuthority` poison.** The role is the net mode's (`GetNetMode() != NM_Client`), never the session's
  `HasAuthority()`, which is always true (`SimulationManagerUImpl-rationale.md` §13.9). The header defines
  `HasAuthority` to the session's poison name after its includes and undefines it at its end, as the host's `.cpp`
  did around this statement before (`TickDriverBackendUImpl-rationale.md` §1).
* **`BrawlerTickDriverGame`** is the alias of the one instantiation, after the class; the seam
  (`PhysicsBackendHostUImpl.h`) and the host's explicit instantiation name the host over it.
