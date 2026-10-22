<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- ================= DECLARED LINT ESCAPES ==================================
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: FRewindData::RewindToFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::SetTargetStateAtFrame -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsSolver::ConditionalApplyRewind_Internal -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FRewindData::ApplyTargets -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: FPBDRigidsEvolutionGBF::Integrate -- Chaos engine method, outside every scan root -->
<!-- lint-external-ref: SetXR -- Chaos engine particle-handle method, outside every scan root -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
# `ChaosTickBackendUImpl.h` and `.cpp` — guards

The prohibitions on the Chaos host: the async callback, its rewind-push probe, the restored-state write and the
input hook's tick. Every entry here moved from `OGBrawlerSimulationManager-guards.md` in og-simulationscheduler-withjolt
task 82, with the code it guards; each names its old id. In the source a single line
`// ⛔G-nn  docs/ChaosTickBackendUImpl-guards.md` sits directly above the statement or declaration where the forbidden
edit would be typed.

Named ChaosBackendUImpl-guards.md until og-tickdriver task 15b (historical name), when the class and its files were
renamed `ChaosTickBackendUImpl`. The ids did not change; every citation was re-pointed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix this file; do not
soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time check, moves to §R
and its number is spent forever. A retired id may be **named** in prose or in an assertion message; it may never
again appear as a `⛔G-nn` **tag**.

⭐ **The join is machine-checked in both directions** by this repository's guard-tag lint: every tag resolves to an
entry here, every live entry is referenced by exactly one tag, no id is duplicated, and no retired id reappears as
a tag. ⚠ It checks that an entry EXISTS. It never checks that this text is TRUE.

⛔ **Nothing in this file is a rationale.** That lives in `ChaosTickBackendUImpl-rationale.md`.

**Re-verified when moved (task 82).** The body names `G-01` lists (`CharacterCapsule`, `WeaponAxis`, `GuardAxis`,
and the projectile slots' literal-returning initializer); `isSimilarToField` and `kDefaultSimilarityEpsilon` where
`G-04` places them; the probe's `grants` and `prepares` counters `G-06` relies on; the mapper's atomic offset; the
`ChaosPhysicsBodyAdapter` setters `G-08` names; and, in this session's engine source, that
`np2.Resim.ApplyTargetsWhileResimulating` defaults to false, and that without it `FRewindData::ApplyTargets` runs
only on the first replayed step (`G-08`). `G-06`'s engine claims were not re-read; it says so.

---

## G-01 — `bodyName` stores a pointer to a STRING LITERAL, and only a string literal

*Was `OGBrawlerSimulationManager-guards.md` G-02; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Tag site:** `ChaosTickBackendUImpl.h`, directly above `FRewindPushProbeStashedBody::bodyName`.

**The fence, verbatim — the bytes it occupied in `OGBrawlerSimulationManager.h` before task 11:**

<!-- header lines 272-273 at b9f6d81 -->
```
// ⛔ `bodyName` IS A STRING LITERAL (a declaration's `D::name`), never owned and
// never freed. Storing a `const char*` is safe only for that reason.
```

**What breaks if the edit is made.** The stash outlives the hook that fills it: it is written in `FirstPreResimStep_Internal` and read one hook later, at the top of `OnPreSimulate_Internal` on the resetting frame, or printed as `verdict=UNREAD` by the discard path. Every value stored today is a physics declaration's `static constexpr const char* name` (`CharacterCapsule`, `WeaponAxis`, `GuardAxis`, and the projectile slots' literal-returning `constexpr` initializer), whose storage is static. A pointer into anything with a lifetime — an `FString`'s buffer, a `std::string::c_str()`, a stack array — dangles across that gap and is printed as `body=` out of freed memory, on the physics thread.

---

## G-02 — the push-probe verdict read stays at the top of `OnPreSimulate_Internal`, and its `else` stays

*Was `OGBrawlerSimulationManager-guards.md` G-51; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `if (m_pushProbeStashFrame != INDEX_NONE)` block at the top of
`FSimulationManagerAsyncCallback::OnPreSimulate_Internal`.

**The prohibition, as it stood in the source:**

> ⛔ IT MUST STAY ABOVE `onGameSimulation`. The radial's setIdlePose /
> setInitialConditions write the live body during the replayed step; a read taken after
> them would be measuring OG's own writes and would report a mismatch that says nothing
> about whether the engine read the target.
>
> ⛔ THE `else` IS NOT DEFENSIVE PADDING. A stash that is still pending on a frame that
> is NOT resetting was never read at its verdict point, and a silently dropped stash is
> indistinguishable from a rewind that never happened - so it is printed as
> `verdict=UNREAD`. That branch firing at all is an engine-ordering finding.

The same reasoning covers the `discardPushProbeStash(TEXT("newPushBeforeRead"))` call at the top
of `FirstPreResimStep_Internal`. That call carries no tag of its own, because one entry cannot
have two sites.

**The consequence.** A read taken below `onGameSimulation` reports a mismatch on every rewind,
because it is measuring OG's own writes. Without the `else`, an unread stash disappears silently,
and a rewind that never happened reads the same as a clean refutation.

**What breaks if the tag moves.** Nothing mechanical checks the position of this block. The
tag is the only record that its line position is what the probe measures.

---

## G-03 — the push probe reads X/R, never P/Q

*Was `OGBrawlerSimulationManager-guards.md` G-52; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `particle->GetX()` / `GetR()` reads in `readLiveBodyForPushProbe`.

**The prohibition, as it stood in the source:**

> ⛔ X/R, NOT P/Q - AND THAT IS THE DIFFERENCE BETWEEN A PROBE AND A CONFIRMATION
> MACHINE. `ChaosPhysicsBodyAdapter::captureBodyState` reads `GetP()`/`GetQ()`
> because it runs from PostSolve, where P/Q are the solved pose and X/R still hold
> the start-of-step pose. Here the phase is the opposite one:
> `FRewindData::RewindToFrame` restores the pose with `SetXR`,
> `FRewindData::ApplyTargets` applies a target with `SetXR` - neither writes P/Q -
> and `FPBDRigidsEvolutionGBF::Integrate` starts the replayed step from
> `XCom()`/`RCom()`. X/R IS therefore the state the replay starts from, and
> reading P/Q here would report a stale pre-rewind pose and print a mismatch every
> single time. `dXP=` carries the X-to-P distance so that skew stays visible.

and, beside the struct the read fills:

> The live particle, read the way the replayed step is going to read it. `solvedP`
> is NOT part of that answer and is never compared — it exists only so that `dXP=`
> can show how far the uncommitted P has drifted from the rewound X.

**The consequence.** Matching `captureBodyState` looks like a harmless consistency fix, but it
turns the probe into one that reports `MISMATCH` on every body on every rewind. That confirms
the hypothesis whether or not it is true.

**What breaks if the tag moves.** Nothing else points at these two lines.

---

## G-04 — the push probe compares with the production predicate, over the fields the body's wire shape carries

*Was `OGBrawlerSimulationManager-guards.md` G-53; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** `pushProbeFieldsSimilar`.

**The prohibition, as it stood in the source:**

> ⛔ THE MATCH PREDICATE IS THE PRODUCTION ONE, never a re-derivation:
> `isSimilarToField` (`SimulationComparisonGlm.h`) at `kDefaultSimilarityEpsilon`
> (`SimulationTypes.h`), the same test `isSimilarTo` applies to a correction. Its
> quaternion arm is |abs(dot) - 1|, so an antipodal-but-identical rotation reads as
> a match where a component compare would have shouted mismatch. `inert=` runs the
> SAME predicate over the SAME field set, so "inert but mismatched" can never be an
> artefact of the two questions looking at different fields.
>
> ⛔ ONLY THE FIELDS THE BODY'S WIRE SHAPE ACTUALLY CARRIES ARE COMPARED. A
> declaration whose `bodyStateOf` yields `LinearBodyState` - the character capsule,
> `BrawlerMovementSimulation.h` - fabricates an identity rotation and a zero
> angular velocity on the way to `PhysicsBodyState`, so comparing those two would
> report a mismatch that means nothing. `cmp=` names the set compared; anything
> that is not exactly `PhysicsBodyState` takes the conservative
> position-plus-linear-velocity set. `moved=` is the one deliberate exception: it
> compares two LIVE reads of the same particle, so the wire shape is irrelevant
> there and all four fields count.

The helpers beside it said the same thing about the numbers they print:

> REPORTED MAGNITUDES, each the quantity its own `isSimilarToField` overload tests:
> worst component for a vec3, |abs(dot) - 1| for a quat. A number that measured
> something other than what the verdict tested would be worse than no number.

> The deciding magnitude for `pushProbeFieldsSimilar`, over the SAME field set: every
> arm is tested against the same `kDefaultSimilarityEpsilon`, so the worst arm is
> exactly the number whose comparison to eps produces the bool. Reporting anything
> else would be reporting a quantity the verdict did not use.

**The consequence.** A hand-written component compare reports a match as a large mismatch when
the two quaternions are antipodal. Comparing rotation on the capsule reports a mismatch on a
value no wire ever carried. The task-10 standalone run found that both of these wrong rules
lean toward `MISMATCH`, which means both would confirm the hypothesis for free.

**What breaks if the tag moves.** `pushProbeWorstDelta` must follow any change to this
function, and it has no tag of its own.

---

## G-05 — the push-probe verdict's denominator is `nonInert`, and `VACUOUS` is a third verdict

*Was `OGBrawlerSimulationManager-guards.md` G-54; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** `pushProbeVerdictText`.

**The prohibition, as it stood in the source:**

> ⛔ `compared` IS NOT THE DENOMINATOR OF THE VERDICT - `nonInert` IS. An inert
> comparison (the pushed value already equalled the live pre-push state) tests nothing
> whatever the replay then does, so counting it as a match would let a session in which
> the shape never reproduced read as a refutation.

> ⛔ VACUOUS IS A THIRD VERDICT, not a rounding of ALL_MATCH, and rework 1 widened
> what reaches it. `compared == 0` was never the only way to test nothing: a rewind
> whose every push was inert compares six bodies, matches six and tests nothing at all.
> Both now read VACUOUS, because this task's whole value is that a refutation be
> trustworthy.

**The consequence.** If the verdict is taken over `compared`, a session where the shape never
reproduced prints `ALL_MATCH`, and that closes a real defect as "not a defect".

**What breaks if the tag moves.** The counters live in `PushProbeTally`, and a change there is
where the denominator would drift. The tag stays on the function that turns those counters into
a word.

---

## G-06 — `noteResimGrant` is recorded before `prepareResimulation`

*Was `OGBrawlerSimulationManager-guards.md` G-55; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** the `m_manager->noteResimGrant(PhysicsStep);` line in `FirstPreResimStep_Internal`. `m_manager` is the callback's `ChaosTickBackendUImpl*`, which forwards to the session's resim-gate probe.

**The prohibition, as it stood in the source:**

> THE GRANT. Chaos starts at `PhysicsStep`, which can differ from ours only by being DEEPER. §8
>
> ⛔ A SHALLOW CLAMP IS STRUCTURALLY IMPOSSIBLE here: validation walks DOWN and the merge
> can only deepen, so `clampedGrants` reads 0 and a nonzero is an engine-change alarm.
>
> ⛔ BEFORE prepareResimulation, so a grant is recorded even if anything below returns
> early: `grants` and `prepares` straddle this boundary and their agreement is the check.

**The consequence.** If the grant moves below an early return, `grants` and `prepares` count the
same side of the boundary. They then always agree, and the wiring check they form becomes
vacuous.

**What breaks if the tag moves.** Nothing mechanical relates the two calls.

**Not re-verified.** The engine claims in the middle paragraph (validation walks down, the merge
only deepens) were not re-read from engine source in this conversion, nor when task 82 moved this
entry. They are carried as they stood.

---

## G-07 — the upcoming sim tick comes from the mapper, and the `+ 1` is derived

*Was `OGBrawlerSimulationManager-guards.md` G-71; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Only its site and its cross-references changed.*

**Site:** `const int32 firstUpcomingSimTick =` in `ChaosTickBackendUImpl::InjectInputs_External`, the Chaos
configuration's input hook (`ChaosTickBackendUImpl-rationale.md` §2). Re-sited there from
`releaseDelayedInputsForStep` by og-simulationscheduler-withjolt task 51, id kept: the computation is the
same, and it feeds the session's `onFrameStepsDue_GameThread`, the drain and the reap with one value, as it
did before the split. It retires with the Chaos configuration in task 21.

**The prohibition, as it stood in the source:**

> TICK ALIGNMENT. ⛔ An off-by-one here shifts EVERY player's input, silently and uniformly. §9
>
> `physicsStep` is the UPCOMING solver step; the frame counter increments at a tick's END,
> so step N's OnPreSimulate_Internal sees frame N and writes the mapper offset there,
> BEFORE onGameSimulation, whose first action on the authority is to advance the clock:
>
>     offset = K - S(K-1)     where S(K) is the sim tick simulated at step K
>     S(K)   = S(K-1) + 1     authority advance is unconditional - no Stall, Skip or
>                             resim exists on the server
>  => offset = (K - S(K)) + 1
>  => toSimulationTick(X) = X - offset = S(X) - 1
>
> ⛔ So toSimulationTick(physicsStep) names the tick BEFORE that step's: hence the `+ 1`.
>
> ⛔ WHY NOT CROSS-CHECK AGAINST THE SERVER CLOCK: reading it here IS the unsynchronized
> cross-thread read this design exists to avoid. The mapper's offset is the safe source. §1

and, at the reap and in PROBE A:

> ⛔ The tick comes from the game-thread-safe mapper, NOT the server clock. §9

> ⛔ WHY REUSING `firstUpcomingSimTick` IS SAFE ON THE CLIENT although its `+1` derivation
> assumes authority: the probe consumes only DELTAS, so a constant skew cancels, and a
> departure is counted as a `kFrameHealthDiscontinuityTicks` discontinuity. ⛔ A DIFFERENT
> tick source would not be safe. §9

The quoted `physicsStep` is the hook's `PhysicsStep` parameter since the re-siting.

**Why a guard and not a derivation tag.** This line has two readings: the derivation of the
`+ 1` (`ChaosTickBackendUImpl-rationale.md` §4) and the prohibition on any other tick source. The rule gives
a coinciding G and D one tag and merges the entries, so that section is this entry's derivation.

**The consequence.** Dropping the `+ 1` shifts every player's input by one tick. Swapping in the
server clock adds an unsynchronized read of a physics-thread-written value on the game thread.

---

## G-08 — the restored body state is WRITTEN onto the particle in `FirstPreResimStep_Internal`, never pushed as a rewind target

*Was `OGBrawlerSimulationManager-guards.md` G-76; moved here by og-simulationscheduler-withjolt task 82, when the Chaos host left the UImpl. Reworded by og-simulationscheduler-withjolt task 84, when the hand-written per-body write loop became one call to og-simulation's push: the prohibitions are unchanged, and their site is now that call.*

*Reworded again by og-simulationscheduler-withjolt task 88, when the push moved into og-simulation's
`SimulationManager::prepareResimulation`: the site is now the `prepareResimulation` call, which writes the
bodies as its last step. The prohibition against moving the write above `prepareResimulation` became
structural and was dropped. The rule that the probe's passes bracket the write now names this call.*

**Site:** the `m_manager->prepareResimulation(PhysicsStep, simTick);` call in
`FSimulationManagerAsyncCallback::FirstPreResimStep_Internal` (`ChaosTickBackendUImpl.cpp`). It reaches
`SimulationManager::prepareResimulation` through `ChaosTickBackendUImpl::prepareResimulation` and the owner's
`m_manager`. That call restores og-sim state from the correction cache and then, last, writes the restored
bodies through `SimulationIntegrationExecutor::pushCorrectedBodyStatesAll`.

**The prohibition.** Written by og-netcode-v2-field-defects task 10's fix (2026-09-25). No shipped
comment preceded it.

* ⛔ **Do not turn the write this call makes back into `FRewindData::SetTargetStateAtFrame`**, or add any
  other rewind-target write for the frame being replayed. In public UE 5.6, `FPBDRigidsSolver::ConditionalApplyRewind_Internal`
  calls `FRewindData::ApplyTargets` for a step **before** it calls `PreResimStep_Internal`, and this
  hook runs from there. `FRewindData::ApplyTargets` is repeated on later steps only when
  `np2.Resim.ApplyTargetsWhileResimulating` is set, and that setting defaults to false. So a target
  written from this hook arrives after the only read of it. Task 10's probe measured this on the
  running binary: **185 of 185 rewinds `verdict=MISMATCH`, `nonInertMatched=0`.**
* **Structural since task 88: the write cannot run before the restore.** The push is the last statement
  of `prepareResimulation`, after the cache restore, so the earlier bullet "do not move it above
  `m_manager->prepareResimulation(...)`" no longer names an edit that can be made here.
* ⛔ **Do not move this call into `OnPreSimulate_Internal` or below `onGameSimulation`.** There it would run
  after the probe's verdict read (`⛔G-02`), or after the sub-sims' own writes for the replayed step.
* ⛔ **Keep the probe's before pass above this call and its after pass below it.** The before pass reads
  `beforePush`, which gives `inert=`; above the call, storage still holds the present tick, so it stashes
  no `pushed`. The after pass stashes `pushed` from the restored storage (the push only reads storage),
  re-reads each stashed body, which gives `moved=` and shows the write took effect, and counts
  `skipped=` (`⛔G-10`).

**Why a direct write is sound here.** The push writes through `ChaosPhysicsBodyAdapter`: `setBodyPose`
(`SetX`, `SetR`), then `setBodyLinearVelocity` (`SetV`) and, for a full `PhysicsBodyState` declaration,
`setBodyAngularVelocity` (`SetW`). A body whose declared state already equals its start-of-step state
is skipped (og-simulation's restore skip, `ChaosTickBackendUImpl-rationale.md` §5). These are the
physics-thread API's setters, which the adapter uses every tick (`setBodyTransform`,
`setBodyLinearVelocity`, `setBodyAngularVelocity`). The sub-sims make
them during replayed steps too. They land in the same phase as `FRewindData::ApplyTargets`: after
`FRewindData::RewindToFrame` and before the replayed step integrates. They also do what an applied
target would have done: set X/R (and P/Q), set V/W, and wake a sleeping body, matching the old target,
which always requested an awake body.

**The consequence.** Every rewind replays the client's own recorded body history instead of the
restored state. In the knockback-revert capture this left the weapon's swing spin alive across a correction
that had already adopted idle. Each correction re-stated the same disagreement (the `DivergenceProbe`'s
constant 0.0359505 rotation component), and the storm ended only at the next swing edge.

**What breaks if the tag moves.** Nothing mechanical checks where the call is. The after pass's
`ensureMsgf` checks only that the two passes visit the same bodies in the same order, not where they sit.
What the push writes, and that it runs after the cache restore, is covered by og-simulation's push and
step-driver tests, and the adapter's `static_assert`s pin that the Chaos adapter reads
the restored body at the start-of-step phase (`PhysicsBodyStartOfStepReader`). But `ChaosTickBackendUImpl.cpp` is a UE module file that neither LLT target compiles,
so this tag and the probe's PIE run are the only checks on the call's position.

---

## G-10 — the probe's `skipped=` count uses the push's restore-skip rule, on the push's start-of-step read

*Added by og-simulationscheduler-withjolt task 84. Task 88 moved its call from the probe's before pass to its
after pass, with the stash of `pushed`; the predicate and its tag did not change.*

**Site:** `pushProbeRestoreSkipFires` (`ChaosTickBackendUImpl.cpp`), called from the probe's after pass in
`FirstPreResimStep_Internal`, below the `prepareResimulation` call.

**The prohibition.** ⛔ **Keep `pushProbeRestoreSkipFires` identical to og-simulation's restore-skip
predicate** (`SimulationIntegrationExecutor`'s private `bodyStateEqualsRestored`): exact `operator==`,
position and linear velocity always, rotation and angular velocity only when the declaration's body state
is exactly `PhysicsBodyState` (`wireCarriesRotationAndSpin`). ⛔ **Keep comparing against `beforePush`**,
the probe's X/R/V/W read, which is the same read `ChaosPhysicsBodyAdapter::captureStartOfStepBodyState`
makes for the push. Change the push's rule, or the adapter's read, and change this function in the same
edit.

**The consequence.** `skipped=` on `[ResimProbe.PushVerdict]` is the measurement of task 84's deliberate
deviation: how many bodies per rewind the push left unwritten (and unwoken). A predicate that drifts from
the push's still prints a plausible number, and nothing compares the two.

**What breaks if the tag moves.** Nothing mechanical. The push's predicate is private to an og-simulation
class template, so this file cannot call it, and `ChaosTickBackendUImpl.cpp` is built by no LLT target.

---

## G-11 — DIAGNOSTIC ONLY — READ BY NOTHING IN PRODUCTION: the `[ChaosDilation]` window

*Added by og-tickdriver task 18.*

**Site:** `latencyBudget::ScalarWindowStats m_chaosDilationWindow;` (`ChaosTickBackendUImpl.h`), the first of the three
members (`m_chaosDilationWindow`, `m_lastPhysicsTickOffset`, `m_physicsTickOffsetResets`) of the block that exists only
while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1.

**The prohibition.** ⛔ These three members are the `[ChaosDilation]` line's window, and only the host's diagnostics view
(`ChaosTickBackendUImpl::Diagnostics`) reads or writes them. Do not read one from the host's production code (the async
callback, the input hook, the mapper, the seam's other entry points), do not add an accessor that hands one out, and
do not move a production value into this block: the engine's network delta-time scale and the player controller's
physics tick offset are observed here, never acted on.

**The consequence.** The block is compiled out of Shipping and out of the fence build
(`UELatencyBudgetDiagnostics-rationale.md` §3, §5, in OGSimulationUnreal's docs). A production reader either fails to compile there or, if it is
itself gated to compile, makes Shipping behave differently from Development.

**What breaks if the tag moves.** Nothing mechanical: the gate makes an ungated reader fail to compile in a Shipping or
fence build, but an edit inside the view can still hand a member out.

---

## §R — Retired ids

*(The whole file retires with the Chaos configuration in task 21.)*

### G-09 — RETIRED by og-simulationscheduler-withjolt task 84 (2026-10-15): the wire-shape field rule moved into og-simulation's push

**Was:** *"the restored-state write sets only the fields the body's wire shape carries"*. Its tag sat in
`ChaosTickBackendUImpl.cpp`, above the `if (wireCarriesRotationAndSpin)` branch of `writeRestoredBodyState`. It
said: never write rotation or angular velocity for a body whose `bodyStateOf` is not exactly
`PhysicsBodyState`, because a `LinearBodyState`'s widening fabricates an identity rotation and a zero
angular velocity; and the flag was shared with the probe, so the probe compared exactly the fields the write
wrote.

**Why it retired.** Task 84 deleted `writeRestoredBodyState` and its branch. The Chaos host now calls
`SimulationIntegrationExecutor::pushCorrectedBodyStatesAll`, and the field rule lives there.
**Successor:** og-simulation's push rule, its
`if constexpr (std::is_same_v<BodyStateT, PhysicsBodyState>)` branch, described in og-simulation's
`SimulationStepDriver-rationale.md` §9 and covered by its push tests. A `LinearBodyState` declaration on
Chaos is written with `setBodyPose(position, restored.rotation)` and `setBodyLinearVelocity`. The restored
rotation is the body's own start-of-step R (`captureStartOfStepBodyState`), narrowed to float; the
fabricated identity rotation and zero spin are never written. That narrowing is the one field change
against the deleted function, which left R untouched (`ChaosTickBackendUImpl-rationale.md` §5). The probe's
`cmp=` flag (`⛔G-04`) is computed with the same `std::is_same_v` predicate in the probe's first pass.
