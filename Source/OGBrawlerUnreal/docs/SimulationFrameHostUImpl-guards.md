<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `SimulationFrameHostUImpl.h` and `.cpp` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/SimulationFrameHostUImpl.cpp`
(og-simulationscheduler-withjolt task 54, the Jolt configuration's frame host; design D, §7.1b). Ids are
opaque, stable, and retired rather than reused. The reasoning lives in
`SimulationFrameHostUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The rules the compiler or a runtime check holds need no entry: the authority is refused a worker at
`begin` (a `checkf`, design D2), a full occupancy ring is a `checkf`, and the two waits `checkf` that
this thread does not hold the manager's world mutex (rationale §6).

---

## G-01 — the authority steps inline, chosen by role, never by the engine's threading switch

**Site:** `if (m_stepsInline)` in `startSteps_GameThread`.

**The prohibition** (design D2, F9). ⛔ Do not dispatch an authority step to a worker, and do not key
the inline-or-worker choice on the engine's ShouldUseThreadingForPerformance or any other engine
threading switch. The choice is the manager's `m_joltStepsInline`, fixed at `BeginPlay` from the role
(authority always inline; a client on a worker unless `og.Sim.RunInline` is 1).

**The consequence.** The authority's per-step input release (`BrawlerStepHooksUImpl-guards.md` G-01)
delivers into `USimmableUpdateComponent`, a UObject, so an authority step on a worker touches UObjects
off the game thread. The engine's switch would also put a listen-server or standalone authority on a
worker, as Chaos does.

**What breaks if the tag moves.** The branch is the only place a batch chooses its thread. `begin`
refuses an authority with `stepsInline` false (a `checkf`), but a new dispatch path typed elsewhere
would bypass both.

---

## G-02 — the per-frame probe call runs once per frame, before the steps, only when steps are due

**Site:** the `m_owner->onFrameStepsDue_GameThread(firstUpcomingSimTick, ...)` call in
`startSteps_GameThread`.

**The prohibition** (design D3, F5). ⛔ Do not call `onFrameStepsDue_GameThread` per step, from a hook,
or in a frame with no due step (the `numSteps == 0` return above it). It runs once per frame, before
the frame's steps, with the frame's first upcoming tick and its step count: the same condition and
arguments as the Chaos configuration's `InjectInputs_External`.

**The consequence.** PROBE A measures sim ticks per game-thread frame. Called per step, every sample
after a frame's first lands in the same `GFrameCounter` and reads as `dFrame0`, and PROBE 6 samples the
send budget several times a frame. Called in a frame without steps, PROBE A gains zero-step samples
Chaos never produced.

---

## G-03 — the frame's tick is the dispatch counter plus the published offset, with no `+ 1`

**Site:** `const int32 firstUpcomingSimTick =` in `startSteps_GameThread`.

**The prohibition** (design D8, §2.4; successor of the UImpl's G-71 derivation). ⛔ Do not read a
simulation clock here, do not substitute the last completed tick, and do not add 1. The value is the
scheduler's first step of this frame plus the offset the hooks publish (J3, `BrawlerStepHooksUImpl-guards.md`
G-02).

**The consequence.** A game-thread clock read is an unsynchronized read of a value the worker step
writes. The last completed tick lags by the steps still in flight and jitters PROBE A's tick delta. A
`+ 1` makes the value one high: harmless to PROBE A's deltas, but the authority's reap reuses the value
(G-06) and its receipt-gate reference would be one tick ahead.

---

## G-04 — the scheduler is pumped with the accumulated world delta

**Site:** `const uint32 numSteps = m_scheduler->pump(m_hostTimeSeconds);` in `startSteps_GameThread`.

**The prohibition** (design D6). ⛔ Do not pump with the platform clock (FPlatformTime) or any time
source other than `m_hostTimeSeconds`, which accumulates the tick function's `DeltaTime` on the line
above.

**The consequence.** Chaos accumulates the world's delta seconds, so a paused world (no tick functions
run), slomo and the world's frame-time clamp step the simulation as they did. A wall clock steps a
paused world on resume, ignores slomo, and catches up a clamped hitch the engine meant to drop.

---

## G-05 — the game thread never waits for a step task while holding the world mutex

**Site:** the `WaitUntilTasksComplete` call in `waitForOutstandingSteps` (the `EndPlay` wait, called
from the manager's `⛔G-80` site).

**The prohibition** (design §2.4, "No deadlock"). ⛔ Do not wait for a step task, here or in
`endSteps_GameThread`, while this thread holds the manager's world mutex (J2), and do not take J2
around either wait.

**The consequence.** A step task takes J2 for each whole tick. A game thread that holds J2 and waits
for that task deadlocks: the task waits for the mutex, the game thread waits for the task.

**Enforcement.** Both waits begin with a `checkf` on the manager's "this thread holds J2" flag. The
end-of-physics wait never blocks the game thread itself: it holds the tick group open with
`DontCompleteUntil` and runs `OnPostPhysicsStep` as a game-thread task once the steps finish (the
engine's own end-of-physics pattern), so the tag marks the one blocking wait.

---

## G-06 — the authority reaps once per frame, after the inline steps

**Site:** the `coordinator->reapConnections(firstUpcomingSimTick)` call in `startSteps_GameThread`.

**The prohibition** (design D3, F5). ⛔ Do not move the reap into the step loop or a hook, and do not
call it before the frame's steps or in a frame with no due step. It runs once, after `runBatch`, with
the same `firstUpcomingSimTick` the probe call received.

**The consequence.** The reap is the only feed of the coordinator's `noteServerTick`, the receipt
gate's reference tick. Per step, the reference would be the frame's last tick when the next frame's
receipts are judged, instead of the frame's first, and the gate's acceptance window would shift by
the frame's step count in every multi-step frame. On the authority the value is exactly the server
clock plus one, because inline means every earlier step has completed and published its offset.

Design D's guard table (§7.1b) gave this prohibition and G-02 one id ("the two per-frame calls"); the
comment rule allows one tag per site, so the reap has its own id.
