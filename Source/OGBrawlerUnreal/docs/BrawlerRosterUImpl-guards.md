<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: seedRingoutSpawnPointsFromLevel -- retired name: became BrawlerRosterUImpl::seedSpawnPointsFromLevel (og-ue-simulationcomponentandmanagersplit task 23); named in the Moved-from lines -->
# `BrawlerRosterUImpl.h` and `.cpp` — guards

These are the prohibitions of the brawler roster: the simulation-character id allocator, the ring-out spawn slots, the
pre-diet cap and its id set, the ring-out spawn-point seed and the score push. Each one is reached by a one-line
`⛔G-nn` tag at the line where the wrong edit would be typed. The reasoning behind them lives in
`BrawlerRosterUImpl-rationale.md`.

All five moved in from `SimulationManagerUImpl-guards.md` with the code, when og-ue-simulationcomponentandmanagersplit
task 23 (2026-10-21) moved the roster out of `ASimulationManagerUImpl`. They keep that file's format (the fence or
prohibition verbatim, then what breaks), with their site and cross-references updated and one R0 line each from task 23.

| here | site | was, in the manager's guards |
|---|---|---|
| G-01 | the declaration of `seedSpawnPointsFromLevel` | G-14 |
| G-02 | the refusal branch of `allocateSimCharacterId` | G-78 |
| G-03 | the spawn-slot write in `onBodiesCreated` | G-67 |
| G-04 | the authority block of `onRegistered` | G-68 |
| G-05 | the spawn-slot release in `forgetCharacter` | G-74 |

**If this file and the source disagree, the source is authoritative and this file is stale.** ⛔ An id is never reused:
a guard that is deleted, or that becomes a compile-time or run-time check, moves to a retired section here and its
number is spent.

---

## G-01 — `seedSpawnPointsFromLevel` is a DELIBERATE post-construction write of `StaticData`

**Moved from** `SimulationManagerUImpl-guards.md` G-14 (og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21),
with the code; the text is that entry's, with its site and cross-references updated. Until then the function was the
manager's private `seedRingoutSpawnPointsFromLevel`, taking only the world.

**Tag site:** `BrawlerRosterUImpl.h`, directly above the declaration of `seedSpawnPointsFromLevel`.

**The fence, verbatim — the bytes it occupied in `SimulationManagerUImpl.h` before task 11:**

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

⚠ **R0, task 11 — two positional words in the fence are wrong.** *"The banner above states StaticData as constructed once and never moved"*: no banner above this declaration says that; the statement stood at `m_staticData`, BELOW it. *"`readMovementStaticDataCVars()` above"*: that declaration is BELOW this one too. (`SimulationManagerUImpl-rationale.md` §11 C12.) Since task 23 neither member is in this class at all; both stay in `ASimulationManagerUImpl`.

⛔ **R0, corrected after task 12's review — the fence's first sentence also MISATTRIBUTES (`SimulationManagerUImpl-rationale.md` §11 C19).** *"What that discipline actually protects is that NO TICK EVER SEES IT CHANGE"*: the construct-once, never-copied-or-moved discipline protects `StaticData`'s **internal sibling references**, and it does not forbid a write — `StaticData` deletes copy and move, not assignment to its members. "No tick sees it change" is a separate, true property, and it is what the `checkf`s hold.

⚠ **R0, task 23 — "the `checkf`s at the definition hold that, mechanically" now needs the caller's half.** The roster
holds no reference to the actor (review m-16), so the second `checkf` reads its `compositionExists` parameter. `BeginPlay`
computes it at the call as `m_manager.has_value() || m_integrationLayer.has_value()`, and passes `m_staticData` as the
table to write. *Since og-ue-simulationcomponentandmanagersplit task 27b* the session's `beginPlay` computes both, from its
own members, and passes them to the brawler's hook `beforeWorldBuild`, which passes them on unchanged. A call that passed a literal `false`, or a copy of the table, would compile and disarm the check or seed
nothing. The `m_spawnSlots` the fence calls "above" is now this class's member, declared after the seed's declaration.
Verified: `m_staticData` is a default member initializer of `UESimulationSession` (`StaticData m_staticData{ Game::makeStaticData() };`)
since og-ue-simulationcomponentandmanagersplit task 24a, held in the actor's member `m_session`, so it is still built during the
actor's construction (the sentence two paragraphs up holds); the integration executor still stores a `const StaticDataT&`
to it (`SimulationIntegrationExecutor.h`), and the eight `Ringout.SpawnPoints.*` cases exist in
`BrawlerRingoutSimulationTest.cpp`. *R0, task 27b rework 1:* re-verified against code: the session's initializer and the
actor's `m_session` member; the executor's `const StaticDataT& m_staticData` member; eight `Ringout.SpawnPoints.*`
`TEST_CASE`s. Until this rework the sentence said the initializer was `ASimulationManagerUImpl`'s (false since task 24a). That `APlayerStart` actors are unreachable at actor construction is
engine behaviour and was not re-measured.

---

## G-02 — a refused `SimCharacterId` allocation is final: never wrap, reset or fall back to another id source

**Moved from** `SimulationManagerUImpl-guards.md` G-78 (og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21),
with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `if (simId == SimCharacterId::None)` in `BrawlerRosterUImpl::allocateSimCharacterId`.

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

⚠ **R0, task 23.** "255 ids issued by this manager" is still true: the allocator is a member of this roster, and the
roster is a member of `ASimulationManagerUImpl`, so its lifetime is the manager's (`SimulationManagerUImpl-rationale.md`
§15). Verified: `SimCharacterIdAllocator::kLastAssignable` is the `uint8_t` maximum, 255, and `SimCharacterId::None` is 0;
the `static_assert` that the allocator is neither copyable nor assignable is in `SimCharacterId.h`; the six consumers
listed above are the six bullets of that guard. ⚠ **"the planned hit ledger" is stale there and here**: that guard says the
ledger shipped (netcode task 27, `dAttackRadialSimulation::State::hitTargets`). The client-world `checkf` now reads the
`ENetMode` its caller passes; the component passes `regManager->GetNetMode()`, the manager's own mode, as before.

---

## G-03 — the spawn slot is written through from `acquire`, never clamped and never derived per peer

**Moved from** `SimulationManagerUImpl-guards.md` G-67 (og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21),
with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `ringoutIC.spawnSlot = m_spawnSlots.acquire(id);` in `BrawlerRosterUImpl::onBodiesCreated`, which
`ASimulationManagerUImpl::tryRegister` calls on its first-call pass.

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
`SimulationManagerUImpl-rationale.md` §13.10, correction C13-9.

**Already partly enforced.** `kNoFreeSlot`'s *value* is pinned by a `static_assert` at its
declaration. That assert says in its own message that it exists "instead of needing a second
guard at the UE call site". Nothing stops a clamp being typed *here*, and that is what this tag
is for.

⚠ **R0, task 23.** Verified: `SpawnSlotAllocator::kNoFreeSlot` is `kMaxSpawnPoints`, pinned by its `static_assert`, the
out-of-range branch of `brawlerRingout::integrate` logs `[Warning][Ringout.spawnSlot]` and writes no seed, and §11a
exists. `GetPlayerSlotForActor`'s behaviour is engine behaviour and was not re-measured. The harness shows the
written-through value on every session: the server logs `tryRegister: ring-out spawnSlot=…` per character.

---

## G-04 — the pre-diet cap warns once per over-cap character, at `Warning`, and never asserts

**Moved from** `SimulationManagerUImpl-guards.md` G-68 (og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21),
with the code; the text is that entry's, with its site and cross-references updated.

**Site:** the `if (isAuthority)` block of `BrawlerRosterUImpl::onRegistered` that inserts into `m_authorityRegisteredIds`
(`tryRegister` calls it with `record.isAuthority`, at the statement position the block held).

**The prohibition, as it stood in the source:**

> THE PRE-DIET CAP FENCE, runtime half. Deleted with kPreDietCharacterCap by the diet. §10
>
> ⛔ ONCE PER OVER-CAP CHARACTER: a per-session latch would go silent after the fifth.
>
> ⛔ WARNING, not Log, and not an assert: an over-cap session still RUNS - report, do not crash. §3

**The consequence.** A latch stops reporting after the fifth character. An assert takes the
server down mid-brawl.

⚠ **R0, task 23.** The quote's `§10` is `SimulationManagerUImpl-rationale.md` §10, whose cap subsection is now this file's
rationale, *The pre-diet character cap* (∴D-01 there). Its `§3` is the old source's own numbering and resolves to nothing
in either rationale doc. *"Deleted with kPreDietCharacterCap by the diet"* describes the future: nothing owns that
deletion today (og-netcode-v2-input-relay item 40 closed on 2026-10-03 without doing it). Verified: the line is a
`LogOGNet` `Warning`, one per registration past the cap, with no latch. It is not exercised by the harness, which never
has more than three characters.

---

## G-05 — unregistration releases the ring-out spawn slot, ungated

**Moved from** `SimulationManagerUImpl-guards.md` G-74 (og-ue-simulationcomponentandmanagersplit task 23, 2026-10-21),
with the code; the text is that entry's, with its site and cross-references updated.

**Site:** `m_spawnSlots.release(id);` in `BrawlerRosterUImpl::forgetCharacter`, which
`ASimulationManagerUImpl::unregisterFromNewFramework` calls after the write probe's forget and before the
input-history forget.

**The prohibition, as it stood in the source:**

> Same unregister contract for the ring-out spawn-slot table (task 3). ⛔ WITHOUT THIS a
> session that churns characters exhausts a four-entry table and every later join is handed
> the out-of-range value, respawning nowhere. UNGATED for the same reason the erase above is:
> nothing on the client role ever acquired, so `release` is a no-op there by construction
> rather than by a role test.

**Verified.** `brawlerRingout::kMaxSpawnPoints` is `8` (raised from 4 by og-brawler-uploadtosteam T18, 2026-09-29; the release still matters, because churn past 8 concurrent characters would exhaust it the same way).

**The consequence.** Without the release, after `kMaxSpawnPoints` (8) joins in a session every later character is handed
`kNoFreeSlot` and gets no respawn point.

⚠ **R0, task 23.** "The erase above" is still the statement directly above: `m_authorityRegisteredIds.erase(id)`, the
first statement of `forgetCharacter`. Verified: `SpawnSlotAllocator::release` walks the table and returns without a write
when no entry holds the id, so a client's release is a no-op. ⚠ *"a four-entry table"* is stale since T18 (the
**Verified** line above says so). The harness shows the release working: the rejoining character is handed the slot the
departed one held (`spawnslots23.py`, task 23's notes).
