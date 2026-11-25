<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionHooksUImpl.h` — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/BrawlerSessionHooksUImpl.h` (og-tickdriver task 8).
Ids are opaque, stable, and retired rather than reused. The reasoning lives in
`BrawlerSessionHooksUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.**

The header has no live guard since og-session-runtime task 26. What it keeps is checked by the compiler: its one prohibition, no
`HasAuthority()`, is the `#define` fence at its top (`OGBrawlerSimulationManager-rationale.md` §13.9); the backend's
`TickDriverBackendComposition` and its calls fix the shapes of `gravity` and `callWithSlotRequest`; and the tick driver's game is
og-sim-session's `SessionTickGame` (its hooks' shapes, `TickDriverGame` and the four refining concepts, are asserted there).

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag.

### G-01 — RETIRED by og-session-runtime task 26 (2026-11-25): moved with the release call to `SessionTickGame-guards.md` G-01

**Was:** *"the authority's release passes a step count of 1"*. Its tag sat above
`m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);` in `releaseInputsForAuthorityTick`.

**Why it retired.** The hook and the literal it guarded moved into og-sim-session: `SessionTickGame::releaseInputsForAuthorityTick`
passes `1` to `SessionNetHooks::releaseForTick`, and the prohibition is that file's G-01 (og-sim-session
`OGSimSession/docs/SessionTickGame-guards.md`), with the same consequence. The binding no longer forwards any hook.

The entry as it stood before its retirement:

> **Site:** `m_session.releaseDelayedInputsForStep(static_cast<int32>(authorityTick), 1);` in
> `releaseInputsForAuthorityTick` (the call went through `owner()` from og-ue-simulationcomponentandmanagersplit task 24a until task 26a moved the release into the session; the literal is unchanged).
>
> **The prohibition.** ⛔ The binding releases the delayed inputs for `authorityTick` with a step count of **1**. Do not
> pass a larger count, a frame's step count, or a tick other than the one the hook was given. *Split from
> G-01 of the brawler's former step hooks (retired, with a historical record of its own) by og-tickdriver task 8: the hook's half moved to
> og-simulation with its call (`TickDriverStepHooks-guards.md` G-01), and the count, which is typed here, kept a site
> in the brawler.*
>
> **The consequence.** og-simulation's authority `beforeTick` calls this once per step, immediately before that tick is
> simulated, with the tick the step driver will simulate (`serverTick + 1`). A count above 1 releases the inputs of
> later ticks into the queue early, which is the batch pattern the per-step drain replaced
> (og-simulationscheduler-withjolt design D4); another tick releases for the wrong one.
>
> **What breaks if the tag moves.** Nothing mechanical: the concept fixes the hook's signature (one `SimTick`, no
> count), not the literal the binding passes on.
