<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerTickDriverGameUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h` (og-tickdriver task 8).
Ids are opaque, stable, and retired rather than reused. The reasoning lives in
`BrawlerTickDriverGameUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The binding's shape is checked by the compiler, not here: the tick driver's template parameter is constrained by
og-simulation's `TickDriverGame` concept, so a missing hook, a wrong return type or a manager by value does not
compile where `TickDriverBackendUImpl` names the driver.

---

## G-01 — the authority's release passes a step count of 1

**Site:** `m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);` in
`releaseInputsForAuthorityTick` (the call went through `owner()` from og-ue-simulationcomponentandmanagersplit task 24a until task 26a moved the release into the session; the literal is unchanged).

**The prohibition.** ⛔ The binding releases the delayed inputs for `authorityTick` with a step count of **1**. Do not
pass a larger count, a frame's step count, or a tick other than the one the hook was given. *Split from
G-01 of the brawler's former step hooks (retired, with a historical record of its own) by og-tickdriver task 8: the hook's half moved to
og-simulation with its call (`TickDriverStepHooks-guards.md` G-01), and the count, which is typed here, kept a site
in the brawler.*

**The consequence.** og-simulation's authority `beforeTick` calls this once per step, immediately before that tick is
simulated, with the tick the step driver will simulate (`serverTick + 1`). A count above 1 releases the inputs of
later ticks into the queue early, which is the batch pattern the per-step drain replaced
(og-simulationscheduler-withjolt design D4); another tick releases for the wrong one.

**What breaks if the tag moves.** Nothing mechanical: the concept fixes the hook's signature (one `SimTick`, no
count), not the literal the binding passes on.
