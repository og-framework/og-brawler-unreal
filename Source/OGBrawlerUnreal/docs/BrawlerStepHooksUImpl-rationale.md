<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerStepHooksUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerStepHooksUImpl.h` (og-simulationscheduler-withjolt task
54; design D §2.2; the step cost and the step-context asserts are task 52's, design D19 and D20). Prohibitions: `BrawlerStepHooksUImpl-guards.md`. The frame host it calls is
`SimulationFrameHostUImpl` (`SimulationFrameHostUImpl-rationale.md`).

---

## 1. What it is

The brawler's implementation of og-simulation's `StepHooks` concept, which `SimulationStepDriver`
calls on the step's thread once per `runTick`, never during a replay. It holds a reference to the frame
host and calls only the host's step doors, so the header needs neither the manager's type nor the
driver's: it is complete before the manager declares it, and a `static_assert` checks the concept. It
has no arm and no reflection markup and is included only from Jolt arms (design §2.6).

| hook | client | authority |
|---|---|---|
| `beforeTick(u)` | take `t0` (§3); drain J1 into the driver's `noteOccupancy` | take `t0`; drain J1; `checkf` the game thread (design D2); release the delayed inputs for `*u.authorityTick`, one tick (`⛔G-01`) |
| `beforeSimulate(u)` | take the step-start time `t1` (`⛔G-03`): after any replay, before the scratch save and the normal step | the same: after the release, immediately before `onGameSimulation` |
| `beforePhysics(t)` | `stampLatencyAfterStep_Internal(start time)` | the same |
| `afterTick(o)` | take `t2` (§3); `stampLatencyStepEnd_Internal()`; publish the render snapshot J5 from the outcome and `currentStepDeadline`; hand the step's saved state slot to the game-thread shadow world (a worker client only, and only when the step saved one); publish J3, `o.tick − o.physicsStep` (`⛔G-02`); push the step-cost sample (J6, §3) | the same; the authority has no shadow world, so the hand-over returns at once |

Every hook first `checkf`s that a step of this manager is running on this thread (§3).
`currentStepDeadline` is set by the manager before each `runTick` from the frame's batch (J4); the
render publish copies it into the snapshot in `afterTick`.

## 2. Why each hook does what it does

* **J1 first.** Occupancy must reach the driver on the driver's thread, before the step reads the world,
  and before the replay, so a vacated slot is cleared from every held tick before a replay could restore
  it (driver G-05). The game thread never applies occupancy (manager G-81).
* **The drain per step** (design D3, D4). Each authority tick releases exactly the inputs due for it,
  immediately before it is simulated. No receipt lands between inline steps (receipts happen in the
  engine's receive dispatch), so the queue contents are those of the Chaos configuration's batch; the
  difference is at consumption, in a multi-step frame with a per-character gap tick, where the batch let
  a later input fill the gap early (design §2.6, "Where D4 shows"). The authority's `+ 1` comes from the
  driver (`UpcomingTick{true, serverTick + 1}`), not from a mapper.
* **The latency stamps** (design D9, D18; task 49). The Chaos configuration took the time immediately
  before `onGameSimulation` and stamped after it. `beforeSimulate` is that instant on both roles; on a
  client it is early only by the Jolt scratch save, microseconds. The stamps themselves read what
  `onGameSimulation` produced, so they are posted from `beforePhysics`. The step-end stamps go in
  `afterTick`, after the post-step pass and the ring save.
* **The render publish, every `runTick`** (design D11, §8; og-simulationscheduler-withjolt task 55).
  `afterTick` runs after the post-step pass, when every occupied body's post-step pose is in its
  simulatable's state, so the snapshot is filled from og-simulation's storage, not from Jolt. It runs
  once per `runTick` on both roles and never during a replay (the driver calls no hook then), so a
  replay never shows intermediate poses. Every step publishes, not only a frame's last: the render
  interpolation (task 19) blends consecutive steps' snapshots by their deadlines. The manager owns
  the channel and the fill (manager rationale §18, "The Jolt arm's render publish and apply").
* **The shadow hand-over** (design D §2.5, "(e) in detail"; og-simulationscheduler-withjolt task 56).
  `afterTick` runs after the driver's ring save, so the step world's ring already holds this step's tick
  when the step saved one. The host's `publishShadowSlot_Step` door hands the outcome to the manager,
  which copies that slot into the shadow channel on a worker client and does nothing elsewhere. Never
  in a replay, for the same reason as the render publish. The manager owns the channel, the copy and
  the restore (manager rationale §18, "The Jolt arm's game-thread shadow world").
* **J3 last.** The offset is stored after the step, from the outcome; the game thread reads it at the
  next `TG_StartPhysics` (frame-host rationale §5). Only the step-cost push follows it, and it changes
  nothing the step or the game thread decides on.

## 3. Step cost and the step-context asserts (design D19, D20)

**Step cost.** Three wall-clock times per `runTick`, on the step's thread:
* `t0`, the first statement of `beforeTick`, before the J1 drain;
* `t1`, the step-start time `beforeSimulate` already takes for the latency stamps (`⛔G-03`), after any
  replay on a client and after the input release on the authority;
* `t2`, the first statement of `afterTick`, after the step, the post-step pass and the ring save.

`afterTick` pushes `StepCostSampleUImpl{t1 - t0, t2 - t1, replayedTicks, resimRefused}` into the frame
host's step-cost ring J6 through `pushStepCost_Step`. So `t2 - t1` is the normal step on both roles,
and `t1 - t0` is, on a client, the J1 drain, the resimulation check and any replay, and on the
authority the J1 drain and the input release. The game thread drains the ring and reports both in
`[SimHost.Window]` (frame-host rationale §8). The push goes last, after the publishes the step and the
game thread depend on; the times it reports were taken before them. Three platform-clock
calls per tick; no hook runs during a replay, so a replay adds no sample of its own: it is inside
the replaying tick's `t1 - t0`.

**The step-context asserts.** Every hook begins with `checkInStep`, a `checkf` that this manager's
step flag is set on this thread (the frame host's `isStepRunningOnThisThread`, backed by the
manager's per-role "step is running" bit, which `runJoltStep_Step` sets around `runTick`). The hooks
reach the step driver (J1 into `noteOccupancy`), the world and the step-owned mailboxes; called from
anywhere but a step, they race the running step. In `beforeTick` and `afterTick` the check follows the
time stamp, so the stamp stays the first statement; the check compiles out of shipping builds.
