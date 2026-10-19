<!-- SPDX-License-Identifier: MPL-2.0 -->
# `JoltEngineTraitsUImpl` — guards

Every prohibition that governs a line of `JoltEngineTraitsUImpl.h` and `JoltEngineTraitsUImpl.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/JoltEngineTraitsUImpl-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**There is no live entry.** What the engine traits must provide is the `PhysicsEngineTraits` concept
(og-simulation's `PhysicsEngineTraits.h` since og-tickdriver task 15f), which a game's seam asserts; the moved statements carried no guard. Moved from the game
module's docs (BUSL-1.1) to OGSimulationJoltUnreal's (MPL-2.0) by og-tickdriver task 15e, with the code. Why each member is what it is lives in
`JoltEngineTraitsUImpl-rationale.md`.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

---

## R. Retired ids

None.
