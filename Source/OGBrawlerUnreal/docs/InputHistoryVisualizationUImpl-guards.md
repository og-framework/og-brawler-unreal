<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: UGameInstance::AddLocalPlayer -- Unreal Engine method (GameInstance.cpp), outside every scan root; quoted in the R0 correction of the index-0 claim -->
<!-- lint-external-ref: UGameInstance::RemoveLocalPlayer -- Unreal Engine method (GameInstance.cpp), outside every scan root; its RemoveAt is why the local-player array is not append-only -->
<!-- lint-external-ref: AddUnique -- Unreal Engine TArray method, outside every scan root -->
# `InputHistoryVisualizationUImpl.h` and `.cpp` — guards

These are the prohibitions left after the comment conversion of the two files (og-netcode-v2-field-defects task 25,
2026-09-26). Each one is reached by a one-line `⛔G-nn` tag at the line where the wrong edit would be typed.
The reasoning behind them lives in `InputHistoryDisplay-rationale.md`, and §8 of that document covers this pair.
Each entry opens with the text the source carried, verbatim, then says what the conversion verified or corrected.

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
remember. The manager calls `m_inputHistory.forgetCharacter(id)` once, in `unregisterFromNewFramework`. A store the
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
this argument is in `SimulationManagerUImpl-rationale.md` ("The presence test crosses nothing either").

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
holds, for a narrower reason. The project's own removal path, `AOGBrawlerPlayerController::LeaveLocalPlayer`, refuses
the controller whose player controller id is 0, so a removal only ever shifts entries *after* index 0.

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

## §R — Retired ids

None.
