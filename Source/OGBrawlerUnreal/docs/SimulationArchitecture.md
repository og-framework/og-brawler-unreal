<!-- SPDX-License-Identifier: BUSL-1.1 -->
# How the brawler's simulation fits together

A map for a programmer who knows Unreal and C++ and is new to this repository. It shows how the character, its two
simulation components, the manager actor, the session template and the og-simulation core connect, and in what order
they run. It explains no single class in depth: each class has a rationale document (the reasoning) and a guards
document (the prohibitions), listed in the table of §6.

The code is split in two halves by licence:

* **[plugin, MPL]** is game-agnostic, MPL-2.0, under Plugins/OGSimulation. The Unreal templates and components are in
  its OGSimulationUnreal module; the engine-free core (og-simulation) is in its OGSimulation module.
* **[brawler, BUSL]** is this game, BUSL-1.1, under Source/OGBrawlerUnreal.

---

## 1. The map

```
 USimulationManagerSubsystem                                    [brawler, BUSL]
   at world begin play: spawns ASimulationTimingRelay (authority only),
   then AOGBrawlerSimulationManager (every game world)

+------------------------------------------------------------------------------+
| AOGBrawlerUECharacter (an APawn, one per character)          [brawler, BUSL] |
|  +------------------------------------------------------------------------+  |
|  | UOGBrawlerSimulatableComponent                         [brawler, BUSL] |  |
|  |   CRTP base: UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, |  |
|  |              BrawlerSessionTraitsUImpl>                  [plugin, MPL] |  |
|  +------------------------------------------------------------------------+  |
|        | m_netSync (pointer)              ^ m_binding (ISimulatableOwner-    |
|        v                                  |            Binding interface)    |
|  +------------------------------------------------------------------------+  |
|  | USimulatableNetSyncComponent (replicated wire surface)   [plugin, MPL] |  |
|  +------------------------------------------------------------------------+  |
|  +------------------------------------------------------------------------+  |
|  | UOGBrawlerInputCollectionComponent (local input)       [brawler, BUSL] |  |
|  +------------------------------------------------------------------------+  |
+------------------------------------------------------------------------------+
        | registers through SessionOwner::instanceFor(isAuthority)->session()
        v
+------------------------------------------------------------------------------+
| AOGBrawlerSimulationManager (an AActor, one per world)       [brawler, BUSL] |
|  +------------------------------------------------------------------------+  |
|  | m_roster            BrawlerRosterUImpl                 [brawler, BUSL] |  |
|  | m_inputHistoryFeed  BrawlerInputHistoryFeedUImpl       [brawler, BUSL] |  |
|  | m_game              BrawlerSessionGameUImpl (hooks)    [brawler, BUSL] |  |
|  +------------------------------------------------------------------------+  |
|  +------------------------------------------------------------------------+  |
|  | m_session  UESimulationSession<BrawlerSessionTraitsUImpl>              |  |
|  |            calls m_game's hooks; holds the core, the physics backend,  |  |
|  |            input reception, UELatencyBudgetDiagnostics   [plugin, MPL] |  |
|  +------------------------------------------------------------------------+  |
+------------------------------------------------------------------------------+
        | listens to (three listener interfaces)         | drives
        v                                                v
+---------------------------------------------+  +-----------------------------+
| relay actors                  [plugin, MPL] |  | og-simulation core          |
|   ASimulationTimingRelay      one per world |  |               [plugin, MPL] |
|   ASimulationConnectionRelay  one per conn. |  |   SimulationManager,        |
|   ASimulationInputRelay       one per char. |  |   storage, reconciliation,  |
+---------------------------------------------+  |   input resolution, net sync|
                                                 +-----------------------------+
```

---

## 2. Who owns and creates what

* **The character**, `AOGBrawlerUECharacter` (the pawn of one simulated character), creates its components in its
  constructor with `CreateDefaultSubobject`, in this order: `SimulatableNetSync` (the `USimulatableNetSyncComponent`,
  the replicated wire surface), `BrawlerSimulatable` (the `UOGBrawlerSimulatableComponent`, the game's per-character
  simulation component) and `InputCollection` (the `UOGBrawlerInputCollectionComponent`, which collects the local
  player's input). In `PostInitializeComponents` it binds the first two:
  `BrawlerSimulatable->bindNetSync(*SimulatableNetSync)`.
* **The component's base**, `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>`, is a
  CRTP template that holds the registration loop, the netsync pointer and the typed ends netsync calls.
* **`USimulationManagerSubsystem`**, a world subsystem, spawns the manager in `OnWorldBeginPlay`. On an authority world it
  spawns the `ASimulationTimingRelay` (the world's clock relay) first.
* **`AOGBrawlerSimulationManager`**, the actor that owns the brawler's session, holds four members, constructed in
  declaration order: `m_roster` (the `BrawlerRosterUImpl`: character ids, spawn points, the ring-out scores),
  `m_inputHistoryFeed` (the `BrawlerInputHistoryFeedUImpl`: the input-history display's data; it stores a const reference
  to the session, which is constructed after it), `m_game` (the `BrawlerSessionGameUImpl`, which holds references to the
  roster and the feed) and `m_session`, which holds references to the actor and to `m_game`.
* **`UESimulationSession<BrawlerSessionTraitsUImpl>`** is the game-agnostic session: the og-simulation core (storage,
  reconciliation, input resolution, net sync, the systems executor and the `SimulationManager`, the core's step loop),
  the physics backend, the transport, the input reception and `UELatencyBudgetDiagnostics` (the latency stamps). Its
  `m_staticData` is built during the actor's construction by the hook `makeStaticData`.
* **`BrawlerSessionTraitsUImpl`** is the type list that ties the plugin templates to the brawler: the simulatable, its
  input and state, the component, the character, the session owner, the hooks class, the composition, the net config
  and the physics backend. `SimulatableBrawlerOwnerTraits.h` binds the core's owner concept to the component for both
  roles.
* **`instanceFor(isAuthority)`** finds the running session. The session keeps one slot for the authority and one for the
  client, filled in `UESimulationSession::beginPlay` and cleared in `UESimulationSession::endPlay`; `beginPlay` checks the
  slot is free. `AOGBrawlerSimulationManager::instanceFor` returns the session's owner. "Authority" means any world
  whose net mode is not `NM_Client`; the binding, the component and the session compute it that way.

**Where the hook bodies are.** `neutralInput` and the two latency hooks are inline in `BrawlerSessionGameUImpl.h`; the
other eleven hook bodies are in `BrawlerSessionGameUImpl.cpp`, inside its own `OGSIM_OPTIMIZE_OFF` region, so they
compile unoptimized in Development, like the actor's own members. The bodies need what the actor's header declares (the
complete actor and `LogOGBrawler`), so the `.cpp` includes `OGBrawlerSimulationManager.h`, right after its own header
(`BrawlerSessionGameUImpl-rationale.md` §3 and §4).

---

## 3. Lifecycle, in call order

**Session start.** `AOGBrawlerSimulationManager::BeginPlay` calls `Super::BeginPlay`, then `UESimulationSession::beginPlay`:

1. `m_game.beforeWorldBuild`: the gravity cross-check, then the roster seeds the level's spawn points.
2. The step-size check, then the latency diagnostics begin, the session knobs are read and the backend builds the world.
3. In the role's branch, on both roles: claim the instance slot and register the timing listener; emplace the
   integration layer; `Game::emplaceSystems` emplaces the brawler's systems; emplace the core `SimulationManager` (the
   authority's without prediction, the client's with it); set the loggers and `Game::neutralInput`. Then the authority
   applies the session knobs (publishing the relay delay floor to the timing relay), starts the tier consumer,
   registers the connection and input listeners and starts the reception coordinator; the client starts the tier
   consumer, registers the same two listeners and replays the relay values that arrived before it. Both then call
   `Game::installGameLoggers`.
4. The resim trigger policy, then `m_game.afterSessionConfigured` (the roster checks it was seeded), then the backend log
   line with `Game::backendFingerprint`, then the backend starts stepping.

**Registration.** Every character on every peer registers, on the instance its own world selects:

1. `UOGBrawlerSimulatableComponent::BeginPlay` calls `UESimulatableOwnerBinding::beginRegistration`, which schedules
   `tryInitializeWithManager` for the next tick.
2. `tryInitializeWithManager` looks up the session owner with `instanceFor(isAuthority)` and retries each tick until it
   exists (600 attempts, then a fatal check). It calls the component's hook `initializeWithManager`, which caches the
   input collection and registers a visualization query volume (`Retry` while the volume cannot be registered).
3. Next tick, `tryRegisterWithNewFramework` calls the component's hooks in order: `resolveRegistrationKey` (the
   authority allocates the character's `SimCharacterId` through the roster; the client retries until it has replicated),
   then `makeInputProvider` (only for a client's own, autonomous-proxy character), then `makeSimulatable`.
4. It calls `UESimulationSession::tryRegister` directly. The first call creates the bodies through the backend and calls
   `m_game.afterBodiesCreated` (the roster stamps the capsule body, seeds the teleport position and, on the authority,
   takes a spawn slot), then answers `Pending`.
   Later calls test `Game::rootBodyId` and every body for resolvability; once all resolve, the simulatable is registered
   with the core (the client's input provider wrapped by `UELatencyBudgetDiagnostics::wrapInputProvider`), then
   `m_game.afterRegistered` (the authority's character-cap warning), the systems executor's notify and the backend's
   `onRegistered_GameThread`. The binding retries every `Pending` on the next tick.
5. On `Ready` the binding links the input relay host (the authority spawns it with `ASimulationInputRelay::spawnForCharacter`;
   the client finds it with `ASimulationInputRelay::findForOwner`) and, on the authority, records the component as the
   delivery target for its remote input.

The component's fifth hook, `storageKeyOfOwner`, is the binding's key for netsync and the latency stamps. Both it and the
component's `EndPlay` read the replicated id through the component's helper `simCharacterId`.

**Each frame.** The backend calls the session before the steps (`onFrameStepsDue_GameThread`, and the authority's
release of delayed inputs) and after physics (`onPostPhysicsStep`): on Chaos from the physics scene's post-tick event in
`ChaosTickBackendUImpl.cpp`, on the tick driver from `BrawlerTickDriverGameUImpl::onPostPhysics` (the driver's brawler
side). The post-physics pass runs the backend's pre-present step, the core's post-simulation pass, the authority's
server-clock publish to the timing relay, the backend's visualization update, `Game::afterVisualization` (the authority
pushes ring-out scores) and the latency diagnostics' tick. Separately, the component's `TickComponent` draws its debug
visualization from the storage.

**Teardown.** No code here relies on an order between the manager's and the characters' `EndPlay`.

1. `UOGBrawlerSimulatableComponent::EndPlay` calls `AOGBrawlerSimulationManager::unregisterFromNewFramework` (skipped if
   `instanceFor` already returns null), which calls `UESimulationSession::unregisterFromNewFramework`: the executor's
   notify if the character is stored, the core unregister, the reception forget, `m_game.onUnregistered` (the roster and
   the input-history feed forget the id), the latency forget, then the backend's `onUnregistered_GameThread`.
2. Still in the component's `EndPlay`: if bound, `USimulatableNetSyncComponent::detachInputRelayHost` (the authority
   destroys the relay host; the client clears its callback), then `unbindNetSync`.
3. `AOGBrawlerSimulationManager::EndPlay` calls `UESimulationSession::endPlay` before `Super::EndPlay`: the backend's
   end-play start, the delayed-input map and the reception coordinator, the tier consumer, the instance slot and its
   three listeners, the latency diagnostics, then the backend's end of world.

---

## 4. Netsync

`USimulatableNetSyncComponent` is the character's reflected wire surface. A `UCLASS` cannot be a template, so the
replicated property, its notification and the RPC live in this one non-template class, which names no game type. It owns:

* the replicated correction-state buffer and its notification `OnRep_CorrectionState` (with the wire-format check);
* the unreliable input RPC `ServerReceiveRemoteMove`, sent by `sendInputBundleToAuthority`;
* the link to the character's `ASimulationInputRelay`, and two fallback rings for when no host is linked;
* the connection-tier send, `sendConnectionTierToOwningClient`, which writes the client's `ASimulationConnectionRelay`.

**Binding.** `bindNetSync` stores the pointer in the binding's `m_netSync` and calls
`USimulatableNetSyncComponent::attachBinding`, which stores the binding as `ISimulatableOwnerBinding`. Netsync reaches the
game only through that interface. `unbindNetSync` reverses both, in the component's `EndPlay`. The bind sits in the
character's `PostInitializeComponents` because a joining client can receive the first `OnRep_CorrectionState` before any
`BeginPlay` (`SimulatableNetSyncComponent-rationale.md` §3).

**Typed ends.** The binding implements the interface's five members: the key (from `storageKeyOfOwner`), the accepted
correction's read and stamp, the server's receive of an input bundle, the relayed ring's arrival stamp and the
ring-flush callback. It also gives the core's `SimulationNetSync` the owner calls it needs (the callback setters, the
correction buffer, `sendLocalInputToAuthority`), and the session its two input ends, `deliverDelayedRemoteInput` and
the staging write `stageRelayedInput`.

---

## 5. Server and client

* **Roles.** The authority runs the core without prediction and owns input reception; a client predicts every character
  and corrects each against its replicated correction state. In one process there is at most one of each.
* **Timing relay** (`ASimulationTimingRelay`): the authority publishes its server clock and relay delay floor; on the
  client its notifications reach the session through `ISimulationTimingRelayListener` (round-trip time, floor).
* **Connection relay** (`ASimulationConnectionRelay`): one per client connection, carrying that client's connection tier;
  on the client it reaches the session through `ISimulationConnectionRelayListener`, which sets the input delay.
* **Input relay** (`ASimulationInputRelay`): one per character, carrying its inputs to the clients that do not own it.
  A client's copy calls `ISimulationInputRelayListener::onInputRelayHostReady` from its `BeginPlay`, its owner's arrival
  and its ring notification; the session then finds the character's netsync with `FindComponentByClass` and links the
  host (the binding's own `findForOwner` link at registration covers the other arrival order).
* **Input reception.** A client's binding sends its pending inputs (`sendLocalInputToAuthority` → `ServerReceiveRemoteMove`).
  On the authority, the binding's `receiveRemoteMoveBundle` hands the bundle to the session's reception coordinator,
  which delays it, delivers it per step and stages it for the input relay.
* **Latency budget.** `UELatencyBudgetDiagnostics` stamps inputs and states at each hop and reports per window. It
  exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1 (not in Shipping).
* **Logging.** `UESimulationLogRoutes.h` declares the `LogOG` categories; `RouteOGMessage` sends core log lines to them.

Further reading: `UESimulationSession-rationale.md` §9 (transport), `UESimulatableOwnerBinding-rationale.md` §3 (the RPC
tail) and `UELatencyBudgetDiagnostics-rationale.md`.

---

## 6. Where to read next

Rationale and guards documents sit in the `docs` folder beside each module's sources.

| class | role | licence | docs | file |
|---|---|---|---|---|
| `AOGBrawlerUECharacter` | the pawn; creates and binds the components | brawler, BUSL | `OGBrawlerUECharacter-rationale.md` · `OGBrawlerUECharacter-guards.md` | `OGBrawlerUECharacter.h` |
| `UOGBrawlerSimulatableComponent` | per-character game component; registration hooks, visualization | brawler, BUSL | `OGBrawlerSimulatableComponent-rationale.md` · `OGBrawlerSimulatableComponent-guards.md` | `OGBrawlerSimulatableComponent.h` |
| `UOGBrawlerInputCollectionComponent` | samples local input, builds the player input | brawler, BUSL | none | `OGBrawlerInputCollectionComponent.h` |
| `UESimulatableOwnerBinding` | CRTP base: registration loop, netsync binding, typed ends | plugin, MPL | `UESimulatableOwnerBinding-rationale.md` · `UESimulatableOwnerBinding-guards.md` | `UESimulatableOwnerBinding.h` |
| `USimulatableNetSyncComponent` | replicated wire surface | plugin, MPL | `SimulatableNetSyncComponent-rationale.md` · `SimulatableNetSyncComponent-guards.md` | `SimulatableNetSyncComponent.h` |
| `USimulationManagerSubsystem` | spawns the timing relay and the manager | brawler, BUSL | none | `SimulationManagerSubsystem.h` |
| `AOGBrawlerSimulationManager` | session owner; holds the four members | brawler, BUSL | `OGBrawlerSimulationManager-rationale.md` · `OGBrawlerSimulationManager-guards.md` | `OGBrawlerSimulationManager.h` |
| `BrawlerRosterUImpl` | ids, spawn points, cap, ring-out scores | brawler, BUSL | `BrawlerRosterUImpl-rationale.md` · `BrawlerRosterUImpl-guards.md` | `BrawlerRosterUImpl.h` |
| `BrawlerInputHistoryFeedUImpl` | input-history display data | brawler, BUSL | `InputHistoryDisplay-rationale.md` · `InputHistoryVisualizationUImpl-guards.md` | `InputHistoryVisualizationUImpl.h` |
| `BrawlerSessionGameUImpl` | the hooks; eleven bodies in `BrawlerSessionGameUImpl.cpp`, three inline in its header | brawler, BUSL | `BrawlerSessionGameUImpl-rationale.md` · `BrawlerSessionGameUImpl-guards.md` | `BrawlerSessionGameUImpl.h` |
| `BrawlerSessionTraitsUImpl` | ties the templates to the brawler | brawler, BUSL | `BrawlerSessionTraitsUImpl-rationale.md` · `BrawlerSessionTraitsUImpl-guards.md` | `BrawlerSessionTraitsUImpl.h` |
| `UESimulationSession` | the session: core, backend, transport, sequences | plugin, MPL | `UESimulationSession-rationale.md` · `UESimulationSession-guards.md` | `UESimulationSession.h` |
| `BrawlerTickDriverGameUImpl` | the tick driver's brawler side | brawler, BUSL | `BrawlerTickDriverGameUImpl-rationale.md` · `BrawlerTickDriverGameUImpl-guards.md` | `BrawlerTickDriverGameUImpl.h` |
| `ChaosTickBackendUImpl` | the Chaos physics backend | brawler, BUSL | `ChaosTickBackendUImpl-rationale.md` · `ChaosTickBackendUImpl-guards.md` | `ChaosTickBackendUImpl.h` |
| `UELatencyBudgetDiagnostics` | latency stamps and reports | plugin, MPL | `UELatencyBudgetDiagnostics-rationale.md` · `UELatencyBudgetDiagnostics-guards.md` | `UELatencyBudgetDiagnostics.h` |
| `ASimulationTimingRelay` | server clock and relay delay floor | plugin, MPL | none | `SimulationTimingRelay.h` |
| `ASimulationConnectionRelay` | a connection's tier | plugin, MPL | none | `SimulationConnectionRelay.h` |
| `ASimulationInputRelay` | a character's relayed inputs | plugin, MPL | none | `SimulationInputRelay.h` |

---

## 7. Adding a second game

A second game supplies a traits struct like `BrawlerSessionTraitsUImpl`, a hooks class like `BrawlerSessionGameUImpl`
(the session checks each hook's signature at compile time), a physics backend named in the traits, a session-owner
actor that holds the session and offers `instanceFor` and `session()`, a character that creates the netsync component
and binds it, an owner component deriving from `UESimulatableOwnerBinding` with its registration hooks and
`storageKeyOfOwner`, and its specialization of the core's `SimulatableOwnerTraits`. It gets for free the
session's start, registration, per-frame and teardown sequences, the relays, netsync, input reception, the registration
retry loop and the latency diagnostics. Start with `UESimulationSession-rationale.md` §1 and
`UESimulatableOwnerBinding-rationale.md` §9.

---

## 8. Finding the core systems

The session spells its core types with og-simulation's own templates, so Go To Definition (F12) reaches the core. From
the manager: F12 on `BrawlerSessionTraitsUImpl` in `m_session`'s type, then on the template of a row of its core-system
map (`SimulationNetSync` in `NetSync = SimulationNetSync<SimulatableBrawler>`). From the session: F12 on the template.

| session member | alias | og-simulation template | header |
|---|---|---|---|
| `m_storage` | `Storage` | `SimulationObjectStorage` | `SimulationObjectStorage.h` |
| `m_reconciliation` | `Reconciliation` | `SimulationReconciliation` | `SimulationReconciliation.h` |
| `m_inputResolution` | `InputResolution` | `SimulationInputResolution` | `SimulationInputResolution.h` |
| `m_netSync` | `NetSync` | `SimulationNetSync` | `SimulationNetSync.h` |
| `m_manager` | `ManagerType` | `SimulationManager` | `SimulationManager.h` |
| `m_systemsExec` | `SystemsExec` | `SimulationSystemsExecutor`, over the game's `Systems` (two lines, below) | `SystemsExecutor.h` |
| `m_integrationLayer` | `IntegrationExec` | `SimulationIntegrationExecutor` | `SimulationIntegrationExecutor.h` |
| none: the engine contract | `EngineTraits` | the concept `PhysicsEngineTraits` | `PhysicsEngineTraits.h` |

The two executors are spelled the same way: an alias template that fixes the executor's leading parameters, and
`apply_t` to unpack the list. For the systems, in the session (`UESimulationSession.h`):

```cpp
template <typename... SystemTs>
using SystemsExecFor = SimulationSystemsExecutor<Simulatables, StaticData, SystemTs...>;
using SystemsExec     = apply_t<SystemsExecFor, typename Composition::Systems>;
```

The game's `Systems` is og-simulation's `SystemList`, in firing order (`SimulatableList.h`, beside `SimulatableList` and
`apply`); the brawler's composition declares `Systems = SystemList<HitDetectionSystem, brawlerHitRouting::System,
brawlerRingout::ScoreSystem>` and spells its own `SystemsExec` with the same two lines (its static data spelled
`simulatableBrawler::StaticData`).

**What stays the game's.** The simulatable list (`Simulatables`), the systems and their order (`Systems`), the static
data, the engine (`EngineTraits`) and the physics backend are the game's; it does not substitute the core systems or the
two executors, which the session spells from those choices (user rulings 2026-10-24). The session reads them from
`TraitsT` and `TraitsT::Composition`: the MPL plugin names no brawler type (the brawler may name og-simulation's, never
the reverse). The session asserts each core alias and executor against the composition's, the traits each map row
against `BrawlerCompositionUImpl` (`UESimulationSession-rationale.md` §13 and §14, `BrawlerSessionTraitsUImpl-rationale.md` §4).

**`PhysicsBackend`** is the physics backend host: it owns and steps the physics worlds and updates the visuals (Jolt
through the tick driver, or Chaos). The session holds it as `m_physicsBackend` and names it through the traits'
`PhysicsBackend<SessionT>`.
