<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerStepHooksUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerStepHooksUImpl.h`
(og-simulationscheduler-withjolt task 54; design D, §7.1c). Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `BrawlerStepHooksUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The concept conformance is a `static_assert` and needs no entry; so are the game-thread `checkf` of an
authority `beforeTick` (design D2) and its `authorityTick` presence check.

---

## G-01 — the authority `beforeTick` drains one tick's input, and nothing else

**Site:** `m_host.releaseDelayedInputsForStep(*upcoming.authorityTick, 1u);` in `beforeTick`.

**The prohibition** (design D3, F5). ⛔ The authority hook releases the delayed inputs for
`*upcoming.authorityTick` with a step count of 1: the drain only. Do not call PROBE A, PROBE 6
(`onFrameStepsDue_GameThread`) or `reapConnections` from here, do not release more than one tick, and
do not substitute another tick for `authorityTick`.

**The consequence.** The probes and the reap are per-frame instruments (frame-host G-02, G-06): per
step, PROBE A records `dFrame0` samples and the reap moves the receipt gate's reference to the frame's
last tick. A count above 1 releases the inputs of later ticks into the queue early, which is the batch
pattern the per-step drain replaced (design D4). `authorityTick` is the tick the driver will simulate
(`serverTick + 1`, checked by the driver after the step); another tick releases for the wrong one.

---

## G-02 — the published offset is the outcome's integrated tick minus its physics step

**Site:** the `m_host.publishTickOffset_Step(...)` call in `afterTick`.

**The prohibition** (design D8, §2.4). ⛔ The offset is `outcome.tick − outcome.physicsStep`, from the
outcome. Do not read a clock here, and do not add or subtract 1.

**The consequence.** The game thread adds the offset to its dispatch counter to get the frame's first
upcoming tick (frame-host G-03), which PROBE A and the authority's reap use. `outcome.tick` is the tick
the step integrated, so the sum is already the upcoming tick; a `± 1` shifts it, and a clock read (the
prediction clock moves in the replay and on Skip/Stall) breaks the "advances by the dispatched step
count" property the mapper had.

---

## G-03 — the step-start time is taken in `beforeSimulate`, on both roles

**Site:** `m_stepStartSeconds = FPlatformTime::Seconds();` in `beforeSimulate`.

**The prohibition** (design D9, D18). ⛔ Do not take the "consumed" time in `beforeTick` or in
`beforePhysics`. `beforeSimulate` runs after any replay and immediately before the normal step consumes
input, which is the instant the Chaos configuration stamps (before `onGameSimulation`).

**The consequence.** `beforeTick` runs before the client's replay, so the time would be early by the
whole replay; `beforePhysics` runs after the integrate, so it would be late by input collection, the
pre-integrate systems and the integrate. Either biases the H4, H6 and H6c hops against the Chaos
build at gate 22.
