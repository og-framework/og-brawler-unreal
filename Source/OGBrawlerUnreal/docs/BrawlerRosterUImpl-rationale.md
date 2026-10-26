<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: OnPhysScenePreTick -- Unreal Engine delegate (ChaosScene.h), outside every scan root; this module stopped binding it in og-simulationscheduler-withjolt task 51 (§1 quotes the old source) -->
<!-- lint-external-ref: OnPhysSceneStep -- Unreal Engine delegate (ChaosScene.h), outside every scan root; this module stopped binding it in og-simulationscheduler-withjolt task 51 (§1 quotes the old source) -->
<!-- lint-external-ref: FastLess -- Unreal Engine FName comparator, outside every scan root -->
<!-- lint-external-ref: FNameFastLess -- Unreal Engine FName comparator, outside every scan root -->
<!-- lint-external-ref: CompareIndexes -- Unreal Engine FName method, outside every scan root -->
<!-- lint-external-ref: UE_LOG -- Unreal Engine macro, outside every scan root -->
<!-- lint-external-ref: USimmableUpdateComponent::tryRegisterWithNewFramework -- RETIRED by og-ue-simulationcomponentandmanagersplit task 29: the registration loop is UESimulatableOwnerBinding's skeleton since then; §2 and §3 quote the shipped comments verbatim. It must NOT resolve -->
<!-- lint-external-ref: PendingRegistration::parentBodyId -- ABSENCE FENCE (§2): the record field deleted with the two-source tripwire. It must NOT resolve -->
# `BrawlerRosterUImpl` — rationale

`BrawlerRosterUImpl.h` and `.cpp` (BUSL-1.1, `Source/OGBrawlerUnreal/`). The guards are in `BrawlerRosterUImpl-guards.md`;
a `⛔G-nn` tag in the source points there, and the one `∴D-nn` tag points at the heading in this file that ends with it.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes. The roster is brawler code: it names the
> ring-out sub-simulation, the brawler character and its score, so it stays out of the MPL plugin.

---

## 0. What the roster is, and how it reaches the actor's state (og-ue-simulationcomponentandmanagersplit task 23)

**What it holds.** Everything about *which characters this peer has and where they spawn* that the manager used to hold
itself, moved out of `AOGBrawlerSimulationManager` by task 23 (2026-10-21), by content and in order:

| roster | was, in `AOGBrawlerSimulationManager` |
|---|---|
| `m_simCharacterIds`, `allocateSimCharacterId` | the same names (the allocator member; the public method) |
| `m_spawnSlots` | the same member |
| `kPreDietCharacterCap`, its `static_assert` against `brawlerRingout::kMaxSpawnPoints`, `m_authorityRegisteredIds` and its set `static_assert` | the same names; the set assert sat in `compositionContractsHold` |
| `seedSpawnPointsFromLevel`, `m_ringoutSpawnPointsSeeded` | the private seed method and its flag |
| `checkSeeded` | the seeded `checkf` after both role branches of `BeginPlay` (since task 27b called from the hook `afterSessionConfigured`) |
| `pushRingoutScores` (a static member) | the file-local score push of `OGBrawlerSimulationManager.cpp` |
| `onBodiesCreated` | the per-character init after the bodies: the capsule-id stamp, the teleport seed and the spawn-slot seed of `tryRegister`'s first-call pass |
| `onRegistered` | the pre-diet cap block of `tryRegister`'s Ready path |
| `forgetCharacter` | the two forgets of `unregisterFromNewFramework`: the id-set erase and the spawn-slot release |

**It holds no reference to the actor (review m-16).** Each call takes what its body reads, and the actor computes that at
the statement position the moved code held. Since og-ue-simulationcomponentandmanagersplit task 27b the two start-up calls
are made by the brawler's hooks `BrawlerSessionGameUImpl::beforeWorldBuild` and `afterSessionConfigured`, which pass their
own parameters through; the session's `beginPlay` computes those at the same statement positions, through its owner
(`m_owner.GetNetMode()`) and its own members (`BrawlerSessionGameUImpl-rationale.md` §6):

| call | argument | what the moved body used to read |
|---|---|---|
| `seedSpawnPointsFromLevel(world, netMode, compositionExists, staticData)` | `*uWorld`, `GetNetMode()`, `m_manager.has_value()` or `m_integrationLayer.has_value()`, `m_staticData` | the same world; `GetNetMode()` in the log; `!m_manager.has_value() && !m_integrationLayer.has_value()` in the second `checkf`; `m_staticData`, written in place |
| `checkSeeded(worldNetMode)` | `BeginPlay`'s `worldNetMode` local (since task 27b `beginPlay`'s, through the hook's `netMode`) | the same local, in the `checkf`'s message |
| `allocateSimCharacterId(netMode)` | the component passes `regManager->GetNetMode()` | the manager's own `GetNetMode()`, in the client-world `checkf` |
| `onBodiesCreated(simulatable, character, id, isAuthority, netMode)` | `*record.simulatable`, `*character`, `id`, `record.isAuthority`, `GetNetMode()` | the same record fields and character; `GetNetMode()` in the authority `checkf` |
| `onRegistered(id, isAuthority)` | `id`, `record.isAuthority` | the same |
| `forgetCharacter(id)` | `id` | the same |
| `pushRingoutScores(scoreSystem, routes)` | `m_systemsExec->get<brawlerRingout::ScoreSystem>()` and `m_delayedInputComponentsById` (both the session's; the map since task 26a), both by `const&` | the same two references, handed to the file-local function |

Two arguments are now evaluated where they used to be read lazily: `GetNetMode()` at the `seedSpawnPointsFromLevel` and
`onBodiesCreated` calls is evaluated before the call, also in Shipping, where the `checkf` that read it compiles out, and
`compositionExists` likewise. Both reads are `const` and have no side effect, and the net mode cannot change inside
`BeginPlay` or `tryRegister`, so the values are the ones the old statements read.

**Where it sits, and why it is safe there.** `BrawlerRosterUImpl m_roster;` is the actor's first private data member, the
position the four moved members held (`m_authorityRegisteredIds`, `m_spawnSlots`, `m_simCharacterIds`,
`m_ringoutSpawnPointsSeeded`, in that order, which is also their order inside the roster). So it is constructed first and
destroyed last, as they were, and before `m_inputHistoryFeed` (manager design §4.5: roster, feed, then the hooks and the
session). It reads nothing at construction. The actor reaches it as `m_roster`; the component and any UI reach it through
`roster()`, a mutable and a `const` overload, because allocation mutates the allocator.

**Threads.** Every entry point runs on the game thread, as the code did before it moved: `BeginPlay`, `tryRegister`,
`unregisterFromNewFramework` (the session's since og-ue-simulationcomponentandmanagersplit task 28, through the brawler's
registration hooks), the component's registration attempt and the post-physics pass (the actor's
`OnPostPhysicsStep` until og-ue-simulationcomponentandmanagersplit task 27a; since then the session's
`onPostPhysicsStep`, through the brawler's hook `BrawlerSessionGameUImpl::afterVisualization`). The score push is the one
cross-thread read, and its argument (§3, and `OGBrawlerSimulationManager-rationale.md` §1, "The second read crossing") is
unchanged: the roster adds no member to any crossing.

**Optimization state (lead ruling F1).** Every moved body came from inside `OGBrawlerSimulationManager.cpp`'s
`OGSIM_OPTIMIZE_OFF`/`ON` pair, so `BrawlerRosterUImpl.cpp` wraps all seven definitions in a pair of its own. None is a
template, so the pair keeps them unoptimized in Development, as before; task 23 measured it by disassembly before and
after (its notes). The header defines no function.

**Log text.** Every log line moved byte for byte, so `[Ringout.spawnPoints]`, `tryRegister: ring-out spawnSlot=…`,
`allocateSimCharacterId: …` and `[PreDietCap]` keep their category, level and text (the `tryRegister:` prefix included,
since `tryRegister` is still the only caller). Three fatal-only `checkf` messages name the renamed seed,
`seedSpawnPointsFromLevel`, instead of its old name (design Q7: neutral checkf texts in moved code).

**Section numbers.** The text below moved verbatim from `OGBrawlerSimulationManager-rationale.md`; a bare `§n` or Cnn
inside it is that file's.

---

## 1. The ring-out respawn points come from the level

*Moved from `OGBrawlerSimulationManager-rationale.md` §13.1 (task 23; ruling Q-B). The method is `seedSpawnPointsFromLevel`
since task 23, and the seeded check is `checkSeeded`.*

> ⭐⭐ [ringout task 9, 2026-09-13] THE RESPAWN POINTS ARE THE LEVEL'S `APlayerStart` ACTORS.
>
> USER RULING, 2026-09-13, out of a PIE session: the authored table in
> `brawlerRingout::StaticData` is placeholder authoring at (±200, ±200, Z=200), the platform
> this mode ships on is somewhere else, and every respawn therefore dropped the fighter off
> the map. Respawn slot N is now PLAYER START N, so moving a spawn point is level design.
>
> ══ ⛔⛔ WHY THIS WRITES `StaticData` AFTER CONSTRUCTION, WHICH THE BANNER FORBIDS ══════════
> §2's ownership rule is that `StaticData` is constructed in place once and never moves. What
> that rule PROTECTS is that no tick ever sees it change: a constant the simulation reads
> mid-session is a constant two peers can disagree about and a resim can replay against the
> wrong value. This is a ONE-TIME INIT that happens before the first tick can exist, which is
> inside what the rule protects rather than an exception to it. ⛔ DO NOT "FIX" IT BACK INTO
> A CONSTRUCTOR ARGUMENT: `m_staticData` is brace-initialised in a member initializer, which
> runs during ACTOR CONSTRUCTION, and no level actor is reachable then. `APlayerStart` exists
> at `BeginPlay` and not one moment earlier. (`readMovementStaticDataCVars()` has the same
> shape for the same reason, one member above it.)

⚠ **Correction C13-8.** §2 does not forbid a write. Its rule is that `StaticData` is never
**copied or moved**, and what it protects is the internal references its nested members hold to
their siblings. It says nothing about a value changing mid-session. So the conflict this banner
resolves does not exist. The two facts the banner relies on are still true: the write happens
once, before any tick, and no level actor is reachable at construction.

> ══ ⭐ AND THE ORDERING IS ENFORCED, NOT ASSUMED ═══════════════════════════════════════════
> The two `checkf`s below are the mechanical form of "exactly once, before the first
> integrate". The second is the load-bearing one: every simulation step in this process runs
> through `m_manager`, which is emplaced LATER IN THIS SAME `BeginPlay` (both role branches),
> and the physics delegates that drive it (`OnPhysScenePreTick` / `OnPhysSceneStep` /
> `OnPhysScenePostTick`) plus the async callback are bound at the very END of it. An empty
> `m_manager` is therefore proof that no tick has happened yet. ⚠ It is also why this must not
> drift below the `emplace` calls: `m_integrationLayer` holds a REFERENCE to `m_staticData`,
> so a later write would still be visible and would still compile — the assertion is what
> makes the ordering a stated requirement instead of an accident of line order.
>
> ══ ⛔ BOTH ROLES, DELIBERATELY ════════════════════════════════════════════════════════════
> Unlike `m_spawnSlots` (authority-only bookkeeping), this table is read by
> `brawlerRingout::integrate` on EVERY PEER — a client predicts its own respawn. So a client
> that skipped this would predict to the placeholder point and be corrected on every respawn.
> There is no role branch here and there must not be one.

⭐ **Now a check rather than a sentence.** A `checkf(m_ringoutSpawnPointsSeeded, …)` directly
after the two role branches in `BeginPlay` fails on any peer that did not seed (since task 27b `checkSeeded`, called by the
hook `afterSessionConfigured` after the session's resim-policy apply). A role gate
wrapped around the seed call therefore fails on the first client. It is compiled into this
build, but nobody has run it in PIE to watch it fire (the UE layer has no low-level test target).

> ⭐ ALL THE POLICY IS IN THE ENGINE-FREE CORE. The sort, the fallback and the (0,0,0) guard
> are `brawlerRingout::spawnPointsFromLevelPlacements`, asserted by `Ringout.SpawnPoints.*` in
> `BrawlerRingoutSimulationTest.cpp`. This function walks actors and converts a name to bytes.

⚠ **Correction C13-12.** `spawnPointsFromLevelPlacements` has no `(0,0,0)` check. It sorts, and
it overwrites the first `min(count, kMaxSpawnPoints)` entries of a copy of the authored table.
The only protection against origins is that **zero** placements leave the authored table whole
(`Ringout.SpawnPoints.ZeroPlayerStartsKeepsTheWholeAuthoredTable`).

> ⛔ THE NAME, AS BYTES, AND THIS CONVERSION IS THE WHOLE REASON THE CORE TAKES A
> `std::string`. `FName`'s own ordering (`FastLess` / `FNameFastLess` / `CompareIndexes`)
> compares the NAME TABLE INDEX, which the engine documents as *"only stable during this
> process' lifetime"* - the order the string was first interned in THIS process. A server and
> a client never intern in the same order, so sorting FNames would hand two peers different
> tables while looking exactly like a deterministic sort. `GetName()` is the string saved in
> the map package; every peer that loaded the map has the same bytes.

This needs no tag, because the type enforces it: `brawlerRingout::LevelSpawnPoint::name` is a
`std::string` in an engine-free header that cannot name `FName`.

**Units and axes (og-sim-metres-yup task 8).** Each `APlayerStart` location enters through `ueFrame::vectorToSim`, so the
seeded table holds og-simulation positions, like the authored one it overwrites. The per-slot `[Ringout.spawnPoints]`
rows print that table as it is (brawler state, design ruling Q9), each component with `%.*f` and
`simFrame::lengthDecimals(2)`: two decimals in centimetres, which is the shipped text unchanged, and four in metres. The
teleport seed of §2 (`teleportPos`, the capsule's location) enters through `ueFrame::vectorToSim` too.

> ⛔ THE FALLBACK IS THE TABLE AS IT STANDS RIGHT NOW, which - because nothing else ever
> writes it and this runs once - is exactly the authored default. So "fewer player starts than
> slots leaves the rest authored" is literally "leaves the rest alone", and ZERO player starts
> is a no-op rather than a table of origins.

> ⛔ `UE_LOG`, NOT `OGBLOG_G`, AND THAT IS NOT A STYLE CHOICE. The `ogblog` sink routes
> everything to `LogOGBrawler`, which the shipped ini pins at `Warning` - which is why the
> sub-sim's own `[Ringout.respawn]` line appears in NO captured log and could not be used to
> answer "where did it teleport to?" while this task was being scoped. This is the line that
> answers that question, at Warning, on its own category, once per session per manager.
>
> Volume: one line plus `kMaxSpawnPoints` rows, at composition. Not on any tick path.

⚠ **Correction C13-4.** At `b9f6d81` the shipped `Config/DefaultEngine.ini` sets
`LogOGBrawler=Verbose`, not `Warning`. The line still belongs on `LogOGMgmt` at `Warning`,
because that is where the other composition-time lines are. But the reason given, that the
`ogblog` route is silenced, does not hold for the ini as committed.

At the call site in `BeginPlay`:

> ⭐⭐ [ringout task 9] THE RING-OUT SPAWN TABLE IS SEEDED FROM THE LEVEL, HERE, ONCE.
>
> ⛔ POSITION IS LOAD-BEARING AND IS ASSERTED AT THE DEFINITION. This sits ABOVE both
> role branches, so it runs before `m_integrationLayer` and `m_manager` are emplaced and
> therefore before any tick can read `StaticData::spawnPoints`. Moving it below either
> `emplace` still COMPILES - the integration layer holds a reference, so a later write is
> still visible - which is exactly why the ordering is a `checkf` and not a comment.
>
> ⛔ NOT ROLE-GATED: a client predicts its own respawn, so it needs the same table.

⚠ **R0, task 23.**
- **C23-1.** *"the physics delegates that drive it (`OnPhysScenePreTick` / `OnPhysSceneStep` / `OnPhysScenePostTick`) plus
  the async callback are bound at the very END of it"* is stale since og-simulationscheduler-withjolt task 51: this module
  binds none of them. Stepping starts at `m_physicsBackend.startStepping`, still the last call but one of the start-up (the
  session's `beginPlay` since og-ue-simulationcomponentandmanagersplit task 27b), so the conclusion holds: an empty
  `m_manager` proves that no step has run.
- **C23-2.** *"directly after the two role branches"*: the resim-policy block (`OGBrawlerSimulationManager-guards.md` G-63;
  since og-ue-simulationcomponentandmanagersplit task 26b the session's `applyResimTriggerPolicy`, called at the block's
  position, `UESimulationSession-guards.md` G-17) sits between the role branches and the check. The check still runs before the backend starts stepping, which is what
  makes it a check on every peer.
- **C23-3.** *"on its own category"*: `LogOGMgmt` is shared with the other composition-time lines (as C13-4 says). The
  volume claim is true: one line plus `kMaxSpawnPoints` (8) rows per manager per session, which the harness shows on
  every process.
- **What the `checkf`s now rest on.** The second one reads `compositionExists`, which the call computes (this file's §0). Moving the
  call below either `emplace` makes that argument true, so the check still fires; passing a constant would not.
- Verified true: the integration executor stores a reference to `m_staticData` (`SimulationIntegrationExecutor.h`), the
  call sits above both role branches, `LevelSpawnPoint::name` is a `std::string`, and zero placements leave the
  authored table whole.

---

## 2. The per-character init after the bodies

*Moved from `OGBrawlerSimulationManager-rationale.md` §13.4, its brawler part (task 23; ruling Q-B): the capsule-id stamp,
the teleport seed and the spawn-slot seed, now `onBodiesCreated`. The cast, the body creation and the resolvability gate
stayed in `tryRegister` and in §13.4; task 28 moved them into the session (`UESimulationSession-rationale.md` §12.1), the
quotes to `BrawlerSessionGameUImpl-rationale.md` §7.3, and the call of `onBodiesCreated` into the hook `afterBodiesCreated`.*

> Stamp the authoritative capsule body id into the brawler's CharacterBindings. §10
>
> SOURCE SINCE [movement-sim T13]: the movement sub-simulation's OWN PhysicsDeclaration
> bindings, not the pawn's own capsule lookup. That is why this stamp sits AFTER the fold —
> before it, `bindings.ownBodyId` is still zero. (The `#if DO_CHECK` block above catches a
> zero id in development and test builds; being `checkf`, it is compiled out of Shipping, so
> it is a development instrument and not a Shipping-build guarantee.)
>
> ⭐ THE VALUE IS UNCHANGED, AND THAT IS THE POINT. Task 11's descriptor sets `isRoot`, so
> the factory ADOPTS the pawn's existing root capsule instead of creating a body; that is
> what makes `ownBodyId == parentBodyId == capsuleBodyId` true by construction. This
> identity is exactly what `isRoot` buys, and it is why task 11's review refused to defer
> `isRoot` to a later task. ⛔ Anyone "simplifying" `isRoot` away silently breaks this line.

⚠ **Correction C13-13.** Not silently: `BrawlerMovementSimulation.h` has a `static_assert` that
`kCharacterCapsuleBody` has both `simulatePhysics` and `isRoot`. Removing `isRoot` fails the build.

> ⭐ [movement-sim task 17] THE TWO-SOURCE TRIPWIRE IS GONE. It asserted
> `movementOwnBodyId == record.parentBodyId` for as long as the two sources coexisted;
> `PendingRegistration::parentBodyId` — the record field that held the second one — is deleted
> with it, and the resolvability gate below now reads this same declaration. The identity it
> watched is UNCHANGED and still stated above: it is `isRoot` that makes it hold, not an
> assertion.
>
> ⭐ AND THE IDENTITY IS STILL ASSERTED, one layer down and by a check this task does not touch:
> `ChaosPhysicsFactory::createPhysicalObject`'s adopt-root arm ends in
> `checkf(bodyId == m_parentBodyId, …)` (`ChaosPhysicsFactory.cpp:195`), and its `m_parentBodyId`
> is derived from the SAME capsule component this function passed as the attach parent. So the
> removal drops a duplicate, not the only witness.
> ⚠ Neither was ever a Shipping-build guarantee — `checkf` compiles out there (task 36).

⚠ **Correction C13-14, and the check that replaces the paragraph.** The factory's adopt-root
`checkf` runs only on the adopt arm. So it does not witness either of the two edits this file
warned about at the stamp: (a) the stamp moved above the fold, which stamps a zero id, and (b)
the factory creating a body instead of adopting one. The `#if DO_CHECK` block does not catch (a)
either, because the fold populates the ids after such a misplaced stamp has already read them.
There is now a `checkf(movementOwnBodyId == parentBodyId, …)` directly above
`setCharacterBindings`. `parentBodyId` is still a local of the first-call pass, and the check
witnesses both edits. It is compiled but has not been run in PIE.

> THE TELEPORT SEED — spawn. BrawlerMovementSimulation.h's InitialConditions is a
> COUNTER-FREE edge: the UE layer sets `teleportPending` non-zero here, the sub-sim's first
> step consumes it and clears it back to zero in the same tick. It is ON THE WIRE (16 B) so
> that a respawn replays identically on a client and through a resim.
>
> ⛔ FIRST-CALL PASS ONLY. This branch runs once per character (guarded by
> `record.bodiesCreated`), and the record is moved wholesale into storage at registration
> below, so the seed survives to the sub-sim's first integrate. Seeding it per call would
> re-teleport the character every tick until it registers.
>
> The pose comes from the capsule component, which is where the engine has the character
> standing at spawn — this branch runs on the FIRST tryRegister call, before the sub-sim has
> integrated once, so nothing has driven the capsule yet whatever `drivesBody` says. (It says
> `true`: task 15 flipped it together with `simulatePhysics` and retired the CMC. This is
> still a READ of the engine's authoritative spawn pose, and the reason is the ORDER, not a
> second authority.)
>
> ⚠ THE CONSUMER DOES WRITE BACK. The teleport branch is documented as the ONE body write
> that ignores `drivesBody` — it calls setBodyTransform + setBodyLinearVelocity(0) on the
> capsule. Seeded from the capsule's OWN current location that is a value no-op, but it is
> a real engine call, and a future seed from any other source would MOVE the character.

Verified: `InitialConditions` is `uint32_t teleportPending` plus `glm::vec3 teleportPos`, which
is 16 B. `drivesBody` defaults to `true`. The teleport arm calls `setBodyTransform` and
`setBodyLinearVelocity(0)`.

> THE SPAWN-SLOT SEED - ring-out, task 3. Same first-call branch, and for the same reason:
> `brawlerRingout::InitialConditions::spawnSlot` is written ONCE per character and read on
> every respawn for the life of that character. Unlike the teleport seed above it is NOT a
> counter-free edge - nothing consumes it and nothing clears it.
>
> ⛔ FIRST-CALL PASS ONLY, inherited from the branch it sits in. Re-seeding per call would
> not merely be wasteful: `acquire` is idempotent, so it would return the same index, but a
> character whose slot had been RELEASED and handed to someone else would silently change
> spawn point mid-session. Once, at registration, is the contract.

⚠ **Not reachable as stated.** The only `release(id)` call for a character is its own
`unregisterFromNewFramework`, after which no further `tryRegister` call for that id occurs
(ids are authority-assigned `SimCharacterId`s and never reused, §15). A re-seed per call would therefore just return the same
slot. The contract of seeding once is still right; the failure described cannot happen.

> ⛔⛔ AUTHORITY ONLY, AND THIS IS NOT HasAuthority(). `bReplicates = false` makes
> `GetLocalRole()` report authority on this actor in every role, which is why this file uses
> the WORLD-level test everywhere. `record.isAuthority` IS that test: the caller
> (`USimmableUpdateComponent::tryRegisterWithNewFramework`) computes it as
> `(GetNetMode() != NM_Client)` - literally the expression `BeginPlay` names
> `worldIsAuthority` - and passes it in. The `checkf` states that equivalence rather than
> leaving it to a comment, because `tryRegister` is a public entry point.
>
> ⭐ WHY A CLIENT MAY SAFELY LEAVE ITS OWN COPY AT THE DEFAULT 0, AND WHY THAT CANNOT BE
> READ BEFORE THE SERVER'S NUMBER ARRIVES. `tryRegister` runs on BOTH roles, so a client
> registers its own character locally with `spawnSlot` at its in-class initialiser, 0. That
> value is read by exactly one thing: the respawn arm of `brawlerRingout::integrate`, which
> is reached only after the character has (a) crossed the kill plane and (b) waited
> `StaticData::respawnDelayTicks` - 120 ticks, 2.0 s at 60 Hz - for `tick >= respawnAtTick`.
> The correction that carries the authority's `InitialConditions` is an ORDINARY state
> correction on the already-running rotation, arriving within a handful of ticks of
> registration and restoring the whole struct by assignment. So the client's 0 is
> overwritten two orders of magnitude before the only code that reads it can run, and a
> resim RESTORES the authority's value rather than recomputing it. There is no path on
> which the placeholder is read first: death itself cannot occur earlier than the first
> correction and still leave 120 ticks of countdown to run before the value matters.

Verified: `respawnDelayTicks` defaults to `120u`. The caller computes `isAuthority` as
`(GetNetMode() != NM_Client)`.

⚠ **R0, task 23.**
- *"The `#if DO_CHECK` block above"* is still above in execution order: it ends `tryRegister`'s body-creation half, and
  the call to `onBodiesCreated` is the next statement.
- *"`record.isAuthority` IS that test … the `checkf` states that equivalence"*: the `checkf` now compares the passed
  `isAuthority` with the passed `netMode`, which `tryRegister` takes from the manager's own `GetNetMode()` at the call,
  exactly the value the old statement read. The caller still computes `isAuthority` as `(GetNetMode() != NM_Client)` in
  `tryRegisterWithNewFramework` (*task 29:* the binding's skeleton, `UESimulatableOwnerBinding.h`, since then; the same
  expression on the component, through `derived()`).
- *"Not reachable as stated"*: still true. The only `release` call is in `forgetCharacter`, and its only caller is
  `unregisterFromNewFramework`.
- Verified: `InitialConditions` is still 16 B (`uint32_t teleportPending`, `glm::vec3 teleportPos`), `drivesBody`
  defaults to `true`, `respawnDelayTicks` to `120u`, and `kCharacterCapsuleBody`'s `static_assert`s are in
  `BrawlerMovementSimulation.h`.

---

## 3. The ring-out score push

*Moved from `OGBrawlerSimulationManager-rationale.md` §13.5 (task 23; ruling Q-B). The function is
`BrawlerRosterUImpl::pushRingoutScores` since task 23; its one call is still in `OnPostPhysicsStep`, under the same gate.
Since task 27a the call and its gate are the brawler's hook `BrawlerSessionGameUImpl::afterVisualization`, which the
session's `onPostPhysicsStep` calls at the same position (`BrawlerSessionGameUImpl-rationale.md` §5).*

*The cross-thread argument is in §1, the second read crossing. What follows is the rest.*

*Call site by configuration: the post-physics pass (the session's `onPostPhysicsStep` since task 27a, the actor's
`OnPostPhysicsStep` before) is called by the scene's post-tick delegate in the
Chaos configuration and by the frame host's `TG_EndPhysics` tick function, through the tick driver, in the Jolt one, at the same
point of the frame (after the wait for earlier frames' steps) and every frame. On the authority every
step has run inline before it, so the push reads a score table no step is writing; the argument below
is unchanged (task 54).*

> [ringout task 5] ⭐ THE AUTHORITY-SIDE SCORE PUSH
>
> WHAT IT IS. One game-thread pass that copies each character's authority-side ring-out
> score out of `brawlerRingout::ScoreSystem` and onto that character's replicated
> `RingoutScore` property, from which UE replicates it to every client. It carries NO
> POLICY — who scores, how much and when is entirely task 4's law, in engine-free core.
>
> ⛔ WHY A FILE-LOCAL FUNCTION AND NOT A MEMBER. Everything it needs is passed in, so it
> adds no name to the class's surface and no member to its threading table. The class
> banner's NARROW PASSTHROUGHS list exists precisely to stop this kind of thing becoming an
> accessor; a free function that the one call site below hands two references to cannot be
> reached by anything else.
>
> ⛔ THE ROUTE TABLE IS `m_delayedInputComponentsById`, AND REUSING IT IS A REAL COUPLING —
> stated here because nothing else would state it. That map is registered in
> `USimmableUpdateComponent::tryRegisterWithNewFramework` under `if (isAuthority)` and
> erased in `unregisterFromNewFramework`, so it is exactly "the authority's live characters,
> resolvable to their actors" — which is what this push needs and what no other member is.
> ⚠ IF THAT REGISTRATION EVER NARROWS (a condition beyond `isAuthority`, a later call site,
> a role that stops registering), THIS PUSH SILENTLY LOSES THOSE CHARACTERS: their score
> never leaves the server and their scoreboard row freezes at whatever last replicated. It
> fails quiet, so it is written down rather than left to be rediscovered.

The edit that would break this coupling is typed in `UESimulatableOwnerBinding.h` (since og-ue-simulationcomponentandmanagersplit
task 29; `OGBrawlerSimulatableComponent.cpp` before), not here, which is why it has no tag in this file. Verified: the route
is registered under `if (isAuthority)` in `tryRegisterWithNewFramework`, the binding's registration skeleton
(`UESimulatableOwnerBinding-guards.md` G-11). *R0, task 29:* the quote's component-qualified `tryRegisterWithNewFramework`
is the binding's member now; the coupling is unchanged.

> ⛔⛔ THIS IS A CROSS-THREAD READ, AND THE ARGUMENT FOR IT IS THE CLASS BANNER'S THIRD
> CROSSING — read that before touching this. The short form: `ScoreSystem`'s score table is
> WRITTEN on the physics thread (`postIntegrate`, beneath `OnPreSimulate_Internal`) and read
> here on the GAME thread. What makes it the same ACCEPTED TEAR the input-history poll takes
> rather than a new hazard is that the table cannot be RESTRUCTURED under this reader: the
> only inserting and erasing calls are `onCharacterRegistered` / `onCharacterUnregistered`,
> both driven from `tryRegister` / `unregisterFromNewFramework`, both GAME THREAD. The award
> itself reaches an EXISTING entry, so it writes one naturally-aligned four-byte word and
> cannot rehash. Worst case: a scoreboard number one tick stale, on a display that decides
> nothing — which is the same bound `BrawlerColor` and this property both rest on.
>
> ⭐ THAT ARGUMENT HAS A PRECONDITION, AND THE PRECONDITION IS MACHINE-CHECKED BELOW rather
> than asserted in prose. `postIntegrate` awards through `operator[]`, which INSERTS — and
> therefore MAY REHASH — for an id the roster has not got. Task 4 chose that deliberately so
> an award to an unseeded id is a real award rather than a dropped one. It is unreachable in
> a legal session because `onCharacterRegistered` seeds every authority-registered id, and
> it seeds it BEFORE `tryRegister` returns `Ready`, which is before the route entry this
> walk reads even exists. The `checkf` states exactly that ordering, so a future change that
> breaks it fails loudly in Development instead of racing silently.

> A stale handle is SKIPPED, not erased. The two transport sinks prune the map when they
> meet one because they are its owners; this is a reader and pruning here would mutate the
> container mid-walk for no benefit - `unregisterFromNewFramework` erases it promptly anyway.

Now enforced by type, so it carries no tag: the function takes the route table by `const&`, so
`routes.erase(…)` inside the walk does not compile (C2663, seen in §13.9).

> ⛔ THE UNCHANGED-VALUE GUARD IS INSIDE THE SETTER, not here. One write site, one guard:
> a second early-out here could drift out of step with it and neither would be obviously wrong.

> ---- [ringout task 5] THE SCORE PUSH ----------------------------------
>
> ⛔ THE GATE IS `!runsPrediction()`, THE SAME EXPRESSION TWELVE LINES ABOVE, and it is now
> the layer's ONLY role site for ring-out - [ringout task 19] deleted the BeginPlay wiring
> line, the flag it wrote, and the two-sided checkf that cross-checked them, because there is
> no longer a second gate to disagree with: brawlerRingout::ScoreSystem declares
> kRoleAffinity = AuthorityOnly and SimulationSystemsExecutor skips it off the authority.
> ⛔ NOT HasAuthority(): `bReplicates = false` pins Role to authority on every peer.
>
> ⚠ THIS GATES THE REPLICATION, NOT THE AWARD. A client whose gate here were deleted would
> push scores its ScoreSystem never computed - an empty roster, so zeroes over the replicated
> values - which is loud rather than silent. Per F26 nothing mechanical checks this line.
>
> WHY HERE, beside updateVisualizationAll. This is the one game-thread point that runs
> directly after the simulation has advanced, and harvesting a physics-side result for
> PRESENTATION is exactly what its neighbour already does. The push is a poll, not an event:
> it costs one int compare per character per pass when nothing changed, and the score
> changes at most once per death tick.

⚠ **Correction C13-10.** The other `!runsPrediction()` in `OnPostPhysicsStep` was 26 lines
above, not twelve. A distance is not a fence, and the conversion removes it.
⚠ **Correction C13-11.** Something mechanical does check this line.
`AOGBrawlerUECharacter::SetAuthoritativeRingoutScore` opens with a `checkf` on `HasAuthority()`.
The pawn replicates, so on the pawn that test discriminates. A client that reached the push
would fire it in Development.

⚠ **R0, task 23.**
- **C23-4.** *"WHY A FILE-LOCAL FUNCTION AND NOT A MEMBER … cannot be reached by anything else"* is no longer true. The push
  is a public `static` member of the roster since task 23, so anything that includes `BrawlerRosterUImpl.h` can call it.
  What the paragraph protected still holds: it reads no member (it is `static`), it adds no name to the actor and no
  member to its threading table, and its one caller is `OnPostPhysicsStep` (since task 27a the brawler's hook
  `BrawlerSessionGameUImpl::afterVisualization`, which the session's `onPostPhysicsStep` calls).
- The route table is still registered under `if (isAuthority)` in `tryRegisterWithNewFramework`
  (`UESimulatableOwnerBinding-guards.md` G-11 since task 29, `OGBrawlerSimulatableComponent-guards.md` G-07 before), and
  erased in `unregisterFromNewFramework` ahead of the component's
  unbind. The push reads it by `const&`, so `routes.erase(…)` still does not compile.
- `SetAuthoritativeRingoutScore`'s `checkf` message names the push as `BrawlerRosterUImpl::pushRingoutScores`, called
  from `OnPostPhysicsStep`, since task 23. *Task 27a:* the message still says the actor's `OnPostPhysicsStep`; the call
  is the hook's since then (the message is code, outside that task's ownership, and is listed for the lead).

---

## 4. The pre-diet character cap ∴D-01

*Moved from `OGBrawlerSimulationManager-rationale.md` §10, its subsection *The pre-diet character cap*, with the constant and both of its `static_assert`s (task 23). The manager's `D-01` is retired there; this heading carries the roster's.*

`kPreDietCharacterCap = 4`, checked at character registration on the authority — a path that
provably runs every session, once per character. **A cap that depends on nobody spawning a fifth
character is not a fence**, and this tree has already shipped three things that were silently inert.

**Where 4 came from, and what the same arithmetic gives now.** The binding constraint is the
**input guarantee**: all the remote characters' relay rings must fit one packet by themselves,
because a ring that gets scheduled out under redundancy 0 loses its whole staged burst with no
recovery path. In the test's own terms that is `44*SumE + 11*(N-1) <= 943 B`: 44 B is the relay
ring's entry stride (capture tick 4 + dA 1 + the 39 B `simulatableBrawler::SyncedPlayerInput`), 11 B
is each ring object's fixed cost (u16 length prefix, 2 B codec header, 7 B batch framing), and
943 B is the 952 B bunch less 9 B of per-packet overhead. Modelling a join as "the joiner's ring at
the stage cap (8 entries), everyone else at the measured average (1.132)", the largest N whose
join still fits is **11** (N = 11 needs 919.272 B, 32.728 B or 0.744 of an entry to spare); N = 12
needs 980.080 B and does **not** fit. The same arithmetic is asserted from inside the suite by
`Source/OGBrawlerTests/extern/og-brawler-tests/Source/OGBrawlerTests/RoundVsPacketBudgetTest.cpp`'s pre-diet table. The
other half of the pre-diet configuration is `TimeConfig::correctionRotationK` (§3).

⚠ **R0, og-syncedInput-rework task 3 (2026-10-03).** This paragraph used to derive 4: it priced
the entry at `81*SumE` (the stride was 82 B by then) and concluded *"N = 4 clears that bound with
about nine tenths of one entry to spare and N = 5 does **not**"*. That was true of the six-slice
77 B input composite. Task 3 replaced it with the flat 39 B `SyncedPlayerInput` (stride 82 -> 44 B),
and the bound moved from 4 to 11. **`kPreDietCharacterCap` is retained at 4 anyway**: the constant
and its check are unchanged, and lifting the cap is unscheduled — og-netcode-v2-input-relay item
40, which owned it, was closed on 2026-10-03 without doing so. Until someone lifts it, 4 is a
tested-size statement rather than the packet-budget boundary.

**Emission is once per over-cap character**, not per frame and not per session: registration runs
exactly once per character, so the emission site is its own throttle. No memoization is needed and
none is used — a per-session latch would report the fifth character and stay silent about the
sixth. It is a `Warning` rather than a `Log` because a `Log` line does not exist on a dedicated
server, and it is not an `ensure`/`check` because an over-cap session still **runs**: it runs with
input-loss margins the design has not underwritten, which is a thing an operator must be told, not
a thing that should take the server down mid-brawl.

**The denominator is a `std::set`, not a counter**, and the reason is that the two ends are not
symmetric: `tryRegister` increments only on the `Ready` path, while `unregisterFromNewFramework`
runs for any component ending play including one abandoned mid-`Pending`, so a bare counter would
silently drift downwards and disarm the cap. Reaping on unregister means a session that churns
characters is judged on the roster actually resident rather than on a high-water mark, and erasing
an id that never completed registration is a no-op — which is exactly why a set is the right shape.

**Lifting the cap means deleting both the constant and its check**, and their **absence** afterwards
is the "cap lifted" statement: there is no flag to flip and no value to raise, which is deliberate,
because a cap you can quietly widen is not a cap. (This was written as the wire diet's job,
og-netcode-v2-input-relay item 40; that item closed on 2026-10-03, so nobody owns it today.)

⛔ **The cap is coupled to `brawlerRingout::kMaxSpawnPoints`, and since task 11 the header asserts
it:** `static_assert(kPreDietCharacterCap <= kMaxSpawnPoints)`. The cap only WARNS — it allocates
nothing.
- The ring-out spawn table has exactly `kMaxSpawnPoints` entries. A character that registers while the table is full gets `SpawnSlotAllocator::kNoFreeSlot`.
- It then respawns with **no teleport seed**, logging one `[Warning][Ringout.spawnSlot]` per respawn.
- Its body is still below the kill plane, so it dies again on the next tick, every respawn delay: a death loop that feeds everyone else's score (T10 spike flag 1).
- A cap above the table would stop warning about exactly the characters that cannot be placed.

**Since og-brawler-uploadtosteam T18 (2026-09-29) the table is 8 and the cap stays 4.**
- The cap was the packet-budget number derived above (it is 11 since og-syncedInput-rework task 3, and the cap was kept at 4); the table is sized by a playtest ruling: 8 characters, and nobody refused for numbers.
- Characters 5-8 therefore get real slots and trigger the `[PreDietCap]` warning, which is the truthful "above the tested size" signal.
- The assertion still holds (4 ≤ 8) and still forbids the one dangerous direction: the cap above the table.
- The 9th concurrent character is back in the loop above. The engine allows up to 16 per server. *(The header's prose said
raising the cap alone "SILENTLY STOPS RESPAWNING EVERY CHARACTER PAST THE 4th". Both halves were
wrong — §11 C7.)*

⚠ **This heading carries `∴D-01`**, the derivation tag on `kPreDietCharacterCap`'s declaration.
Whoever deletes the constant deletes that tag's site, so they must retire `D-01` in
the same change. The tag lint's orphan check will say so if it does not.
⚠ **R0, task 23.**
- **C23-5.** *"N = 11 needs 919.272 B, 32.728 B or 0.744 of an entry to spare"* mixes two forms of the bound. 919.272 B is
  the packet total **including** the 9 B per-packet overhead, against the 952 B bunch, as `RoundVsPacketBudgetTest.cpp`
  computes it (`8 + 9 x 1.132` entries). In the stated form, `44*SumE + 11*(N-1)` is 910.272 B against 943 B. The spare,
  32.728 B, is the same in both; N = 12 (980.080 B against 952 B) does not fit in either.
- *"the header asserts it"* now means `BrawlerRosterUImpl.h`; the assert moved with the constant, and fires on a cap of 9
  (task 23's POISON P1).
- *"The denominator is a `std::set`"* is still asserted, now beside the member in the roster; a `std::multiset` fails it
  (POISON P2).
- *"checked at character registration on the authority"*: `onRegistered`, called once per character on `tryRegister`'s
  Ready path. The harness sessions never pass the cap (three characters at most), so the `[PreDietCap]` line has not been
  observed at run time.

---

## 5. The simulation character id

The allocator, `allocateSimCharacterId` and its refusal branch moved here from the manager in task 23. The design (why
the id is peer-stable, why the counter is a member and not a process static, PIE's two managers, couch co-op, a rejoin)
stays in `OGBrawlerSimulationManager-rationale.md` §15, which describes the manager's lifetime that the roster shares; the
permanence rule is guard G-01 of `SimCharacterId-guards.md`, and the refusal branch is this file's guard G-02.

---

## 6. Corrections found by task 23

| id | the claim | what is true |
|---|---|---|
| C23-1 | §1: the scene delegates are bound at the end of `BeginPlay` | none is bound since task 51; `m_physicsBackend.startStepping` starts stepping there |
| C23-2 | §1: the seeded check sits directly after the two role branches | the resim-policy block sits between them |
| C23-3 | §1: the seed's line is on its own category | `LogOGMgmt` is shared with the other composition-time lines |
| C23-4 | §3: the score push is file-local and cannot be reached by anything else | it is a public `static` member of the roster |
| C23-5 | §4: N = 11 needs 919.272 B against a 943 B bound | 919.272 B is against 952 B; the 943 B form gives 910.272 B, with the same spare |
| C23-6 | guard G-02: the hit ledger is "planned" | it shipped in netcode task 27 (`SimCharacterId-guards.md` G-01) |
| C23-7 | guard G-05: the spawn-slot table has four entries | eight since T18 (the guard's own **Verified** line) |
