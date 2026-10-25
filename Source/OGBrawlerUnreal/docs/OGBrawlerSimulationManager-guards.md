<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: BrawlerInputProviderFn -- retired alias: removed with the callerless wrappers by og-ue-simulationcomponentandmanagersplit task 36; named in the census and in retired G-01's entry and quoted last text -->
<!-- lint-external-ref: SimulationManagerUImpl.h -- RENAMED by og-ue-simulationcomponentandmanagersplit task 30 to OGBrawlerSimulationManager.h; kept in retired G-18's quoted last text -->
<!-- lint-external-ref: writesThisFrame -- a term in the stated ceiling formula (G-17), not a declared identifier -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
<!-- lint-external-ref: getRingoutVizState -- retired name: became AOGBrawlerSimulationManager::getVizSlice (og-ue-simulationcomponentandmanagersplit task 22); historical mention in the closed census -->
<!-- lint-external-ref: m_movementStaticDataCVars -- retired member: folded into BrawlerSessionGameUImpl::makeStaticData by og-ue-simulationcomponentandmanagersplit task 24a; historical mention in the closed census -->
<!-- lint-external-ref: BrawlerNetSync -- retired alias: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a (the session named the composition's NetSync until task 34, which spells it with og-simulation's template, checked against it); quoted text of retired G-18 -->
<!-- lint-external-ref: BrawlerSimulatables -- retired alias: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a; quoted text of retired G-18 -->
# `OGBrawlerSimulationManager.h` and `.cpp` — guards

Every prohibition that survived the triage of the header (task 11, ids G-01..G-49) and of the
implementation file (task 12, ids G-50 onward). Each entry has an **opaque, stable id**; in the
source a single line `// ⛔G-nn  docs/OGBrawlerSimulationManager-guards.md` sits directly above the
statement or declaration where the forbidden edit would be typed.

**If this file and the source disagree, the source is authoritative and this file is stale.** Fix
this file; do not soften the source to match it.

⛔ **An id is never reused.** A guard that is deleted, or that becomes a compile-time or run-time
check, moves to §R and its number is spent forever. A retired id may be **named** in prose or in
an assertion message; it may never again appear as a `⛔G-nn` **tag**.

⭐ **The join is machine-checked in both directions** by this repository's guard-tag lint: every
tag resolves to an entry here, every live entry is referenced by exactly one tag, no id is
duplicated, and no retired id reappears as a tag. ⚠ It checks that an entry EXISTS. It never
checks that this text is TRUE.

⛔ **Nothing in this file is a rationale.** Orientation, provenance and derivations live in
`OGBrawlerSimulationManager-rationale.md`, and the header's full pre-conversion comment text is carried
there, verbatim, in §12. A guard here is a prohibition, the fence that stated it, and what breaks.

⚠ **This file is not the source of truth for any VALUE.** Where a number appears below it makes an
argument readable; the declaration is authoritative.

## The census — every prohibition in the pre-conversion header, and where it went

Task 11 converted this header from its third convention (prose plus 83 `§N` pointers, no tags) to
tags. The prohibitions in it went five ways. Only the first row is in this file.

| disposition | fences | count |
|---|---|---|
| **live guard, one tag each** | G-01 … G-18 | **18** |
| **converted to a `static_assert`** in the header — seen to FIRE on its own poison | construction order (5 fences: banner, `m_storage`, `m_movementStaticDataCVars`, the reorder block, `m_inputResolution`), `m_authorityRegisteredIds` is a set, the cap vs `brawlerRingout::kMaxSpawnPoints`, the callback option set vs its overrides, `getRingoutVizState` const-and-by-value, `getTimeConfigPtr` returns a pointer | **10 fences, 6 conversions** |
| **already held by the compiler** — the forbidden edit was measured to fail to compile, so the prose was redundant | `StaticData` never copied or moved (`C2280`); the cvar result is `const` (`C3490`); the clock seam has no bare accessor (`C2039`/`C2248`); the relayed-read and relayed-arrival getters return pointer-to-const (`C2440`, both); `onPostSimulationGameThread` stays out of line (`C2027`); one relayed source per poll (the pure poll's pairing `static_assert`, `C2338`) | **7** |
| **dropped — its stated consequence was measured FALSE** | *"CAST BEFORE THE SUBTRACTION … two unsigned ticks would wrap it into a vast positive"*: `driftTicks` is `int32_t`, and under C++20 the uncast spelling yields the identical value (rationale §11, C9) | **1** |
| **re-triaged as rationale, no tag** — the forbidden edit is typed in another file, or nowhere in particular | e.g. *"DO NOT WIDEN ONE INTO AN ACCESSOR"*, *"NO PROBE FAMILY MAY BE FILED UNDER `[Resim.`"*, *"THE SERVER IS THE SOLE OWNER OF THE RTT TIER"*, *"Emplaced on BOTH roles"*, *"Erased in unregisterFromNewFramework"*, *"NO ROLE LOGIC LIVES HERE"* | see rationale §12 |

⚠ **The census is closed.** It records what task 11 converted. A guard added later is listed
below this line with the task that added it; it is not folded into the table above.

* **G-75** — og-netcode-v2-field-defects task 9, 2026-09-23: the systems executor's detection adapters.
* **G-18** — retired by og-simulationscheduler-withjolt task 81, 2026-10-15, when the alias chain moved to
  `BrawlerCompositionUImpl.h`; its successor is that file's G-01. See §R.
* **G-02, G-51 to G-55, G-71, G-76, G-77** — retired by og-simulationscheduler-withjolt task 82, 2026-10-15,
  when the Chaos host moved to `ChaosTickBackendUImpl`; their successors are that file's G-01 to G-09. See §R.
* **G-80 to G-84** — retired by og-simulationscheduler-withjolt task 83, 2026-10-15, when the Jolt
  host moved to `TickDriverBackendUImpl`; their successors are that file's G-01 to G-05. See §R.
* **G-85** — og-simulationscheduler-withjolt task 83 (rework 1), 2026-10-15: `EndPlay`'s first statement, the
  backend's `onEndPlayBegin`; the half of the old G-80 that is typed in this class.
* **G-05 to G-13** — retired by og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21, when the input-history
  feeds moved to `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h`); their successors are
  `InputHistoryVisualizationUImpl-guards.md` G-09 to G-17. See §R.
* **G-14, G-67, G-68, G-74, G-78** — retired by og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21, when the
  brawler roster (the sim-id allocator, the spawn slots, the pre-diet cap, the ring-out seed and the score push) moved to
  `BrawlerRosterUImpl`; their successors are `BrawlerRosterUImpl-guards.md` G-01, G-03, G-04, G-05 and G-02. See §R.
* **G-75** — retired by og-ue-simulationcomponentandmanagersplit task 24a, 2026-10-21, when the composition (the adapters, the storage, the static data, the peers,
  the systems executor, the integration layer, the manager and the backend) moved into OGSimulationUnreal's
  `UESimulationSession`; its successor is `UESimulationSession-guards.md` G-01. See §R.
* **G-04** — retired by og-ue-simulationcomponentandmanagersplit task 24b, 2026-10-21, when the composition reads and
  `getVizSlice` moved into OGSimulationUnreal's `UESimulationSession`; its successor is `UESimulationSession-guards.md`
  G-02. See §R.
* **G-03, G-15, G-16, G-61, G-62, G-65, G-66, G-79** — retired by og-ue-simulationcomponentandmanagersplit task 25, 2026-10-21,
  when the transport's registry, its listener interfaces, the effective input delay, the stall and the timing relay moved
  into OGSimulationUnreal's `UESimulationSession`; their successors are `UESimulationSession-guards.md` G-03 to G-10, in
  that order. The session's G-11 (the transport seam) is new. See §R.
* **G-17** — retired by og-ue-simulationcomponentandmanagersplit task 26a, 2026-10-21, when the input reception (the
  coordinator, the delayed-input map, the two sinks with PROBE 5, the per-step release, the reap and the forget) moved
  into OGSimulationUnreal's `UESimulationSession`; its successor is `UESimulationSession-guards.md` G-12. The session's
  G-13 (the map erase stays in the unregistration) is new. See §R.
* **G-56, G-57, G-58, G-63, G-70, G-72** — retired by og-ue-simulationcomponentandmanagersplit task 26b, 2026-10-21, when the
  session knobs (the three ini intakes, the authority apply and the resim-policy apply with its proof line) and the
  per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A, the coordinator test, `[PacketBudget]`
  and PROBE 6) moved into OGSimulationUnreal's `UESimulationSession`; their successors are `UESimulationSession-guards.md`
  G-15, G-14, G-16, G-17, G-18 and G-19, in that order. See §R.
* **G-64, G-85** — retired by og-ue-simulationcomponentandmanagersplit task 27a, 2026-10-21, when the teardown (`EndPlay`'s statements before
  `Super`) and the post-physics pass (`OnPostPhysicsStep`) moved into OGSimulationUnreal's `UESimulationSession` as `endPlay` and
  `onPostPhysicsStep`; their successors are `UESimulationSession-guards.md` G-21 and G-20. This class's `EndPlay` is now
  `m_session.endPlay()`, then `Super`. See §R.
* **G-50, G-59, G-60** — retired by og-ue-simulationcomponentandmanagersplit task 27b, 2026-10-21, when the start-up (`BeginPlay`'s statements after `Super`,
  with `bindCorrectionFieldDiffGate`) moved into OGSimulationUnreal's `UESimulationSession` as `beginPlay`, and its brawler
  statements became the six phase hooks of `BrawlerSessionGameUImpl`; the successors of G-50 and G-59 are
  `UESimulationSession-guards.md` G-22 and G-23, and G-60's is `BrawlerSessionGameUImpl-guards.md` G-01 (it stays BUSL with
  the hook `neutralInput()`, ruling Q-J). This class's `BeginPlay` is now `Super`, then `m_session.beginPlay()`. See §R.
* **G-69, G-73** — retired by og-ue-simulationcomponentandmanagersplit task 28, 2026-10-21, when the registration sequences
  (`tryRegister` and `unregisterFromNewFramework`, with `PendingRegistration` and `TryRegisterStatus`) moved into
  OGSimulationUnreal's `UESimulationSession`, and their brawler statements became the four registration hooks of
  `BrawlerSessionGameUImpl` (`rootBodyId`, `afterBodiesCreated`, `afterRegistered`, `onUnregistered`); their successors are
  `UESimulationSession-guards.md` G-25 and G-26. This class keeps the `SimCharacterId` wrappers (the `None` check and
  `toStorageKey`), which call the session's sequences (task 36 removed the callerless `tryRegister` one; the
  `unregisterFromNewFramework` one remains). Since then `OGBrawlerSimulationManager.cpp` carries no tag. See §R.
* **G-01** — retired by og-ue-simulationcomponentandmanagersplit task 36, 2026-10-24, when the callerless `tryRegister` and
  `noteDelayedInputComponent` wrappers were removed and `BrawlerInputProviderFn`, then used only by its own contract
  check, went with them. No successor id: the compiler holds the prohibition. See §R. Since then
  `OGBrawlerSimulationManager.h` carries no tag either.

---

*G-01 retired by og-ue-simulationcomponentandmanagersplit task 36 with the alias it marked; no successor id, the compiler holds the prohibition. See §R.*

---

*G-02 moved to `ChaosTickBackendUImpl-guards.md` as G-01 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

*G-03 moved to `UESimulationSession-guards.md` as G-03 (og-ue-simulationcomponentandmanagersplit task 25), with the member it marks. See §R.*
---

*G-04 moved to `UESimulationSession-guards.md` as G-02 (og-ue-simulationcomponentandmanagersplit task 24b), with the accessor it marks. See §R.*

---

*G-15 moved to `UESimulationSession-guards.md` as G-04 (og-ue-simulationcomponentandmanagersplit task 25), with the declaration it marks. See §R.*
---

*G-16 moved to `UESimulationSession-guards.md` as G-05 (og-ue-simulationcomponentandmanagersplit task 25), with the declaration it marks. See §R.*
---

*G-17 moved to `UESimulationSession-guards.md` as G-12 (og-ue-simulationcomponentandmanagersplit task 26a), with the member it marks. See §R.*

---

## Entries G-50 and up — `OGBrawlerSimulationManager.cpp`

<!-- ================= DECLARED LINT ESCAPES — entries G-50 and up ==========
     Every token below is CORRECT and cannot resolve. ========================= -->
<!-- lint-external-ref: OutBytes -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: OutPackets -- Unreal Engine connection stat, outside every scan root -->
<!-- lint-external-ref: OnPhysSceneStep -- Unreal Engine delegate (ChaosScene.h), outside every scan root; this file stopped binding it in og-simulationscheduler-withjolt task 51 -->
<!-- lint-external-ref: ResimCooldownTicks -- ABSENCE FENCE (G-58): the ini key that was built and removed on a ruling. It must NOT resolve -->

Ids **G-50 onward** belong to the implementation file; the header's are below G-50. Each quote
is the comment as it shipped in the `.cpp` immediately before conversion. Where a quoted
sentence was found false in verification, the correction sits directly beneath the quote and
the quote is left exactly as it was.

Six prohibitions this file used to state in prose are now compile-time or runtime checks, and
seven more turned out to be enforced already by something else. None of those has an entry
here. The list, and how each was seen to fail, is in `OGBrawlerSimulationManager-rationale.md` §13.9.

---

*G-50 moved to `UESimulationSession-guards.md` as G-22 (og-ue-simulationcomponentandmanagersplit task 27b), with the function it marks: `bindCorrectionFieldDiffGate` is the session's private static member, called from its `beginPlay`. See §R.*
---

*G-51, G-52, G-53, G-54, G-55 moved to `ChaosTickBackendUImpl-guards.md` as G-02, G-03, G-04, G-05, G-06 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

*G-56 moved to `UESimulationSession-guards.md` as G-15 (og-ue-simulationcomponentandmanagersplit task 26b), with the blank line it marks, between the two intakes, which moved into the session's `readSessionKnobs`. See §R.*
---

*G-57 moved to `UESimulationSession-guards.md` as G-14 (og-ue-simulationcomponentandmanagersplit task 26b), with the intake it marks, which moved into the session's `readSessionKnobs`. See §R.*
---

*G-58 moved to `UESimulationSession-guards.md` as G-16 (og-ue-simulationcomponentandmanagersplit task 26b), with the intake it marks, which moved into the session's `readSessionKnobs`. See §R.*
---

*G-59 moved to `UESimulationSession-guards.md` as G-23 (og-ue-simulationcomponentandmanagersplit task 27b), with the call it marks, the authority branch's neutral input, now in the session's `beginPlay`. See §R.*
---

*G-60 moved to `BrawlerSessionGameUImpl-guards.md` as G-01 (og-ue-simulationcomponentandmanagersplit task 27b): it guards the value, so it rides on the body of the brawler's hook `neutralInput()` (ruling Q-J), which the session's `beginPlay` calls in both role branches. See §R.*
---

*G-61 moved to `UESimulationSession-guards.md` as G-06 (og-ue-simulationcomponentandmanagersplit task 25), with the emplace it marks, which moved into the session's `beginTierConsumer`. See §R.*
---

*G-62 moved to `UESimulationSession-guards.md` as G-07 (og-ue-simulationcomponentandmanagersplit task 25), with the two pulls it governs, which moved into the session's `replayLatchedClientValues`. See §R.*
---

*G-63 moved to `UESimulationSession-guards.md` as G-17 (og-ue-simulationcomponentandmanagersplit task 26b), with the block it marks, which became the session's `applyResimTriggerPolicy`; the owner calls it after both role branches. See §R.*
---

*G-64 moved to `UESimulationSession-guards.md` as G-21 (og-ue-simulationcomponentandmanagersplit task 27a), with the statements it marks: `EndPlay`'s statements before `Super` became the session's `endPlay`. See §R.*
---

*G-65 moved to `UESimulationSession-guards.md` as G-08 (og-ue-simulationcomponentandmanagersplit task 25), with the statement it marks. See §R.*
---

*G-66 moved to `UESimulationSession-guards.md` as G-09 (og-ue-simulationcomponentandmanagersplit task 25), with the statement it marks. See §R.*
---

*G-69 moved to `UESimulationSession-guards.md` as G-25 (og-ue-simulationcomponentandmanagersplit task 28), with the call it marks: `tryRegister` became the session's own sequence. See §R.*
---

*G-70 moved to `UESimulationSession-guards.md` as G-18 (og-ue-simulationcomponentandmanagersplit task 26b), with the test it marks, which moved with `onFrameStepsDue_GameThread` into the session. See §R.*
---

*G-71 moved to `ChaosTickBackendUImpl-guards.md` as G-07 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

*G-72 moved to `UESimulationSession-guards.md` as G-19 (og-ue-simulationcomponentandmanagersplit task 26b), with the call it marks, which moved with `onFrameStepsDue_GameThread` into the session. See §R.*
---

*G-73 moved to `UESimulationSession-guards.md` as G-26 (og-ue-simulationcomponentandmanagersplit task 28), with the test it marks: `unregisterFromNewFramework` became the session's own sequence. See §R.*
---

*G-75 moved to `UESimulationSession-guards.md` as G-01 (og-ue-simulationcomponentandmanagersplit task 24a), with the declaration it marks. See §R.*

---

*G-76, G-77 moved to `ChaosTickBackendUImpl-guards.md` as G-08, G-09 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

*G-79 moved to `UESimulationSession-guards.md` as G-10 (og-ue-simulationcomponentandmanagersplit task 25), with the owner registry; this class's destructor, whose whole body the guarded code was, became the session's. See §R.*
---

*G-80, G-81, G-82, G-83, G-84 moved to `TickDriverBackendUImpl-guards.md` as G-01, G-02, G-03, G-04, G-05 (og-simulationscheduler-withjolt task 83), with the Jolt host's code. See §R.*

*G-85 moved to `UESimulationSession-guards.md` as G-20 (og-ue-simulationcomponentandmanagersplit task 27a), with the call it marks: `EndPlay`'s statements before `Super` became the session's `endPlay`. See §R.*
---

## §R — Retired ids

*(The compile-time conversions in the census never held an id — the header had none before task 11 —
so they are recorded there, not here.)*

### G-18 — RETIRED by og-simulationscheduler-withjolt task 81 (2026-10-15): the alias chain moved to `BrawlerCompositionUImpl.h`

**Was:** *"The simulatable-pack aliases are CLASS-scoped"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly
above the first alias of the chain, `using BrawlerSimulatables = SimulatableList<SimulatableBrawler>;`. The fence,
verbatim — the bytes it occupied in the header before task 11:

<!-- header lines 1032-1035 at b9f6d81 -->
```
// ---- SIMULATABLE-PACK ALIAS CHAIN -------------------------------------
//
// Single source of truth: widen this one alias and every type below inherits it.
// ⛔ CLASS-scoped so these names cannot leak to global scope from an adapter header. §2
```

Its last text said: *"`SimulationManagerUImpl.h` is included by most of this module, and the OGSim core lives in
the global namespace. Hoist the chain to file scope and five generic names (`BrawlerStorage`, `BrawlerNetSync`,
...) land in the global namespace of every includer. Inside the class they are the one source of truth for the
pack: widen `BrawlerSimulatables` and every storage and executor type below it follows."* ⚠ *"most of this
module"* was not true at retirement: 12 of the module's 29 `.cpp` files include the header directly.

**Why it retired.** Its subject moved. Task 81 moved the chain into `struct BrawlerCompositionUImpl` in
`BrawlerCompositionUImpl.h`, so a backend host declared outside this class (task 83) can name the manager type.
The class now holds only one-line re-exports of the struct's members. **Successor:**
`BrawlerCompositionUImpl-guards.md` G-01 — the aliases live inside the struct, never at file scope, and
`Simulatables` is the single source of truth.

### G-02 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-01

**Was:** *"`bodyName` stores a pointer to a STRING LITERAL, and only a string literal"*. Its tag sat in `OGBrawlerSimulationManager.h`, above `FRewindPushProbeStashedBody::bodyName`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-01.

### G-51 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-02

**Was:** *"the push-probe verdict read stays at the top of `OnPreSimulate_Internal`, and its `else` stays"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above the push-probe verdict read at the top of `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-02.

### G-52 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-03

**Was:** *"the push probe reads X/R, never P/Q"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above the X/R reads in `readLiveBodyForPushProbe`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-03.

### G-53 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-04

**Was:** *"the push probe compares with the production predicate, over the fields the body's wire shape carries"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above `pushProbeFieldsSimilar`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-04.

### G-54 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-05

**Was:** *"the push-probe verdict's denominator is `nonInert`, and `VACUOUS` is a third verdict"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above `pushProbeVerdictText`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-05.

### G-55 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-06

**Was:** *"`noteResimGrant` is recorded before `prepareResimulation`"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above the `noteResimGrant` call in `FirstPreResimStep_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-06.

### G-71 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-07

**Was:** *"the upcoming sim tick comes from the mapper, and the `+ 1` is derived"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above `firstUpcomingSimTick` in the Chaos configuration's input hook.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-07.

### G-76 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-08

**Was:** *"the restored body state is WRITTEN onto the particle in `FirstPreResimStep_Internal`, never pushed as a rewind target"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above the `writeRestoredBodyState` call in `FirstPreResimStep_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-08.

### G-77 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosTickBackendUImpl-guards.md` G-09

**Was:** *"the restored-state write sets only the fields the body's wire shape carries"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, above the `wireCarriesRotationAndSpin` branch of `writeRestoredBodyState`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into ChaosBackendUImpl (historical name; `ChaosTickBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosTickBackendUImpl-guards.md` G-09.


### G-80 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `TickDriverBackendUImpl-guards.md` G-01

**Was:** *"`EndPlay` waits for every outstanding step task before any teardown"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, in the Jolt configuration's code: the second statement of `EndPlay`, the frame host's wait for outstanding steps.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into JoltBackendUImpl (historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `TickDriverBackendUImpl-guards.md` G-01.

### G-81 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `TickDriverBackendUImpl-guards.md` G-02

**Was:** *"the Jolt bind runs under the world mutex, and the game thread never applies occupancy"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, in the Jolt configuration's code: the world-mutex scope line (JoltWorldLockScopeUImpl, since deleted) of `tryRegister`'s first-call bind.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into JoltBackendUImpl (historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `TickDriverBackendUImpl-guards.md` G-02.

### G-82 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `TickDriverBackendUImpl-guards.md` G-03

**Was:** *"the shadow world is built by the step world's calls, and every bind and release is mirrored into it"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, in the Jolt configuration's code: the shadow world's construction in `buildShadowWorld`.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into JoltBackendUImpl (historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `TickDriverBackendUImpl-guards.md` G-03.

### G-83 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `TickDriverBackendUImpl-guards.md` G-04

**Was:** *"the render apply teleports the capsule: no sweep"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, in the Jolt configuration's code: the actor move (the engine's SetActorLocationAndRotation) in the render apply (applyRenderSnapshot_GameThread, since deleted).

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into JoltBackendUImpl (historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `TickDriverBackendUImpl-guards.md` G-04.

### G-84 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `TickDriverBackendUImpl-guards.md` G-05

**Was:** *"a client never picks a Jolt slot itself: it binds the slot the authority chose, or waits"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, in the Jolt configuration's code: the `decideBodySlot` call in the slot acquire (tryAcquireJoltSlot, since deleted).

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into JoltBackendUImpl (historical name; `TickDriverBackendUImpl` since og-tickdriver task 15b), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `TickDriverBackendUImpl-guards.md` G-05.

### G-05 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-09

**Was:** *"The prediction-offset read keeps its `runsPrediction()` guard"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the `(m_manager.has_value() && m_manager->runsPrediction())` predicate of `predictionOffsetTicks` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-09.

### G-06 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-10

**Was:** *"Local control is the CAPTURE-LINE test, never a role test"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `const bool isLocallyControlled = isLocallyControlledOnThisPeer(id);` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-10.

### G-07 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-11

**Was:** *"The tier decomposition is drawn only for a LOCALLY controlled character"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the `(includeDelay && isLocallyControlled ...` predicate of `delay` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-11.

### G-08 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-12

**Was:** *"The clock reading keeps its `runsPrediction()` guard"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `if (m_manager.has_value() && m_manager->runsPrediction())` around the `ClockDriftReading` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-12.

### G-09 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-13

**Was:** *"Each event counter is read BEFORE its tick"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the first of the seven `predictionClock.getDiagnostics()` reads in `pollInputHistoryLanes` (`reading.skipCount`).

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-13.

### G-10 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-14

**Was:** *"The relay-health window is `TimeConfig::rollbackWindowTicks`, not the per-tier ceiling"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the `(m_manager.has_value() && m_manager->getTimeConfig().rollbackWindowTicks > 0)` predicate of `rollbackWindowTicks` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-14.

### G-11 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-15

**Was:** *"The relay-health bar is NOT gated on `includeDelay`"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `if (relayedReads != nullptr && relayedArrivals != nullptr)` in `pollInputHistoryLanes`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-15.

### G-12 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-16

**Was:** *"The readout's `dLatest` stamp comes from the NEWEST observation by tick, never the last slot visited"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `if (!anyObservation || observation->simTick > newestSimTick)` in `getRelayReadReadout`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-16.

### G-13 — RETIRED by og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21): moved to `InputHistoryVisualizationUImpl-guards.md` G-17

**Was:** *"The nearest-character gather reads the VIZ snapshot, never live state"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `const auto& movement = simulatable.getVizState().getState()...` inside `gatherNearestCandidates`.

**Why it retired.** Its subject moved. Task 22 moved the input-history feeds out of this class into `BrawlerInputHistoryFeedUImpl` (`InputHistoryVisualizationUImpl.h` and `.cpp`), and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `InputHistoryVisualizationUImpl-guards.md` G-17.

### G-14 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): moved to `BrawlerRosterUImpl-guards.md` G-01

**Was:** *"The seed is a DELIBERATE post-construction write of `StaticData`"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the declaration of the manager's private ring-out seed method.

**Why it retired.** Its subject moved. Task 23 moved the brawler roster out of this class into `BrawlerRosterUImpl` (`BrawlerRosterUImpl.h` and `.cpp`), and the guarded declaration went with it, as `seedSpawnPointsFromLevel`. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `BrawlerRosterUImpl-guards.md` G-01.

### G-67 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): moved to `BrawlerRosterUImpl-guards.md` G-03

**Was:** *"the spawn slot is written through from `acquire`, never clamped and never derived per peer"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `ringoutIC.spawnSlot = m_spawnSlots.acquire(id);` in the first-call pass of `tryRegister`.

**Why it retired.** Its subject moved. Task 23 moved the brawler roster out of this class into `BrawlerRosterUImpl` (`BrawlerRosterUImpl.h` and `.cpp`), and the guarded code went with it, into `onBodiesCreated`. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `BrawlerRosterUImpl-guards.md` G-03.

### G-68 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): moved to `BrawlerRosterUImpl-guards.md` G-04

**Was:** *"the pre-diet cap warns once per over-cap character, at `Warning`, and never asserts"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the `if (record.isAuthority)` block of `tryRegister` that inserted into `m_authorityRegisteredIds`.

**Why it retired.** Its subject moved. Task 23 moved the brawler roster out of this class into `BrawlerRosterUImpl` (`BrawlerRosterUImpl.h` and `.cpp`), and the guarded code went with it, into `onRegistered`. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `BrawlerRosterUImpl-guards.md` G-04.

### G-74 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): moved to `BrawlerRosterUImpl-guards.md` G-05

**Was:** *"unregistration releases the ring-out spawn slot, ungated"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_spawnSlots.release(id);` in `unregisterFromNewFramework`.

**Why it retired.** Its subject moved. Task 23 moved the brawler roster out of this class into `BrawlerRosterUImpl` (`BrawlerRosterUImpl.h` and `.cpp`), and the guarded code went with it, into `forgetCharacter`. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `BrawlerRosterUImpl-guards.md` G-05.

### G-78 — RETIRED by og-ue-simulationcomponentandmanagersplit task 23 (2026-10-21): moved to `BrawlerRosterUImpl-guards.md` G-02

**Was:** *"a refused `SimCharacterId` allocation is final: never wrap, reset or fall back to another id source"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `if (simId == SimCharacterId::None)` in `allocateSimCharacterId`.

**Why it retired.** Its subject moved. Task 23 moved the brawler roster out of this class into `BrawlerRosterUImpl` (`BrawlerRosterUImpl.h` and `.cpp`), and the guarded code went with it, with `allocateSimCharacterId` itself. The entry moved with its text unchanged apart from its site and cross-references, plus an R0 line. **Successor:** `BrawlerRosterUImpl-guards.md` G-02.

### G-75 — RETIRED by og-ue-simulationcomponentandmanagersplit task 24a (2026-10-21): moved to `UESimulationSession-guards.md` G-01

**Was:** *"the systems executor is emplaced beside the adapters, and hands detection the same body adapter the integration
layer uses"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the systems executor's declaration, the optional of
the composition's systems executor.

**Why it retired.** Its subject moved. Task 24a moved the composition out of this class into OGSimulationUnreal's
`UESimulationSession` (`UESimulationSession.h`), and the guarded declaration went with it, as `m_systemsExec` of the
session. The entry moved with its text game-neutral (the session names the executor and the adapters through its traits),
plus an R0 line: the text said only the Chaos configuration builds, and both do. The emplace sites it governs are still
this class's `BeginPlay` until task 27b. The brawler's verification of the wrong adapter (the hit detector, measured on
the Chaos configuration) stayed with the brawler: `OGBrawlerSimulationManager-rationale.md` §2, "The systems executor's
adapters". **Successor:** `UESimulationSession-guards.md` G-01 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above `std::optional<BrawlerSystemsExec> m_systemsExec;`.
The two emplace sites it governs are in `OGBrawlerSimulationManager.cpp`, one per role branch, each between
`m_integrationLayer.emplace(...)` and `m_manager.emplace(...)`.

**The prohibition.** `BrawlerHitDetectionSystem` (`brawlerHitDetection::System<EngineTraits::BodyAdapter,
EngineTraits::QueryAdapter>`, the backend's types from `PhysicsBackendUImpl.h`) is constructed
from `*m_physAdapter` and `*m_queryAdapter`, the SAME two objects `m_integrationLayer` integrates with.
⛔ Never from `m_physReaderAdapter` (an `EngineTraits::ReaderAdapter`), and never before the
adapters are emplaced or after `m_manager` is.

**Rewritten for the traits types (og-simulationscheduler-withjolt task 51).** The text named the Chaos
types until the backend switch (rationale §18). The prohibition is the same in every configuration:
detection reads the body state the step itself reads, never the reader. Only the Chaos configuration
builds today, and the verification below is that configuration's.

**Verified 2026-09-23 (Chaos configuration).** `ChaosPhysicsBodyAdapter::getBodyTransform` reads
`proxy->GetPhysicsThreadAPI()`; `ChaosPhysicsBodyReaderAdapter::getBodyTransform` reads
`proxy->GetGameThreadAPI()` (its own banner: "Reads GT-interpolated state"). Both satisfy
`PhysicsBodyReaderAdapter`, so the wrong one compiles. The detector runs on the physics thread, in
`firePreIntegrate` of the next tick (og-netcode-v2-field-defects task 20; `firePostIntegrate` before
it), and reads the TARGET's guard transform that the target's integrate wrote on the physics-thread
particle, as the physics step left it.

**What breaks if the edit is made.**
* Handed the reader adapter, the detector classifies every guard block against the game-thread copy of
  the guard transform — an interpolated value from an earlier push, not this tick's — which is
  read (b) of task 9's defect reintroduced, with a staleness that now depends on game-thread timing
  instead of integrate order. Nothing fails to compile and no LLT can see it: the LLT targets do not
  compile the UE layer.
* The executor cannot be a plain member any more: the system stores pointers to the adapters, which are
  `std::optional`s emplaced in the role branches, so a default-constructed executor would hold pointers
  into empty optionals. Emplaced after `m_manager`, the manager would bind a reference to an empty
  `std::optional`'s storage.

### G-04 — RETIRED by og-ue-simulationcomponentandmanagersplit task 24b (2026-10-21): moved to `UESimulationSession-guards.md` G-02

**Was:** *"the scoreboard's ring-out slice is read off the VIZ snapshot, never `getAllState()`"*. Its tag sat in
`OGBrawlerSimulationManager.h`, directly above the `return` of `getVizSlice` (`getRingoutVizState` until task 22).

**Why it retired.** Its subject moved. Task 24b moved the composition reads out of this class into OGSimulationUnreal's
`UESimulationSession` (`UESimulationSession.h`), and `getVizSlice` went with them, as a member of the session. The entry
moved with its text made game-neutral (the session names the simulatable through its traits), plus an R0 line: the
text named the copy as `updateVisualizationAll(m_storage)` taking `m_vizState = m_allState`; the copy is the game
simulatable's `updateVizState()`, which og-simulation's `updateVisualizationAll` calls from both hosts, and on the Jolt
host the presenter then writes the rendered body poses into it (the ring-out slice is not a body state, so it is the plain
copy on both). The scoreboard's by-value assertion stays beside the scoreboard, re-targeted to the session's member.
**Successor:** `UESimulationSession-guards.md` G-02 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above the `return m_storage.get<SimulatableBrawler>(id).getVizState()...` statement in `getVizSlice`. Until og-ue-simulationcomponentandmanagersplit task 22 the accessor was `getRingoutVizState`; task 22 made it the generic `getVizSlice<SliceT>`, which the scoreboard instantiates for `brawlerRingout::State`, and the guard stayed with it.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 521-525 at b9f6d81 -->
```
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()`. `updateVisualizationAll(m_storage)`
//   takes `m_vizState = m_allState` once per game-thread pass in OnPostPhysicsStep, on the
//   line immediately above the score push; that copy is the SANCTIONED physics->game
//   handoff. A HUD reading `getAllState()` would be a fresh, unargued crossing of the
//   fence this header's banner draws, so this accessor does not offer one. §1
```

**What breaks if the edit is made.** `getVizState()` is the copy `updateVisualizationAll(m_storage)` takes once per game-thread pass in `OnPostPhysicsStep` — the sanctioned physics-to-game handoff. `getAllState()` is the live simulation state the physics thread writes. A HUD reading it would be a game-thread read of physics-written state that no crossing argument covers: `OGBrawlerSimulationManager-rationale.md` §0 and §1 enumerate the crossings, and this would be an unlisted one.

The accessor's other two properties — `const`, and returning **by value** — are no longer prose: they are a `static_assert` on its signature (task 11). Since task 22 that assert is re-expressed for the scoreboard's instantiation, `getVizSlice` for `brawlerRingout::State`, in `ScoreboardVisualizationUImpl.cpp`, beside the scoreboard's call.

### G-03 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-03

**Was:** *"`requestInputDelayIncreaseStall` keeps its `runsPrediction()` guard"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the early return in `requestInputDelayIncreaseStall`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the member it marks. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-03 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above the `if (!m_manager.has_value() || !m_manager->runsPrediction())` early return in `requestInputDelayIncreaseStall`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 372-372 at b9f6d81 -->
```
// Stall debt for a positive tier delta. ⛔ getClientClock() std::terminates on a server. §5
```

**What breaks if the edit is made.** `SimulationManager::editClientClock()` calls `std::terminate()` when the manager holds no client clock, which is every authority and standalone manager. Remove the guard and the first call that reaches a manager that does not predict ends the process — not a wrong number, a dead server.

⚠ **R0, task 11.** The fence names `getClientClock()`. The call this guard protects is `editClientClock()`. Both terminate identically (`SimulationManager.h`), so the consequence stands; only the name was imprecise.

### G-15 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-04

**Was:** *"The relay-delay-floor advisory is ADVISORY — never an assert"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the declaration of `logRelayDelayFloorAdvisory`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the declaration it marks. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-04 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above the declaration `void logRelayDelayFloorAdvisory(int32 floorTicks);`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 945-945 at b9f6d81 -->
```
// ADVISORY-ONLY, from BOTH intake points. ⛔ Never an assert: floor 0 must stay silent. §3
```

**What breaks if the edit is made.** Floor `0` is the documented "scheduled regime OFF" mode, and both intake points — the ini override and the `OnRep` — call this. An assert, `ensure` or `check` here would fire on the shipped default configuration. `classifyRelayDelayFloor` (`Network/ConnectionTierTable.h`) never flags `0`; this function only reports what that classification says. Both call sites in the `.cpp` said *"Advisory-only - see logRelayDelayFloorAdvisory"*, which is why the tag is on the declaration. ⚠ *(Task 12: those two comments went with the `.cpp`'s conversion; the same prohibition is guarded inside the body, at the classification `switch`, by **G-66**.)*

### G-16 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-05

**Was:** *"ONE recompute publishes the effective input delay, over BOTH inputs"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the declaration of `recomputeAndPublishEffectiveInputDelay`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the declaration it marks. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-05 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above the declaration `int32 recomputeAndPublishEffectiveInputDelay();`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 948-952 at b9f6d81 -->
```
// THE SHARED TWO-INPUT RECOMPUTE over the floor and the tier. §5
//
// ⛔ ONE SITE, BOTH CHANNELS: two half-formula writers answer stale when OnReps interleave.
//
// RETURNS the change in published delay, so a caller that must pay for an increase can.
```

<!-- header lines 82-87 at b9f6d81 -->
```
// THE CLIENT'S EFFECTIVE INPUT DELAY - the one formula this header serves:
//   effective = max(floor, tierKnown ? tierInputDelayTicks(tier)
//                                    : rttTierInputDelays[kMaxConnectionTierIndex])
// Two independent channels feed it (session floor, per-wire tier) and either
// OnRep may land first, so ⛔ both go through one recompute
// (recomputeAndPublishEffectiveInputDelay) and neither writes the atomic alone. §5
```

**What breaks if the edit is made.** The floor rides the session relay and the tier rides the per-connection relay; the two `OnRep`s are independent and land in either order. Two writers each holding half the formula answer with a stale other half whenever they interleave. Every publish — tier `OnRep`, floor `OnRep`, both replays and the composition root's baseline — goes through this one function, which reads both cached inputs through `ReplicatedTierConsumer::effectiveInputDelayTicks`, the same derivation the server's queue mirrors. Its return value is the change in published delay, which is how a caller that must pay for an increase can.

The second fence block is the orientation banner's statement of the same rule (lines 82-87), which now lives in `OGBrawlerSimulationManager-rationale.md` §0.

### G-61 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-06

**Was:** *"the tier cache is emplaced on the authority world too"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the tier consumer's emplace on the authority branch of `BeginPlay`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the emplace it marks, which moved into the session's `beginTierConsumer`. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-06 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `m_replicatedTierConsumer.emplace(...)` on the authority branch.

**The prohibition, as it stood in the source:**

> Client tier cache on the AUTHORITY world too - a listen-server host's local player uses it. §5
>
> ⛔ No tier ever arrives on an authority world, so this stays at the no-tier fallback.

**The consequence.** If the emplace moves into the client branch only, a listen-server host's
local player reads `0` delay where the no-tier fallback is correct.

### G-62 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-07

**Was:** *"the client binds its relay listeners, then PULLS the latched tier and floor"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the client branch's connection-listener registration in `BeginPlay`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the two pulls it governs, which moved into the session's `replayLatchedClientValues`. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-07 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);`
on the client branch, the start of the bind-then-pull block.

**The prohibition, as it stood in the source:**

> Bind the tier listener, then PULL. ⛔ The property dirties only on change, so an earlier
> OnRep would never be re-notified and the channel would be silently stranded. §5

> Same treatment for the FLOOR, for the same reason; a missing relay means no floor yet.

**The consequence.** If a pull is deleted, or moved above its bind, an `OnRep` that fired before
the bind is never re-delivered. The client then runs on the pre-arrival fallback for the whole
session.

### G-65 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-08

**Was:** *"`hadAnyTier` is read before `applyReplicatedConnectionTier`"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the `hadAnyTier` read in `onConnectionTierReceived`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the statement it marks. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-08 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the `const bool hadAnyTier =` statement in `onConnectionTierReceived`.

**The prohibition, as it stood in the source:**

> ⛔ Read BEFORE applyReplicatedConnectionTier, which sets hasReceivedTier() unconditionally. §5

**The consequence.** If the read comes after the apply, every call reports "had a tier",
including the first. The first real tier resolution then requests a spurious stall.

### G-66 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-09

**Was:** *"the relay-delay-floor advisory is a log line, never an assert"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the classification `switch` in `logRelayDelayFloorAdvisory`.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the statement it marks. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-09 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the `switch (classifyRelayDelayFloor(...))` in `logRelayDelayFloorAdvisory`.

**The prohibition, as it stood in the source:**

> ⛔ ADVISORY ONLY, never an assert: floor 0 is scheduled-regime-OFF, and
> classifyRelayDelayFloor (ConnectionTierTable.h) never flags it. That file has the table. §3
>
> Called from BOTH floor intake points, the same belt-and-braces shape the clamp uses.

**The consequence.** An assert on `BelowHiccupBaseline` takes a server down on a legal
configured floor of `1`.

**The same prohibition, at the declaration:** G-15 carries it at the header's declaration of
`logRelayDelayFloorAdvisory`. The two tags sit where the two different wrong edits would be typed:
one would assert in the function body, the other would change the function's contract.

### G-79 — RETIRED by og-ue-simulationcomponentandmanagersplit task 25 (2026-10-21): moved to `UESimulationSession-guards.md` G-10

**Was:** *"the destructor touches no `UObject`: it clears the static instance table and nothing else"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the first statement of this class's destructor.

**Why it retired.** Its subject moved. Task 25 moved the transport's registry, its three listener interfaces, the effective input delay, the stall and the timing relay out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded code went with the owner registry; this class's destructor, whose whole body the guarded code was, became the session's. The entry moved with its text made game-neutral and verified (an R0 line on each). **Successor:** `UESimulationSession-guards.md` G-10 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the first statement of `~AOGBrawlerSimulationManager` (the `s_instances[0]` test).

**The prohibition** (og-brawler-uploadtosteam task 12, 2026-09-29). ⛔ Do not call `GetWorld()`,
`GetPhysicsScene()` or any other method that reaches the actor's outer, its world or another
`UObject` from the destructor. Unbinding from engine objects belongs in `EndPlay`, which runs for
every world teardown while the world still exists. The Chaos configuration removes its post-tick
handle and its two solver callbacks there.

**The consequence of getting it wrong.** The destructor runs inside the garbage collector's purge,
after the actor's outer level may already have been destroyed. The destructor used to call
`GetWorld()` to unbind `OnPhysSceneStep`, and that read freed memory: a packaged Development client
crashed with `EXCEPTION_ACCESS_VIOLATION reading address 0xffffffffffffffff` in
UObjectBaseUtility::GetTypedOuter ← AActor::GetWorld ← this destructor, called from
UEngine::LoadMap → TrimMemory → CollectGarbage (engine frames). That happened the first time it travelled from
the standalone join-screen world, which has a manager, to a server (measured 2026-09-29). An editor
`-game` run of the same travel did not crash, so only a packaged client shows it.

**What breaks if the tag moves.** The tag marks the destructor's body. A `GetWorld()` typed above
the tagged statement is typed directly under the tag. Nothing mechanical checks the destructor: the
crash depends on the collector's purge order.

### G-17 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26a (2026-10-21): moved to `UESimulationSession-guards.md` G-12

**Was:** *"The relay write probe's capacity is the CODEC CONSTANT `relayedInputRing::kMaxDepth`"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above the declaration of `m_relayWriteProbe`.

**Why it retired.** Its subject moved. Task 26a moved the input reception (the coordinator, the delayed-input map, the two sinks with PROBE 5, the per-step release, the reap and the forget) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`), and the guarded member went with PROBE 5, whose write and forget both moved. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-12 (OGSimulationUnreal).

The entry as it stood before the move:

**Tag site:** `OGBrawlerSimulationManager.h`, directly above `RelayWriteProbe m_relayWriteProbe{ RelayStageCapacity{ ...kMaxDepth } }`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 991-991 at b9f6d81 -->
```
// Capacity is INJECTED: under flush-on-poll the ceiling is min(writesThisFrame, kMaxDepth). §6
```

**What breaks if the edit is made.** Under flush-on-poll the observable ceiling per frame is `min(writesThisFrame, kMaxDepth)`, and the stage the probe models is sized by that compile-time constant, inside the codec. The session-configurable depth that used to size the ring was retired with the replace-latest path (`OGBrawlerSimulationManager-rationale.md` §3, *The retired fourth knob*). Feed the probe any other number — a `TimeConfig` field, a literal — and `observableX1000` reports a ceiling the running write path does not have.

### G-56 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-15

**Was:** *"there is no relay-ring depth ini intake, deliberately"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, on the blank line between the relay-delay-floor intake and the rotation-width intake in `BeginPlay` (an absence tag).

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): the three ini intakes moved into the session's `readSessionKnobs`, which the owner calls at their position, and the absence tag moved to the same blank line there. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-15 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the line between the relay-delay-floor intake and the rotation-width intake in
`BeginPlay`, where the retired intake used to be.

**The prohibition, as it stood in the source** (two fences, one at the intake and one at the
apply block. They are now one tag, because the apply block has nothing left to re-add without
the intake first):

> ⛔ RETIRED: there is deliberately no relay ring depth ini intake here any more. It read
> a session-configurable retention depth for the outbound ring's replace-latest write
> path; the flush-on-poll replacement takes its capacity from `relayedInputRing::kMaxDepth`,
> a compile-time constant with no ini key, so the intake, its clamp, its setter and its
> startup proof line were all removed together. §6

> ⛔ RETIRED: there is deliberately no relay ring depth clamp, setter or proof-line block
> here any more. It published a session-configurable retention depth that flush-on-poll
> had already made inert - the stage capacity is `relayedInputRing::kMaxDepth`, a
> compile-time constant - so the whole inert path went rather than keep publishing a
> number nothing on the live relay path reads. §6

**The consequence.** Restoring the key restores a knob that nothing on the live path reads. That
is the "silently inert setting" class this file has shipped three times.

**What breaks if the tag moves.** This is an absence fence: no identifier exists to grep for,
and the tag is the only record.

### G-57 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-14

**Was:** *"the relay-delay floor is read from the ini on the authority only"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the floor intake's `if (worldIsAuthority && GConfig != nullptr)` in `BeginPlay`.

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): the three ini intakes moved into the session's `readSessionKnobs`, which the owner calls at their position, and the tag moved with the floor intake. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-14 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `if (worldIsAuthority && GConfig != nullptr)` at the floor intake in `BeginPlay`.

**The prohibition, as it stood in the source:**

> ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
> reading its own ini could disagree with the server it is meant to match.

**The consequence.** A client that takes its own ini floor predicts against a delay the server
does not use.

**What breaks if the tag moves.** Nothing else distinguishes this gate from the rotation-width
gate below it. That gate is authority-only for a different and weaker reason (a client read has
no reader).

### G-58 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-16

**Was:** *"the resim-trigger-policy intake is not authority-gated, is read once, and has no cooldown key"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the resim-trigger-policy intake's `if (GConfig != nullptr)` in `BeginPlay`.

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): the three ini intakes moved into the session's `readSessionKnobs`, which the owner calls at their position, and the tag moved with the policy intake. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-16 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `if (GConfig != nullptr)` at the resim-trigger-policy intake in `BeginPlay`.

**The prohibition, as it stood in the source:**

> ⛔ THERE IS DELIBERATELY NO `ResimCooldownTicks` KEY. A trigger-rate ceiling was built
> here and REMOVED on a user ruling: it defers acting on a correction already known to
> disagree, which is the defect this mechanism repairs. If a design document names that
> key, the document predates the ruling. The throttle is structural instead.
>
> ⚠ NOT AUTHORITY-GATED, unlike the two above: the gate exists ONLY on a predicting client.
>
> ⛔ ONE-SHOT, and here that is THREAD SAFETY: the policy is read unsynchronized at every
> correction landing, which is sound ONLY because it is written once before any land.

and, from the file banner:

> ⛔ NO KNOB HERE MAY BECOME A CVAR. Each is read ONCE at composition: the
> rotation width because a cadence that moves mid-run makes a probe window
> unattributable, and the resim policy because it is pushed into every
> correction cache and read unsynchronized on the landing path - which is sound
> only because it is written before any correction can land. §3

**The consequence.** Copying the neighbours' `worldIsAuthority &&` guard onto this intake reads
the ini on the one role that cannot use it and skips it on the only role that can. Turning the
policy into a cvar makes it a value written mid-session that `policyEnforcesDepthCeiling` reads
on the physics thread with no synchronization.

**What breaks if the tag moves.** The rotation-width intake above has the same no-cvar rule for
the weaker reason (attribution). This tag sits on the intake where the rule is a correctness
matter.

### G-63 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-17

**Was:** *"the resim-trigger policy is applied and proved once, after both role branches"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the bare scope block that follows the role `if`/`else` in `BeginPlay`.

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): the scope block's statements became the session's `applyResimTriggerPolicy`, which the owner calls at the block's position, and the tag moved to the function's first statement (task 27b moves the call itself and re-sites the "after both branches" half there). The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-17 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the scope block that follows the role `if`/`else` in `BeginPlay`.

**The prohibition, as it stood in the source:**

> ⛔ AFTER BOTH ROLE BRANCHES: duplicating apply-plus-proof is how the roles drift. §3

> STEP 2 - PARSE + VALIDATE. ⛔ An unrecognised string is REPORTED and the default kept:
> a typo silently selecting the other value changes the gate on a build nobody touched. §3

> STEP 4 - THE PROOF LINE. Unconditional, at Warning; see the banner. §3
>
> ⛔ THIS LINE IS THE BEHAVIOUR-NEUTRALITY RECEIPT: a later claim names WHICH POLICY WAS LIVE.
>
> ⛔ Values are read back from TimeConfig, so it cannot claim a setting nothing stored.
>
> ⛔ `depthPolicy` and `rateLimit` state inertness rather than falling silent. §3

**The consequence.** If apply-plus-proof is duplicated into the two branches, the roles drift.
If an unrecognised string falls through to a value, the gate changes on a build nobody touched.
If the proof line becomes conditional, a run's log can no longer say which policy was live.

**Correction (R0).** The banner's reason for `Warning` is that `Config/DefaultEngine.ini` sets
`LogOGNet=Warning`. At `b9f6d81` that file sets `LogOGNet=Verbose`. The line's other reason, that
it is unconditional, still holds. See §13.10, correction C13-1.

### G-70 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-18

**Was:** *"the frame function's coordinator test guards PROBE 6 only, never PROBE A"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `if (!m_session.m_receptionCoordinator.has_value()) return;` in `onFrameStepsDue_GameThread`.

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `onFrameStepsDue_GameThread` (PROBE A, the coordinator test, `[PacketBudget]` and PROBE 6) moved into the session whole, and the tag moved with the test. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-18 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `if (!m_session.m_receptionCoordinator.has_value()) return;` in `onFrameStepsDue_GameThread`, between
PROBE A and PROBE 6. The coordinator is the session's since og-ue-simulationcomponentandmanagersplit task 26a; the
test reads it there, at the same position.

**The prohibition, as it stood in the source** (in `releaseDelayedInputsForStep`, before
og-simulationscheduler-withjolt task 51):

> ⛔ The coordinator early-return guards ONLY the drain, never PROBE A, which runs on both. §8

**Rewritten and re-sited, id kept (task 51).** Task 51 split `releaseDelayedInputsForStep`: PROBE A,
PROBE 6 and `[PacketBudget]` moved, in that order, into `onFrameStepsDue_GameThread`, and the drain
kept the old name with a coordinator test of its own (rationale §7). The return this tag marks now
stands between PROBE A and PROBE 6. ⛔ Do not hoist it above PROBE A. PROBE 6 and `[PacketBudget]`
below it are server-only and correctly behind it.

**The consequence.** Hoisted to the top of the function, the return stops the frame-health probe on
every client, because a pure client never has a coordinator. The drain and the reap no longer depend
on it: `releaseDelayedInputsForStep` (the session's since task 26a) tests the coordinator itself, and the Chaos configuration's
`InjectInputs_External` tests it before the drain and the reap.

### G-72 — RETIRED by og-ue-simulationcomponentandmanagersplit task 26b (2026-10-21): moved to `UESimulationSession-guards.md` G-19

**Was:** *"the send-budget probe feeds the engine's cumulative counters, not `OutBytes` / `OutPackets`"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the `m_connectionBudgetProbe.noteSample(` call in `onFrameStepsDue_GameThread`.

**Why it retired.** Its subject moved. Task 26b moved the session knobs and the per-frame work before the steps (`onFrameStepsDue_GameThread`, with PROBE A and PROBE 6 and their two members) out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `onFrameStepsDue_GameThread` (PROBE A, the coordinator test, `[PacketBudget]` and PROBE 6) moved into the session whole, with the probe's member, and the tag moved with the call. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-19 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the `m_connectionBudgetProbe.noteSample(` call in `onFrameStepsDue_GameThread` (moved there
with PROBE 6 from `releaseDelayedInputsForStep` by og-simulationscheduler-withjolt task 51; the call
is unchanged).

**The prohibition, as it stood in the source:**

> ⛔ CUMULATIVE COUNTERS, NOT `OutBytes`/`OutPackets`: the engine zeroes those mid-window.

**The consequence.** The per-period accumulators reset on the engine's own schedule, so
differencing them across a probe window silently loses whatever was reset in the middle.

### G-64 — RETIRED by og-ue-simulationcomponentandmanagersplit task 27a (2026-10-21): moved to `UESimulationSession-guards.md` G-21

**Was:** *"`EndPlay` resets the coordinator and the tier cache while `m_manager` still exists"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_session.m_delayedInputComponentsById.clear();` in `EndPlay`.

**Why it retired.** Its subject moved. Task 27a moved this class's teardown and post-physics pass out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `EndPlay`'s statements before `Super` became the session's `endPlay`, which `EndPlay` now calls before `Super`, and `OnPostPhysicsStep` became the session's `onPostPhysicsStep`. The tag moved with the statements it marks. The entry moved with its text made game-neutral and verified (an R0 line, with a correction). **Successor:** `UESimulationSession-guards.md` G-21 (OGSimulationUnreal).

The entry as it stood before the move (names as they stood then: the session's `m_backend` is `m_physicsBackend` since og-ue-simulationcomponentandmanagersplit
task 35):

**Site:** `m_session.m_delayedInputComponentsById.clear();` in `EndPlay`, the first statement after
`m_backend.onEndPlayBegin();` (`⛔G-85`); the tag covers it, and the coordinator's and the tier cache's resets
follow it.

**The prohibition, as it stood in the source:**

> ⛔ The coordinator borrows m_manager's TimeConfig, so it is reset BEFORE the manager. §2

> ⛔ Same borrow rule as the coordinator: the tier cache holds m_manager's TimeConfig. §2

and, at the coordinator's emplace:

> ⛔ AUTHORITY BRANCH ONLY, and it borrows m_manager's TimeConfig, so it must not outlive it. §7

**Verified, and why the reset is load-bearing rather than tidy.** `EndPlay` never resets
`m_manager`. The manager lives until the actor is destroyed. In the header as measured,
`m_receptionCoordinator` and `m_replicatedTierConsumer` are both declared **before** `m_manager`,
so member destruction (reverse declaration order) would destroy the manager **first**. These two
resets are the only reason neither borrower outlives the `TimeConfig` it holds a reference to.

*Since og-ue-simulationcomponentandmanagersplit task 25* the tier consumer is the session's `m_replicatedTierConsumer`,
declared after the session's `m_manager`, and the session asserts that order (`UESimulationSession-rationale.md` §2), so
member destruction now destroys it first. Its reset here (`m_session.m_replicatedTierConsumer.reset()`) is unchanged and
still runs while the manager exists.

*Since og-ue-simulationcomponentandmanagersplit task 26a* the coordinator and the delayed-input map are the session's
too (`m_receptionCoordinator`, emplaced by `beginReceptionCoordinator`, and `m_delayedInputComponentsById`), declared after
the session's `m_manager`; a `compositionContractsHold()` row asserts the coordinator's order
(`UESimulationSession-rationale.md` §2). Member destruction now destroys both borrowers before the manager, so neither
reset is the only protection any more; both run here unchanged (`m_session.m_receptionCoordinator.reset()`), while the
manager exists, until task 27a moves `EndPlay` into the session.

**The consequence.** If either reset is deleted, or moved below a future `m_manager.reset()`, a
`const TimeConfig&` dangles.

### G-85 — RETIRED by og-ue-simulationcomponentandmanagersplit task 27a (2026-10-21): moved to `UESimulationSession-guards.md` G-20

**Was:** *"`m_backend.onEndPlayBegin()` stays the first statement of `EndPlay`: nothing moves above it"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_session.m_backend.onEndPlayBegin();` in `EndPlay`.

**Why it retired.** Its subject moved. Task 27a moved this class's teardown and post-physics pass out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `EndPlay`'s statements before `Super` became the session's `endPlay`, which `EndPlay` now calls before `Super`, and `OnPostPhysicsStep` became the session's `onPostPhysicsStep`. The tag moved with the call it marks. The entry moved with its text made game-neutral and verified (an R0 line, with a correction). **Successor:** `UESimulationSession-guards.md` G-20 (OGSimulationUnreal).

The entry as it stood before the move (names as they stood then: the session's `m_backend` is `m_physicsBackend` since og-ue-simulationcomponentandmanagersplit
task 35):

**Site:** `m_backend.onEndPlayBegin();`, the first statement of `EndPlay`.

**The prohibition** (og-simulationscheduler-withjolt task 83; the half of the old G-80 that is typed in this
class since the Jolt host left it). ⛔ Do not move any `EndPlay` statement above `m_backend.onEndPlayBegin()`, do
not add one above it, and do not drop the call. Everything else in `EndPlay` (G-64's resets, the static instance
table and listener unregistration, the post-tick-flush unbind, the backend's `onEndPlayWorld`) runs after it. The
rule is the same in both configurations:
- on Jolt, `onEndPlayBegin` unregisters the frame host's two tick functions and waits for every outstanding step
  task (`TickDriverBackendUImpl-guards.md` G-01);
- on Chaos it is an empty inline body today; a Chaos teardown that must run first belongs inside it, not above it.

**The consequence of getting it wrong.** On a worker client the frame's step task runs on another thread through
the rest of the frame and reads the coordinator, the tier cache and the manager's state. A statement above the call
tears that state down while the step still runs: a use-after-reset on a worker thread, which nothing reports.

**What breaks if the tag moves.** The tag marks the call. A statement typed above it is typed directly under the
tag. Nothing mechanical checks the order: the race needs a step in flight at `EndPlay`. Deleting the call takes the
tag with it and orphans this entry.

### G-50 — RETIRED by og-ue-simulationcomponentandmanagersplit task 27b (2026-10-21): moved to `UESimulationSession-guards.md` G-22

**Was:** *"the field-diff predicate must ask the category and verbosity `RouteOGMessage` sends `[DivergenceProbe` to, live"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above the `correctionFieldDiff::setEnabledPredicate(` call in the file-local `bindCorrectionFieldDiffGate`.

**Why it retired.** Its subject moved. Task 27b moved this class's start-up out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `BeginPlay`'s statements after `Super` became the session's `beginPlay`, which `BeginPlay` now calls after `Super`, and the brawler statements in it became the phase hooks of `BrawlerSessionGameUImpl` (manager design §4.3), each at its statements' position. The file-local function moved with it, unchanged, as the session's private static member; the tag moved with the call it marks. The entry moved with its text made game-neutral and verified (an R0 line). **Successor:** `UESimulationSession-guards.md` G-22 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** the `correctionFieldDiff::setEnabledPredicate(` call inside `bindCorrectionFieldDiffGate`.

**The prohibition, as it stood in the source:**

> ⛔ A PREDICATE, NOT A LATCHED BOOL, AND THAT IS WHY IT IS A LAMBDA. `UE_LOG_ACTIVE`
> re-reads the category's CURRENT verbosity, so `LogOGDivergenceProbe Verbose` typed into
> the console mid-session starts naming fields on the next correction, and Warning stops
> it again. Reading it once at BeginPlay would pin the session to whatever the ini said.
>
> ⛔ IT MUST NAME THE SAME CATEGORY `RouteOGMessage` SENDS `[DivergenceProbe` TO, at the
> SAME verbosity the line carries (`[Verbose]`). Re-route that prefix and this gate starts
> answering about a channel the line no longer uses — it would then pay for a walk whose
> result is dropped, or drop a walk whose line is printed. This predicate and
> `RouteOGMessage`'s `[DivergenceProbe` arm are the ONLY TWO SITES IN CODE that USE this
> category — one reads its verbosity, the other logs to it; the DECLARE, the DEFINE and the
> ini value are the only other mentions. Change one and you must change the other.

**The consequence.** A predicate asking a different category than the router uses either pays
for a per-field walk whose line is then dropped, or skips a walk whose line is printed without
its `field=` tail. A latched bool loses the console switch mid-session.

**What breaks if the tag moves.** The other half of this pair is the `DivergenceProbe` row of
`kOGLogRoutes`. That row is now checked by a `static_assert`, but only for ORDER; nothing
checks that the row and this predicate name the same category. This tag is the only thing
that says so at the predicate. Since og-tickdriver task 12 the row is og-simulation's (its
`kSimulationLogRoutes`, route `SimulationLogRoute::DivergenceProbe`), and the category is chosen
by `ogLogRouteOf`'s `DivergenceProbe` case, then by `RouteOGMessage`'s `OGLogRoute::DivergenceProbe`
arm, both in the og-simulation-ue plugin's `UESimulationLogRoutes.h` since og-ue-simulationcomponentandmanagersplit
task 19 (`OGBrawlerSimulationManager.h` and its `.cpp` before). A re-route is typed at either of those two.

### G-59 — RETIRED by og-ue-simulationcomponentandmanagersplit task 27b (2026-10-21): moved to `UESimulationSession-guards.md` G-23

**Was:** *"the authority branch sets the neutral input too, before any registration"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_session.m_inputResolution.setNeutralInput<SimulatableBrawler>(...)` on `BeginPlay`'s authority branch.

**Why it retired.** Its subject moved. Task 27b moved this class's start-up out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `BeginPlay`'s statements after `Super` became the session's `beginPlay`, which `BeginPlay` now calls after `Super`, and the brawler statements in it became the phase hooks of `BrawlerSessionGameUImpl` (manager design §4.3), each at its statements' position. The call is the session's own statement on `beginPlay`'s authority branch, its value the game's hook `neutralInput()`; the tag moved with the call. **Successor:** `UESimulationSession-guards.md` G-23 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `m_inputResolution.setNeutralInput<SimulatableBrawler>(...)` on the authority branch.

**The prohibition, as it stood in the source:**

> Inject the game's zero input for the client input delay line. §5
>
> ⛔ SET ON THE AUTHORITY BRANCH TOO, and not as a precaution: a DEDICATED server reads this
> on every tick it substitutes an input for a remote character, and seeds each character's
> replicated applied-input with it. Deleting it makes the authority simulate - and publish
> to every peer - a zero forward vector for the whole of every join window.
>
> ⛔ ORDER IS LOAD-BEARING: this must precede every registerAuthorityOwner call.

**The consequence.** The authority simulates and publishes a zero forward vector through every
join window.

**What breaks if the tag moves.** The client branch's call carries its own id (G-60) for a
different wrong edit.

### G-60 — RETIRED by og-ue-simulationcomponentandmanagersplit task 27b (2026-10-21): moved to `BrawlerSessionGameUImpl-guards.md` G-01

**Was:** *"the neutral input is `getZeroPlayerInput()`, not `PlayerInput{}`"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_session.m_inputResolution.setNeutralInput<SimulatableBrawler>(...)` on `BeginPlay`'s client branch.

**Why it retired.** Its subject moved. Task 27b moved this class's start-up out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `BeginPlay`'s statements after `Super` became the session's `beginPlay`, which `BeginPlay` now calls after `Super`, and the brawler statements in it became the phase hooks of `BrawlerSessionGameUImpl` (manager design §4.3), each at its statements' position. The value `getZeroPlayerInput()` is the body of the brawler's hook `neutralInput()`, and this guard guards the value, not the sequence, so it stays BUSL with the hook body (ruling Q-J); the session's client-branch call carries no tag. **Successor:** `BrawlerSessionGameUImpl-guards.md` G-01.

The entry as it stood before the move:

**Site:** `m_inputResolution.setNeutralInput<SimulatableBrawler>(...)` on the client branch.

**The prohibition, as it stood in the source:**

> Fills the [0, effectiveDelay) window. ⛔ NOT PlayerInput{}: (0,0,1) forwards, load-bearing. §5

**Why this is still a tag, measured.** Making it a `static_assert` was tried and does not
compile: `simulatableBrawler::PlayerInput` has no `operator==` (C2678), and
`getZeroPlayerInput()` is not `constexpr` in any case.

**The consequence.** A value-initialised input carries zero forward vectors where the game
expects `(0,0,1)`.

### G-69 — RETIRED by og-ue-simulationcomponentandmanagersplit task 28 (2026-10-21): moved to `UESimulationSession-guards.md` G-25

**Was:** *"the executor is notified only once the character is in storage"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `m_session.m_manager->notifyCharacterRegistered(id);` in `tryRegister`.

**Why it retired.** Its subject moved. Task 28 moved this class's registration sequences out of this class into OGSimulationUnreal's `UESimulationSession` (`UESimulationSession.h`): `tryRegister`'s statements after the `SimCharacterId::None` check and the `toStorageKey` conversion became the session's `tryRegister`, which this class's `tryRegister` now calls, and the brawler statements in it became the registration hooks of `BrawlerSessionGameUImpl` (manager design §4.3), each at its statements' position. The notify is the session's own statement; the tag moved with it. **Successor:** `UESimulationSession-guards.md` G-25 (OGSimulationUnreal). Its R0 there corrects §10's "on the authority path": the notify runs on both roles, filtered per system.

The entry as it stood before the move:

**Site:** `m_manager->notifyCharacterRegistered(id);` in `tryRegister`.

**The prohibition, as it stood in the source:**

> Notify the executor: the character is IN STORAGE, so
> brawlerHitRouting::System::onCharacterRegistered can index it and read capsuleBodyId. §10
>
> ⛔ The notify and the drop of the adapter's m_byRootBodyId insert land TOGETHER.

**A second dependant, which the source did not name here.** `brawlerRingout::ScoreSystem`'s
roster is seeded by the same notify. The ring-out score push's `checkf` relies on that seed
landing **before** `tryRegister` returns `Ready`, and so before the route entry the push walks
exists. See `OGBrawlerSimulationManager-rationale.md` §1, *the second read crossing*.

**The consequence.** A notify moved above `registerSimulatable` indexes a character that is not
in storage. A notify moved after the `return` breaks the score push's no-rehash precondition.

### G-73 — RETIRED by og-ue-simulationcomponentandmanagersplit task 28 (2026-10-21): moved to `UESimulationSession-guards.md` G-26

**Was:** *"unregistration notifies the executor before `unregisterSimulatable`, behind the `has<>` guard"*. Its tag sat in `OGBrawlerSimulationManager.cpp`, directly above `if (m_session.m_storage.has<SimulatableBrawler>(id))` at the top of `unregisterFromNewFramework`.

**Why it retired.** Its subject moved. Task 28 moved `unregisterFromNewFramework`'s statements after the `toStorageKey` conversion into the session's `unregisterFromNewFramework`, which this class's `unregisterFromNewFramework` now calls; the roster forget and the input-history forget became the hook `onUnregistered`, and the latency forget is the session's own statement. The guarded test is the session's own; the tag moved with it. **Successor:** `UESimulationSession-guards.md` G-26 (OGSimulationUnreal).

The entry as it stood before the move:

**Site:** `if (m_storage.has<SimulatableBrawler>(id))` at the top of `unregisterFromNewFramework`.

**The prohibition, as it stood in the source:**

> ⛔ Drop the routing entry BEFORE unregisterSimulatable destroys it, while still in storage. §10
>
> ⛔ The has<> guard is preserved: an unregistered character's view.get<>(id) is unsafe.

**The consequence.** A notify after the destroy lets the routing hook dereference a character
that storage has already freed. Dropping the guard runs `view.get<>(id)` for a character that
never finished registering.

### G-01 — RETIRED by og-ue-simulationcomponentandmanagersplit task 36 (2026-10-24): its site was deleted; the compiler holds it

**Was:** *"The input provider is HANDED the raw-capture history; it does not fetch it"*. Its tag sat in `OGBrawlerSimulationManager.h`, directly above `using BrawlerInputProviderFn = std::function<...>`.

**Why it retired.** Its site was deleted. Task 36 removed this class's two callerless wrappers, `tryRegister` and `noteDelayedInputComponent` (no caller since task 29: the binding's registration skeleton calls the session's directly, re-verified by grep across every repository and by two arms that re-add a call and fail with `C2039`). The wrapper's parameter was the alias's last user apart from its own contract check in `compositionContractsHold`, so the alias, the check and `assertCompositionContracts` went with it, and the tag with the alias. The prohibition is still true, and it no longer needs a tag: the one provider type is spelled once, the latency member's `InputProvider` (`UELatencyBudgetDiagnostics.h`, in OGSimulationUnreal), so there is no second spelling to drift; its second parameter is the history; and the compiler rejects a provider without it at both places it could be typed. Seen failing (task 36): drop the parameter from `InputProvider` alone and the brawler's provider lambda no longer converts (`OGBrawlerSimulatableComponent.cpp`, `C2440`, arm G1); drop it there AND from the lambda, so the provider would have to fetch the history itself, and the session's registration fails, because og-simulation's `registerSimulatable` (`SimulationNetSync.h`) takes the provider with the `LocalInputCache` parameter (`C2665`, "cannot convert argument 8 from 'std::function<BrawlerSessionTraitsUImpl::PlayerInput (const SimulationTimeStep &)>'", arm G2). The component's own fence at that lambda (`OGBrawlerSimulatableComponent-guards.md` G-05) is unchanged. **Successor:** none as an id; the compiler, at the provider's conversion to `InputProvider` (arm G1) and at og-simulation's `registerSimulatable` (arm G2).

The entry as it stood before the retirement:


**Tag site:** `OGBrawlerSimulationManager.h`, directly above the `using BrawlerInputProviderFn = std::function<...>` alias, whose second parameter is the history.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 208-209 at b9f6d81 -->
```
// The provider signature, named once so the four sites passing one cannot drift apart. §5
// ⛔ The raw-capture history is a PARAMETER: the sequence matcher runs inside the provider.
```

**What breaks if the edit is made.** The one production provider — `UOGBrawlerSimulatableComponent`'s, built only for a locally predicted character — is a lambda that captures the input component and the id and nothing else, and runs the sequence matcher over the `LocalInputCache` it is passed. The component's own fence at that lambda forbids capturing the manager. Drop the parameter and the matcher has to find the history for itself: by capturing something that can reach the resolution peer, and by looking the cache up by id instead of being handed it by the peer that owns it. The alias is named once so that its users — this alias, this class's `tryRegister` parameter and the component's local — cannot drift apart; they change together or not at all. Since og-ue-simulationcomponentandmanagersplit task 28 the pending record and the session's `tryRegister` parameter hold the latency member's `InputProvider` (`UESimulationSession.h`), which this class's `compositionContractsHold` asserts is the same type (`std::is_same_v`), so a drift there fails to compile. *R0, task 28:* verified: `BrawlerInputProviderFn` is spelled in `OGBrawlerSimulationManager.h` (the alias, the `tryRegister` declaration and the `static_assert`), `OGBrawlerSimulationManager.cpp` (the `tryRegister` definition) and `OGBrawlerSimulatableComponent.cpp` (the component's local), and in no other code file. *Task 29:* the component's local is the binding's skeleton's now, typed as the latency member's `InputProvider`, and the component's hook `makeInputProvider` returns that type, which the binding's inline hook check requires; `BrawlerInputProviderFn` is spelled in the two `OGBrawlerSimulationManager` files only (re-verified by grep). The one production provider is built by that hook.
