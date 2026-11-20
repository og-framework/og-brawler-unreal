<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: ASimulationTimingRelay -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ASimulationInputRelay::spawnForCharacter -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ASimulationInputRelay::findForOwner -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ASimulationInputRelay -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ASimulationConnectionRelay -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationTimingRelayListener -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationConnectionRelayListener -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulationInputRelayListener::onInputRelayHostReady -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: findForOwner -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: SimulationTimingRelay.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: SimulationConnectionRelay.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: SimulationInputRelay.h -- deleted by og-input-latency task P5-9 (the relay actors, their listeners, the Iris sim buffers, serializers and proofs, and UEConnectionHandle); kept in the history -->
<!-- lint-external-ref: ISimulatableOwnerBinding -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: ServerReceiveRemoteMove -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSyncComponent-guards.md -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSyncComponent-rationale.md -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: SimulatableNetSyncComponent.h -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent::attachBinding -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent::detachInputRelayHost -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: bindNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: receiveRemoteMoveBundle -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: unbindNetSync -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
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
   at world begin play: spawns AOGBrawlerSimulationManager (every game world)

+------------------------------------------------------------------------------+
| AOGBrawlerUECharacter (an APawn, one per character)          [brawler, BUSL] |
|  +------------------------------------------------------------------------+  |
|  | UOGBrawlerSimulatableComponent                         [brawler, BUSL] |  |
|  |   CRTP base: UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, |  |
|  |              BrawlerSessionTraitsUImpl>                  [plugin, MPL] |  |
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
|  |            og-sim-net's server (authority), the lanes binding,         |  |
|  |            UELatencyBudgetDiagnostics                    [plugin, MPL] |  |
|  +------------------------------------------------------------------------+  |
+------------------------------------------------------------------------------+
        | binds to the driver's sim lanes                | drives
        v                                                v
+---------------------------------------------+  +-----------------------------+
| og-sim-net over og-net        [plugin, MPL] |  | og-simulation core          |
|   UESimNetLanes, owned by the og-net        |  |               [plugin, MPL] |
|   driver (UOGNetDriver): the transport, a   |  |   SimulationManager,        |
|   client's SimNetClient and its roster      |  |   storage, reconciliation,  |
|   mirror; one og-sim-net channel per        |  |   input resolution, net sync|
|   character (the core's owner of it)        |  |                             |
+---------------------------------------------+  +-----------------------------+
```

---

## 2. Who owns and creates what

* **The character**, `AOGBrawlerUECharacter` (the pawn of one simulated character), creates its components in its
  constructor with `CreateDefaultSubobject`, among them `BrawlerSimulatable` (the `UOGBrawlerSimulatableComponent`, the
  game's per-character simulation component) and then `InputCollection` (the `UOGBrawlerInputCollectionComponent`, which
  collects the local player's input). Until og-input-latency task P5-8 it also created `SimulatableNetSync` (the
  `USimulatableNetSyncComponent`, the replicated wire surface), first, and bound it to `BrawlerSimulatable` in
  `PostInitializeComponents` (`bindNetSync`); P5-8 deleted both (`OGBrawlerUECharacter-rationale.md` §15).
* **The component's base**, `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>`, is a
  CRTP template that holds the registration loop (`UESimulatableOwnerBinding-rationale.md` §9). Until P5-8 it also held
  the netsync pointer and the typed ends netsync called (its §10).
* **`USimulationManagerSubsystem`**, a world subsystem, spawns the manager in `OnWorldBeginPlay`, in every game world.
  Until og-input-latency task P5-7 an authority world also spawned the `ASimulationTimingRelay` (the world's clock relay)
  first.
* **`AOGBrawlerSimulationManager`**, the actor that owns the brawler's session, holds four members, constructed in
  declaration order: `m_roster` (the `BrawlerRosterUImpl`: character ids, spawn points, the ring-out scores),
  `m_inputHistoryFeed` (the `BrawlerInputHistoryFeedUImpl`: the input-history display's data; it stores a const reference
  to the session, which is constructed after it), `m_game` (the `BrawlerSessionGameUImpl`, which holds references to the
  roster and the feed) and `m_session`, which holds references to the actor and to `m_game`.
* **`UESimulationSession<BrawlerSessionTraitsUImpl>`** is the game-agnostic session: the og-simulation core (storage,
  reconciliation, input resolution, net sync, the systems executor and the `SimulationManager`, the core's step loop),
  the physics backend, the og-sim-net hookup (on the authority og-sim-net's server `SimNetServer`, which holds the input
  reception, and on both roles the binding to the og-net driver's sim lanes) and `UELatencyBudgetDiagnostics` (the
  latency stamps). Its `m_staticData` is built during the actor's construction by the hook `makeStaticData`.
* **`BrawlerSessionTraitsUImpl`** is the type list that ties the plugin templates to the brawler: the simulatable, its
  input and state, the component, the character, the session owner, the hooks class, the composition, the net config
  and the physics backend. `SimulatableBrawlerOwnerTraits.h` binds the core's owner concept to the character's og-sim-net
  channel for both roles (to the component until og-input-latency task P5-7).
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
3. In the role's branch, on both roles: claim the instance slot; emplace the integration layer; `Game::emplaceSystems`
   emplaces the brawler's systems; emplace the core `SimulationManager` (the authority's without prediction, the
   client's with it); set the loggers and `Game::neutralInput`. Then the authority binds og-sim-net (`beginServerSimNet`:
   it finds the og-net driver's sim lanes, emplaces og-sim-net's server and its host and binds the server's frame points
   to the lanes), applies the session knobs (publishing the relay delay floor through og-sim-net's server) and starts the
   tier consumer; the client starts the tier consumer, then binds itself to the lanes' client session
   (`beginClientSimNet`), which replays the relay delay floor and the connection tier that arrived before it. Without the
   og-net driver a networked world logs `[SimNet.Host] refused …`, shows a toast and runs detached. Both then call
   `Game::installGameLoggers`. (Until og-input-latency task P5-7 the branches registered the timing, connection and input
   relay listeners instead, and the authority started the reception coordinator itself.)
4. The resim trigger policy, then the latency knobs (`applyLatencyKnobs`), then `m_game.afterSessionConfigured` (the
   roster checks it was seeded), then the backend log line with `Game::backendFingerprint`, then the backend starts stepping.

**Registration.** Every character on every peer registers, on the instance its own world selects:

1. `UOGBrawlerSimulatableComponent::BeginPlay` calls `UESimulatableOwnerBinding::beginRegistration`, which schedules
   `tryInitializeWithManager` for the next tick.
2. `tryInitializeWithManager` looks up the session owner with `instanceFor(isAuthority)` and retries each tick until it
   exists (600 attempts, then a fatal check). It calls the component's hook `initializeWithManager`, which caches the
   input collection and registers a visualization query volume (`Retry` while the volume cannot be registered).
3. Next tick, `tryRegisterWithNewFramework` calls the component's hooks in order: `resolveRegistrationKey` (the
   authority allocates the character's `SimCharacterId` through the roster; the client retries until it has replicated
   and then until og-sim-net's roster has an entry for it, through the session's `clientBindStep`),
   then `makeInputProvider` (only for a client's own, autonomous-proxy character), then `makeSimulatable`.
4. It calls `UESimulationSession::tryRegister` directly. The first call creates the bodies through the backend and calls
   `m_game.afterBodiesCreated` (the roster stamps the capsule body, seeds the teleport position and, on the authority,
   takes a spawn slot), then answers `Pending`.
   Later calls test `Game::rootBodyId` and every body for resolvability; once all resolve, the authority adds the
   character to og-sim-net's server (`addSimNetCharacter`; a refusal answers `Refused`) and a client finds its channel
   in og-sim-net's client (`Pending` until it exists), and the simulatable is registered with the core, the channel as
   its owner (the client's input provider wrapped by `UELatencyBudgetDiagnostics::wrapInputProvider`), then
   `m_game.afterRegistered` (the authority's character-cap warning), the systems executor's notify and the backend's
   `onRegistered_GameThread`. The binding retries every `Pending` on the next tick.
5. On `Ready` the binding logs `NewFramework: registered`. Until og-input-latency task P5-7 it also linked the input
   relay host there (the authority spawned it with `ASimulationInputRelay::spawnForCharacter`; the client found it with
   `ASimulationInputRelay::findForOwner`) and, on the authority, recorded the component as the delivery target for its
   remote input.

The component's fifth hook, `storageKeyOfOwner`, is the key the binding names the owner by when the session owner never
spawns. Both it and the component's `EndPlay` read the replicated id through the component's helper `simCharacterId`.

**Each frame.** The backend calls the session before the steps (`onFrameStepsDue_Step`, and the authority's
release of delayed inputs) and after physics (`onPostPhysicsStep`): on Chaos from the physics scene's post-tick event in
`ChaosTickBackendUImpl.cpp`, on the tick driver from `BrawlerTickDriverGameUImpl::onPostPhysics` (the driver's brawler
side). The post-physics pass runs the backend's pre-present step, the core's post-simulation pass, the authority's
server-clock publish (`publishServerClock_GameThread`, to og-sim-net's server), the backend's visualization update,
`Game::afterVisualization` (the authority pushes ring-out scores) and the latency diagnostics' tick. The sim traffic
moves at the og-net driver's own points: after its receive dispatch and after its tick flush it calls the lanes, which
run og-sim-net's server's or client's `dispatchFrame` and `flushFrame` (`UESimNetLanes-rationale.md` §3). Separately, the
component's `TickComponent` draws its debug visualization from the storage.

**Teardown.** No code here relies on an order between the manager's and the characters' `EndPlay`.

1. `UOGBrawlerSimulatableComponent::EndPlay` calls `AOGBrawlerSimulationManager::unregisterFromNewFramework` (skipped if
   `instanceFor` already returns null), which calls `UESimulationSession::unregisterFromNewFramework`: the executor's
   notify if the character is stored, the core unregister, the og-sim-net forget (the authority removes the character
   from og-sim-net's server, a client marks it unregistered), `m_game.onUnregistered` (the roster and the input-history
   feed forget the id), the latency forget, then the backend's `onUnregistered_GameThread`. The call does nothing for an
   id the session already unregistered (a client's roster Remove of the character, or the og-net driver's stop), so
   `NewFramework: unregistered` prints once.
2. `UOGBrawlerSimulatableComponent::EndPlay` then calls `Super::EndPlay`. Until og-input-latency task P5-8 it also
   detached the input relay host here, if bound (`USimulatableNetSyncComponent::detachInputRelayHost`; P5-7 already
   removed that call), and then unbound the netsync component (`unbindNetSync`).
3. `AOGBrawlerSimulationManager::EndPlay` calls `UESimulationSession::endPlay` before `Super::EndPlay`: the backend's
   end-play start, the detach from the og-net driver's sim lanes, the reset of og-sim-net's server, its host and the
   registered characters' maps, the tier consumer, the instance slot, the latency diagnostics, then the backend's end of
   world.

---

## 4. Sim traffic: og-sim-net over og-net

Since og-input-latency task P5-7 every simulation payload (inputs, corrections, relayed inputs, the clock, the relay
delay floor, the connection tier and the roster) rides og-net's sim lanes, not Unreal replication:

* **The lanes.** The og-net driver, `UOGNetDriver` (OGNetUnreal), owns one lanes object per driver, made by the game's
  factory `makeBrawlerSimNetLanes` (`BrawlerSimNetLanesUImpl.cpp`), which the game module's `StartupModule` registers with
  `UOGNetDriver::SetSimLanesFactory`. The object is `UESimNetLanes<BrawlerSessionTraitsUImpl>` (OGSimulationUnreal): it
  holds og-sim-net's transport and, on a client, og-sim-net's client session `SimNetClient` with its host
  `UESimNetClientHost` (`UESimNetLanes-rationale.md`).
* **The session's hookup.** On the authority the session holds og-sim-net's server `SimNetServer` with its host
  `UESimNetServerHost` and binds the server's dispatch and flush to the lanes (`beginServerSimNet`); a client's session
  binds itself to the lanes' client session (`beginClientSimNet`). The hosts are og-sim-net's view of Unreal: the round
  trip, the latency stamps, the log and the toasts (`UESimNetHost-rationale.md`).
* **The core's owner.** The core's owner of a character, on both roles, is the character's og-sim-net channel
  (`UESimNetChannel`, named for both roles by `SimulatableBrawlerOwnerTraits.h`). The session gets it from og-sim-net's
  server or client when it registers the character (§3).

The switch is `UESimulationSession-rationale.md` §16. Until og-input-latency task P5-8 the character also had a reflected
wire surface, the non-template `USimulatableNetSyncComponent`: the replicated correction state and its notification
`OnRep_CorrectionState`, the input RPC `ServerReceiveRemoteMove`, the link to the character's `ASimulationInputRelay`
and the connection-tier send. The component bound it in the pawn's `PostInitializeComponents` (`bindNetSync`,
`USimulatableNetSyncComponent::attachBinding`) and reached the game through `ISimulatableOwnerBinding`; P5-8 deleted it,
and the binding's net half with it (`UESimulatableOwnerBinding-rationale.md` §10).

---

## 5. Server and client

* **Roles.** The authority runs the core without prediction and owns input reception; a client predicts every character
  and corrects each against the correction states og-sim-net's client delivers to its channel. In one process there is
  at most one of each.
* **Clock and floor.** The authority publishes its server clock each frame and the session's relay delay floor once,
  through og-sim-net's server (`publishClock`, `publishFloor`); on a client the client host hands them to the session
  (`onTimingInfoReceived`, `onRelayDelayFloorReceived`).
* **Connection tier.** og-sim-net's server sends each owning client its connection tier; the client host hands it to the
  session's `onConnectionTierReceived`, which sets the input delay.
* **Relayed inputs.** og-sim-net's server stages each character's arrivals (`RelayRingStager`) and sends the ring to the
  other clients (`flushRelays`); a client stores it in the character's channel (`storeRing`).
* **Input reception.** A client's channel sends its pending inputs as bundles on the SimInput lane. On the authority,
  og-sim-net's server checks each bundle against the roster, takes a round-trip sample and hands it to its reception
  coordinator, which delays it, delivers it per step (`releaseDelayedInputsForStep`) and stages it for the relay.
* **Roster.** og-sim-net's server gives every client each character's sim id, physics slot, owner and local index; a
  client registers a character only once the roster entry binds it (`clientBindStep`).
* **Latency budget.** `UELatencyBudgetDiagnostics` stamps inputs and states at each hop (the receipt and hand-off stamps
  through the sim-net hosts) and reports per window. It exists only while `OG_LATENCY_BUDGET_DIAGNOSTICS` is 1 (not in
  Shipping).
* **Logging.** `UESimulationLogRoutes.h` declares the `LogOG` categories; `RouteOGMessage` sends core log lines to them.

> **History.** Until og-input-latency task P5-9 three relay actors carried part of this: the timing relay
> (`ASimulationTimingRelay`, the clock and the floor, through `ISimulationTimingRelayListener`), one connection relay per
> client connection (`ASimulationConnectionRelay`, the tier, through `ISimulationConnectionRelayListener`), and one input
> relay per character (`ASimulationInputRelay`, the relayed inputs; a client's copy called
> `ISimulationInputRelayListener::onInputRelayHostReady`, and the binding's `findForOwner` link covered the other arrival
> order). Inputs reached the authority on the net-sync component's RPC (`sendLocalInputToAuthority` →
> `ServerReceiveRemoteMove` → the binding's `receiveRemoteMoveBundle`). Nothing spawned or wrote the actors after P5-7.

Further reading: `UESimulationSession-rationale.md` §16 (the switch) and §9 (the transport as it stood until P5-7),
`UESimNetLanes-rationale.md`, `UESimNetHost-rationale.md` and `UELatencyBudgetDiagnostics-rationale.md`.

---

## 6. Where to read next

Rationale and guards documents sit in the `docs` folder beside each module's sources.

| class | role | licence | docs | file |
|---|---|---|---|---|
| `AOGBrawlerUECharacter` | the pawn; creates the components | brawler, BUSL | `OGBrawlerUECharacter-rationale.md` · `OGBrawlerUECharacter-guards.md` | `OGBrawlerUECharacter.h` |
| `UOGBrawlerSimulatableComponent` | per-character game component; registration hooks, visualization | brawler, BUSL | `OGBrawlerSimulatableComponent-rationale.md` · `OGBrawlerSimulatableComponent-guards.md` | `OGBrawlerSimulatableComponent.h` |
| `UOGBrawlerInputCollectionComponent` | samples local input, builds the player input | brawler, BUSL | none | `OGBrawlerInputCollectionComponent.h` |
| `UESimulatableOwnerBinding` | CRTP base: the registration loop | plugin, MPL | `UESimulatableOwnerBinding-rationale.md` · `UESimulatableOwnerBinding-guards.md` | `UESimulatableOwnerBinding.h` |
| `USimulationManagerSubsystem` | spawns the manager | brawler, BUSL | none | `SimulationManagerSubsystem.h` |
| `AOGBrawlerSimulationManager` | session owner; holds the four members | brawler, BUSL | `OGBrawlerSimulationManager-rationale.md` · `OGBrawlerSimulationManager-guards.md` | `OGBrawlerSimulationManager.h` |
| `BrawlerRosterUImpl` | ids, spawn points, cap, ring-out scores | brawler, BUSL | `BrawlerRosterUImpl-rationale.md` · `BrawlerRosterUImpl-guards.md` | `BrawlerRosterUImpl.h` |
| `BrawlerInputHistoryFeedUImpl` | input-history display data | brawler, BUSL | `InputHistoryDisplay-rationale.md` · `InputHistoryVisualizationUImpl-guards.md` | `InputHistoryVisualizationUImpl.h` |
| `BrawlerSessionGameUImpl` | the hooks; eleven bodies in `BrawlerSessionGameUImpl.cpp`, three inline in its header | brawler, BUSL | `BrawlerSessionGameUImpl-rationale.md` · `BrawlerSessionGameUImpl-guards.md` | `BrawlerSessionGameUImpl.h` |
| `BrawlerSessionTraitsUImpl` | ties the templates to the brawler | brawler, BUSL | `BrawlerSessionTraitsUImpl-rationale.md` · `BrawlerSessionTraitsUImpl-guards.md` | `BrawlerSessionTraitsUImpl.h` |
| `UESimulationSession` | the session: core, backend, transport, sequences | plugin, MPL | `UESimulationSession-rationale.md` · `UESimulationSession-guards.md` | `UESimulationSession.h` |
| `BrawlerTickDriverGameUImpl` | the tick driver's brawler side | brawler, BUSL | `BrawlerTickDriverGameUImpl-rationale.md` · `BrawlerTickDriverGameUImpl-guards.md` | `BrawlerTickDriverGameUImpl.h` |
| `ChaosTickBackendUImpl` | the Chaos physics backend | brawler, BUSL | `ChaosTickBackendUImpl-rationale.md` · `ChaosTickBackendUImpl-guards.md` | `ChaosTickBackendUImpl.h` |
| `UELatencyBudgetDiagnostics` | latency stamps and reports | plugin, MPL | `UELatencyBudgetDiagnostics-rationale.md` · `UELatencyBudgetDiagnostics-guards.md` | `UELatencyBudgetDiagnostics.h` |
| `UESimNetLanes` | the og-net driver's sim lanes: og-sim-net's transport, a client's session | plugin, MPL | `UESimNetLanes-rationale.md` · `UESimNetLanes-guards.md` | `UESimNetLanes.h` |
| `UESimNetServerHost`, `UESimNetClientHost` | og-sim-net's hosts on Unreal: round trip, stamps, log | plugin, MPL | `UESimNetHost-rationale.md` · `UESimNetHost-guards.md` | `UESimNetHost.h` |

Until og-input-latency tasks P5-8 and P5-9 the table also listed `USimulatableNetSyncComponent` (the replicated wire
surface, `SimulatableNetSyncComponent.h`, with `SimulatableNetSyncComponent-rationale.md` and
`SimulatableNetSyncComponent-guards.md`) and the three relay actors `ASimulationTimingRelay` (`SimulationTimingRelay.h`),
`ASimulationConnectionRelay` (`SimulationConnectionRelay.h`) and `ASimulationInputRelay` (`SimulationInputRelay.h`).

---

## 7. Adding a second game

A second game supplies a traits struct like `BrawlerSessionTraitsUImpl`, a hooks class like `BrawlerSessionGameUImpl`
(the session checks each hook's signature at compile time), a physics backend named in the traits, a session-owner
actor that holds the session and offers `instanceFor` and `session()`, a character, an owner component deriving from
`UESimulatableOwnerBinding` with its registration hooks and `storageKeyOfOwner`, its specialization of the core's
`SimulatableOwnerTraits` (naming the og-sim-net channel for both roles), and a sim lanes factory registered with the
og-net driver (the brawler's `makeBrawlerSimNetLanes`). It gets for free the session's start, registration, per-frame
and teardown sequences, the sim traffic over og-net, input reception, the registration retry loop and the latency
diagnostics. Start with `UESimulationSession-rationale.md` §1 and
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
