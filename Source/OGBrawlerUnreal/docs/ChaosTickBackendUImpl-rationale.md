<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- ================= DECLARED LINT ESCAPES ==================================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: FRewindData::FindValidResimFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::SetTargetStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::RewindToFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::ApplyTargets -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsEvolutionGBF::Integrate -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: SetXR -- Chaos engine particle-handle method, outside every scan root -->
<!-- lint-external-ref: PushStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: ApplyCallbacks_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: ChaosMarshallingManager.h -- Chaos engine header, outside every scan root -->
<!-- lint-external-ref: PBDRigidsSolver.cpp -- Chaos engine source, outside every scan root -->
<!-- lint-external-ref: RewindToFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: AddUObject -- Unreal Engine delegate method, outside every scan root -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
# `ChaosTickBackendUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/ChaosTickBackendUImpl.h` and `Source/OGBrawlerUnreal/ChaosTickBackendUImpl.cpp`. Both
source files carry no prose: a two-line docs pointer, code, assertion messages, and one-line tags `⛔G-nn` → an entry
in `ChaosTickBackendUImpl-guards.md`.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says why the class exists and what it is.
* **§2** is the host itself: ownership, the delegates, the input hook, and the two pieces `buildWorld` moved.
* **§3**, **§4** and **§5** moved here from `SimulationManagerUImpl-rationale.md` (§8's rewind-push probe, §9's tick
  alignment, §14's first replayed step) in og-simulationscheduler-withjolt task 82, with the code they describe.
* **Names before og-tickdriver task 15b (historical):**
  class ChaosBackendUImpl (historical name), in ChaosBackendUImpl.h and ChaosBackendUImpl.cpp (historical file names);
  this document ChaosBackendUImpl-rationale.md and its guards ChaosBackendUImpl-guards.md (historical names);
  worlds struct ChaosBackendWorldsUImpl (historical name).
  Task 15b renamed them by who schedules the steps (Chaos's own tick), and changed nothing else. The sections below
  use today's names, also where they tell what an earlier task did.

---

## §1 Why this class exists

The user asked whether `ASimulationManagerUImpl` could be split so that the Chaos and Jolt backends are more
separate (og-simulationscheduler-withjolt phase F, 2026-10-15). The UImpl is the brawler's per-role simulation
session: the netcode session, the relay listeners, registration and input, the per-frame sends, the latency budget
and log routing. None of that depends on the physics engine. Task 82 moved the Chaos host out of it, into this
class; task 83 did the same for the Jolt host (JoltBackendUImpl, its historical name; since og-tickdriver task 15b
the template `TickDriverBackendUImpl`).

**What it is.**

* **Plain C++, not reflected.** It holds a reference to its owner, `ASimulationManagerUImpl`, and the owner declares
  it a friend (`friend physicsBackendUImpl::Backend;`, which names this class in the Chaos configuration), so its bodies read the owner's members exactly as the code did when it lived in the owner.
* **Not copyable.** The copy constructor and assignment are deleted; it is a member of its owner and nothing else.
* **The owner calls it through one fixed set of entry points, "the seam".** The table is in
  `SimulationManagerUImpl-rationale.md` §18. Each entry point is called from exactly one place, the site where the
  owner's `#if OG_PHYSICS_BACKEND_CHAOS` arm used to hold the code. Task 82 implemented the entry points that have a
  Chaos body; task 83 added empty inline bodies for the rest (`onEndPlayBegin`, `beforePresent_GameThread`,
  `onRegistered_GameThread`, `onUnregistered_GameThread`) and the concept that checks the set
  (`PhysicsBackendHostUImpl.h`).
* **The header has no `#if`.** Chaos headers exist in both configurations, so `ChaosTickBackendUImpl.h` compiles in both.
  Only the selector `PhysicsBackendHostUImpl.h`, which the owner includes, includes it, and only in the Chaos
  configuration (task 83; the owner included it directly before). **The `.cpp` is a whole-file arm,** as
  `BrawlerJoltBindingUImpl.cpp` is on the Jolt side (and the Jolt host's own `.cpp` was until og-tickdriver task 15e
  made the host a header template in OGSimulationUnreal): in the Jolt configuration it compiles to nothing. (The Jolt frame host's `.cpp` was a third
  such arm until og-tickdriver task 11 moved it to OGSimulationUnreal as `UETickDriverHost.cpp`, which has none.)

**What it holds.** The async callback `FSimulationManagerAsyncCallback` and its input and output types; the
rewind-push probe's stash type; the callback pointer and the two delegate handles; the `ChaosTickMapper`; the
`[ChaosDilation]` window (`m_chaosDilationWindow`, `m_lastPhysicsTickOffset`, `m_physicsTickOffsetResets`); the
input hook `InjectInputs_External`; the six passthroughs the callback calls (`onGameSimulation` …
`noteResimGrant`; task 84 added a seventh for the push and task 88 removed it again, because
`prepareResimulation` now pushes, §5); and forwarders to the owner's shared members (`editStorage`, the two clocks, `runsPrediction`,
the two `stampLatency*_Internal` calls). The callback and its input hold a `ChaosTickBackendUImpl*` where they used to
hold the owner.

---

## §2 The host

**Ownership.** The owner declares `physicsBackendUImpl::Backend m_backend{ *this, m_backendWorlds };`, which is this
class in the Chaos configuration, directly after `std::optional<ManagerType> m_manager`, so the backend is destroyed
before the manager. Task 82 declared ChaosBackendUImpl m_backend{ *this }; (the historical name) inside an `#if`; task 83 gave both hosts
the same two-argument constructor so the declaration has none. The second argument is the owner's `m_backendWorlds`,
which in the Chaos configuration is `ChaosTickBackendWorldsUImpl`, an empty struct: the Chaos host has no state that must
outlive the owner's adapters, so the constructor ignores it (`TickDriverBackendUImpl-rationale.md` §1 says why the Jolt host
needs it). Its own destruction is
trivial: the callback object is freed by the engine in `onEndPlayWorld`, never by a destructor. `m_asyncCallback`
starts as `nullptr`. When it was the owner's member it had no initializer and read as null only because the engine
zero-fills a `UObject`'s memory before construction; the initializer states that.

**The delegates keep their lifetime.** `startStepping` binds the physics scene's post-tick delegate and the rewind
callback's `InjectInputsExternal` delegate with `AddWeakLambda(&owner, …)`. The post-tick binding was already a weak
lambda on the owner. The input hook was bound with `AddUObject` to the owner's own member function of the same name; both
forms skip the call once the owner is gone, and the lambda now calls the backend's `InjectInputs_External`. The
backend is a member of the owner, so it lives exactly as long as the weak object the delegate tests. Both bindings
are removed, and the callback freed, in `onEndPlayWorld`.

**`startStepping` fetches the scene and the solver again,** with the same `unexpected state` checks `BeginPlay` used,
because the seam passes the world, not the solver. Nothing changes them between `buildWorld` and `startStepping`.

**The input hook.** `InjectInputs_External` fills the callback's producer input with the world and this backend,
computes `firstUpcomingSimTick` from the mapper (`⛔G-07`, the `+ 1` of §4), and calls the owner's
`onFrameStepsDue_GameThread`, then its drain and reap when the owner has a reception coordinator. The order of those
calls is unchanged.

**`buildWorld` moved two pieces, and nothing else in the order changed.**

* **The adapter emplacement** (`m_physAdapter`, `m_physReaderAdapter`, `m_queryAdapter`) ran in each role branch of
  `BeginPlay`, after the instance slot was taken, the timing-relay listener registered and the role's logger lambdas
  defined. It now runs once, in `buildWorld`, called where the Jolt configuration builds its world: after the ini
  reads, before the role branches. The two branches' copies were identical. The query adapter takes the brawler's
  six-row category table from the Chaos binding, `BrawlerChaosBindingUImpl::categoryChannels`, since og-tickdriver
  task 15a; until then the rows were an inline list in this file's emplaceBrawlerQueryAdapter helper
  (`BrawlerChaosBindingUImpl-rationale.md` §2).
* **The solver checks** (scene and solver present, solver async dt equal to the step dt) ran before
  `createLatencyBudget`. They now run at the top of `buildWorld`.
* **Why it is safe.** Nothing between the old and the new positions reads an adapter. In the tree before task 82,
  the first statement of `SimulationManagerUImpl.cpp` that names any of the three adapters was the emplacement
  itself. What lies between is `createLatencyBudget` (three allocations), the post-tick-flush registration, the
  three ini reads, the `s_instances` check and store, the timing-relay `registerInstance` (a static pointer store)
  and the two logger lambda definitions. `BeginPlay` runs on the game thread, and the async callback is registered
  only at its end, so no other thread can observe the order.

**Two things stayed on the owner, and why.**

* **`m_latencyClockAtWindowStart`.** Only the `[ChaosDilation]` line reads it, but its type is the owner's private
  `LatencyClockCounts`. This header is included by the owner's header before the owner's class is complete, so a
  backend member cannot name that type. The backend reads and writes the owner's member, as the code always did.
  Task 83's selector header does not change this.
* **The root-capsule check in `tryRegister`.** It compared the movement declaration's own body with a local
  `parentBodyId` that each backend's bind computed. The seam's `createBodies_GameThread` returns only whether the bind
  happened, so the check now reads the movement declaration's `bindings.parentBodyId`, which both binds stamp with
  that same value on every declaration. Its message is unchanged.

**The `[ChaosDilation]` window** is sampled once per game frame by `sampleLatencyWindow_GameThread` and logged and
reset by `logLatencyWindow_GameThread`, both called from the owner's `tickLatencyBudget_GameThread` where the two
Chaos blocks were. What the line means is in `SimulationManagerUImpl-rationale.md` §16.

---

## §3 The rewind-push probe (`[ResimProbe.PushTarget]` / `[ResimProbe.PushVerdict]`), client only

*Moved from `SimulationManagerUImpl-rationale.md` §8 by og-simulationscheduler-withjolt task 82; the text is unchanged apart from its section references.*

*Chaos configuration only (`SimulationManagerUImpl-rationale.md` §18). The Jolt configuration has no rewind push probe: the step
driver restores the Jolt world's saved tick and calls `prepareResimulation`, which pushes the corrected body
states (§5), so neither line prints there (task 54; the push moved into `prepareResimulation` in task 88).*

**What it exists to decide, and that a REFUTATION is the point.** og-netcode-v2-field-defects
task 10 asks whether the body-state push `FirstPreResimStep_Internal` makes is ever read.
That initiative's bug report infers that it is not; AC 1 makes that inference
falsifiable. **A mismatch on an adoption-terminated swing confirms; a match REFUTES and closes
the task as "not a defect".** The probe is therefore built so the refuting reading is exactly as
easy to quote as the confirming one: `verdict=ALL_MATCH` on one line, no re-derivation.

⛔⛔ **CORRECTED, rework cycle 1.** The paragraph that stood here said: *"The AFTER read is
the verdict value. Taking it after the push is what keeps the refutation reachable."* **It does
not, and the claim was wrong in the one way that mattered.** `FRewindData::SetTargetStateAtFrame`
delegates to `PushStateAtFrame`, which writes the target history buffers and touches no particle.
So the after-read equals the before-read BY CONSTRUCTION, on every body, on every rewind — which
means `match=1` could only ever mean *the pushed value already equalled the live one*, i.e. the
push was inert, and the one shape that could REFUTE the hypothesis, a **non-inert match**, was
unreachable. Worse, a binary whose target consumer runs after the push and before integrate would
have printed exactly the same line as a binary that reads the target never. The reading moved; the
sentence is kept here rather than deleted because a reader who saw the old revision is entitled to
know which claim was withdrawn and why.

**Where the reading is taken, since rework cycle 1 — TWO HOOKS, THREE READS.**

| read | where | what it yields |
|---|---|---|
| 1 | `FirstPreResimStep_Internal`, in a pass over every declaration immediately BEFORE the `prepareResimulation` call, which restores og-sim state and then pushes (since task 88; from task 84 the pass sat between `prepareResimulation` and the push call; `writeRestoredBodyState` from task 10's fix; `FRewindData::SetTargetStateAtFrame` before it) | `beforePush`, stashed. Feeds `inert=` and `dBefore=` |
| 2 | the same hook, in a pass over the declarations again immediately AFTER that call | the restored `pushed` value, stashed, and `moved=`, never the verdict. Before the fix `moved=` was an EAGER-APPLY discriminator. Since the fix it shows the direct write took effect (§5) |
| 3 | `OnPreSimulate_Internal`, on the frame where `IsResetting()` holds, at the TOP of the hook | `match=`, the verdict |

**Two passes, since task 84.** Until task 84, reads 1 and 2 sat inside the per-body write loop: read 1,
write, read 2, body by body. The write is now og-simulation's push (§5), so the probe brackets it with
two passes. Since task 88 the push runs inside `prepareResimulation`, so the passes bracket that call.

* **The before pass, above the call.** It walks the same declarations the push walks, in the same order.
  For each one it counts `bodies=`, resolves the proxy and its physics-thread API (counting `unresolved=`
  when either is missing), takes read 1, and stashes read 1 (`beforePush`), the ids, the body name and
  `wireCarriesRotationAndSpin`. It stashes no `pushed`: storage still holds the present tick here.
* **The after pass, below the call.** It walks the declarations again, in the same order, and skips the
  ones whose proxy does not resolve, as the before pass did. An `ensureMsgf` checks that it visits exactly
  the stashed bodies, in order (their count and `bodyId`). For each one it stashes `pushed` from storage,
  which is now the restored state that the push read (the push only reads storage), re-reads the body
  and sets `movedAtPush`, and counts `skipped=`.

**Why moving the before pass changed no value (task 88).** Until task 88 read 1 was taken after
`prepareResimulation` and before the push call. Between that point and its new one, above the call, only
og-sim code runs: the probe's `notePrepare`, the clock's `startResimulation`, the cache restore
`prepareResimAll`, which writes og-sim storage only, and `firstResimStepAll`, which is a no-op on every
brawler simulatable. None of them writes a body. The walk is over the physics composite's declarations,
which the restore does not touch, and the set of simulatables does not change inside the hook. So read 1,
the ids and the order are the same, and `pushed` is the same restored state, read after the push instead
of before it.

Each body is written once, and the setters touch only that body's particle, so a body reads the same
before or after another body's write. The `[ResimProbe.PushTarget]` line keeps its text and fields.
`[ResimProbe.PushVerdict]` gained one field in task 84, `skipped=` (after `moved=`, also on the `UNREAD`
form): the number of bodies in that rewind that the push's restore skip left unwritten (§5). The after
pass counts it by applying the push's own skip rule to `pushed` and read 1, which is the same X/R/V/W read
the push makes (`⛔G-10`). A skipped body is exactly equal to its start-of-step state, so it is always
`inert=1`, and it reads `moved=0`.

⭐ **Since task 10's fix (§5), `moved=1` is the expected reading on every non-inert push.** The push is
now a direct write onto the particle, so the read after it differs from the read before it. The
verdict is unchanged: read 3 still asks whether the replayed step starts from the pushed value, and it
still reports `MISMATCH` when the answer is no. That is what the pre-fix binary printed on 185 of 185
rewinds.

The pushed value and read 1 wait in `m_pushProbeStash` (`FRewindPushProbeStashedBody`, physics
thread only) between hook one and hook two. By the time read 3 is taken,
`FPBDRigidsSolver::ConditionalApplyRewind_Internal` has run `FRewindData::ApplyTargets` and
everything else it runs for this step, and the solver task has reached
`ApplyCallbacks_Internal` — so a target consumer anywhere in that window IS visible here.

⛔ **Read 3 must stay above `onGameSimulation`.** The radial's own `setIdlePose` /
`setInitialConditions` write the live body during the replayed step. A reading taken after them
measures OG's writes, not the engine's, and would report a mismatch that says nothing about
whether the target was read.

⚠ **The residual blind spot, stated rather than hidden.** A consumer that runs INSIDE the
evolution after `ApplyCallbacks_Internal` — between read 3 and `FPBDRigidsEvolutionGBF::Integrate`
— is still invisible. The rework narrows the window; it does not close it. A `verdict=ALL_MATCH`
refutation is a statement about consumers up to that point, and the runbook says so.

⛔⛔ **`inert=` IS WHAT MAKES A REFUTATION MEAN ANYTHING, and it is why the verdict rule
changed.** A push whose value already equals the state the body is in cannot distinguish "the
engine read the target" from "the engine read nothing" — whatever the replay then does, the
comparison matches. A rewind in which every push was inert therefore used to read
`compared=6 matched=6 verdict=ALL_MATCH`: a session in which the shape never reproduced,
presenting itself as a clean refutation. So `inert=` is printed per body, over the same `cmp=`
field set and through the same production predicate as `match=`, and **the verdict is computed
over NON-INERT comparisons only**:

| | verdict |
|---|---|
| `nonInert=0` (nothing resolved, or every push inert) | `VACUOUS` — the rewind tested nothing, and NEITHER reading may be taken from it |
| every non-inert comparison matched | `ALL_MATCH` — refutes |
| any non-inert comparison mismatched | `MISMATCH` — confirms |

⭐ A refutation therefore needs at least one `body=WeaponAxis inert=0 match=1` line. "No
mismatches in the log" is **not** a refutation on its own.

⛔ **A stash that never reaches read 3 is PRINTED, not dropped** — `verdict=UNREAD` with
`readFrame=-1` and `reason=newPushBeforeRead` or `reason=notResetting`, under the same tag so the
runbook's single `grep -v verdict=ALL_MATCH` surfaces it. An absent line is indistinguishable from
a rewind that never happened, and a reader who cannot tell those apart cannot trust a refutation.
That branch firing at all is an engine-ordering finding in its own right.

⚠ **One mismatch source the probe does NOT exclude: mapper skew.** `toChaosTick(Tc)` uses the
CURRENT offset, so a Stall or Skip between frame S(Tc) and the rewind moves F by ±1 and
`RewindToFrame(F)` restores a different instant than `slot[Tc]` — at which point EVERY body
mismatches for a reason that has nothing to do with ordering (§4's hazard). The per-body rows are
the control: on a genuinely confirming rewind, `CharacterCapsule` and `GuardAxis` must **not** all
read `match=0`. If they do, read `[ResimProbe.Request] mapperOffset=` before concluding anything.

⛔ **It reads X/R, and `ChaosPhysicsBodyAdapter::captureBodyState` reads P/Q, and both are
right.** That adapter runs from PostSolve, where P/Q hold the solved pose and X/R still hold the
start-of-step pose. This probe runs at the opposite phase: on UE 5.6's public source
`FRewindData::RewindToFrame` restores the pose with `SetXR`, `FRewindData::ApplyTargets` applies a
target with `SetXR`, neither writes P/Q, and `FPBDRigidsEvolutionGBF::Integrate` begins the
replayed step from `XCom()`/`RCom()`. X/R is therefore the state the replay starts from. A P/Q
read here would report a stale pre-rewind pose and print a mismatch on every body on every
rewind — a probe that cannot fail is worth nothing, and this one was one line away from being
that. `dXP=` carries the X-to-P distance so the skew stays visible rather than assumed.

⛔ **The match predicate is the production one.** `isSimilarToField`
(`SimulationComparisonGlm.h`) at `kDefaultSimilarityEpsilon` (`SimulationTypes.h`) — the same
test `isSimilarTo` applies to a correction, so "the probe disagrees but the cache does not" is
not a state this can reach. Its quaternion arm is `|abs(dot) - 1|`, which matters: an
antipodal-but-identical rotation is a match, and a hand-rolled component compare would have
called it a large mismatch. **Measured, not argued** — the task-10 notes carry a standalone run
in which two plausible wrong rules (component-wise quaternion; ignore the wire shape) are scored
against the same cases, and **both err toward MISMATCH**, i.e. both would have confirmed the
hypothesis for free.

⛔ **Only the fields the body's wire shape carries are compared.** A declaration whose
`bodyStateOf` yields `LinearBodyState` — the character capsule — fabricates an identity rotation
and a zero angular velocity on the way to `PhysicsBodyState`. Comparing those two would report a
mismatch about a value no wire ever carried. `cmp=` names the set that was compared, and
anything that is not exactly `PhysicsBodyState` takes the conservative position-plus-linear-
velocity set.

⛔ **`verdict=VACUOUS` is a third answer, not a rounding of `ALL_MATCH`.** With `compared=0` the
counters read all-zero, which is indistinguishable from a clean refutation. An unresolved proxy or
handle is counted in `unresolved=`, never silently skipped. ⛔ **Rework cycle 1 widened what
reaches this verdict**: `compared=0` was never the only way to test nothing — see the `nonInert`
table above, which is now the rule.

**Volume and cost.** Per-body detail and the per-rewind summary, both at `Verbose`, and every
read and format sits behind one `UE_LOG_ACTIVE(LogOGResimProbe, Verbose)`, so at the shipped
`LogOGResimProbe=Warning` the whole probe is one predicate per body. It is deliberately NOT a
per-window summary: the question is per-rewind and an aggregate would hide which body disagreed.

⚠ **The engine facts are about the API, not about the running binary.** `ApplyTargets(Step,
bFirst)` before `PreResimStep_Internal(Step, bFirst)`, and the repeat gated on
`np2.Resim.ApplyTargetsWhileResimulating` (default `false`), are read off public
`EpicGames/UnrealEngine@5.6`. This project builds against a local source engine, and the two can
differ. **That gap is precisely what the probe measures**, so nothing here may be quoted as the
answer AC 1 asks for.

~~⚠ **Not yet run.** Confirming or refuting needs a PIE session with an adoption-terminated swing
and task 2's fix off, which only the user can produce. The runbook is in that task's
implementation notes.~~ **Superseded 2026-09-25.** The probe ran on 2026-09-22 (floor-3 PIE session)
and **confirmed**: 185 of 185 rewinds read `verdict=MISMATCH` with `nonInertMatched=0`, on every body.
At the phantom tick the weapon's line read `pushedAng=(0,0,0) liveAng=(0,0,6.1088) moved=0
resimType=FullResim objState=Dynamic`. The fix is §5. The probe stays as its regression check, and
the post-fix PIE run has not happened yet.

---

## §4 Tick alignment — the `+ 1`

*Moved from `SimulationManagerUImpl-rationale.md` §9 by og-simulationscheduler-withjolt task 82; the text is unchanged apart from its section references. That section keeps the Jolt configuration's counterpart.*

**An off-by-one here shifts every player's input by one tick, silently and uniformly.**

`physicsStep` is the **upcoming** solver step: Chaos passes
`MarshallingManager.GetInternalStep_External()`, documented in `ChaosMarshallingManager.h` as *"the internal step that the current PushData will
be associated with once it is marshalled over"*, and the solver's frame counter is
only incremented at the **end** of a tick (`GetCurrentFrame()++`, `PBDRigidsSolver.cpp`), so step `N`'s `OnPreSimulate_Internal` observes
`GetCurrentFrame() == N`.

`ChaosTickMapper`'s offset is written in `OnPreSimulate_Internal` as `chaosTick - simulationTick`.
Crucially it is written **before** `onGameSimulation()` runs, and `onGameSimulationAuthority()`
advances the server clock as its first action. So the `simulationTick` captured at step `K` is the
tick simulated at step `K−1`:

```
offset = K - S(K-1)          where S(K) is the sim tick simulated at step K
S(K) = S(K-1) + 1            authority advanceTick() is unconditional -
                             no Stall, Skip or resim exists on the server
=>  offset = K - (S(K) - 1) = (K - S(K)) + 1
=>  toSimulationTick(X) = X - offset = S(X) - 1
```

`toSimulationTick(physicsStep)` therefore names the tick **before** the one that step will
simulate, and the upcoming tick is that value **plus one**. The `+ 1` in
`releaseDelayedInputsForStep` is a derived correction, not a fudge factor.

**Why not cross-check against the server clock at runtime:** the clock is written on the physics
thread, and reading it from the game thread is exactly the unsynchronized cross-thread read this
whole design exists to avoid. The mapper's offset is a `std::atomic` and is the only tick source
safe to read from the game thread, which is why the resolution specifies it.

**Sub-stepping:** with `NumSteps > 1` the physics frame runs several sim ticks back to back, so the
drain releases input for each of them. The normal fixed-tick case is `NumSteps == 1` and collapses
to a single drain.

In the Chaos configuration the caller is `ChaosTickBackendUImpl::InjectInputs_External`, bound to the rewind callback's
`InjectInputsExternal` delegate (`FNetworkPhysicsCallback`). Chaos broadcasts it on the game thread,
once per physics frame, as it hands the frame's steps to the solver. It computes
`firstUpcomingSimTick` (`⛔G-07`) and passes the same value to `onFrameStepsDue_GameThread`, the drain
and the reap (`SimulationManagerUImpl-rationale.md` §7). The drain is a no-op on a client and on a server with nothing parked; the
frame-health probe is **not** a no-op on either role (`SimulationManagerUImpl-rationale.md` §11 C20).

---

## §5 The first replayed step — the restored body state is written, not targeted

*Moved from `SimulationManagerUImpl-rationale.md` §14 by og-simulationscheduler-withjolt task 82; the text is unchanged apart from its section references and guard ids.*

*og-netcode-v2-field-defects task 10's fix, 2026-09-25. The prohibition is `⛔G-08` (where and how
the write happens). Which fields it writes was G-09 until og-simulationscheduler-withjolt task 84,
which moved the write into og-simulation's push; the field rule is that push's now (guards §R).*

*Chaos configuration only (`SimulationManagerUImpl-rationale.md` §18), retiring with it in task 21. The Jolt configuration's successor is the
step driver: on a granted resimulation it restores the Jolt world's saved tick and calls
`prepareResimulation`, which restores og-sim state and then pushes the restored body states through
`SimulationIntegrationExecutor::pushCorrectedBodyStatesAll` (the wire-shape rule lives in the core),
before the first replayed step (task 54). Since task 84 this hook uses the same push, and since task 88
both reach it through `prepareResimulation`.*

**What the hook does.** A granted rewind makes `FirstPreResimStep_Internal` do two things, in this
order:

1. It records the grant.
2. It calls `prepareResimulation`, which restores the synced composite from the correction cache and then
   writes every composite body's restored state onto its particle.

Before the fix, the write was `FRewindData::SetTargetStateAtFrame` at the replayed frame. From the fix to
task 84, it was a hand-written loop over the declarations calling `writeRestoredBodyState`: the
physics-thread API's `SetX` and `SetV`, plus `SetR` and `SetW` when the body's wire shape carries them.
From task 84 to task 88, it was a third step of this hook, one call, `m_manager->pushCorrectedBodyStatesAll()`,
which forwarded to the owner's `m_integrationLayer`. Since task 88 the call is the last statement of
og-simulation's `SimulationManager::prepareResimulation`, which calls
`SimulationIntegrationExecutor::pushCorrectedBodyStatesAll` on the same executor (the manager holds the
owner's integration layer by reference), on the physics thread. The push writes through
`ChaosPhysicsBodyAdapter`. For every corrected body it writes the same fields and bits as before; it
deliberately differs below float precision (below).

**Why the target was never read.** This is public UE 5.6's order for the first replayed step:

1. `FRewindData::RewindToFrame` restores X/R/V/W from the client's own recorded history. It recomputes
   each dirty particle's follower-ness from its object state and overwrites its resim type.
2. `FRewindData::ApplyTargets` reads the targets stored at that frame.
3. `PreResimStep_Internal` runs, and this hook runs inside it.
4. The solver task runs. On the way to the evolution it passes `ApplyCallbacks_Internal`, which is
   where `OnPreSimulate_Internal` fires.
5. The evolution integrates, starting from X/R.

On later replayed steps `FRewindData::ApplyTargets` runs only if `np2.Resim.ApplyTargetsWhileResimulating` is set,
and it defaults to false. So a target written from this hook for the frame being replayed is read by
nothing in that cycle. Task 10's probe measured that on the running binary before the fix.

**Why the fix is a direct write.** `FRewindData::ApplyTargets` itself does no more than this. For a non-follower
particle it calls `SetXR`, then `SetV`/`SetW`, and it sets the object state from the target. The fix
makes the same kind of write, from the same phase: after `FRewindData::ApplyTargets`, before the step integrates. It
uses the physics-thread handle API that `ChaosPhysicsBodyAdapter` and the sub-sims use on every tick,
replayed steps included. Two consequences:

* **P/Q.** That API's `SetX`/`SetR` also set P/Q. So after the write the body's X and P agree, and
  the probe's `dXP=` reads 0 for it.
* **Sleep.** It wakes a sleeping body, which is what the old target asked for: every push requested an awake body.

Task 10's fix changed nothing in og-simulation. The restore still happens in `prepareResimulation`,
before the write, where it always happened; since task 88 the write happens there too, after the restore.

**Candidates that were not taken.**

* **Pushing from `TriggerRewindIfNeeded_Internal`, before `RewindToFrame`.** `FRewindData::ApplyTargets` would read
  such a target. But this callback's frame is only a request. The engine takes the minimum over every
  rewindable callback and the replication frame. It then runs `FRewindData::FindValidResimFrame`,
  which can move the frame or refuse it. A target written at the requested frame can therefore land
  on a frame that is not replayed. It then stays in the target history and is applied by some later
  rewind to that frame.

  This path also needs the restored state before `prepareResimulation` has produced it. That means
  either reading the correction cache from the adapter, or moving og-simulation's restore earlier.
* **Pushing at injection time.** It has the same frame problem, and a worse one. A correction is
  injected before `onCheckIsSimilar` decides whether it triggers a rewind at all, and to which tick.
* **The `np2.Resim.ApplyTargetsWhileResimulating` cvar.** It re-applies targets on later steps, but
  the target for the first replayed step is still written after that step's `FRewindData::ApplyTargets`.

**What the removed call also did.** `FRewindData::SetTargetStateAtFrame` had three side effects the fix drops:

* **Lerped targets.** It fills gaps: a push to frame F whose previous target sat up to five frames
  earlier also writes interpolated targets for the frames in between.
* **The rewind-frame search.** It gives each body a target history, and `FRewindData::FindValidResimFrame` consults
  that history when it picks a rewind frame.
* **Object state.** Every target carries "dynamic, enabled", and `FRewindData::ApplyTargets` applies that state
  whenever it does read a target.

This file no longer writes any rewind target, so none of the three happens on its account.

**The wire shape decides the fields.** A `LinearBodyState` body has no rotation or spin on the wire.
Writing the fabricated identity rotation and zero spin would replace the rewound ones, so the push never
writes them: a linear body gets its position and linear velocity, plus its OWN restored rotation through
`setBodyPose`. Until task 84 this was G-09, a runtime branch in `writeRestoredBodyState` that left R
untouched. Since task 84 the rule is og-simulation's push (its `SimulationStepDriver-rationale.md` §9):
the `if constexpr (std::is_same_v<BodyStateT, PhysicsBodyState>)` branch. The push reads every
declaration generically, so a new body-state type still edits no call site.

**One push on both backends (task 84): why the Chaos adapter reads the start of the step.**
og-simulation's push reads the restored body once per declaration, for two uses: (a) the restore skip,
which compares the declaration with it and skips an equal one; and (b) a `LinearBodyState`
declaration's pose, which passes its rotation back to `setBodyPose`. By default that read is
`captureBodyState`, and on Chaos that is the wrong phase here.
`ChaosPhysicsBodyAdapter::captureBodyState` reads P/Q, the solved pose its post-solve capture needs.
`FRewindData::RewindToFrame` restores X/R/V/W and leaves P/Q stale, and the replay integrates from X/R;
§3's `dXP=` exists because the two can differ. So (a) could skip a corrected body whose stale P happens
to equal the cache, and (b) would write the stale Q over the capsule's rewound R. The adapter therefore
models og-simulation's optional `PhysicsBodyStartOfStepReader` (`static_assert`ed in
`ChaosPhysicsBodyAdapter.h`): its `captureStartOfStepBodyState` reads X/R through the particle handle and
V/W through the physics-thread API, the same reads as this file's probe (read 1, `⛔G-03`), and the push
uses it for both (a) and (b).

**The writes per body.**

| Declaration | Until task 84 (`writeRestoredBodyState`) | Since task 84 (the push, through the adapter) |
|---|---|---|
| full `PhysicsBodyState` (weapon, guard), corrected | `SetX`, `SetV`, `SetR`, `SetW` | `setBodyPose` → `SetX`, `SetR`; `setBodyLinearVelocity` → `SetV`; `setBodyAngularVelocity` → `SetW` |
| `LinearBodyState` (capsule), corrected | `SetX`, `SetV` | `setBodyPose` → `SetX`, `SetR(float(R))`; `setBodyLinearVelocity` → `SetV` |
| any body equal (in float) to its start-of-step state | written as above | not written (the restore skip) |

Each value goes through the same conversion as before (`uglm::toFVector`, `uglm::toFQuat`) from the same
`PhysicsBodyState` (the declaration's state, widened). The setters are independent: `SetX` writes X and
P, `SetR` writes R and Q, `SetV` writes V, `SetW` writes W. The only order change is `SetR` before `SetV`
on a full body. Waking is unaffected by it, because `SetX` comes first in both orders and wakes a
sleeping body before any other setter runs.

**The deliberate deviation, below float precision** (user ruling, 2026-10-15: the one-capability
design). Chaos stores X/R/V/W as doubles; og-sim state, and every read the push compares, is float.
* **A body equal to its rewound state, in float, is no longer written.** It keeps its full double
  state, and a sleeping one is not woken. Until task 84 every body was written, which rounded its pose
  and velocities to float and woke it. Every corrected body is still written, so it is still woken.
  The probe's `skipped=` counts these bodies per rewind (§3).
* **A corrected `LinearBodyState` body also gets `SetR(float(R))`**: its own rewound rotation, narrowed
  to float. Until task 84 its R was not touched. The capsule's rotation is locked, so this is expected to
  change nothing that a float read can see; `SetR` also sets Q, which the replay recomputes from R.
  The probe cannot see the narrowing itself (its reads are float); task 84's notes report the capsule's
  `dBeforeRot` before and after.

⚠ **One difference that is not a write.** The hand-written loop silently skipped a body whose proxy or
physics-thread API did not resolve; the probe counted it in `unresolved=`. The adapter's setters
instead fire `ensureAlwaysMsgf` and return when the proxy does not resolve, as they do for every other
write the sims make through the adapter on every tick; like those, they do not check for a proxy that
resolves without a physics-thread handle. Every capture so far has printed `unresolved=0`, and the
sims' own integrate writes reach the same bodies through the same setters every tick.

**What the probe says after the fix (§3).** Non-inert pushes read `moved=1` at read 2 and `match=1`
at read 3, so the verdict can read `ALL_MATCH`. It can still read `MISMATCH`. That happens when
something between this hook and `OnPreSimulate_Internal` overwrites the body, or when the write is
skipped or writes the wrong field set. The pre-fix binary is that case, and it printed `MISMATCH` on
every rewind.

⚠ **The probe does not prove convergence.** The residual blind spot in §3 still stands: a writer
inside the evolution after `ApplyCallbacks_Internal` is invisible to read 3. Convergence is proved only
by the corrections stopping.

**The authored resim policy stays inert.** `ChaosPhysicsFactory` stamps `ResimAsFollower` on the
weapon through the game-thread API, and `RewindToFrame` overwrites it to `FullResim` on every rewind
of a dynamic body. The direct write does not consult resim type, so that stamp has no bearing on this
fix. It was left unchanged.
