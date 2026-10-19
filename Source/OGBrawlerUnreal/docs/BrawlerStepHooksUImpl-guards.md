<!-- SPDX-License-Identifier: BUSL-1.1 -->
# BrawlerStepHooksUImpl — guards (historical: retired ids only)

The brawler's engine-hosted step hooks, a header in `Source/OGBrawlerUnreal/` written by
og-simulationscheduler-withjolt task 54, were **deleted by og-tickdriver task 8 (2026-10-16)**. og-simulation's
`TickDriverStepHooks` (og-tickdriver task 3) replaced them, constructed by the tick driver `SimulationTickDriver`
(task 7). The hooks' explanation went with them: `TickDriverStepHooks-rationale.md` (og-simulation) for the hooks,
and `BrawlerTickDriverGameUImpl-rationale.md` for what the brawler's game binding does at each hook.

This file is kept only as the record of the three ids the header's tags used, so that none is ever reused. It has
no live entry and no source file.

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-tickdriver task 8 (2026-10-16): the authority's one-tick release moved into og-simulation's step hooks

**Was:** *"the authority beforeTick drains one tick's input, and nothing else"*. Its tag sat above the authority
`beforeTick`'s call of the frame host's input release, with a step count of 1.

**Why it retired.** Task 8 deleted the header. The release is now og-simulation's `beforeTick`, which calls the
game's `releaseInputsForAuthorityTick` with the upcoming authority tick. **Successors:** `TickDriverStepHooks-guards.md`
G-01 (og-simulation; the hook releases that tick and calls no per-frame game hook), and
`BrawlerTickDriverGameUImpl-guards.md` G-01 (the binding forwards it to `releaseDelayedInputsForStep` with a step
count of 1, the half of this prohibition that is typed in the brawler's file).

### G-02 — RETIRED by og-tickdriver task 8 (2026-10-16): the published offset moved into og-simulation's step hooks

**Was:** *"the published offset is the outcome's integrated tick minus its physics step"*. Its tag sat above the
`afterTick` offset publish.

**Why it retired.** Task 8 deleted the header; og-simulation's `afterTick` publishes the same difference.
**Successor:** `TickDriverStepHooks-guards.md` G-02.

### G-03 — RETIRED by og-tickdriver task 8 (2026-10-16): the step-start time moved into og-simulation's step hooks

**Was:** *"the step-start time is taken in beforeSimulate, on both roles"*. Its tag sat above the `beforeSimulate`
clock read.

**Why it retired.** Task 8 deleted the header; og-simulation's `beforeSimulate` takes `t1` at the same point, from
the engine clock (FPlatformTime::Seconds through `SimulationEngineUImpl`). **Successor:**
`TickDriverStepHooks-guards.md` G-03.
