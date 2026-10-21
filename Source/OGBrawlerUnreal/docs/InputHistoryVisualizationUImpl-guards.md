<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: UGameInstance::AddLocalPlayer -- Unreal Engine method (GameInstance.cpp), outside every scan root; quoted in the R0 correction of the index-0 claim -->
<!-- lint-external-ref: UGameInstance::RemoveLocalPlayer -- Unreal Engine method (GameInstance.cpp), outside every scan root; its RemoveAt is why the local-player array is not append-only -->
<!-- lint-external-ref: AddUnique -- Unreal Engine TArray method, outside every scan root -->
# `InputHistoryVisualizationUImpl.h` and `.cpp` — guards

These are the prohibitions left after the comment conversion of the two files (og-netcode-v2-field-defects task 25,
2026-09-26). Each one is reached by a one-line `⛔G-nn` tag at the line where the wrong edit would be typed.
The reasoning behind them lives in `InputHistoryDisplay-rationale.md`, and §8 of that document covers this pair.
Each entry opens with the text the source carried, verbatim, then says what the conversion verified or corrected.
G-09 to G-17 moved in from `SimulationManagerUImpl-guards.md` (its G-05 to G-13) with the input-history feeds, which
og-ue-simulationcomponentandmanagersplit task 22 (2026-10-21) moved into `BrawlerInputHistoryFeedUImpl` in these two
files. They keep that file's format: the fence verbatim, what breaks, and the R0 line task 22 added.

---

## G-01 — the relay-health bar is tied to the nearest stack here, and nowhere else

**Site:** `selection.relayHealth    = isNearestStack && relayHealthEnabled();` in `barSelection`.

**The prohibition, as it stood in the source:**

> ⛔ THE ONLY PLACE THE RELAY BAR IS TIED TO A STACK. The primary follows a character
> this client controls, which resolves no relayed input at all.

and, at the declaration:

> `isNearestStack` is the one thing the toggles alone cannot say: the relay-health bar
> belongs to the stack following someone else's character and to no other, so the answer
> differs per stack. ⛔ THE ONLY PLACE THAT TIE IS MADE.

**Consequence.** Dropping `isNearestStack &&` draws a relay-health bar on the primary stack. That bar can only ever
read "no relay", because a character this client controls has a local input provider and resolves no relayed input.
Moving the tie to the HUD (for example, zeroing the bit there) makes a second place that decides it, and the two can
disagree.

**Verified (R0).** `OGBrawlerUEHUD.cpp` calls `barSelection(false)` for the primary and `barSelection(true)` for the
nearest stack, and it never writes `relayHealth` itself.

---

## G-02 — `anyBarEnabled` asks the stack that can draw the most bars

**Site:** `return brawlerInputHistoryVisualization::frameMeterEnabledBarCount(barSelection(true)) != 0u;` in
`anyBarEnabled`.

**The prohibition, as it stood in the source:**

> ⛔ ASKED OF THE STACK THAT CAN DRAW THE MOST BARS: a session running the relay bar
> alone still needs the lane poll that feeds it.

**Consequence.** Passing `false` here makes a session with only `OGBrawler.InputHistoryRelayHealth` on report "no bar".
`USimmableUpdateComponent::TickComponent` gates the lane poll on `anyBarEnabled()` (`feedAnyBar`), and
`AOGBrawlerUEHUD::DrawHUD` gates the whole frame-meter branch on it. So the relay-health bar would never be fed and
never drawn.

---

## G-03 — every display-toggle accessor folds the master in

**Site:** `return GInputHistory && GInputHistoryDisplay;` in `displayEnabled`. That is the first of the six folding
accessors. The other five are `provenanceEnabled`, `inputDelayEnabled`, `characterStateEnabled`, `relayHealthEnabled`
and `nearestStackEnabled`.

**The prohibition, as it stood in the source:**

> ⛔ THE ACCESSOR BLOCK -- THE ONLY PLACE ANY `G*` DISPLAY BOOL IS READ. Folding the
> master in HERE, once each, is what makes "every site looks at it" true by construction.

and, at the declarations: *"⛔ FOLDS THE MASTER IN: false whenever the master is off, whatever this reads."* and
*"⛔ FOLDS THE MASTER IN, same as every accessor on this page."*

**Correction (R0).** "Same as every accessor on this page" was false. The knob accessors `pauseLanesWhileIdle`,
`retainedLaneTicks`, `panelScale`, `panelBackgroundAlpha` and `panelVisibleRows` do not fold the master, and never
did. Only the six display toggles fold it. The knobs are read only on paths the toggles have already gated.

**Consequence.** A toggle accessor that returns its child bool without `GInputHistory &&` turns its display on at
the shipped defaults: every child defaults to `true`, and only the master defaults to `false`. Call sites never read
`GInputHistory` themselves, which is the design (§7.10 of the rationale), so no second check would catch it.

**What the compiler already holds.** Every `G*` variable is in the `.cpp`'s anonymous namespace, so no other
translation unit can read one directly. The fold is what this entry protects.

**Scope of the tag (honest).** The tag covers `displayEnabled`'s return only. Dropping the fold from one of the other
five, or adding a new toggle accessor without it, is typed elsewhere in the same block.

---

## G-04 — one map, one key, for both stores

**Site:** `std::unordered_map<unsigned int, CharacterHistory> m_byId;` in `InputHistoryStore`.

**The prohibition, as it stood in the source:**

> ONE key for both stores, so nothing can survive an unregister the other honoured.

**Consequence.** Splitting the rows and the lanes into two maps opens a second erase that `forgetCharacter` must
remember. The manager calls the feed's `forgetCharacter(id)` once, in `unregisterFromNewFramework`, and the feed
forwards it to `m_inputHistory.forgetCharacter(id)` (task 22; the manager erased the store directly before). A store the
erase misses keeps a ring keyed on a character that has gone. Since task 25, character ids are never reused within
an authority manager's lifetime, so that entry is a pure leak rather than a collision.

---

## G-05 — the reported distance comes off the same candidate list the choice was made from

**Site:** `if (const std::optional<float> distance =` in `nearestCharacterIdTo` (the `.cpp`).

**The prohibition, as it stood in the source:**

> ⛔ OFF THE SAME LIST THE CHOICE WAS MADE FROM. A second gather could be taken a
> frame later and would report a range the choice was not made at.

and, at the declaration: *"⛔ AN OUT-PARAM RATHER THAN A SECOND CALL."*

**Consequence.** A second `gatherNearestCandidates` (or a second call from the HUD) can observe moved characters. The
header would then print a `d=` that does not belong to the character the stack follows.

---

## G-06 — the presence test stays a presence test

**Site:** `return m_reconciliation.template findCorrectionCache<SimulatableT>(m_id) != nullptr;` in
`ReconciliationSlotReader::hasCorrectionCache`.

**The prohibition, as it stood in the source:**

> ⛔ SAME-THREAD, NOT A CROSSING -- the cache map is mutated on the GAME THREAD alone
> (createCacheFor/removeCacheFor); a presence test, not the input read the fence bars.

**Verified (R0).** `createCacheFor` is called only from `registerSimulatable`, and `removeCacheFor` only from
`unregisterSimulatableContainers`, through the `unregisterSimulatable` facade. Both are reached from
`ASimulationManagerUImpl::tryRegister` / `unregisterFromNewFramework` on the game thread. The reader is called from
the lane poll, which `USimmableUpdateComponent::TickComponent` runs on the game thread. The manager's own side of
this argument is in `InputHistoryDisplay-rationale.md` §8.6 ("The presence test crosses nothing either"), which task 22
moved there from `SimulationManagerUImpl-rationale.md` §1 with the feed.

**Consequence.** Turning this into a read *through* the found cache (its states or inputs) is a game-thread read of
physics-thread-written slots. That is a crossing this reader was argued free of.

---

## G-07 — the first local player is index 0

**Site:** `ULocalPlayer* localPlayer = gameInstance->GetLocalPlayers()[0];` in `firstLocalPlayerController`.

**The prohibition, as it stood in the source:**

> GetLocalPlayers() is append-only, and LeaveLocalPlayer already refuses to remove
> the primary local player.
> ⛔ INDEX 0 IS THEREFORE THE FIRST-JOINED LOCAL PLAYER, deterministically.

**Correction (R0).** "Append-only" was false. `UGameInstance::AddLocalPlayer` appends (`AddUnique`), but
`UGameInstance::RemoveLocalPlayer` erases with `RemoveAt`, which shifts every later entry down. The conclusion still
holds, for a narrower reason. The project's own removal path, `AOGBrawlerPlayerController::LeaveLocalPlayer`, never
removes local player 0. Since og-brawler-uploadtosteam task 13 it always removes the LAST local player (whichever local
controller received the leave key, End since og-attackstatetransition-cleanup task 13), so a removal shifts no entry at all.

**Consequence.** Picking another index, or iterating every local player, breaks the one-HUD rule.
`AOGBrawlerUEHUD` draws only when its owning controller equals this function's answer, so each couch co-op sibling's
HUD would stack the same panel. It also moves the row panel's selection to a different character.

---

## G-08 — `masterEnabled` reads the raw master, alone

**Site:** `return GInputHistory;` in `masterEnabled`.

**The prohibition, as it stood in the source:**

> THE MASTER GATE -- `OGBrawler.InputHistory`, DEFAULT OFF. Read ALONE, once, by the
> poll's early-out; every accessor below also folds it in, so no call site can forget it.

**Consequence.** `USimmableUpdateComponent::TickComponent`'s input-history block returns on `!masterEnabled()` before
it reads any child. Folding a child in here (for example `GInputHistory && GInputHistoryDisplay`) makes that early-out
fire whenever the row panel is off. The lane poll behind it would then never run for a session that wants only the
bars. The second half of the quoted sentence is corrected under G-03.

---

## G-09 — The prediction-offset read keeps its `runsPrediction()` guard

**Moved from** `SimulationManagerUImpl-guards.md` G-05 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above the `(m_session.getManager().has_value() && m_session.getManager()->runsPrediction())` predicate of `predictionOffsetTicks` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 549-549 at b9f6d81 -->
```
// ⚠ getNetworkEstimator() exists only on a predicting role, hence the guard. §5
```

**What breaks if the edit is made.** `SimulationManager::getNetworkEstimator()` calls `std::terminate()` on a manager that holds no estimator — every authority manager. And the guard's `std::nullopt` is itself the signal: an authority passes no offset rather than `0`, because a display told "the offset is 0" draws an authority marker on the newest cell (`InputHistoryDisplay-rationale.md` §8.6).

**Verified (R0, task 22).** `SimulationManager::getNetworkEstimator()` calls `std::terminate()` when `m_networkEstimator` is empty, and the manager's constructor emplaces the estimator (and the client clock) only on its `shouldRunPrediction` branch, so every authority manager holds none.

---

## G-10 — Local control is the CAPTURE-LINE test, never a role test

**Moved from** `SimulationManagerUImpl-guards.md` G-06 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above `const bool isLocallyControlled = m_session.isLocallyControlledOnThisPeer(id);` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 569-571 at b9f6d81 -->
```
// The same test `pollInputHistory` makes, and for the same reason: a capture line
// exists for exactly the characters this client controls. ⛔ NOT A ROLE TEST -- a
// client can control several brawlers, and a listen-server host controls one. §1
```

**What breaks if the edit is made.** `runsPrediction()`, `HasAuthority()` and the net mode answer per MANAGER, not per character. A couch-co-op client drives several brawlers, and a listen-server host drives one on an AUTHORITY manager, so a role test gets both cases wrong. Everything after this line branches on the answer: the tier decomposition (G-11) and the two relayed-input sources are chosen from it, so a wrong answer draws the local connection's delay on a remote proxy, or blinds a local character's delay bar.

**Verified (R0, task 22).** `isLocallyControlledOnThisPeer` answers `getLocalInputCache(id) != nullptr`, and the capture line exists only for an id with a local input provider (`SimulationInputResolution::registerLocalCharacter`), which is per character, not per manager.

---

## G-11 — The tier decomposition is drawn only for a LOCALLY controlled character

**Moved from** `SimulationManagerUImpl-guards.md` G-07 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above the `(includeDelay && isLocallyControlled ...` predicate of `delay` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 574-575 at b9f6d81 -->
```
// A remote proxy's stack shows the relay it is being predicted from instead.
// ⛔ THE TIER DECOMPOSITION IS THE LOCAL CONNECTION'S and is not read for a remote.
```

**What breaks if the edit is made.** `decomposeInputDelay` reads THIS connection's tier consumer, the shared `TimeConfig` and the resolution peer's published delay. For a remote proxy none of those describe the proxy's wire. Drop `isLocallyControlled` from the predicate and a remote proxy's delay bar is labelled with the viewer's own tier and delay — plausible-looking numbers that describe the wrong connection.

**Verified (R0, task 22).** The three inputs are the tier consumer's `hasReceivedTier`, `currentTierIndex` and `effectiveInputDelayTicks`, the manager's `getTimeConfig()` and the resolution peer's `getClientEffectiveInputDelayTicks()`: this connection's state, read in the predicate's true arm only.

---

## G-12 — The clock reading keeps its `runsPrediction()` guard

**Moved from** `SimulationManagerUImpl-guards.md` G-08 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above `if (m_session.getManager().has_value() && m_session.getManager()->runsPrediction())` around the `ClockDriftReading` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 588-588 at b9f6d81 -->
```
// ⛔ GUARDED LIKE THE OFFSET ABOVE: getClientClock() std::terminates on a server. §5
```

**What breaks if the edit is made.** The block calls both `getClientClock()` and `getNetworkEstimator()`, and both call `std::terminate()` on a manager that does not predict. The guard is what makes the reading absent, rather than fatal, on every authority and standalone manager.

**Verified (R0, task 22).** Both `SimulationManager::getClientClock()` and `getNetworkEstimator()` call `std::terminate()` on an empty optional, and both optionals are emplaced only on the constructor's `shouldRunPrediction` branch.

---

## G-13 — Each event counter is read BEFORE its tick

**Moved from** `SimulationManagerUImpl-guards.md` G-09 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above the first of the seven `predictionClock.getDiagnostics()` reads in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes` (`reading.skipCount`).

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 606-606 at b9f6d81 -->
```
// ⛔ COUNT BEFORE ITS TICK: the clock writes tick then count, so the tear is one-way.
```

**What breaks if the edit is made.** `ClientPredictionClock` writes each pair tick-first, count-second (`ClientPredictionClock.cpp`: `lastSkipTick` then `++skipCount`, and likewise for stall and hard resync). Reading count-first makes the harmful straddle — a fresh count beside a stale tick, which files an event against a tick that predates it — the one the program order does not produce. Reversing the reads makes it the likely one.

⛔ **R0, task 11 — the fence overclaims, and the rationale doc's copy said "CANNOT be observed".** Both sides are plain `unsigned int`s: neither the writes nor the reads are atomic, so C++ gives no ordering guarantee at all, and an optimizing compiler may reorder either pair. Source order makes the harmful straddle unlikely; it does not make it impossible. The guard is still worth keeping — reversing the order makes the wrong case the expected one — but the property it protects is "unlikely", not "unreachable". A real guarantee needs acquire/release atomics in `og-simulation`, which is not this file's to change.

The line above the fence — *"THE SEAM, THROUGH THE VIEW: the clock publishes no bare accessor"* — is not a guard. The compiler already holds it (task 11 measured `c.skipCount()` → `C2039` and `c.m_eventSeamDiagnostics` → `C2248`).

**Verified (R0, task 22).** All four write sites are in `ClientPredictionClock::advancePrediction`, and each writes its tick (the hard resync: both ticks) before `++` on its count. The seven fields are plain `unsigned int`s of `EventSeamDiagnostics`, read through `getDiagnostics()`. The task-11 R0 paragraph above stands.

---

## G-14 — The relay-health window is `TimeConfig::rollbackWindowTicks`, not the per-tier ceiling

**Moved from** `SimulationManagerUImpl-guards.md` G-10 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above the `(m_session.getManager().has_value() && m_session.getManager()->getTimeConfig().rollbackWindowTicks > 0)` predicate of `rollbackWindowTicks` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 633-637 at b9f6d81 -->
```
// How far back a resim may still reach, which is what separates an arrival that could
// still have been replayed into a tick from one that could not.
// The per-tier ceiling beside it escalates nothing today and its own banner says so, and
// a negative value disables the authority's future guard and is no window at all here.
// ⛔ THE SHIPPED WINDOW, `TimeConfig::rollbackWindowTicks`. §5
```

**What breaks if the edit is made.** The window separates an arrival that could still have been replayed into its tick from one that could not, so it must be the window a resim actually reaches. The per-tier ceiling beside it, `TimeConfig::rollbackWindowHardCap`, escalates nothing today — its own declaration says the escalation is *"INTENDED, NOT IMPLEMENTED"*. Substitute it and the bar sorts arrivals against a number no resim uses. The `> 0` test is part of the same fence: a negative value is the authority's "future guard disabled" setting and is no window at all here, so it maps to `0`.

⛔ **Correction (R0, task 22).** The paragraph above names the per-tier ceiling `TimeConfig::rollbackWindowHardCap`. That field is a single degraded-mobile ceiling, not a per-tier one, and it is read on both roles (the correction cache's log and release gates, the floor's derived ceiling, the reception coordinator's staleness bounds), so "a number no resim uses" does not describe it either. The per-tier ceiling the fence meant is `TimeConfig::rttTierRollbackCeilings`, read through `ReplicatedTierConsumer::effectiveRollbackCeiling`, whose declarations say "INTENDED API, NO PRODUCTION CALLER". The prohibition is unchanged and covers both: the window is `rollbackWindowTicks`. The `> 0` half is verified: the server's too-far-future guard runs only when `rollbackWindowTicks >= 0` (`SimulationQueues.h`, `queueMove`), so a negative value disables it.

---

## G-15 — The relay-health bar is NOT gated on `includeDelay`

**Moved from** `SimulationManagerUImpl-guards.md` G-11 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above `if (relayedReads != nullptr && relayedArrivals != nullptr)` in `BrawlerInputHistoryFeedUImpl::pollInputHistoryLanes`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 647-650 at b9f6d81 -->
```
// The relay-health bar rides the same two sources and is its own toggle, so the delay bar
// being off must not blind it; `includeDelay` still decides the tier decomposition above,
// which is the reading it actually names.
// ⛔ NOT GATED ON `includeDelay`.
```

**What breaks if the edit is made.** The relay-health bar has its own toggle and rides the same two relayed sources as the delay bar's client half. Add `includeDelay` to this condition and turning the delay bar off silently blinds the relay-health bar too. `includeDelay` still decides the tier decomposition (G-11), which is the reading it actually names.

**Verified (R0, task 22).** The condition is the two relayed-ring pointers alone; both are `nullptr` exactly when the character is locally controlled or has no ring yet.

---

## G-16 — The readout's `dLatest` stamp comes from the NEWEST observation by tick, never the last slot visited

**Moved from** `SimulationManagerUImpl-guards.md` G-12 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above `if (!anyObservation || observation->simTick > newestSimTick)` in `BrawlerInputHistoryFeedUImpl::getRelayReadReadout`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 716-718 at b9f6d81 -->
```
// The stamp of the NEWEST observation, found by its own tick: the ring is addressed by
// sim tick and therefore walked out of order, so the last slot read is not the last one
// written. ⛔ NEVER THE LAST SLOT VISITED.
```

**What breaks if the edit is made.** The ring is addressed by sim tick, so walking it by index visits slots out of write order. "Last slot visited" is therefore an arbitrary observation, and the nearest stack would print a schedule stamp from whichever tick happened to sit in the highest index.

**Verified (R0, task 22).** `RelayedReadObservationRing` stores each observation at `simTick % kRelayedReadObservationCapacityTicks`, so slot order is not write order.

---

## G-17 — The nearest-character gather reads the VIZ snapshot, never live state

**Moved from** `SimulationManagerUImpl-guards.md` G-13 (og-ue-simulationcomponentandmanagersplit task 22, 2026-10-21), with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `InputHistoryVisualizationUImpl.cpp`, directly above `const auto& movement = simulatable.getVizState().getState()...` inside `BrawlerInputHistoryFeedUImpl::gatherNearestCandidates`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

<!-- header lines 736-738 at b9f6d81 -->
```
// `updateVisualizationAll(m_storage)` is the sanctioned physics->game handoff, and a HUD
// reading live state would be a fresh, unargued crossing.
// ⛔ READ OFF THE VIZ SNAPSHOT, NOT `getAllState()` -- the ring-out slice's reason. §1
```

**What breaks if the edit is made.** Same reason as `SimulationManagerUImpl-guards.md` G-04 (the scoreboard's slice read), at a second site: `getVizState()` is the copy taken by `updateVisualizationAll`, and reading the live composite from a per-frame HUD gather would be a new game-thread read of physics-written state that no crossing argument covers.

**Verified (R0, task 22).** The gather reads `getVizState()` inside `forEachSimulatable` on the manager's `const` storage; `getAllState()` is the live composite.

---

## §R — Retired ids

None.
