<!-- SPDX-License-Identifier: MPL-2.0 -->
# `UERenderTargetTable.h` and `.cpp` — guards

Every prohibition that governs a line of `UERenderTargetTable.cpp`. Each entry has an **opaque, stable id**. In the
source a single line `// ⛔G-nn  docs/UERenderTargetTable-guards.md` sits directly above the statement where the
forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose; it may never again appear as a `⛔G-nn`
tag.

⛔ **Nothing in this file is a rationale.** What the table is, its doors and its units live in
`UERenderTargetTable-rationale.md`.

---

## G-01 — the root-pose apply teleports the actor: no sweep

*Was a guard of the game's engine-hosted render apply; moved here when that apply became og-simulation's
`TickDriverPresenter` and this table became its render sink. The game's retired entry names this one as its
successor. Only its site, its wording for a game-agnostic actor and its cross-references changed.*

**Site:** the actor move (the engine's SetActorLocationAndRotation) in `UERenderTargetTable::applyRootPose`.

**The prohibition.** ⛔ Do not pass true for the sweep argument, and do not replace the call with one that sweeps or
with a non-teleport move. The actor's root is set to the render pose with the engine's TeleportPhysics teleport type:
the pose the presenter interpolated behind the host time and corrected (`TickDriverPresenter-rationale.md` in
og-simulation's docs).

**The consequence.** The pose comes from the physics world the tick driver steps, which has already resolved every
contact. A swept move tests the root again against the engine's own collision scene (the arena, and the other actors'
roots, which still carry their collision profiles) and stops it short at the first blocking hit, so the root sits
away from the simulated pose. Where the game reads the root as an input (an aim point, a camera anchor), the input
fed into the next simulated tick would be computed from the wrong point, so render sync is input-relevant there;
the caller's rationale, beside the code that names the expected root to `registerTarget`, says whether its root
is such an input. In a development build the presenter's applied check logs a `[SimHost.RenderApply]` MISMATCH
line on the first frame it happens.

**What breaks if the tag moves.** The tag marks the one statement that moves the actor. A second move added
elsewhere is not covered; the presenter's held check logs a MOVED line for a move made between two applies, in a
development build only.

---

## R. Retired ids

*(None yet.)*
