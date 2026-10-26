<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: bGlobalGravitySet -- Unreal Engine AWorldSettings field, outside every scan root (§6.1; moved with OGBrawlerSimulationManager-rationale.md §13.2, task 27b) -->
<!-- lint-external-ref: GlobalGravityZ -- Unreal Engine AWorldSettings field, outside every scan root (§6.1; moved with OGBrawlerSimulationManager-rationale.md §13.2, task 27b) -->
<!-- lint-external-ref: PendingRegistration::parentBodyId -- ABSENCE FENCE (§7.3, moved with OGBrawlerSimulationManager-rationale.md §13.4, task 28): the record field deleted with the two-source tripwire. It must NOT resolve -->
<!-- lint-external-ref: m_movementStaticDataCVars -- retired member: the actor's cvar member that makeStaticData replaced (og-ue-simulationcomponentandmanagersplit task 24a); named in section 4 to say what the hook folded -->
# `BrawlerSessionGameUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.h` and, since og-ue-simulationcomponentandmanagersplit task
33, `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.cpp`. The source files carry no prose: a two-line docs pointer and
code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the class is.
* **§2** is the two latency hooks and the brawler's input-sample time (hop `H1`'s start).
* **§4** is the static-data hook, `makeStaticData` (task 24a).
* **§5** is the post-visualization hook, `afterVisualization` (task 27a): the ring-out score push.
* **§6** is the six start-up hooks (task 27b), with the gravity cross-check and the neutral input's guard.
* **§7** is the four registration hooks (task 28), with the brawler's part of the registration: the capsule body id, the
  movement sub-simulation, the notify's dependants and the first-call pass's quoted comments.
* **§3** is how the header and the `.cpp` are compiled, and who includes the header.

---

## §1 What the class is

`BrawlerSessionGameUImpl` holds the brawler's **phase hooks**: the statements a game-agnostic OGSimulationUnreal template
calls at a fixed point of its own sequence, where the brawler-specific statement used to be. The templates call them
statically through `BrawlerSessionTraitsUImpl::Game` (no virtual call): most are static member functions; since task 27b
some are members of the one object, `m_game`, because they reach the brawler's parts (two since task 27b, §6; five since
task 28, §7). Each hook body is the brawler's
statement, verbatim; the hook sits at the exact position of the statement it replaces.

As of og-ue-simulationcomponentandmanagersplit task 19a it had two hooks, both for the latency template
`UELatencyBudgetDiagnostics`. og-ue-simulationcomponentandmanagersplit task 24a added the first of the session's, `makeStaticData` (§4), and the actor
holds one instance, `m_game`, which the session holds by reference. Task 27a added the second, `afterVisualization` (§5).
Task 27b added the six of the session's start-up (§6) and the class's one piece of state, a reference to the actor's
roster, bound when the actor constructs `m_game{ m_roster }` (the roster is declared first, so it is constructed first and
destroyed last). Task 28 added the design's last four, the registration hooks (§7), and a second reference, to the actor's
input-history feed, bound in `m_game{ m_roster, m_inputHistoryFeed }` (the feed is declared before `m_game` too).

---

## §2 The latency hooks

| hook | brawler statement | when, which thread |
|---|---|---|
| `inputSampleSource(component)` | `owner.getOwnerInputCollection()`, the component's input collection component (or `nullptr`) | once per registration, in the wrapper's construction (`wrap`), on the game thread |
| `inputSampledSeconds(source)` | `source != nullptr ? source->getDiagnostics().inputSampledSeconds() : 0.0` | once per call of the wrapped provider, on the step's thread |

The read is **two-phase**, as it was before the move: the component is resolved once, on the game thread, and the step
thread only loads the time the input collection component published. Why that matters, and the guard on the call site,
are the plugin's (`UELatencyBudgetDiagnostics-rationale.md` §2 and `UELatencyBudgetDiagnostics-guards.md` G-02, in
OGSimulationUnreal's docs).

**The input-sample time** is `UOGBrawlerInputCollectionComponent`'s `std::atomic<double>`, written by `noteInputSampled`
(`FPlatformTime::Seconds`, relaxed) as the first statement of `updateGameThreadCache` after the owner check, on the game
thread, once per frame: hop `H1`'s start. It starts at the frame's input refresh, not at the device event: Enhanced Input
dispatched the frame's events earlier in the same frame, and the OS queued them up to one frame before that, so the true
input age is **under**-stated by up to one game frame. It exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1, as does
its view `getDiagnostics()`, so `inputSampledSeconds` is declared only then; `inputSampleSource` is a production read and
exists in both states. A time of `0.0` (no refresh yet, or no input collection component) posts no `InputSampled` stamp.

`H1` and `H2` are measured for a remote client's own characters only: `tryRegister` (the session's since task 28) wraps the
provider on the client branch, and a listen host's local players have no client provider.

---

## §3 How the header and the .cpp are compiled

* **Inline, in one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair.** The hook bodies are the statements that compiled in
  the latency class's own translation unit under that pair, so they keep its optimization state; the declarations sit
  above the pair.
* **Who includes it.** Since task 24a, `OGBrawlerSimulationManager.h`: the actor holds `m_game` by value, and the
  session's constructor, which calls `makeStaticData()`, is instantiated wherever the actor's constructors are (its
  `.cpp` and the reflection code's helper constructor). So every file that includes the manager's header sees this one.
  `OGBrawlerSimulationManager.cpp` also names it in an include: it instantiates the session's `tryRegister`, which calls
  the registration hooks and the wrapper's `wrapInputProvider`, through the actor's wrapper (since task 29 the
  component's `.cpp` instantiates it too, through the binding's `tryRegisterWithNewFramework`). Since task 33,
  `BrawlerSessionGameUImpl.cpp` includes it too, first (UBT requires a `.cpp`'s own header first). The header needs the
  complete component and input collection component, so it includes both; every file that includes the manager now sees
  them. `BrawlerSessionTraitsUImpl.h` still only forward-declares `BrawlerSessionGameUImpl`.
* **The out-of-line hooks are defined in `BrawlerSessionGameUImpl.cpp`** (since task 33; until then in
  `OGBrawlerSimulationManager.cpp`, between the actor's own members, in its `OGSIM_OPTIMIZE_OFF` region). There are
  eleven, in this order: `makeStaticData` (§4, task 24a), five of the six start-up hooks (§6, task 27b),
  `afterVisualization` (§5, task 27a) and the four registration hooks (§7, task 28). They cannot be inline here: they
  need what the actor's header declares (the complete actor, whose private `readMovementStaticDataCVars()`
  `makeStaticData` calls because the actor befriends this class, and `LogOGBrawler`), and that header includes this one
  before it declares them. So the `.cpp` includes the actor's header, right after this one (UBT requires a `.cpp`'s own
  header to be its first include). It has its own `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair around the bodies, so
  these non-template functions compile unoptimized in Development, as the statements did in the actor; task 33's notes
  compare each one's disassembly with its pre-move body. It also carries the manager's `HasAuthority` fence
  (`OGBrawlerSimulationManager-rationale.md` §13.9), because every moved statement was under it. `afterVisualization`'s
  parameter names the session, so the header declares the session template (the traits header does too). The sixth
  start-up hook, `neutralInput()`, is inline here, in this header's pair, because its guard is this file's (§6).
* **Since task 27b** the header forward-declares the roster (the class holds a reference to it) and `UWorld`, and includes
  the engine's base types header (for `ENetMode`), `<optional>` and `<string_view>`; the hook `emplaceSystems` names the
  composition's `SystemsExec` and the engine traits' two adapters, which the traits header already brings in.
* **Since task 28** the four registration hooks are declared here (§7; defined in `OGBrawlerSimulationManager.cpp`'s
  `OGSIM_OPTIMIZE_OFF` region until task 33, in `BrawlerSessionGameUImpl.cpp`'s since); the header forward-declares the
  input-history feed (the class holds a reference to it) and includes og-simulation's `BodyId.h` (`rootBodyId` returns a
  `BodyId`).

---

## §4 The static-data hook, `makeStaticData` (og-ue-simulationcomponentandmanagersplit task 24a)

| hook | brawler statements | when, which thread |
|---|---|---|
| `static makeStaticData()` | `AOGBrawlerSimulationManager::readMovementStaticDataCVars()`, then the brawler's `StaticData` built from its five fields (the model, the walk speed, the step period, the step speed and the gravity), returned as a prvalue | once per session, in the session's member initializer of `m_staticData`, during the actor's construction, on the game thread |

* **The statements are the actor's, in its order.** Before task 24a the actor's member `m_movementStaticDataCVars` read
  the cvars in its initializer, and `m_staticData`'s initializer built the static data from it, right after `m_storage`.
  The session initializes `m_staticData` from this hook at the same position, so the read and the construction run at
  the same point of construction as before, and the cvar result is a local of the hook instead of a member.
* **In place.** `simulatableBrawler::StaticData` deletes its copy and move operations. The hook returns a prvalue and the
  session initializes its member from it, so the object is constructed once, in the session (guaranteed copy elision).
* **Friendship.** `readMovementStaticDataCVars()` is a private static of the actor, defined in `MovementSchemeCVar.cpp`
  (not edited); the actor befriends this class so the hook can call it. The friendship is the class's, not the file's,
  so the call compiles unchanged from `BrawlerSessionGameUImpl.cpp` (task 33).
* **Defined in `BrawlerSessionGameUImpl.cpp`, in its `OGSIM_OPTIMIZE_OFF` region** (in
  `OGBrawlerSimulationManager.cpp`'s until task 33). The hook needs the complete actor, and this header is included by
  the actor's header before the actor is defined, so the body cannot be inline here (§3). As a non-template function in
  that region it compiles unoptimized in Development, as the two initializers did in the actor's constructor (the
  session's constructor itself is a template and compiles optimized; lead ruling F1).

---

## §5 The post-visualization hook, `afterVisualization` (og-ue-simulationcomponentandmanagersplit task 27a)

| hook | brawler statements | when, which thread |
|---|---|---|
| `static afterVisualization(session)` | on the authority (a manager that does not predict) with its systems executor built: `BrawlerRosterUImpl::pushRingoutScores` with the executor's `brawlerRingout::ScoreSystem` and the session's delayed-input map, the route table of the authority's characters | once per frame, from the session's `onPostPhysicsStep`, after the backend's visualization update and before the latency tick, on the game thread |

* **The statements are the actor's, in its order.** Until task 27a they sat in the actor's `OnPostPhysicsStep` at the
  same position: the gate (`m_manager.has_value() && m_systemsExec.has_value() && !m_manager->runsPrediction()`), then
  the push. The whole `if` is the hook's body; the session calls the hook where the `if` stood
  (`UESimulationSession-rationale.md` §10, in OGSimulationUnreal's docs). The latency tick that followed it is the
  session's own statement.
* **What it reads, and how.** The session passes itself, `const`. The hook reads the manager through `getManager()` and
  the two const reads task 27a added to the session, `getSystemsExec()` and `getDelayedInputComponentsById()`: each
  returns a const reference to the member the actor read (`m_session.m_manager`, `m_session.m_systemsExec`,
  `m_session.m_delayedInputComponentsById`). The score system is reached through the executor's const `get`, the same
  system the actor reached through the non-const one, and `pushRingoutScores` takes both by const reference, as before.
  The push is a static member of the roster (task 23), so the hook needs no roster.
* **The crossing is unchanged.** The push reads the score table on the game thread while the step writes it on the physics
  thread; why that tear is accepted is `OGBrawlerSimulationManager-rationale.md` §1 ("The second read crossing") and the
  `checkf` it rests on is `BrawlerRosterUImpl-rationale.md` §3.
* **Defined in `BrawlerSessionGameUImpl.cpp`, in its `OGSIM_OPTIMIZE_OFF` region,** as `makeStaticData` is (§4); from
  task 27a until task 33 it was in `OGBrawlerSimulationManager.cpp`'s, where the actor's `OnPostPhysicsStep` had been.
  As a non-template function there it compiles unoptimized in Development, as the statements did in the actor; the
  session's `onPostPhysicsStep`, a template, compiles optimized (lead ruling F1).

---

## §6 The start-up hooks (og-ue-simulationcomponentandmanagersplit task 27b)

The session's `beginPlay` (`UESimulationSession-rationale.md` §11, in OGSimulationUnreal's docs) calls six hooks, each at
the position of the brawler statements that sat there in the actor's `BeginPlay`:

| hook | brawler statements | when, which thread |
|---|---|---|
| `beforeWorldBuild(world, netMode, compositionExists, staticData)`, a member | the gravity cross-check (§6.1), then `m_roster.seedSpawnPointsFromLevel(world, netMode, compositionExists, staticData)` (`BrawlerRosterUImpl-rationale.md` §1) | once per session, first after the world check, before the composition is emplaced; game thread |
| `static emplaceSystems(systemsExec, bodyAdapter, queryAdapter)` | the systems executor's emplace: the hit detector over the two adapters, then the hit routing and the ring-out score systems | once per session, in the role's branch, after the integration layer, before the manager; game thread |
| `static neutralInput()` | `simulatableBrawler::getZeroPlayerInput()`, the value of `setNeutralInput` | once per session, in the role's branch; game thread |
| `static installGameLoggers(isAuthority)` | the `ogblog` sink lambda (`LogOGBrawler`, its verbosity from a leading `[Verbose]` / `[Warning]`), then `ogblog::setGlobal` | once per session, in the role's branch, after `simlog::setGlobal`; game thread |
| `afterSessionConfigured(netMode)`, a member | `m_roster.checkSeeded(netMode)` (`BrawlerRosterUImpl-rationale.md` §1) | once per session, after the resim policy, before the backend line; game thread |
| `static backendFingerprint()` | `buildIdentityUImpl::backendFingerprint()` | once per session, in the `[SimHost.Backend]` line; game thread |

* **The statements are the actor's, in its order.** Each body is the statements verbatim; the session calls the hook where
  they stood. What the session hands a hook is what the statements read: the world pointer's target, the actor's net mode
  (through the session's owner), the session's `m_staticData`, its two adapters and its systems slot. Task 27b's
  `transcribe.py` inlines every hook at its call and compares the whole start-up with the actor's `BeginPlay` as it was
  before task 25.
* **Two hooks are members, the rest static.** `beforeWorldBuild` and `afterSessionConfigured` call the roster's two
  start-up members; the class holds a reference to the actor's roster (§1). The roster's argument list is unchanged
  (task 23's: it holds no reference to the actor).
* **`installGameLoggers` holds one sink for both roles.** The actor's two role branches each defined a lambda (ogblogServer
  and ogblogClient), token-identical apart from the name; the hook defines one, `ogblogSink`, and installs it. The
  `isAuthority` parameter is the session's (manager design §4.3); the brawler's sink does not depend on it. The lambda's
  definition moved from beside the router lambda to the `ogblog::setGlobal` it feeds: a captureless lambda's definition has
  no effect, and nothing between the two positions named it.
* **`emplaceSystems` names the hit detector** through the class's private alias `BrawlerHitDetectionSystem`
  (`BrawlerCompositionUImpl::HitDetectionSystem`), the actor's alias until task 27b. Why the detector is built from the
  integration layer's two adapters, never the reader, is the session's `UESimulationSession-guards.md` G-01 and the
  brawler's verification in `OGBrawlerSimulationManager-rationale.md` §2.
* **`neutralInput()` and its guard (G-01, ruling Q-J).** The value is `getZeroPlayerInput()`, never `PlayerInput{}`: the
  brawler's zero input aims at `(0,0,1)`, and a value-initialized input does not. The session sets it on both roles; why the
  authority needs it too is the session's (`UESimulationSession-rationale.md` §11.3, `UESimulationSession-guards.md` G-23).
  The guard stays here because it guards the value, not the sequence.
* **Compiled** as §3 says: five in `BrawlerSessionGameUImpl.cpp`'s `OGSIM_OPTIMIZE_OFF` region (in
  `OGBrawlerSimulationManager.cpp`'s until task 33), `neutralInput()` inline in this header's pair, so every statement
  keeps the unoptimized state it had in the actor (ruling F1 governs only the session's template; task 27b's notes check
  the gravity check's floating point).

### 6.1 The gravity cross-check (was `OGBrawlerSimulationManager-rationale.md` §13.2)

*The first `checkf` of the hook `beforeWorldBuild` (the first `checkf` in the actor's `BeginPlay` until task 27b).*


> ⭐⭐ THE SIM'S GRAVITY MUST AGREE WITH THE ENGINE'S — [movement-sim task 16].
>
> ⛔ THIS IS NOT A TAUTOLOGY, EVEN THOUGH THE SIM'S VALUE WAS READ FROM THE ENGINE.
> `readMovementStaticDataCVars()` runs in a member initializer, during construction, where
> no world exists — so it can only read the PROJECT DEFAULT
> (`UPhysicsSettings::DefaultGravityZ`). A level is free to override gravity in its
> `WorldSettings` (`bGlobalGravitySet` / `GlobalGravityZ`), and `UWorld::GetGravityZ()`
> here is the first moment that override is observable. The two disagreeing means the
> character falls at one rate while every prop in the level falls at another.
>
> ⚠ WHY IT MATTERS DESPITE THE BODY'S OWN GRAVITY BEING OFF (ruling #16 a): the sim applies
> `StaticData::gravity` itself precisely BECAUSE engine gravity would double-apply on the
> character. Everything the character shares a floor with is still on the engine's number.

Verified: `MovementSchemeCVar.cpp` reads `UPhysicsSettings::DefaultGravityZ`. The check's own
message says most of this, and it is code.

*Moved here by og-ue-simulationcomponentandmanagersplit task 27b with the statements it describes (ruling Q-B: SM §13.2
moves to the hooks' doc). R0, task 27b: re-verified: `MovementSchemeCVar.cpp` reads `UPhysicsSettings::DefaultGravityZ`
into the static data's gravity, the hook compares it with the world's `GetGravityZ()` within 1e-3, and the message text is
unchanged.*

**Units: the check compares in UE units (og-sim-metres-yup task 8).** `UPhysicsSettings::DefaultGravityZ` and
`UWorld::GetGravityZ()` are UE values in cm/s². `StaticData::gravity` is in og-simulation units, because
`readMovementStaticDataCVars()` (`MovementSchemeCVar.cpp`) passes the gravity, the walk speed and the step speed through
`ueFrame::lengthToSim` before they reach `FMovementStaticDataCVars`. Its `[Movement.cvar]` line still echoes the UE values
it read, since it reports UE settings. So the hook converts the sim's value back, `simGravityZ` =
`ueFrame::lengthToUe(gravity)` narrowed to `float`, and compares that with `GetGravityZ()` within the same 1e-3. The
message still prints both values in cm/s². At the shipped knobs (centimetres) both conversions are a float-to-double copy
and back, so the compared values are the same floats as before. If the sim's value reached `StaticData` unconverted in
metres, the check would compare -98000 with -980 and fire.

---

## §7 The registration hooks (og-ue-simulationcomponentandmanagersplit task 28)

The session's `tryRegister` and `unregisterFromNewFramework` (`UESimulationSession-rationale.md` §12, in OGSimulationUnreal's
docs) call four hooks, each at the position of the brawler statements that sat there in the actor's functions:

| hook | brawler statements | when, which thread |
|---|---|---|
| `static rootBodyId(simulatable)` | the movement declaration's own body: `simulatable.getPhysicsComposite().get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId` | every call of `tryRegister` after the first-call pass, as the resolvability gate's first read; game thread |
| `afterBodiesCreated(simulatable, character, id, isAuthority, netMode)`, a member | `m_roster.onBodiesCreated(simulatable, character, id, isAuthority, netMode)`: the capsule-id stamp, the teleport seed and the authority's spawn slot (`BrawlerRosterUImpl-rationale.md` §2) | once per character, at the end of `tryRegister`'s first-call pass; game thread |
| `afterRegistered(id, isAuthority)`, a member | `m_roster.onRegistered(id, isAuthority)`: the pre-diet cap (`BrawlerRosterUImpl-rationale.md` §4) | once per character, after the registration line, before the executor's notify; game thread |
| `onUnregistered(id)`, a member | `m_roster.forgetCharacter(id)` (the id-set erase, then the spawn-slot release), then `m_inputHistoryFeed.forgetCharacter(id)` | once per component ending play, after the reception forget, before the latency forget; game thread |

* **The statements are the actor's, in its order.** Each body is the statements verbatim; the session calls the hook where
  they stood, and passes what they read: the record's simulatable, the cast character, the storage key, the record's role
  and the actor's net mode (through the session's owner). Task 28's `transcribe.py` inlines every hook at its call and
  compares both sequences with the actor's functions as they were before task 25.
* **Three are members, one static.** The three members call the roster (task 23's methods, argument lists unchanged) and
  the input-history feed (task 22's `forgetCharacter`), so the class holds references to both (§1). `rootBodyId` reads
  only the simulatable.
* **The guards stay with the roster's methods** the hook bodies call: the spawn-slot write, the authority block of the cap
  and the spawn-slot release are `BrawlerRosterUImpl-guards.md` G-03, G-04 and G-05 (the actor's G-67, G-68 and G-74 until
  task 23). The two guards of the sequences themselves moved to the session (`OGBrawlerSimulationManager-guards.md` G-69 and
  G-73 → `UESimulationSession-guards.md` G-25 and G-26).
* **Compiled** as §3 says: in `BrawlerSessionGameUImpl.cpp`'s `OGSIM_OPTIMIZE_OFF` region (in
  `OGBrawlerSimulationManager.cpp`'s until task 33), non-template, so every statement keeps the unoptimized state it had
  in the actor (ruling F1 governs only the session's template).

### 7.1 The capsule body id, and what the movement sub-simulation costs (was `OGBrawlerSimulationManager-rationale.md` §10)

*The brawler's part of the actor's §10, carried with the statements it describes (task 28). The generic contract is the
session's §12. R0 lines follow each paragraph. In the carried paragraphs "this file" is the actor's
`OGBrawlerSimulationManager.cpp`, where the statements stood until task 28.*

**The capsule body id.** `setCharacterBindings` stamps the authoritative capsule `BodyId` into the
brawler's `CharacterBindings`, and its **source is the movement sub-simulation's own
`PhysicsDeclaration`** — `bindings.ownBodyId`, read out of the physics composite *after* the
creation fold has run. Before the fold that id is still zero, which is why the stamp sits below it.

The value is the capsule's, and it is the capsule's *by construction* rather than by coincidence:
the movement descriptor sets `isRoot`, so `ChaosPhysicsFactory` **adopts** the engine character's
existing capsule instead of creating a body under it, and its adopt-root arm ends in a `checkf`
that the adopted body's id equals the parent id the factory derived from that same component. One
body, one id, reached through the sub-simulation's own declaration.

A **two-source tripwire** used to sit at this stamp: a second `checkf`, comparing the declaration's
`ownBodyId` against a capsule id cached on `PendingRegistration` for as long as the two sources
coexisted. Both are gone. The cached `parentBodyId` field had exactly two readers — that tripwire
and the resolvability gate — and the gate now reads the same declaration, so the field had none
left. Removing it removes a duplicate, not the only witness: the factory's own assertion, which
this file does not own, still watches the identity. The capsule id is still derived inside the
first-call pass, as a **local**, because the factory and each declaration's own
`bindings.parentBodyId` consume it there.

*R0, task 28:* verified: the stamp is `BrawlerRosterUImpl::onBodiesCreated`'s `setCharacterBindings`, after its read of
the movement declaration's `bindings.ownBodyId`, which the hook `afterBodiesCreated` calls at the end of the first-call
pass, after the backend's fold; `kCharacterCapsuleBody` sets `isRoot`; `ChaosPhysicsFactory`'s adopt-root arm ends in
`checkf(bodyId == m_parentBodyId, …)`. The record (`PendingRegistration`, the session's since task 28) has no capsule-id
field. Two precisions: the "first-call pass" that derives the capsule id is the backend's `createBodies_GameThread` (the
self-scheduled host's local `parentBodyId`, from the capsule's handle), which `tryRegister`'s first call reaches; and the
roster's stamp is preceded by a `checkf` that the declaration's own body is its parent body, both read from the
declaration (task 23's restatement of the isRoot fence), which is not the retired two-source tripwire: that compared the
declaration with the record's cached field.

**The movement sub-simulation, and what it costs this file.** It is registered by exactly the same
generic fold as radial, guard and the three projectile slots — `staticDataOf` hands the fold its
static-data slice, `descriptor()` the body and shapes, `queryVolumes` the probe volume — so no
line in this file branches on it. Three things about it are nonetheless this file's business:

* **It adopts; it does not create.** `isRoot` on its descriptor means the engine character's
  capsule *is* its body. `simulatePhysics = true` on that adopted component is what switches the
  engine's own character-movement component off, and the sub-simulation's `drivesBody` is the
  other half of that pair. Neither may be flipped alone: `simulatePhysics` without `drivesBody`
  leaves nobody driving the capsule, and `drivesBody` without `simulatePhysics` leaves two.
* **Its collision category is hand-written engine work, and this file pays it.** `character` needs
  one row in EACH of the two mapping tables in `BeginPlay` — authority branch and client branch,
  duplicated with no shared constant — and the channel itself must be declared in the project's
  collision settings. Generic creation, hand-written collision; the fold's own comment states both
  halves, because stating only the first one overstates the claim.
* **Its spawn pose is seeded here, exactly once.** The first-call pass writes `teleportPending`
  and `teleportPos` into the sub-simulation's `InitialConditions` from the capsule's current
  transform. That edge is on the wire, so a respawn replays identically on a client and through a
  resim, and it is the one body write that ignores `drivesBody` — which is safe here only because
  it is seeded from the capsule's own location and is therefore a value no-op.

*R0, task 28:* verified: the `static_assert` on `kCharacterCapsuleBody` (`simulatePhysics`, `isRoot`, `lockRotation`,
the resim policy) and `StaticData::drivesBody`; the teleport seed is written by the roster's `onBodiesCreated` from the
capsule's location, and `InitialConditions` (`teleportPending`, `teleportPos`) is a serialized field set, append-only, so it
rides the wire; the movement step consumes it with `setBodyTransform` without testing `drivesBody`. **Correction:** the
second bullet's "one row in EACH of the two mapping tables in `BeginPlay` — authority branch and client branch" is stale:
since og-tickdriver task 15a there is one category table per configuration (`BrawlerChaosBindingUImpl::categoryChannels`,
`BrawlerJoltBindingUImpl::categoryChannels`; the Chaos table has the `collisionCategory::character` row), so a new
category needs one row in each configuration's table (`OGBrawlerSimulationManager-rationale.md` §13.3).

**Attach parent and parent body are the same capsule** under the one-deep hierarchy, so passing one
handle expresses "these shapes belong to this character". They used to be passed separately, which
let callers get them out of sync; the factory derives the parent `BodyId` from the attach parent's
body-instance handle internally now, and that factory-owned parent id becomes the root for every
shape it registers — so `overlap()` emits an actor-level `rootBodyId` equal to the capsule id.

*R0, task 28:* this paragraph describes the Chaos configuration's factory (`ChaosPhysicsFactory`, the self-scheduled
host's bind); verified: its adopt-root arm registers the adopted body's shapes with no parent (`registerShape(…,
std::nullopt)`), so a direct hit on it reports the body itself as the root.

#### The notify's brawler dependants

**On the authority path**, registration also notifies the systems executor that the character is
now *in storage*, so `brawlerHitRouting::System::onCharacterRegistered` indexes it for inbound-hit
routing and reads `capsuleBodyId` itself. Wiring that notify and dropping the old adapter-owned
`m_byRootBodyId` insert landed in the **same** change, so there was never a window in which both
the adapter map and the system map were populated and an inbound-hit stream could be double-routed.

*R0, task 28:* verified: the hit-routing system's `onCharacterRegistered` reads
`getCharacterBindings().capsuleBodyId` out of the view and indexes `m_byRootBodyId`; the hit detector's
`onCharacterRegistered` does the same into its own map (`BrawlerHitDetectionSystem-guards.md` G-18); the ring-out score
system's `onCharacterRegistered` seeds the score roster the push's fatal check needs (`BrawlerRosterUImpl-rationale.md`
§3). **Correction:** the paragraph's "On the authority path" is false: the session's `tryRegister` notifies on both roles,
and the executor fires each system by its role affinity (hit routing and hit detection on every role, the score system on
the authority only; `UESimulationSession-rationale.md` §12.4). The order this paragraph and the guard rely on is
`UESimulationSession-guards.md` G-25 (the actor's G-69 until task 28).

### 7.2 The unregistration's brawler forgets

The actor's §10 listed the unregistration in four items; the first three are the session's (`UESimulationSession-rationale.md`
§12.3). The fourth is the brawler's, the hook `onUnregistered`: the roster's `forgetCharacter` erases the id from
`m_authorityRegisteredIds` and then releases its ring-out spawn slot (`BrawlerRosterUImpl-guards.md` G-05), and the
input-history feed's `forgetCharacter` erases the display's rows and lanes for the id (§7.4). Both run after the reception
forget and before the latency forget, as they did in the actor.

### 7.3 The first-call pass, as its comments said it (was `OGBrawlerSimulationManager-rationale.md` §13.4)

*`tryRegister`'s first-call pass and resolvability gate, the session's since task 28. Quoted as they shipped in the
actor's `.cpp`, so this is a move and not a rewrite; where a quoted sentence is no longer true, the note under it says so
and the quote is left intact.*

> First-call body creation pass.
> ⭐ [movement-sim task 19] THIS CAST IS THE REGISTRATION PATH, AND IT IS NOT AN ACCESSOR SWAP.
> It used to name the engine's walking-pawn base; task 19 rebased `AOGBrawlerUECharacter` on
> `APawn`, so that base is no longer in the hierarchy and the old cast would return NULL HERE —
> on the FIRST-CALL body-creation pass — which is registration failing outright: no bodies, no
> simulation, no character. The root capsule this pass needs is declared by
> `AOGBrawlerUECharacter` itself now, so that class IS the type the contract requires.
> ⚠ `checkf` COMPILES OUT IN SHIPPING (task 36). There a wrong owner type is a null dereference
> on the very next line rather than an assert, which is why the message below names the exact
> class rather than a family.

> ⚠ CharacterBindings is NO LONGER STAMPED HERE. [movement-sim T13] moved it BELOW the
> physics fold, because its source is now that fold's output. See the §10 banner there.

> ⛔ Attach and parent-body are the SAME capsule, so ONE handle - two let callers desync. §10

> The factory's parentBodyId roots every shape, so overlap() emits the capsule id.

> Generic — each declaration names its own slice (PhysicsDeclaration.h). Adding a
> body-owning sub-simulation therefore edits no engine file to have its body CREATED,
> BOUND, CAPTURED, REWOUND and CHECKSUMMED.
>
> ⚠ [movement-sim T13, from the task-10 review] THAT IS THE WHOLE OF THE CLAIM, and the
> earlier unqualified wording overstated it. Making that body COLLIDABLE OR QUERYABLE is
> still hand-written engine work: its `collisionCategory` needs one entry in EACH of the two
> ChaosCategoryMapping tables in BeginPlay above (authority branch and client branch,
> duplicated with no shared constant), or ChaosPhysicsFactory::applyDescriptor types the
> shape by the unmapped fallback and no object query can ask for it. Tasks 39 and 43 paid
> exactly that cost for `world` and `character`. Generic creation, hand-written collision —
> state both halves.

Since this conversion there is **one** table per configuration (`OGBrawlerSimulationManager-rationale.md` §13.3; since og-tickdriver task 15a
`BrawlerChaosBindingUImpl::categoryChannels` and `BrawlerJoltBindingUImpl::categoryChannels`), so a new
category needs one row in each, not one per role branch.

*R0, task 28:* the first quote's class is still the type the contract requires: the session's cast reads
`TraitsT::Character`, which is `AOGBrawlerUECharacter` (`BrawlerSessionTraitsUImpl.h`), and since task 28 the check's
text names `TraitsT::Character` (the session's §12.6). "That class's own root capsule" is read by the backend's bind
(both configurations take the character), not by a statement of `tryRegister`.

> Resolvability gate.
>
> ⭐ [movement-sim task 17] THE SOURCE IS THE MOVEMENT DECLARATION'S OWN `ownBodyId`, which is
> what `PendingRegistration::parentBodyId` used to hold and no longer exists to hold. The value
> is the same body — the descriptor's `isRoot` makes the factory ADOPT the pawn's root capsule,
> so the movement declaration's own id IS the capsule id (stated in full at the stamp above).
>
> ⚠ AND IT IS DELIBERATELY REDUNDANT WITH THE FOLD BELOW, which visits every declaration and
> therefore visits this one too. It is kept as the named, order-first read so the gate says out
> loud WHICH body a `Pending` is waiting on; it adds no guarantee the fold does not already give.

*R0, task 28:* the gate's first read is the hook `rootBodyId`, whose body is the movement declaration's own
`bindings.ownBodyId`; the fold below it visits every declaration, this one included, so the first read is still redundant
for the brawler, as the quote says (the session's §12.1 states it for any game).

### 7.4 The display's unregister note (was `OGBrawlerSimulationManager-rationale.md` §13.11)

> Same unregister contract for the display. ⛔ A kept ring is a leak keyed on a dead id. §10

*R0, task 28:* the display forget is `m_inputHistoryFeed.forgetCharacter(id)`, the second statement of the hook
`onUnregistered`; the feed forwards it to its store, which erases the rows and the lanes under one key
(`InputHistoryVisualizationUImpl-guards.md` G-04 and `InputHistoryDisplay-rationale.md` §8.1). The `§10` the note ends with was the actor's
section, now split between this §7 and the session's §12.
