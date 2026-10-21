<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: bGlobalGravitySet -- Unreal Engine AWorldSettings field, outside every scan root (§6.1; moved with SimulationManagerUImpl-rationale.md §13.2, task 27b) -->
<!-- lint-external-ref: GlobalGravityZ -- Unreal Engine AWorldSettings field, outside every scan root (§6.1; moved with SimulationManagerUImpl-rationale.md §13.2, task 27b) -->
<!-- lint-external-ref: m_movementStaticDataCVars -- retired member: the actor's cvar member that makeStaticData replaced (og-ue-simulationcomponentandmanagersplit task 24a); named in section 4 to say what the hook folded -->
# `BrawlerSessionGameUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionGameUImpl.h`. The source file carries no prose: a two-line docs
pointer and code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the class is.
* **§2** is the two latency hooks and the brawler's input-sample time (hop `H1`'s start).
* **§4** is the static-data hook, `makeStaticData` (task 24a).
* **§5** is the post-visualization hook, `afterVisualization` (task 27a): the ring-out score push.
* **§6** is the six start-up hooks (task 27b), with the gravity cross-check and the neutral input's guard.
* **§3** is how the header is compiled and who includes it.

---

## §1 What the class is

`BrawlerSessionGameUImpl` holds the brawler's **phase hooks**: the statements a game-agnostic OGSimulationUnreal template
calls at a fixed point of its own sequence, where the brawler-specific statement used to be. The templates call them
statically through `BrawlerSessionTraitsUImpl::Game` (no virtual call): most are static member functions; since task 27b
two are members of the one object, `m_game`, because they reach the brawler's roster (§6). Each hook body is the brawler's
statement, verbatim; the hook sits at the exact position of the statement it replaces.

As of og-ue-simulationcomponentandmanagersplit task 19a it had two hooks, both for the latency template
`UELatencyBudgetDiagnostics`. og-ue-simulationcomponentandmanagersplit task 24a added the first of the session's, `makeStaticData` (§4), and the actor
holds one instance, `m_game`, which the session holds by reference. Task 27a added the second, `afterVisualization` (§5).
Task 27b added the six of the session's start-up (§6) and the class's one piece of state, a reference to the actor's
roster, bound when the actor constructs `m_game{ m_roster }` (the roster is declared first, so it is constructed first and
destroyed last). The design's other four arrive with the registration (task 28).

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

`H1` and `H2` are measured for a remote client's own characters only: `tryRegister` wraps the provider on the client
branch, and a listen host's local players have no client provider.

---

## §3 How the header is compiled

* **Inline, in one `OGSIM_OPTIMIZE_OFF` / `OGSIM_OPTIMIZE_ON` pair.** The hook bodies are the statements that compiled in
  the latency class's own translation unit under that pair, so they keep its optimization state; the declarations sit
  above the pair.
* **Who includes it.** `SimulationManagerUImpl.cpp`, the one file that instantiates the wrapper (`tryRegister` calls
  `wrapInputProvider`), and, since task 24a, `SimulationManagerUImpl.h`: the actor holds `m_game` by value, and the
  session's constructor, which calls `makeStaticData()`, is instantiated wherever the actor's constructors are (its `.cpp`
  and the reflection code's helper constructor). The header needs the complete component and input collection
  component, so it includes both; every file that includes the manager now sees them. `BrawlerSessionTraitsUImpl.h`
  still only forward-declares `BrawlerSessionGameUImpl`.
* **`makeStaticData` is declared here and defined in `SimulationManagerUImpl.cpp`** (§4), and so, since task 27a, is
  **`afterVisualization`** (§5). Its parameter names the session, so the header declares the session template (the
  traits header does too). Since task 27b five of the six start-up hooks are defined there too, in its `OGSIM_OPTIMIZE_OFF`
  region (one needs `LogOGBrawler`, which the actor's header declares after it includes this one; all five keep the
  unoptimized state the statements had in the actor's `BeginPlay`). The sixth, `neutralInput()`, is inline here, in this
  header's pair, because its guard is this file's (§6).
* **Since task 27b** the header forward-declares the roster (the class holds a reference to it) and `UWorld`, and includes
  the engine's base types header (for `ENetMode`), `<optional>` and `<string_view>`; the hook `emplaceSystems` names the
  composition's `SystemsExec` and the engine traits' two adapters, which the traits header already brings in.

---

## §4 The static-data hook, `makeStaticData` (og-ue-simulationcomponentandmanagersplit task 24a)

| hook | brawler statements | when, which thread |
|---|---|---|
| `static makeStaticData()` | `ASimulationManagerUImpl::readMovementStaticDataCVars()`, then the brawler's `StaticData` built from its five fields (the model, the walk speed, the step period, the step speed and the gravity), returned as a prvalue | once per session, in the session's member initializer of `m_staticData`, during the actor's construction, on the game thread |

* **The statements are the actor's, in its order.** Before task 24a the actor's member `m_movementStaticDataCVars` read
  the cvars in its initializer, and `m_staticData`'s initializer built the static data from it, right after `m_storage`.
  The session initializes `m_staticData` from this hook at the same position, so the read and the construction run at
  the same point of construction as before, and the cvar result is a local of the hook instead of a member.
* **In place.** `simulatableBrawler::StaticData` deletes its copy and move operations. The hook returns a prvalue and the
  session initializes its member from it, so the object is constructed once, in the session (guaranteed copy elision).
* **Friendship.** `readMovementStaticDataCVars()` is a private static of the actor, defined in `MovementSchemeCVar.cpp`
  (not edited); the actor befriends this class so the hook can call it.
* **Defined in `SimulationManagerUImpl.cpp`, in its `OGSIM_OPTIMIZE_OFF` region.** The hook needs the complete actor,
  and this header is included by the actor's header before the actor is defined, so the body cannot be inline here.
  As a non-template function in that region it compiles unoptimized in Development, as the two initializers did in the
  actor's constructor (the session's constructor itself is a template and compiles optimized; lead ruling F1).

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
  thread; why that tear is accepted is `SimulationManagerUImpl-rationale.md` §1 ("The second read crossing") and the
  `checkf` it rests on is `BrawlerRosterUImpl-rationale.md` §3.
* **Defined in `SimulationManagerUImpl.cpp`, in its `OGSIM_OPTIMIZE_OFF` region,** where the actor's `OnPostPhysicsStep`
  was, as `makeStaticData` is (§4). As a non-template function there it compiles unoptimized in Development, as the
  statements did in the actor; the session's `onPostPhysicsStep`, a template, compiles optimized (lead ruling F1).

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
  brawler's verification in `SimulationManagerUImpl-rationale.md` §2.
* **`neutralInput()` and its guard (G-01, ruling Q-J).** The value is `getZeroPlayerInput()`, never `PlayerInput{}`: the
  brawler's zero input aims at `(0,0,1)`, and a value-initialized input does not. The session sets it on both roles; why the
  authority needs it too is the session's (`UESimulationSession-rationale.md` §11.3, `UESimulationSession-guards.md` G-23).
  The guard stays here because it guards the value, not the sequence.
* **Compiled** as §3 says: five in `SimulationManagerUImpl.cpp`'s `OGSIM_OPTIMIZE_OFF` region, `neutralInput()` inline in
  this header's pair, so every statement keeps the unoptimized state it had in the actor (ruling F1 governs only the
  session's template; task 27b's notes check the gravity check's floating point).

### 6.1 The gravity cross-check (was `SimulationManagerUImpl-rationale.md` §13.2)

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
