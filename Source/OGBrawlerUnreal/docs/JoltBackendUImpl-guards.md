<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `JoltBackendUImpl.h` and `.cpp` — guards

The prohibitions on the Jolt host: the end-of-play wait, the bind under the world mutex, the shadow world's
construction and mirroring, the render apply's teleport and the slot decision. Every entry here moved from
`SimulationManagerUImpl-guards.md` in og-simulationscheduler-withjolt task 83, with the code it guards; each names its
old id. In the source a single line `// ⛔G-nn  docs/JoltBackendUImpl-guards.md` sits directly above the statement or
declaration where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose or in an assertion message; it may never
again appear as a `⛔G-nn` **tag**.

⭐ **The join is machine-checked in both directions** by this repository's guard-tag lint: every tag resolves to an
entry here, every live entry is referenced by exactly one tag, no id is duplicated, and no retired id reappears as
a tag. ⚠ It checks that an entry EXISTS. It never checks that this text is TRUE.

⛔ **Nothing in this file is a rationale.** That lives in `JoltBackendUImpl-rationale.md`.

**Re-verified when moved (task 83).** The wait and its `checkf` on the world mutex (`G-01`, `waitForOutstandingSteps`
in `SimulationFrameHostUImpl.cpp`); the query adapter's `registerShape` and `registerVolume` asserting the access
predicate (`G-02`, `JoltSpatialQueryAdapter.cpp`); the three configuration lines above the shadow world's
construction, the two static builders and the non-shipping body-count `checkf` (`G-03`); the teleport type and the
`[SimHost.RenderApply]` MISMATCH and MOVED checks (`G-04`); `SetAuthoritativePhysicsSlot`'s authority `checkf` and the
two wait outcomes (`G-05`). Each check names the code as it stands after the move.

---

## G-01 — `EndPlay` waits for every outstanding step task before any teardown

*Was `SimulationManagerUImpl-guards.md` G-80; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** `m_frameHost.waitForOutstandingSteps();`, the second statement of `onEndPlayBegin` (after the frame
host's tick functions are unregistered). `ASimulationManagerUImpl::EndPlay` calls `onEndPlayBegin` as its first
statement.

**The prohibition** (og-simulationscheduler-withjolt task 54, design §9.1). ⛔ Do not move any
`EndPlay` statement above this wait, and do not drop it. The order is: unregister the two tick
functions (so no new step is dispatched), wait for the step task dispatched in this and earlier frames,
then everything else (`SimulationManagerUImpl-guards.md` G-64's resets, the listener unregistration, the post-tick-flush unbind).
The "do not move any `EndPlay` statement above" half is typed in `ASimulationManagerUImpl::EndPlay`, above its
`m_backend.onEndPlayBegin();`, and is guarded there: `SimulationManagerUImpl-guards.md` G-85. This entry's tag
guards the wait itself (dropping it, or moving a statement above it inside `onEndPlayBegin`).

**The consequence.** On a worker client the frame's step task runs on another thread through the rest
of the frame. A teardown above the wait resets the coordinator or the tier cache, or unbinds what the
step reads, while the step runs: a use-after-reset on a worker thread. The wait itself never deadlocks:
it `checkf`s that this thread does not hold the world mutex (frame-host G-05).

---

## G-02 — the Jolt bind runs under the world mutex, and the game thread never applies occupancy

*Was `SimulationManagerUImpl-guards.md` G-81; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `JoltWorldLockScopeUImpl` line of `createBodies_GameThread`, the bind `tryRegister`'s first call
makes through the seam.

**The prohibition** (og-simulationscheduler-withjolt task 53, design D7). ⛔ Do not move any part of
the bind out of the world-mutex scope: the `JoltPhysicsFactory` construction and every
`createPhysicalObject` (body defaults, user data, the bind table, the shape registrations through
the query adapter's `registerShape`) and every declaration volume's `registerVolume`. ⛔ Do not
replace the mutex with a queue: the bind must hand back body, shape and volume ids synchronously,
because the declaration bindings store them before `tryRegister` returns. ⛔ Do not set a slot's
occupancy from the game thread, here or anywhere: the slot stays parked until the step applies the
Occupy command `onRegistered_GameThread` pushes on J1 from `tryRegister`'s Ready path, after
`registerSimulatable` and `notifyCharacterRegistered` (task 54), and a vacated slot is parked by the step too, from the Vacate
command `releaseJoltSlot` pushes.

**The consequence of getting it wrong.** On a worker client the step runs on another thread and
holds the mutex for each whole tick (task 54, `runJoltStep_Step`). A bind outside the scope writes Jolt bodies, the bind
table's inertia and the query adapter's tables while the step reads them, a data race that corrupts
a tick silently. In a development build the query adapter's access predicate fails first: its
`registerShape` and `registerVolume` demand "this manager's step is running on this thread, or this
thread holds this manager's world mutex". Occupancy set from the game thread would change the world between two saved ticks
without the step's occupancy timeline recording it, so a replay would step with a different body
set than the prediction did.

**What breaks if the tag moves.** The tag marks the scope's opening line. Code typed above it, or
after the scope's closing brace, runs unlocked. Only the access predicate checks it, and only for
the query adapter's calls; the factory's body writes have no runtime check (design §2.6, "Jolt access
checks").

---

## G-03 — the shadow world is built by the step world's calls, and every bind and release is mirrored into it

*Was `SimulationManagerUImpl-guards.md` G-82; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `m_shadowWorld` construction in `buildShadowWorld`, worker client.

**The prohibition** (og-simulationscheduler-withjolt task 56, design D §2.5 "(e) in detail", OQ13 = (e)).
⛔ Do not build the shadow world from any configuration but the step world's `JoltWorldConfig` with
exactly two fields changed, `ringDepthTicks` (0) and `tempAllocatorBytes` (small): the slot count,
the slot template, the static layers, gravity and every other field stay the step world's. ⛔ Do not
build its statics by any calls but the step world's two `JoltStaticWorldBuilder` builds, from the same
two descriptions, in the same order, and do not add a static or a body to either world alone. ⛔ Do
not bind a slot in the step world without the same bind in the shadow (`bindShadowSlot`: the whole
`JoltPhysicsFactory` bind, body defaults included, with the same slot, storage key and options), and
do not release a slot in one world without the other (`releaseShadowSlot`). ⛔ Do not share one
adapter between the two worlds.

**The consequence of getting it wrong.** The restore copies only what a state slot holds: the slot
bodies' Jolt state, their occupancy and the shape enables. It refuses a slot only when the slot
bodies differ. Everything else must already agree, or the visualizations on a worker client query a
world that is not the step world's copy, silently:
- a different slot template or slot count changes the slot bodies, and every restore is refused
  (og-simulation-jolt's "refused: the snapshot's body set differs" Warning), so the shadow never moves;
- a missing, extra or reordered static is invisible to the restore (the save filter skips statics),
  so the arena the visualizations query differs from the one the step collides with;
- a bind that is not mirrored leaves the shadow's bodies without user data, so the query adapter drops
  every hit on them and the block-prediction arcs and target markers find no one; a different storage
  key, friction or mass changes hits or their order;
- a release that is not mirrored leaves the shadow's bind table claiming a body the step world freed,
  so the reader still resolves a character that has left.

In a development build two checks catch most of this: the `BeginPlay` `checkf` in `buildShadowWorld`
compares the two worlds' body counts and static body ids, and `compareShadowBindTables_GameThread`
logs a `[SimHost.Shadow] bindCompare … equal=0` Warning after a bind or release that leaves the two
bind tables, or the two factories' results, different.

**What breaks if the tag moves.** The tag marks the construction. The configuration is copied from the
step world's, and its two fields changed, in the three lines above it; the static builds, adapters and the body-count check follow
it. Any line inserted between the step world's build and the shadow's that changes one world only
breaks the prohibition with no compile error.

---

## G-04 — the render apply teleports the capsule: no sweep

*Was `SimulationManagerUImpl-guards.md` G-83; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** the actor move (the engine's SetActorLocationAndRotation) in
`applyRenderSnapshot_GameThread`.

**The prohibition** (og-simulationscheduler-withjolt task 55, design D11 and §8). ⛔ Do not pass
true for the sweep argument, and do not replace the call with one that sweeps or with a
non-teleport move. The capsule root is set to the render pose, with the engine's TeleportPhysics
teleport type: the newest render snapshot's pose in task 55, the pose interpolated two steps behind the
host time since task 19 (`JoltBackendUImpl-rationale.md` §7).

**The consequence.** The pose comes from the step world, which has already resolved every contact. A
swept move tests the capsule again against the engine's own collision scene (the arena and the other
pawns' capsules, which still carry the Pawn profile) and stops it short at the first blocking hit, so
the capsule sits away from the simulated pose. The capsule is the mouse-aim plane point
(`UOGBrawlerInputCollectionComponent::updateGameThreadCache`) and the camera boom's anchor, so the aim
input fed into the next simulated tick would be computed from the wrong point: render sync is
input-relevant in this project (design F7). In a development build the non-shipping check in
`checkRenderApply_GameThread` logs a `[SimHost.RenderApply]` MISMATCH line on the first frame it happens.

**What breaks if the tag moves.** The tag marks the one statement that moves the capsule. A second
move added elsewhere in the apply is not covered; the check's MOVED line catches a move made between
two applies, in a development build only.

---

## G-05 — a client never picks a Jolt slot itself: it binds the slot the authority chose, or waits

*Was `SimulationManagerUImpl-guards.md` G-84; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `decideBodySlot` call in `tryAcquireJoltSlot`.

**The prohibition** (og-simulationscheduler-withjolt task 66). ⛔ Do not bypass `decideBodySlot` on a
client, and do not pass it `true` for `isAuthority` there. ⛔ Do not turn either wait into a pick: when it
answers `BodySlotOutcome::WaitForReplication` (the pawn's `PhysicsSlotValue` has not arrived) or
`BodySlotOutcome::WaitForRelease` (the replicated slot is still held here by a character that has left the
authority), return no slot and let the caller retry. Never fall back to the lowest free slot, to the slot
the simulation id would give, or to any other slot this peer computes.

**The consequence of getting it wrong.** A slot's bodies have fixed Jolt body ids, so a character's body
ids are its slot's. A client that picks its own slot puts the character in a different slot from the
server whenever the two have seen joins and leaves in a different order: a client that joins late binds
its own pawn first and takes slot 0, while the server gave it the slot after everyone already there. Jolt
orders contact solving by body id, so a contact between two characters is then solved in a different
order on the client than on the server, the results differ by rounding, and the exact correction compare
reports a disagreement the netcode did not cause. Nothing crashes and nothing logs; only the
`[SimHost.Bind]` lines show it, as the same id with a different slot on two peers.

**What breaks if the tag moves.** The tag marks the decision. The role line above it is not covered, but
a client that tells the decision it is the authority also writes the slot to the pawn, and the pawn's
`SetAuthoritativePhysicsSlot` `checkf`s that it runs on the authority, so that edit fails in a
development build on the first client acquire. A pick typed in place of the waits' early return, or in a
caller that does not wait on the empty answer (`registerVizVolume`, and `createBodies_GameThread`, whose
`false` `tryRegister` turns into `Pending`), has no check: the
component's retry budget then never sees a wait, and only the bind lines show the mismatch.

---

## §R — Retired ids

*(None yet.)*
