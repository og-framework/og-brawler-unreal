<!-- SPDX-License-Identifier: MPL-2.0 -->
# `TickDriverBackendUImpl.h` — guards

The prohibition on the Jolt host: the end-of-play wait. The render apply's teleport left with the render apply in
og-tickdriver task 9 (§R, G-04). The bind under the world mutex, the shadow world's construction and mirroring, and
the slot decision left with the world, the slots, the bind and the shadow world in og-tickdriver task 10 (§R, G-02,
G-03, G-05). Every entry here moved from
`SimulationManagerUImpl-guards.md` in og-simulationscheduler-withjolt task 83, with the code it guards; each names its
old id. In the source a single line `// ⛔G-nn  docs/TickDriverBackendUImpl-guards.md` sits directly above the statement or
declaration where the forbidden edit would be typed.

Named JoltBackendUImpl-guards.md until og-tickdriver task 15b (historical name), when the class became the template
`TickDriverBackendUImpl` and its files were renamed. The ids did not change; every citation was re-pointed.

Moved from the game module's docs (BUSL-1.1) to OGSimulationUnreal's (MPL-2.0) by og-tickdriver task 15e, with the
header, by the user's licence ruling of 2026-10-18. The ids and the entries did not change; the one live site,
G-01's, moved with the member definitions from the deleted TickDriverBackendUImpl.cpp (a historical name) into
`TickDriverBackendUImpl.h`, the same statement in the same function.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose or in an assertion message; it may never
again appear as a `⛔G-nn` **tag**.

⭐ **The join is machine-checked in both directions** by this repository's guard-tag lint: every tag resolves to an
entry here, every live entry is referenced by exactly one tag, no id is duplicated, and no retired id reappears as
a tag. ⚠ It checks that an entry EXISTS. It never checks that this text is TRUE.

⛔ **Nothing in this file is a rationale.** That lives in `TickDriverBackendUImpl-rationale.md`.

**Re-verified when moved (task 83).** The wait and its check on the world mutex (`G-01`; since og-tickdriver task 8
`waitForOutstandingSteps` in og-simulation's `SimulationTickDriver.h`, reached through `stop()`; since og-tickdriver
task 15d through the tick backend's `stop()`). The checks the
retired entries named were re-verified by og-tickdriver task 10 at their successors' sites (§R).

---

## G-01 — `EndPlay` waits for every outstanding step task before any teardown

*Was `SimulationManagerUImpl-guards.md` G-80; moved here by og-simulationscheduler-withjolt task 83, when the Jolt host left the UImpl. Only its site and its cross-references changed.*

**Site:** `m_tickBackend.stop();` in `onEndPlayBegin`, after the frame host's tick functions are unregistered. *Moved
by og-tickdriver task 15d (2026-10-18): the site was `m_tickDriver->stop();` inside `if (m_tickDriver.has_value())`;
the tick driver is og-simulation's `SimulationTickBackend`'s member since then, and its `stop()` makes the same call
under the same condition. The prohibition and its id are unchanged.* `ASimulationManagerUImpl::EndPlay` calls `onEndPlayBegin` as its first
statement. *Reworded by og-tickdriver task 8 (2026-10-16): the wait was the frame host's `waitForOutstandingSteps`
and is now the tick driver's `stop()`, which clears the driver's running flag and then waits for the pending and
the blocking step task. The prohibition is unchanged.*

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
it checks that this thread does not hold the world mutex (`SimulationTickDriver-guards.md` G-05, the successor of the
retired frame-host G-05).

---

## §R — Retired ids

⛔ **An id is never reused.** A retired id may be named in prose; it may never again appear as a `⛔G-nn` tag in
this file's sources.

### G-02 — RETIRED by og-tickdriver task 10 (2026-10-16): the bind and the release moved into og-simulation-jolt's world host

**Was:** *"the Jolt bind runs under the world mutex, and the game thread never applies occupancy"* (before it
`SimulationManagerUImpl-guards.md` G-81). Its tag sat above the world-mutex scope (JoltWorldLockScopeUImpl, since
deleted) in this class's `createBodies_GameThread`.

**Why it retired.** Task 10 deleted the bind and the release from this class: `createBodies_GameThread` now asks the
world host for the slot (`acquire`) and calls its `bind`, and `onUnregistered_GameThread` calls its `release`, in
og-simulation-jolt's `JoltTickWorldHost`, which takes the world lock around the same statements and pushes the vacate
through the caller's door. The prohibition is unchanged. **Successors:** `JoltTickWorldHost-guards.md` G-01 (the bind
under the world lock, tag at the bind's lock) and G-02 (the release pushes the vacated slot and never applies
occupancy, tag at the release's push), written by og-tickdriver task 6 with the code. The occupancy half was typed
at another statement than the lock, so its successor is split.

### G-03 — RETIRED by og-tickdriver task 10 (2026-10-16): the shadow world moved into og-simulation-jolt's world host

**Was:** *"the shadow world is built by the step world's calls, and every bind and release is mirrored into it"*
(before it `SimulationManagerUImpl-guards.md` G-82). Its tag sat above the shadow world's construction in this
class's buildShadowWorld (since deleted).

**Why it retired.** Task 10 deleted the shadow world from this class: its construction, its statics, its adapters,
the bind and release mirrors and the bind-table comparison are the world host's (`JoltTickWorldHost`), from the
same configuration with the same two fields changed. The prohibition is unchanged. **Successors:**
`JoltTickWorldHost-guards.md` G-03 (the construction), G-04 (every bind mirrored) and G-05 (every release mirrored),
written by og-tickdriver task 6 with the code. The two mirrors were typed at other statements than the
construction, so the successor is split.

### G-04 — RETIRED by og-tickdriver task 9 (2026-10-16): the render apply's teleport moved into the Unreal render sink

**Was:** *"the render apply teleports the capsule: no sweep"* (before it `SimulationManagerUImpl-guards.md` G-83).
Its tag sat above the actor move (the engine's SetActorLocationAndRotation) in this class's render apply.

**Why it retired.** Task 9 deleted the render apply from this class: the pose build, the apply loop and the checks
are og-simulation's `TickDriverPresenter`, and the actor move is its render sink's `applyRootPose`, in
OGSimulationUnreal's `UERenderTargetTable`, with the same arguments (no sweep, the engine's TeleportPhysics
teleport). The prohibition is unchanged. **Successor:** `UERenderTargetTable-guards.md` G-01 (OGSimulationUnreal),
written by og-tickdriver task 9 with the code.

### G-05 — RETIRED by og-tickdriver task 10 (2026-10-16): the slot decision moved into og-simulation's slot table

**Was:** *"a client never picks a Jolt slot itself: it binds the slot the authority chose, or waits"* (before it
`SimulationManagerUImpl-guards.md` G-84). Its tag sat above the `decideBodySlot` call in this class's slot acquire
(tryAcquireJoltSlot, since deleted).

**Why it retired.** Task 10 deleted the slot table from this class: the decision and the waits are og-simulation's
`TickSlotTable`, which the world host holds and calls from its `acquire`. The prohibition is unchanged.
**Successors:** `TickSlotTable-guards.md` G-01 (the decision is `decideBodySlot`'s, from the caller's role and
replicated slot; tag at the call) and G-02 (a wait returns no slot; tag at the wait's return), written by
og-tickdriver task 5 with the code.

**The caller's half has no tag, by decision.** The role is now computed in the game binding's `callWithSlotRequest`
(the brawler's `BrawlerTickDriverGameUImpl`, `session.GetNetMode() != NM_Client`; in this class, on the owner,
until og-tickdriver task 15e), outside the slot table. A client that passes `true` there makes the table
assign a slot as the authority and then publish it through `SetAuthoritativePhysicsSlot`, whose `checkf` that it runs
on the authority fires on the first client acquire in a development build: a run-time check, so no tag (as this
entry's own "what breaks" said of the role line). The reverse edit, the authority passing `false`, makes the
authority wait for a replicated slot that only it could write, so no character ever binds and every join logs
`[SimHost.Slot] wait=WaitForReplication`. A pick typed in a caller that does not wait on the empty answer
(`registerVizVolume`, `createBodies_GameThread`) had no check before task 10 and has none now.
