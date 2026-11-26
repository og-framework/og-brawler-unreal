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
        | registers through SessionOwner::instanceFor(world, isAuthority)->session()
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
|  |            the Unreal adapter: calls m_game's hooks; holds             |  |
|  |            og-sim-session's SessionRuntime (the core, the tick         |  |
|  |            backend, og-sim-net's server, the lanes binding)            |  |
|  |            and UELatencyBudgetDiagnostics                [plugin, MPL] |  |
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
* **`UESimulationSession<BrawlerSessionTraitsUImpl>`** is the game-agnostic Unreal adapter (since og-session-runtime task 31).
  It holds og-sim-session's `SessionRuntime`, which holds the og-simulation core (storage, reconciliation, input resolution,
  net sync, the systems executor and the `SimulationManager`, the core's step loop), the tick backend, the clock, the
  display, the lifecycle and the og-sim-net hookup (on the authority og-sim-net's server `SimNetServer`, which holds the
  input reception, and on both roles the binding to the og-net driver's sim lanes). The adapter keeps the Unreal parts: the
  frame host, the bodies, the registry, the component maps and `UELatencyBudgetDiagnostics` (the latency stamps)
  (`UESimulationSession-rationale.md` §0.1). The runtime's `m_staticData` is built during the actor's construction by the
  hook `makeStaticData`.
* **`BrawlerSessionTraitsUImpl`** is the type list that ties the plugin templates to the brawler: the simulatable, its
  input and state, the component, the character, the session owner, the hooks class, the composition, the net config
  and the physics engine's traits and binding (`EngineTraits`, `EngineBinding`; the backend selector went with
  og-session-runtime task 31). `SimulatableBrawlerOwnerTraits.h` binds the core's owner concept to the character's og-sim-net
  channel for both roles (to the component until og-input-latency task P5-7).
* **`instanceFor(world, isAuthority)`** finds the session of the caller's world. The session registry holds one slot per
  world (several in one editor process: a single-process PIE session), taken when `UESimulationSession::beginPlay` starts
  or refuses the world (the authority's on its runner, before its first batch) and
  released in `UESimulationSession::endPlay`; a second world of a role in one process starts no session
  (`UESimulationSession-rationale.md` §0.3, the runtime's `onConfigure` hook, and §23.1). `AOGBrawlerSimulationManager::instanceFor` returns the session's owner. "Authority" means any world
  whose net mode is not `NM_Client`; the binding, the component and the session compute it that way.

**Where the hook bodies are.** `neutralInput` and the two latency hooks are inline in `BrawlerSessionGameUImpl.h`; the
other eleven hook bodies are in `BrawlerSessionGameUImpl.cpp`, inside its own `OGSIM_OPTIMIZE_OFF` region, so they
compile unoptimized in Development, like the actor's own members. The bodies need what the actor's header declares (the
complete actor and `LogOGBrawler`), so the `.cpp` includes `OGBrawlerSimulationManager.h`, right after its own header
(`BrawlerSessionGameUImpl-rationale.md` §3 and §4).

---

## 3. Lifecycle, in call order

**Session start.** `AOGBrawlerSimulationManager::BeginPlay` calls `Super::BeginPlay`, then `UESimulationSession::beginPlay`.
First the start rule (`refuseToStart`): only a dedicated server and a client on the og-net driver start a session. A
standalone or listen-server world, a world whose driver is not og-net and a retired knob start none (a dedicated server
then exits); the session still takes its role's instance slot (`UESimulationSession-rationale.md` §23). Then:

1. `m_game.beforeWorldBuild`: the gravity cross-check, then the roster seeds the level's spawn points.
2. The step-size check, the latency diagnostics' begin, the `SessionStart` (the role, the step length, the caps, the
   authority's idle slack) and the static geometry's import (`UESimulationSession-rationale.md` §0.2).
3. og-sim-session's `SessionRuntime::start` runs the rest in the order `beginPlay` ran it until og-session-runtime task 31
   (`SessionRuntime-rationale.md` §2): it reads the session knobs; builds the world; claims the instance slot (the
   adapter's `onConfigure`); emplaces the integration layer, `Game::emplaceSystems`'s systems, the core
   `SimulationManager` (the authority's without prediction, the client's with it), the clock, the display, the net hooks and
   the lifecycle; sets the loggers and `Game::neutralInput`; on the authority it attaches og-sim-net (`attachServerLink`:
   og-sim-net's server and its host, the runner's RTT publish and join on the lanes) and applies the session knobs
   (publishing the relay delay floor through og-sim-net's server); on both roles it starts the tier consumer; a client then
   attaches to the lanes' client session (`attachClientLink`, which replays the relay delay floor and the connection tier
   that arrived before it). Both then install `Game::installGameLoggers`.
4. The resim trigger policy, the latency knobs (`applyLatencyKnobs`), `m_game.afterSessionConfigured` (the roster checks it
   was seeded), then the backend log line with `Game::backendFingerprint`.

A client runs steps 3 and 4 on the game thread, between the world build and the start of its stepping. On the authority the
runtime starts the runner and the game thread waits for it: the runner builds the world, runs steps 3 and 4, then steps
(`SessionRuntime-rationale.md` §2). (Until og-input-latency task P5-7 the branches registered the timing, connection and
input relay listeners instead, and the authority started the reception coordinator itself.)

**Registration.** Every character on every peer registers, on the instance its own world selects:

1. `UOGBrawlerSimulatableComponent::BeginPlay` calls `UESimulatableOwnerBinding::beginRegistration`, which schedules
   `tryInitializeWithManager` for the next tick.
2. `tryInitializeWithManager` looks up the session owner with `instanceFor(world, isAuthority)` and retries each tick until it
   exists (600 attempts, then a fatal check). It calls the component's hook `initializeWithManager`, which caches the
   input collection and registers a visualization query volume (`Retry` while the volume cannot be registered).
3. Next tick, `tryRegisterWithNewFramework` calls the component's hooks in order: `resolveRegistrationKey` (the
   authority allocates the character's `SimCharacterId` through the roster; the client retries until it has replicated
   and then until og-sim-net's roster has an entry for it, through the session's `clientBindStep`),
   then `makeInputProvider` (only for a client's own, autonomous-proxy character), then `makeSimulatable`.
4. It calls `UESimulationSession::tryRegister` directly. The first call has the bodies created and calls
   `m_game.afterBodiesCreated` (the roster stamps the capsule body, seeds the teleport position and, on the authority,
   takes a spawn slot), then answers `Pending`: a client creates them through the backend on the game thread, and the
   authority (every call through `tryRegisterOnRunner`) posts the body request to its runner.
   Later calls wait for every body to resolve (`Game::rootBodyId` first); then a client queues the character on
   og-sim-net's client and the authority's runner registers it on og-sim-net's server (a refusal answers `Refused`), and
   og-sim-net registers the simulatable with the core, its channel as its owner (the client's input provider wrapped by
   `UELatencyBudgetDiagnostics::wrapInputProvider`), and notifies the systems executor in its applied hook. On the next
   call after that the session runs `m_game.afterRegistered` (the authority's character-cap warning) and the backend's
   registration hook. The binding retries every `Pending` on the next tick. Until og-session-runtime task 14 an in-frame
   authority (a listen server, a standalone world) registered the character on the game thread in one call.
5. On `Ready` the binding logs `NewFramework: registered`. Until og-input-latency task P5-7 it also linked the input
   relay host there (the authority spawned it with `ASimulationInputRelay::spawnForCharacter`; the client found it with
   `ASimulationInputRelay::findForOwner`) and, on the authority, recorded the component as the delivery target for its
   remote input.

The component's fifth hook, `storageKeyOfOwner`, is the key the binding names the owner by when the session owner never
spawns. Both it and the component's `EndPlay` read the replicated id through the component's helper `simCharacterId`.

**Each frame.** The tick driver calls the session through og-sim-session's `SessionTickGame` (the tick driver's game since
og-session-runtime task 26; the brawler's binding BrawlerTickDriverGameUImpl forwarded to the session before;
the former Chaos backend, removed by og-session-runtime task 3, called it from the physics scene's post-tick event). On a
client, the game thread calls it before the steps (the frame probe, `onFrameStepsDue`) and after them (the adapter's
`onPostPhysics`), and each
step on a worker task drains og-sim-net's client at its start, sends the input bundle at capture and, after the batch,
publishes the display snapshot. On the authority the runner calls it per wake: og-sim-net's server drain before the
steps, the release of delayed inputs per step, and after the batch the net hooks' `afterBatch` (`SessionNetHooks`; the session's `afterAuthorityBatch` until task 26: the core's post-simulation
send, the clock, the corrections and the ring-out scores' presentation slices); og-net is serviced on the runner between
batches. The game thread's post-physics pass (`UESimulationSession::onPostPhysics`) presents the rendered poses and collects
the lifecycle outcomes; then on a client it refreshes the display view and calls `Game::afterVisualization`, and on the
authority it applies the presentation slices (`BrawlerSessionGameUImpl::presentAuthoritySlice` sets each character's ring-out score); then the
latency diagnostics' tick. Who calls what on which thread: og-sim-session's `SessionThreads-rationale.md` §7.
Separately, the component's `TickComponent` draws its debug visualization from the session's display snapshot.

**Teardown.** No code here relies on an order between the manager's and the characters' `EndPlay`.

1. `UOGBrawlerSimulatableComponent::EndPlay` calls `AOGBrawlerSimulationManager::unregisterFromNewFramework` (skipped if
   `instanceFor` already returns null), which calls `UESimulationSession::unregisterFromNewFramework`. The core
   unregister is a request to the role's owner thread: a client queues it for its next step's drain
   (og-sim-session's `SessionLifecycle::requestUnregister` since og-session-runtime task 29), the authority posts it to its runner, which removes the character from
   og-sim-net's server and releases its bodies (`requestRunnerUnregister_GameThread`). On the game thread the session then
   runs `m_game.onUnregistered` (the roster and the input-history feed forget the id), the latency forget and the
   backend's host-side release. The call does nothing for an id the session already unregistered (a client's roster
   Remove of the character, or the og-net driver's stop), so `NewFramework: unregistered` prints once.
2. `UOGBrawlerSimulatableComponent::EndPlay` then calls `Super::EndPlay`. Until og-input-latency task P5-8 it also
   detached the input relay host here, if bound (`USimulatableNetSyncComponent::detachInputRelayHost`; P5-7 already
   removed that call), and then unbound the netsync component (`unbindNetSync`).
3. `AOGBrawlerSimulationManager::EndPlay` calls `UESimulationSession::endPlay` before `Super::EndPlay`, which calls
   og-sim-session's `SessionRuntime::stop` first (`SessionRuntime-rationale.md` §3): the steps stopped (the tick functions
   unregistered, the authority's runner joined, the outstanding step waited for), a client's teardown drain, the detach
   from the og-net driver's sim lanes, the reset of og-sim-net's server and its host, the registered characters' maps, the
   tier consumer, the held display snapshot; then the adapter clears the instance slot and ends the latency diagnostics.

---

## 4. Sim traffic: og-sim-net over og-net

Since og-input-latency task P5-7 every simulation payload (inputs, corrections, relayed inputs, the clock, the relay
delay floor, the connection tier and the roster) rides og-net's sim lanes, not Unreal replication:

* **The lanes.** The og-net driver, `UOGNetDriver` (OGNetUnreal), owns one lanes object per driver, made by the game's
  factory `makeBrawlerSimNetLanes` (`BrawlerSimNetLanesUImpl.cpp`), which the game module's `StartupModule` registers with
  `UOGNetDriver::SetSimLanesFactory`. The object is `UESimNetLanes<BrawlerSessionTraitsUImpl>` (OGSimulationUnreal): it
  holds og-sim-net's `SimNetLink`: the transport and, on a client, og-sim-net's client session `SimNetClient` with its host
  `SimNetLinkClientHost` (`UESimNetLanes-rationale.md`, `SimNetLink-rationale.md`).
* **The session's hookup.** On the authority the session holds og-sim-net's server `SimNetServer` with its host
  `UESimNetLanesServerHost`, og-sim-net's `SimNetServerHostCore` over Unreal's port (`beginServerSimNet`), which its runner
  drains and flushes; a client's session binds itself to
  the lanes' client session (`beginClientSimNet`), which its steps drain. The hosts are og-sim-net's view of Unreal: the round
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
* **Clock and floor.** The authority publishes its server clock after each step batch and the session's relay delay floor once,
  through og-sim-net's server (`publishClock`, `publishFloor`); on a client the client host hands them to the session
  (`onTimingInfoReceived`, `onRelayDelayFloorReceived`).
* **Connection tier.** og-sim-net's server sends each owning client its connection tier; the client host hands it to the
  session's `onConnectionTierReceived`, which sets the input delay.
* **Relayed inputs.** og-sim-net's server stages each character's arrivals (`RelayRingStager`) and sends the ring to the
  other clients (`flushRelays`); a client stores it in the character's channel (`storeRing`).
* **Input reception.** A client's channel sends its pending inputs as bundles on the SimInput lane. On the authority,
  og-sim-net's server checks each bundle against the roster, takes a round-trip sample and hands it to its reception
  coordinator, which delays it, delivers it per step (`SessionNetHooks::releaseForTick`, og-sim-session's since og-session-runtime task 26) and stages it for the relay.
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
| `UESimulationSession` | the Unreal adapter over og-sim-session's `SessionRuntime` (which holds the core, the tick backend, the transport and the sequences) | plugin, MPL | `UESimulationSession-rationale.md` · `UESimulationSession-guards.md` | `UESimulationSession.h` |
| `SessionTickGame` | the tick driver's game: the hooks, calling the session's components in order | og-sim-session, MPL | `SessionTickGame-rationale.md` · `SessionTickGame-guards.md` | `SessionTickGame.h` |
| `BrawlerSessionHooksUImpl` | the session's brawler-side hooks: the input sample, gravity, the slot request | brawler, BUSL | `BrawlerSessionHooksUImpl-rationale.md` · `BrawlerSessionHooksUImpl-guards.md` | `BrawlerSessionHooksUImpl.h` |
| `UELatencyBudgetDiagnostics` | latency stamps and reports | plugin, MPL | `UELatencyBudgetDiagnostics-rationale.md` · `UELatencyBudgetDiagnostics-guards.md` | `UELatencyBudgetDiagnostics.h` |
| `UESimNetLanes` | the og-net driver's sim lanes: og-sim-net's transport, a client's session | plugin, MPL | `UESimNetLanes-rationale.md` · `UESimNetLanes-guards.md` | `UESimNetLanes.h` |
| `UESimNetServerHostPort`, `UESimNetLinkServices`, `UESimNetLinkSession` | og-sim-net's server host's port, and the link's services and session hooks, on Unreal: round trip, stamps, log | plugin, MPL | `UESimNetHost-rationale.md` · `UESimNetHost-guards.md` | `UESimNetHost.h` |

Until og-input-latency tasks P5-8 and P5-9 the table also listed `USimulatableNetSyncComponent` (the replicated wire
surface, `SimulatableNetSyncComponent.h`, with `SimulatableNetSyncComponent-rationale.md` and
`SimulatableNetSyncComponent-guards.md`) and the three relay actors `ASimulationTimingRelay` (`SimulationTimingRelay.h`),
`ASimulationConnectionRelay` (`SimulationConnectionRelay.h`) and `ASimulationInputRelay` (`SimulationInputRelay.h`).
Until og-session-runtime task 3 it also listed ChaosTickBackendUImpl, the Chaos physics backend, with its rationale and
guards documents; task 3 deleted all three.

---

## 7. Adding a second game

A second game supplies a traits struct like `BrawlerSessionTraitsUImpl`, a hooks class like `BrawlerSessionGameUImpl`
(the session checks each hook's signature at compile time), the physics engine's traits and binding named in the traits
(`EngineTraits`, `EngineBinding`; a physics backend until og-session-runtime task 31), a session-owner
actor that holds the session and offers `instanceFor` and `session()`, a character, an owner component deriving from
`UESimulatableOwnerBinding` with its registration hooks and `storageKeyOfOwner`, its specialization of the core's
`SimulatableOwnerTraits` (naming the og-sim-net channel for both roles), and a sim lanes factory registered with the
og-net driver (the brawler's `makeBrawlerSimNetLanes`). It gets for free the session's start, registration, per-frame
and teardown sequences, the sim traffic over og-net, input reception, the registration retry loop and the latency
diagnostics. Start with `UESimulationSession-rationale.md` §0, og-sim-session's `SessionRuntime-rationale.md` and
`UESimulatableOwnerBinding-rationale.md` §9.

---

## 8. Finding the core systems

The session spells its core types with og-simulation's own templates, so Go To Definition (F12) reaches the core. From
the manager: F12 on `BrawlerSessionTraitsUImpl` in `m_session`'s type, then on the template of a row of its core-system
map (`SimulationNetSync` in `NetSync = SimulationNetSync<SimulatableBrawler>`). From the session: F12 on the template.
Since og-session-runtime task 31 the members and aliases below are og-sim-session's `SessionRuntime`'s
(`SessionRuntime.h`), which the Unreal session holds as `m_runtime`; they kept their names.

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
`apply_t` to unpack the list. For the systems, in the runtime (`SessionRuntime.h`; `UESimulationSession.h` until
og-session-runtime task 31):

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
against `BrawlerCompositionUImpl` (`UESimulationSession-rationale.md` §13 and §14, the record; since og-session-runtime
task 31 the runtime's `compositionContractsHold()` in `SessionRuntime.h`; `BrawlerSessionTraitsUImpl-rationale.md` §4).

**The tick backend** owns and steps the physics worlds and updates the visuals (Jolt through the tick driver; the Chaos
backend was removed by og-session-runtime task 3). Since og-session-runtime task 31 it is og-sim-session's `SessionRuntime`'s
member `m_tickBackend` (og-simulation's `SimulationTickBackend`), and the engine's bodies are `TickDriverBackendUImpl`, which
`UESessionRuntimeTraits` names; until then the Unreal session held a backend host the traits named.

---

## Retired guard ids of the game's session docs (og-session-runtime tasks 23 to 31a)

The game's two, both by task 26 (each file's `§R` has the entry):

| Task | Retired id | Successor |
|---|---|---|
| 26 | `BrawlerSessionGameUImpl-guards.md` G-01 | og-brawler's `BrawlerSessionGame-guards.md` G-01 |
| 26 | `BrawlerSessionHooksUImpl-guards.md` G-01 | og-sim-session's `SessionTickGame-guards.md` G-01 |

The plugin's 35 are in OGSimulationUnreal's `UESimulationSession-rationale.md` §R.
