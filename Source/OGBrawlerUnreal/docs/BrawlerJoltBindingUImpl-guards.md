<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerJoltBindingUImpl` — guards

Every prohibition that governs a line of `BrawlerJoltBindingUImpl.h` and `BrawlerJoltBindingUImpl.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/BrawlerJoltBindingUImpl-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**There is no live entry.** What the binding must provide is the `BrawlerEngineBinding` concept, which
`PhysicsBackendUImpl.h` asserts; the moved statements carried no guard. The capsule size contract is
`OGBrawlerUECharacter-guards.md` G-01, whose site is the capsule size, not this check. Why each member is what it is
lives in `BrawlerJoltBindingUImpl-rationale.md`. G-01, found by measurement in og-sim-metres-yup task 8, retired in
task 7b (§R).

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

### G-01 — RETIRED by og-sim-metres-yup task 7b (2026-10-25): the cause moved out of `UEFrameConversion.h`

**Was:** *"never include `UEFrameConversion.h` in `BrawlerJoltBindingUImpl.cpp`"*, an absence tag at the end of the
`.cpp`'s include block (og-sim-metres-yup task 8). It forbade the include, and the capsule check's `ueFrame::lengthToUe`,
because with the include the Editor build's per-function disassembly of this translation unit, which holds the Jolt
host's explicit instantiation, differed from a partition control in simulation code:
`dAttackRadialSimulation::setInitialConditions<JoltPhysicsBodyAdapter>` called the generic `glm::translate` instead of
inlining `glm::translate(glm::mat4(1.f), rootTranslation)` (so an exact `-0` translation component could become `+0`),
and the shadow-pose lambda of `overwriteShadowPoses_GameThread` stopped inlining `glm::mat4_cast`.

**Why it retired.** The cause was the header's `OGSIM_OPTIMIZE_OFF` region: its `ueFrameDetail` helpers, defined in the
header, instantiated those glm templates in every includer. Task 7b moved their definitions into
`UEFrameConversion.cpp`. With the header included again and the check converting with `ueFrame::lengthToUe`, this
translation unit's code equals the partition control except `checkCapsuleContract` (task 7b's measurement), so the
include was restored and the check converts as the Chaos factory's does (`BrawlerJoltBindingUImpl-rationale.md` §3).
**Successor:** `UEFrameConversion-guards.md` G-05, at the place the cause was: the header holds no
`OGSIM_OPTIMIZE_OFF` region.
