<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: writesThisFrame -- a term in the stated ceiling formula (G-17), not a declared identifier -->
<!-- lint-external-ref: writeRestoredBodyState -- retired name: the Chaos rollback write deleted by og-simulationscheduler-withjolt task 84 (historical mention) -->
# `SimulationManagerUImpl.h` and `.cpp` — guards

Every prohibition that survived the triage of the header (task 11, ids G-01..G-49) and of the
implementation file (task 12, ids G-50 onward). Each entry has an **opaque, stable id**; in the
source a single line `// ⛔G-nn  docs/SimulationManagerUImpl-guards.md` sits directly above the
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
`SimulationManagerUImpl-rationale.md`, and the header's full pre-conversion comment text is carried
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
  when the Chaos host moved to `ChaosBackendUImpl`; their successors are that file's G-01 to G-09. See §R.
* **G-80 to G-84** — retired by og-simulationscheduler-withjolt task 83, 2026-10-15, when the Jolt
  host moved to `JoltBackendUImpl`; their successors are that file's G-01 to G-05. See §R.
* **G-85** — og-simulationscheduler-withjolt task 83 (rework 1), 2026-10-15: `EndPlay`'s first statement, the
  backend's `onEndPlayBegin`; the half of the old G-80 that is typed in this class.

---

## G-01 — The input provider is HANDED the raw-capture history; it does not fetch it

**Tag site:** `SimulationManagerUImpl.h`, directly above the `using BrawlerInputProviderFn = std::function<...>` alias, whose second parameter is the history.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 208-209 at b9f6d81 -->
```
// The provider signature, named once so the four sites passing one cannot drift apart. §5
// ⛔ The raw-capture history is a PARAMETER: the sequence matcher runs inside the provider.
```

**What breaks if the edit is made.** The one production provider — `USimmableUpdateComponent`'s, built only for a locally predicted character — is a lambda that captures the input component and the id and nothing else, and runs the sequence matcher over the `LocalInputCache` it is passed. The component's own fence at that lambda forbids capturing the manager. Drop the parameter and the matcher has to find the history for itself: by capturing something that can reach the resolution peer, and by looking the cache up by id instead of being handed it by the peer that owns it. The alias is named once so that its four users — this alias, `tryRegister`'s parameter, `PendingRegistration::inputProvider` and the component's local — cannot drift apart; they change together or not at all.

---

*G-02 moved to `ChaosBackendUImpl-guards.md` as G-01 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

## G-03 — `requestInputDelayIncreaseStall` keeps its `runsPrediction()` guard

**Tag site:** `SimulationManagerUImpl.h`, directly above the `if (!m_manager.has_value() || !m_manager->runsPrediction())` early return in `requestInputDelayIncreaseStall`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 372-372 at b9f6d81 -->
```
// Stall debt for a positive tier delta. ⛔ getClientClock() std::terminates on a server. §5
```

**What breaks if the edit is made.** `SimulationManager::editClientClock()` calls `std::terminate()` when the manager holds no client clock, which is every authority and standalone manager. Remove the guard and the first call that reaches a manager that does not predict ends the process — not a wrong number, a dead server.

⚠ **R0, task 11.** The fence names `getClientClock()`. The call this guard protects is `editClientClock()`. Both terminate identically (`SimulationManager.h`), so the consequence stands; only the name was imprecise.

---

## G-04 — The scoreboard's ring-out slice is read off the VIZ snapshot, never `getAllState()`

**Tag site:** `SimulationManagerUImpl.h`, directly above the `return m_storage.get<SimulatableBrawler>(id).getVizState()...` statement in `getRingoutVizState`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 521-525 at b9f6d81 -->
```
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()`. `updateVisualizationAll(m_storage)`
//   takes `m_vizState = m_allState` once per game-thread pass in OnPostPhysicsStep, on the
//   line immediately above the score push; that copy is the SANCTIONED physics->game
//   handoff. A HUD reading `getAllState()` would be a fresh, unargued crossing of the
//   fence this header's banner draws, so this accessor does not offer one. §1
```

**What breaks if the edit is made.** `getVizState()` is the copy `updateVisualizationAll(m_storage)` takes once per game-thread pass in `OnPostPhysicsStep` — the sanctioned physics-to-game handoff. `getAllState()` is the live simulation state the physics thread writes. A HUD reading it would be a game-thread read of physics-written state that no crossing argument covers: `SimulationManagerUImpl-rationale.md` §0 and §1 enumerate the crossings, and this would be an unlisted one.

The accessor's other two properties — `const`, and returning **by value** — are no longer prose: they are a `static_assert` on its signature after the class (task 11).

---

## G-05 — The prediction-offset read keeps its `runsPrediction()` guard

**Tag site:** `SimulationManagerUImpl.h`, directly above the `(m_manager.has_value() && m_manager->runsPrediction())` predicate of `predictionOffsetTicks` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 549-549 at b9f6d81 -->
```
// ⚠ getNetworkEstimator() exists only on a predicting role, hence the guard. §5
```

**What breaks if the edit is made.** `SimulationManager::getNetworkEstimator()` calls `std::terminate()` on a manager that holds no estimator — every authority manager. And the guard's `std::nullopt` is itself the signal: an authority passes no offset rather than `0`, because a display told "the offset is 0" draws an authority marker on the newest cell (`SimulationManagerUImpl-rationale.md` §1).

---

## G-06 — Local control is the CAPTURE-LINE test, never a role test

**Tag site:** `SimulationManagerUImpl.h`, directly above `const bool isLocallyControlled = isLocallyControlledOnThisPeer(id);` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 569-571 at b9f6d81 -->
```
// The same test `pollInputHistory` makes, and for the same reason: a capture line
// exists for exactly the characters this client controls. ⛔ NOT A ROLE TEST -- a
// client can control several brawlers, and a listen-server host controls one. §1
```

**What breaks if the edit is made.** `runsPrediction()`, `HasAuthority()` and the net mode answer per MANAGER, not per character. A couch-co-op client drives several brawlers, and a listen-server host drives one on an AUTHORITY manager, so a role test gets both cases wrong. Everything after this line branches on the answer: the tier decomposition (G-07) and the two relayed-input sources are chosen from it, so a wrong answer draws the local connection's delay on a remote proxy, or blinds a local character's delay bar.

---

## G-07 — The tier decomposition is drawn only for a LOCALLY controlled character

**Tag site:** `SimulationManagerUImpl.h`, directly above the `(includeDelay && isLocallyControlled ...` predicate of `delay` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 574-575 at b9f6d81 -->
```
// A remote proxy's stack shows the relay it is being predicted from instead.
// ⛔ THE TIER DECOMPOSITION IS THE LOCAL CONNECTION'S and is not read for a remote.
```

**What breaks if the edit is made.** `decomposeInputDelay` reads THIS connection's tier consumer, the shared `TimeConfig` and the resolution peer's published delay. For a remote proxy none of those describe the proxy's wire. Drop `isLocallyControlled` from the predicate and a remote proxy's delay bar is labelled with the viewer's own tier and delay — plausible-looking numbers that describe the wrong connection.

---

## G-08 — The clock reading keeps its `runsPrediction()` guard

**Tag site:** `SimulationManagerUImpl.h`, directly above `if (m_manager.has_value() && m_manager->runsPrediction())` around the `ClockDriftReading` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 588-588 at b9f6d81 -->
```
// ⛔ GUARDED LIKE THE OFFSET ABOVE: getClientClock() std::terminates on a server. §5
```

**What breaks if the edit is made.** The block calls both `getClientClock()` and `getNetworkEstimator()`, and both call `std::terminate()` on a manager that does not predict. The guard is what makes the reading absent, rather than fatal, on every authority and standalone manager.

---

## G-09 — Each event counter is read BEFORE its tick

**Tag site:** `SimulationManagerUImpl.h`, directly above the first of the seven `predictionClock.getDiagnostics()` reads in `pollInputHistoryLanes` (`reading.skipCount`).

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 606-606 at b9f6d81 -->
```
// ⛔ COUNT BEFORE ITS TICK: the clock writes tick then count, so the tear is one-way.
```

**What breaks if the edit is made.** `ClientPredictionClock` writes each pair tick-first, count-second (`ClientPredictionClock.cpp`: `lastSkipTick` then `++skipCount`, and likewise for stall and hard resync). Reading count-first makes the harmful straddle — a fresh count beside a stale tick, which files an event against a tick that predates it — the one the program order does not produce. Reversing the reads makes it the likely one.

⛔ **R0, task 11 — the fence overclaims, and the rationale doc's copy said "CANNOT be observed".** Both sides are plain `unsigned int`s: neither the writes nor the reads are atomic, so C++ gives no ordering guarantee at all, and an optimizing compiler may reorder either pair. Source order makes the harmful straddle unlikely; it does not make it impossible. The guard is still worth keeping — reversing the order makes the wrong case the expected one — but the property it protects is "unlikely", not "unreachable". A real guarantee needs acquire/release atomics in `og-simulation`, which is not this file's to change.

The line above the fence — *"THE SEAM, THROUGH THE VIEW: the clock publishes no bare accessor"* — is not a guard. The compiler already holds it (task 11 measured `c.skipCount()` → `C2039` and `c.m_eventSeamDiagnostics` → `C2248`).

---

## G-10 — The relay-health window is `TimeConfig::rollbackWindowTicks`, not the per-tier ceiling

**Tag site:** `SimulationManagerUImpl.h`, directly above the `(m_manager.has_value() && m_manager->getTimeConfig().rollbackWindowTicks > 0)` predicate of `rollbackWindowTicks` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 633-637 at b9f6d81 -->
```
// How far back a resim may still reach, which is what separates an arrival that could
// still have been replayed into a tick from one that could not.
// The per-tier ceiling beside it escalates nothing today and its own banner says so, and
// a negative value disables the authority's future guard and is no window at all here.
// ⛔ THE SHIPPED WINDOW, `TimeConfig::rollbackWindowTicks`. §5
```

**What breaks if the edit is made.** The window separates an arrival that could still have been replayed into its tick from one that could not, so it must be the window a resim actually reaches. The per-tier ceiling beside it, `TimeConfig::rollbackWindowHardCap`, escalates nothing today — its own declaration says the escalation is *"INTENDED, NOT IMPLEMENTED"*. Substitute it and the bar sorts arrivals against a number no resim uses. The `> 0` test is part of the same fence: a negative value is the authority's "future guard disabled" setting and is no window at all here, so it maps to `0`.

---

## G-11 — The relay-health bar is NOT gated on `includeDelay`

**Tag site:** `SimulationManagerUImpl.h`, directly above `if (relayedReads != nullptr && relayedArrivals != nullptr)` in `pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 647-650 at b9f6d81 -->
```
// The relay-health bar rides the same two sources and is its own toggle, so the delay bar
// being off must not blind it; `includeDelay` still decides the tier decomposition above,
// which is the reading it actually names.
// ⛔ NOT GATED ON `includeDelay`.
```

**What breaks if the edit is made.** The relay-health bar has its own toggle and rides the same two relayed sources as the delay bar's client half. Add `includeDelay` to this condition and turning the delay bar off silently blinds the relay-health bar too. `includeDelay` still decides the tier decomposition (G-07), which is the reading it actually names.

---

## G-12 — The readout's `dLatest` stamp comes from the NEWEST observation by tick, never the last slot visited

**Tag site:** `SimulationManagerUImpl.h`, directly above `if (!anyObservation || observation->simTick > newestSimTick)` in `getRelayReadReadout`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 716-718 at b9f6d81 -->
```
// The stamp of the NEWEST observation, found by its own tick: the ring is addressed by
// sim tick and therefore walked out of order, so the last slot read is not the last one
// written. ⛔ NEVER THE LAST SLOT VISITED.
```

**What breaks if the edit is made.** The ring is addressed by sim tick, so walking it by index visits slots out of write order. "Last slot visited" is therefore an arbitrary observation, and the nearest stack would print a schedule stamp from whichever tick happened to sit in the highest index.

---

## G-13 — The nearest-character gather reads the VIZ snapshot, never live state

**Tag site:** `SimulationManagerUImpl.h`, directly above `const auto& movement = simulatable.getVizState().getState()...` inside `gatherNearestCandidates`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 736-738 at b9f6d81 -->
```
// `updateVisualizationAll(m_storage)` is the sanctioned physics->game handoff, and a HUD
// reading live state would be a fresh, unargued crossing.
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()` -- the ring-out slice's reason. §1
```

**What breaks if the edit is made.** Same reason as G-04, at a second site: `getVizState()` is the copy taken by `updateVisualizationAll`, and reading the live composite from a per-frame HUD gather would be a new game-thread read of physics-written state that no crossing argument covers.

---

## G-14 — `seedRingoutSpawnPointsFromLevel` is a DELIBERATE post-construction write of `StaticData`

**Tag site:** `SimulationManagerUImpl.h`, directly above the declaration `void seedRingoutSpawnPointsFromLevel(UWorld& world);`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 902-922 at b9f6d81 -->
```
// ---- THE RING-OUT SPAWN TABLE, READ OFF THE LEVEL (task 9) ------------
//
// ⛔⛔ THIS IS THE ONE PLACE IN THE TREE THAT WRITES `StaticData` AFTER CONSTRUCTION, AND IT
// IS A DELIBERATE, ARGUED EXCEPTION — NOT AN OVERSIGHT TO BE "FIXED" BACK.
// The banner above states StaticData as constructed once and never moved. What that
// discipline actually protects is that NO TICK EVER SEES IT CHANGE: a constant the sim
// reads mid-session is a constant two peers can disagree about and a resim can replay
// against the wrong value. This write is a ONE-TIME INIT inside `BeginPlay`, BEFORE the
// integration layer and the manager are even constructed, so there is no tick for it to be
// visible to. The `checkf`s at the definition are what hold that, mechanically.
//
// ⛔ WHY IT CANNOT BE A CONSTRUCTOR ARGUMENT INSTEAD, which is the obvious "proper" fix:
// `m_staticData` is brace-initialised in a MEMBER INITIALIZER, which runs during actor
// construction, and level actors are not reachable then. `APlayerStart` exists at
// `BeginPlay` and not one moment earlier. The same sentence is already true of
// `readMovementStaticDataCVars()` above and of the gravity `checkf` in `BeginPlay`.
//
// ⭐ ALL THE POLICY IS IN THE CORE TYPE, exactly as `m_spawnSlots` above:
// `brawlerRingout::spawnPointsFromLevelPlacements` does the sort and the fallback, and it is
// covered by `Ringout.SpawnPoints.*` in `BrawlerRingoutSimulationTest.cpp`. This method holds
// the actor walk, the `FName` -> bytes conversion, and no decisions.
```

**What breaks if the edit is made.** The obvious "proper" fix — pass the level's spawn points to `m_staticData`'s constructor — cannot work: `m_staticData` is built by a default member initializer during actor construction, and `APlayerStart` actors are not reachable until `BeginPlay`. The property this write must not break is that no TICK ever sees `StaticData` change — a value two peers can disagree about, and a resim can replay against, mid-session — and the two `checkf`s at the definition hold that mechanically: the seed runs at most once, and only while neither `m_integrationLayer` nor `m_manager` exists. Deleting this declaration to "fix" the exception deletes this tag and fails the gate, which is the point of the tag's position.

⚠ **R0, task 11 — two positional words in the fence are wrong.** *"The banner above states StaticData as constructed once and never moved"*: no banner above this declaration says that; the statement stood at `m_staticData`, BELOW it. *"`readMovementStaticDataCVars()` above"*: that declaration is BELOW this one too. (§11 C12.)

⛔ **R0, corrected after task 12's review — the fence's first sentence also MISATTRIBUTES (§11 C19).** *"What that discipline actually protects is that NO TICK EVER SEES IT CHANGE"*: the construct-once, never-copied-or-moved discipline (§2) protects `StaticData`'s **internal sibling references**, and it does not forbid a write — `StaticData` deletes copy and move, not assignment to its members. "No tick sees it change" is a separate, true property, and it is what the `checkf`s hold. Task 11's first version of this note called the sentence's substance true, and the paragraph above repeated the misattribution; both are corrected here.

---

## G-15 — The relay-delay-floor advisory is ADVISORY — never an assert

**Tag site:** `SimulationManagerUImpl.h`, directly above the declaration `void logRelayDelayFloorAdvisory(int32 floorTicks);`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 945-945 at b9f6d81 -->
```
// ADVISORY-ONLY, from BOTH intake points. ⛔ Never an assert: floor 0 must stay silent. §3
```

**What breaks if the edit is made.** Floor `0` is the documented "scheduled regime OFF" mode, and both intake points — the ini override and the `OnRep` — call this. An assert, `ensure` or `check` here would fire on the shipped default configuration. `classifyRelayDelayFloor` (`Network/ConnectionTierTable.h`) never flags `0`; this function only reports what that classification says. Both call sites in the `.cpp` said *"Advisory-only - see logRelayDelayFloorAdvisory"*, which is why the tag is on the declaration. ⚠ *(Task 12: those two comments went with the `.cpp`'s conversion; the same prohibition is guarded inside the body, at the classification `switch`, by **G-66**.)*

---

## G-16 — ONE recompute publishes the effective input delay, over BOTH inputs

**Tag site:** `SimulationManagerUImpl.h`, directly above the declaration `int32 recomputeAndPublishEffectiveInputDelay();`.

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

The second fence block is the orientation banner's statement of the same rule (lines 82-87), which now lives in `SimulationManagerUImpl-rationale.md` §0.

---

## G-17 — The relay write probe's capacity is the CODEC CONSTANT `relayedInputRing::kMaxDepth`

**Tag site:** `SimulationManagerUImpl.h`, directly above `RelayWriteProbe m_relayWriteProbe{ RelayStageCapacity{ ...kMaxDepth } }`.

**The fence, verbatim — the bytes it occupied in the header before task 11:**

<!-- header lines 991-991 at b9f6d81 -->
```
// Capacity is INJECTED: under flush-on-poll the ceiling is min(writesThisFrame, kMaxDepth). §6
```

**What breaks if the edit is made.** Under flush-on-poll the observable ceiling per frame is `min(writesThisFrame, kMaxDepth)`, and the stage the probe models is sized by that compile-time constant, inside the codec. The session-configurable depth that used to size the ring was retired with the replace-latest path (`SimulationManagerUImpl-rationale.md` §3, *The retired fourth knob*). Feed the probe any other number — a `TimeConfig` field, a literal — and `observableX1000` reports a ceiling the running write path does not have.

---

## Entries G-50 and up — `SimulationManagerUImpl.cpp`

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
here. The list, and how each was seen to fail, is in `SimulationManagerUImpl-rationale.md` §13.9.

---

## G-50 — the field-diff predicate must ask the category and verbosity `RouteOGMessage` sends `[DivergenceProbe` to, live

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
that says so at the predicate.

---

*G-51, G-52, G-53, G-54, G-55 moved to `ChaosBackendUImpl-guards.md` as G-02, G-03, G-04, G-05, G-06 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

## G-56 — there is no relay-ring depth ini intake, deliberately

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

---

## G-57 — the relay-delay floor is read from the ini on the authority only

**Site:** `if (worldIsAuthority && GConfig != nullptr)` at the floor intake in `BeginPlay`.

**The prohibition, as it stood in the source:**

> ⛔ AUTHORITY ONLY, and that is CORRECTNESS: the floor is REPLICATED, so a client
> reading its own ini could disagree with the server it is meant to match.

**The consequence.** A client that takes its own ini floor predicts against a delay the server
does not use.

**What breaks if the tag moves.** Nothing else distinguishes this gate from the rotation-width
gate below it. That gate is authority-only for a different and weaker reason (a client read has
no reader).

---

## G-58 — the resim-trigger-policy intake is not authority-gated, is read once, and has no cooldown key

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

---

## G-59 — the authority branch sets the neutral input too, before any registration

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

---

## G-60 — the neutral input is `getZeroPlayerInput()`, not `PlayerInput{}`

**Site:** `m_inputResolution.setNeutralInput<SimulatableBrawler>(...)` on the client branch.

**The prohibition, as it stood in the source:**

> Fills the [0, effectiveDelay) window. ⛔ NOT PlayerInput{}: (0,0,1) forwards, load-bearing. §5

**Why this is still a tag, measured.** Making it a `static_assert` was tried and does not
compile: `simulatableBrawler::PlayerInput` has no `operator==` (C2678), and
`getZeroPlayerInput()` is not `constexpr` in any case.

**The consequence.** A value-initialised input carries zero forward vectors where the game
expects `(0,0,1)`.

---

## G-61 — the tier cache is emplaced on the authority world too

**Site:** `m_replicatedTierConsumer.emplace(...)` on the authority branch.

**The prohibition, as it stood in the source:**

> Client tier cache on the AUTHORITY world too - a listen-server host's local player uses it. §5
>
> ⛔ No tier ever arrives on an authority world, so this stays at the no-tier fallback.

**The consequence.** If the emplace moves into the client branch only, a listen-server host's
local player reads `0` delay where the no-tier fallback is correct.

---

## G-62 — the client binds its relay listeners, then PULLS the latched tier and floor

**Site:** `ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);`
on the client branch, the start of the bind-then-pull block.

**The prohibition, as it stood in the source:**

> Bind the tier listener, then PULL. ⛔ The property dirties only on change, so an earlier
> OnRep would never be re-notified and the channel would be silently stranded. §5

> Same treatment for the FLOOR, for the same reason; a missing relay means no floor yet.

**The consequence.** If a pull is deleted, or moved above its bind, an `OnRep` that fired before
the bind is never re-delivered. The client then runs on the pre-arrival fallback for the whole
session.

---

## G-63 — the resim-trigger policy is applied and proved once, after both role branches

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

---

## G-64 — `EndPlay` resets the coordinator and the tier cache while `m_manager` still exists

**Site:** `m_delayedInputComponentsById.clear();` in `EndPlay`, the first statement after
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

**The consequence.** If either reset is deleted, or moved below a future `m_manager.reset()`, a
`const TimeConfig&` dangles.

---

## G-65 — `hadAnyTier` is read before `applyReplicatedConnectionTier`

**Site:** the `const bool hadAnyTier =` statement in `onConnectionTierReceived`.

**The prohibition, as it stood in the source:**

> ⛔ Read BEFORE applyReplicatedConnectionTier, which sets hasReceivedTier() unconditionally. §5

**The consequence.** If the read comes after the apply, every call reports "had a tier",
including the first. The first real tier resolution then requests a spurious stall.

---

## G-66 — the relay-delay-floor advisory is a log line, never an assert

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

---

## G-67 — the spawn slot is written through from `acquire`, never clamped and never derived per peer

**Site:** `ringoutIC.spawnSlot = m_spawnSlots.acquire(id);` in `tryRegister`.

**The prohibition, as it stood in the source:**

> ⛔ THE OVER-CAPACITY ANSWER IS WRITTEN THROUGH, NOT CLAMPED. `acquire` returns
> `kNoFreeSlot`, whose value is `kMaxSpawnPoints`, and the sub-sim's defensive index branch
> turns that into a `[Warning][Ringout.spawnSlot]` and no teleport seed. Clamping to 0 here
> would put the over-capacity character on top of whoever legitimately holds 0, silently.

> ⛔ AND ARRIVAL ORDER DECIDING THE NUMBER IS DELIBERATE - see the ⛔ banner on
> `brawlerRingout::SpawnSlotAllocator`. Clients never derive this; a replay restores it. Do
> not "fix" it into a per-peer derivation such as `GetPlayerSlotForActor`, which is per-wire
> and answers 0 for the primary pawn of every remote client.

**Correction (R0).** No ⛔ banner remains on `brawlerRingout::SpawnSlotAllocator`. That header
was converted, and the argument now lives in `BrawlerRingoutSimulation-rationale.md` §11a. See
§13.10, correction C13-9.

**Already partly enforced.** `kNoFreeSlot`'s *value* is pinned by a `static_assert` at its
declaration. That assert says in its own message that it exists "instead of needing a second
guard at the UE call site". Nothing stops a clamp being typed *here*, and that is what this tag
is for.

---

## G-68 — the pre-diet cap warns once per over-cap character, at `Warning`, and never asserts

**Site:** the `if (record.isAuthority)` block that inserts into `m_authorityRegisteredIds`.

**The prohibition, as it stood in the source:**

> THE PRE-DIET CAP FENCE, runtime half. Deleted with kPreDietCharacterCap by the diet. §10
>
> ⛔ ONCE PER OVER-CAP CHARACTER: a per-session latch would go silent after the fifth.
>
> ⛔ WARNING, not Log, and not an assert: an over-cap session still RUNS - report, do not crash. §3

**The consequence.** A latch stops reporting after the fifth character. An assert takes the
server down mid-brawl.

---

## G-69 — the executor is notified only once the character is in storage

**Site:** `m_manager->notifyCharacterRegistered(id);` in `tryRegister`.

**The prohibition, as it stood in the source:**

> Notify the executor: the character is IN STORAGE, so
> brawlerHitRouting::System::onCharacterRegistered can index it and read capsuleBodyId. §10
>
> ⛔ The notify and the drop of the adapter's m_byRootBodyId insert land TOGETHER.

**A second dependant, which the source did not name here.** `brawlerRingout::ScoreSystem`'s
roster is seeded by the same notify. The ring-out score push's `checkf` relies on that seed
landing **before** `tryRegister` returns `Ready`, and so before the route entry the push walks
exists. See `SimulationManagerUImpl-rationale.md` §1, *the second read crossing*.

**The consequence.** A notify moved above `registerSimulatable` indexes a character that is not
in storage. A notify moved after the `return` breaks the score push's no-rehash precondition.

---

## G-70 — the frame function's coordinator test guards PROBE 6 only, never PROBE A

**Site:** `if (!m_receptionCoordinator.has_value()) return;` in `onFrameStepsDue_GameThread`, between
PROBE A and PROBE 6.

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
on it: `releaseDelayedInputsForStep` tests the coordinator itself, and the Chaos configuration's
`InjectInputs_External` tests it before the drain and the reap.

---

*G-71 moved to `ChaosBackendUImpl-guards.md` as G-07 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

## G-72 — the send-budget probe feeds the engine's cumulative counters, not `OutBytes` / `OutPackets`

**Site:** the `m_connectionBudgetProbe.noteSample(` call in `onFrameStepsDue_GameThread` (moved there
with PROBE 6 from `releaseDelayedInputsForStep` by og-simulationscheduler-withjolt task 51; the call
is unchanged).

**The prohibition, as it stood in the source:**

> ⛔ CUMULATIVE COUNTERS, NOT `OutBytes`/`OutPackets`: the engine zeroes those mid-window.

**The consequence.** The per-period accumulators reset on the engine's own schedule, so
differencing them across a probe window silently loses whatever was reset in the middle.

---

## G-73 — unregistration notifies the executor before `unregisterSimulatable`, behind the `has<>` guard

**Site:** `if (m_storage.has<SimulatableBrawler>(id))` at the top of `unregisterFromNewFramework`.

**The prohibition, as it stood in the source:**

> ⛔ Drop the routing entry BEFORE unregisterSimulatable destroys it, while still in storage. §10
>
> ⛔ The has<> guard is preserved: an unregistered character's view.get<>(id) is unsafe.

**The consequence.** A notify after the destroy lets the routing hook dereference a character
that storage has already freed. Dropping the guard runs `view.get<>(id)` for a character that
never finished registering.

---

## G-74 — unregistration releases the ring-out spawn slot, ungated

**Site:** `m_spawnSlots.release(id);` in `unregisterFromNewFramework`.

**The prohibition, as it stood in the source:**

> Same unregister contract for the ring-out spawn-slot table (task 3). ⛔ WITHOUT THIS a
> session that churns characters exhausts a four-entry table and every later join is handed
> the out-of-range value, respawning nowhere. UNGATED for the same reason the erase above is:
> nothing on the client role ever acquired, so `release` is a no-op there by construction
> rather than by a role test.

**Verified.** `brawlerRingout::kMaxSpawnPoints` is `8` (raised from 4 by og-brawler-uploadtosteam T18, 2026-09-29; the release still matters, because churn past 8 concurrent characters would exhaust it the same way).

**The consequence.** Without the release, after `kMaxSpawnPoints` (8) joins in a session every later character is handed
`kNoFreeSlot` and gets no respawn point.

---

## G-75 — the systems executor is emplaced beside the adapters, and hands detection the same body adapter the integration layer uses

**Tag site:** `SimulationManagerUImpl.h`, directly above `std::optional<BrawlerSystemsExec> m_systemsExec;`.
The two emplace sites it governs are in `SimulationManagerUImpl.cpp`, one per role branch, each between
`m_integrationLayer.emplace(...)` and `m_manager.emplace(...)`.

**The prohibition.** `BrawlerHitDetectionSystem` (`brawlerHitDetection::System<physicsBackendUImpl::BodyAdapter,
physicsBackendUImpl::QueryAdapter>`, the backend's types from `PhysicsBackendUImpl.h`) is constructed
from `*m_physAdapter` and `*m_queryAdapter`, the SAME two objects `m_integrationLayer` integrates with.
⛔ Never from `m_physReaderAdapter` (a `physicsBackendUImpl::ReaderAdapter`), and never before the
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

---

*G-76, G-77 moved to `ChaosBackendUImpl-guards.md` as G-08, G-09 (og-simulationscheduler-withjolt task 82), with the Chaos host's code. See §R.*

---

## G-78 — a refused `SimCharacterId` allocation is final: never wrap, reset or fall back to another id source

**Site:** `if (simId == SimCharacterId::None)` in `allocateSimCharacterId`.

**The prohibition** (task 25, user ruling R2 2026-09-26): when `m_simCharacterIds` is exhausted
(255 ids issued by this manager), the registration is REFUSED. The branch logs an Error, fails an
`OG_CHECK` and returns `SimCharacterId::None`. ⛔ Do not "recover" here by:

* resetting or replacing `m_simCharacterIds`;
* wrapping to a low id;
* scanning for an id no longer registered;
* falling back to the component's or pawn's `GetUniqueID()`.

Widening the type or reusing ids is the user's ruling to make when the ceiling is reached.

**The consequence of getting it wrong.** Each of these hands out an id some consumer still
associates with another character. The consumer list is guard G-01 of `SimCharacterId-guards.md`:
storage, the pending-registration record, the spawn-slot table, the score table, the log joins
and the planned hit ledger. A `GetUniqueID()` fallback is also per-process again, so the peers
disagree on the number and every client registers the character under a different id from the
server. That puts back exactly the defect task 25 removed.

**What breaks if the tag moves.** Nothing mechanical checks the branch body. The allocator
itself cannot be reset by assignment, a `static_assert` in `SimCharacterId.h` makes that a compile
error, but a fallback typed in this branch compiles. Deleting the branch takes the tag with it and
orphans this entry.

---

## G-79 — the destructor touches no `UObject`: it clears the static instance table and nothing else

**Site:** the first statement of `~ASimulationManagerUImpl` (the `s_instances[0]` test).

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

---

*G-80, G-81, G-82, G-83, G-84 moved to `JoltBackendUImpl-guards.md` as G-01, G-02, G-03, G-04, G-05 (og-simulationscheduler-withjolt task 83), with the Jolt host's code. See §R.*

## G-85 — `m_backend.onEndPlayBegin()` stays the first statement of `EndPlay`: nothing moves above it

**Site:** `m_backend.onEndPlayBegin();`, the first statement of `EndPlay`.

**The prohibition** (og-simulationscheduler-withjolt task 83; the half of the old G-80 that is typed in this
class since the Jolt host left it). ⛔ Do not move any `EndPlay` statement above `m_backend.onEndPlayBegin()`, do
not add one above it, and do not drop the call. Everything else in `EndPlay` (G-64's resets, the static instance
table and listener unregistration, the post-tick-flush unbind, the backend's `onEndPlayWorld`) runs after it. The
rule is the same in both configurations:
- on Jolt, `onEndPlayBegin` unregisters the frame host's two tick functions and waits for every outstanding step
  task (`JoltBackendUImpl-guards.md` G-01);
- on Chaos it is an empty inline body today; a Chaos teardown that must run first belongs inside it, not above it.

**The consequence of getting it wrong.** On a worker client the frame's step task runs on another thread through
the rest of the frame and reads the coordinator, the tier cache and the manager's state. A statement above the call
tears that state down while the step still runs: a use-after-reset on a worker thread, which nothing reports.

**What breaks if the tag moves.** The tag marks the call. A statement typed above it is typed directly under the
tag. Nothing mechanical checks the order: the race needs a step in flight at `EndPlay`. Deleting the call takes the
tag with it and orphans this entry.

---

## §R — Retired ids

*(The compile-time conversions in the census never held an id — the header had none before task 11 —
so they are recorded there, not here.)*

### G-18 — RETIRED by og-simulationscheduler-withjolt task 81 (2026-10-15): the alias chain moved to `BrawlerCompositionUImpl.h`

**Was:** *"The simulatable-pack aliases are CLASS-scoped"*. Its tag sat in `SimulationManagerUImpl.h`, directly
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

### G-02 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-01

**Was:** *"`bodyName` stores a pointer to a STRING LITERAL, and only a string literal"*. Its tag sat in `SimulationManagerUImpl.h`, above `FRewindPushProbeStashedBody::bodyName`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-01.

### G-51 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-02

**Was:** *"the push-probe verdict read stays at the top of `OnPreSimulate_Internal`, and its `else` stays"*. Its tag sat in `SimulationManagerUImpl.cpp`, above the push-probe verdict read at the top of `FSimulationManagerAsyncCallback::OnPreSimulate_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-02.

### G-52 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-03

**Was:** *"the push probe reads X/R, never P/Q"*. Its tag sat in `SimulationManagerUImpl.cpp`, above the X/R reads in `readLiveBodyForPushProbe`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-03.

### G-53 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-04

**Was:** *"the push probe compares with the production predicate, over the fields the body's wire shape carries"*. Its tag sat in `SimulationManagerUImpl.cpp`, above `pushProbeFieldsSimilar`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-04.

### G-54 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-05

**Was:** *"the push-probe verdict's denominator is `nonInert`, and `VACUOUS` is a third verdict"*. Its tag sat in `SimulationManagerUImpl.cpp`, above `pushProbeVerdictText`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-05.

### G-55 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-06

**Was:** *"`noteResimGrant` is recorded before `prepareResimulation`"*. Its tag sat in `SimulationManagerUImpl.cpp`, above the `noteResimGrant` call in `FirstPreResimStep_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-06.

### G-71 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-07

**Was:** *"the upcoming sim tick comes from the mapper, and the `+ 1` is derived"*. Its tag sat in `SimulationManagerUImpl.cpp`, above `firstUpcomingSimTick` in the Chaos configuration's input hook.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-07.

### G-76 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-08

**Was:** *"the restored body state is WRITTEN onto the particle in `FirstPreResimStep_Internal`, never pushed as a rewind target"*. Its tag sat in `SimulationManagerUImpl.cpp`, above the `writeRestoredBodyState` call in `FirstPreResimStep_Internal`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-08.

### G-77 — RETIRED by og-simulationscheduler-withjolt task 82 (2026-10-15): moved to `ChaosBackendUImpl-guards.md` G-09

**Was:** *"the restored-state write sets only the fields the body's wire shape carries"*. Its tag sat in `SimulationManagerUImpl.cpp`, above the `wireCarriesRotationAndSpin` branch of `writeRestoredBodyState`.

**Why it retired.** Its subject moved. Task 82 moved the Chaos host out of this class into `ChaosBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `ChaosBackendUImpl-guards.md` G-09.


### G-80 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `JoltBackendUImpl-guards.md` G-01

**Was:** *"`EndPlay` waits for every outstanding step task before any teardown"*. Its tag sat in `SimulationManagerUImpl.cpp`, in the Jolt configuration's code: the second statement of `EndPlay`, the frame host's wait for outstanding steps.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into `JoltBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `JoltBackendUImpl-guards.md` G-01.

### G-81 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `JoltBackendUImpl-guards.md` G-02

**Was:** *"the Jolt bind runs under the world mutex, and the game thread never applies occupancy"*. Its tag sat in `SimulationManagerUImpl.cpp`, in the Jolt configuration's code: the `JoltWorldLockScopeUImpl` line of `tryRegister`'s first-call bind.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into `JoltBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `JoltBackendUImpl-guards.md` G-02.

### G-82 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `JoltBackendUImpl-guards.md` G-03

**Was:** *"the shadow world is built by the step world's calls, and every bind and release is mirrored into it"*. Its tag sat in `SimulationManagerUImpl.cpp`, in the Jolt configuration's code: the shadow world's construction in `buildShadowWorld`.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into `JoltBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `JoltBackendUImpl-guards.md` G-03.

### G-83 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `JoltBackendUImpl-guards.md` G-04

**Was:** *"the render apply teleports the capsule: no sweep"*. Its tag sat in `SimulationManagerUImpl.cpp`, in the Jolt configuration's code: the actor move (the engine's SetActorLocationAndRotation) in `applyRenderSnapshot_GameThread`.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into `JoltBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `JoltBackendUImpl-guards.md` G-04.

### G-84 — RETIRED by og-simulationscheduler-withjolt task 83 (2026-10-15): moved to `JoltBackendUImpl-guards.md` G-05

**Was:** *"a client never picks a Jolt slot itself: it binds the slot the authority chose, or waits"*. Its tag sat in `SimulationManagerUImpl.cpp`, in the Jolt configuration's code: the `decideBodySlot` call in `tryAcquireJoltSlot`.

**Why it retired.** Its subject moved. Task 83 moved the Jolt host out of this class into `JoltBackendUImpl`, and the guarded code went with it. The entry moved with its text unchanged apart from its site and cross-references. **Successor:** `JoltBackendUImpl-guards.md` G-05.
