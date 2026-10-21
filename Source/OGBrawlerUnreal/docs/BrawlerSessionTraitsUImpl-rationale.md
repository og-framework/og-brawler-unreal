<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionTraitsUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h`. The source file carries no prose: a two-line docs
pointer and code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the traits are and who reads them.
* **§2** is each member, as of og-ue-simulationcomponentandmanagersplit task 24a.
* **§3** is what the header includes, and why it is no longer light (task 24a).

---

## §1 What the traits are

`BrawlerSessionTraitsUImpl` is the brawler's type list for OGSimulationUnreal's game-agnostic session templates. A plugin
template takes it as `TraitsT` and names every brawler type through it, so the plugin header names none
(`UELatencyBudgetDiagnostics-rationale.md` §2, in OGSimulationUnreal's docs, is the first reader's list of what it uses).
It holds types only: no data, no function and no behaviour. Since task 24a one `static_assert` follows it (§2). The behaviour a template needs from the game is
`BrawlerSessionGameUImpl`'s static hooks, named here as `Game`.

Three templates read it: the session, `UESimulationSession<BrawlerSessionTraitsUImpl>`, which `ASimulationManagerUImpl`
holds as `m_session` (task 24a; `UESimulationSession-rationale.md`, in OGSimulationUnreal's docs); the session's latency
member, `UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>` (task 19a; the actor's member until task 24a); and the
brawler component's binding base, `UESimulatableOwnerBinding<USimmableUpdateComponent, BrawlerSessionTraitsUImpl>`
(task 21; `UESimulatableOwnerBinding-rationale.md` §2, in OGSimulationUnreal's docs). The initiative's later tasks add the
members the rest of the session needs; the design lists the full set.

---

## §2 The members

| member | brawler type | read by |
|---|---|---|
| `Simulatable` | `SimulatableBrawler` | the latency template: the storage walks and the two input-resolution reads. The binding: the reception coordinator's `receiveInputBundle` |
| `PlayerInput` | `simulatableBrawler::PlayerInput` | the latency template: the input provider's result type; `ASimulationManagerUImpl` asserts that the template's provider type is its own `BrawlerInputProviderFn`. The binding: the bundle and ring element type, the send queue, the remote-move callback and the stage |
| `State` | `simulatableBrawler::State` | the binding: the typed peek of an accepted correction (`onCorrectionStateAccepted`). Added by task 21 |
| `Component` | `USimmableUpdateComponent` | the latency template: the registering component and the delayed-input map's element. The session: that map's element and the two reception sinks' target, since task 26a |
| `Character` | `AOGBrawlerUECharacter` | no template yet. Added by task 21 with `State`, as the design's component list names it; its first reader will be the session template's relay-host hook, in a later task of og-ue-simulationcomponentandmanagersplit |
| `StaticData` | `simulatableBrawler::StaticData` | the session: the type of `m_staticData`. Added by task 24a |
| `SessionOwner` | `ASimulationManagerUImpl` | the session: its owner reference, `owner()`, and its friend line (until task 28: task 27b's finding F27b-1, the registration still reads private members). The latency template: its `instanceFor`, its `editLatencyBudgetDiagnostics()` (the actor's, which forwards to the session's), and, until task 26a, `m_delayedInputComponentsById`, read through the session's `owner()` (the session's own since); the actor befriended the template (`template <typename TraitsT> friend class UELatencyBudgetDiagnostics;`) until task 27b deleted the line, which no read needed since task 26a. The binding: `instanceFor` and `session()` on the authority, both public, and through the session its `getReceptionCoordinator` and `getServerReceptionTick` and the session as both sinks (the actor's own until task 26a). Since task 25 the actor's `instanceFor` forwards to the session's registry and returns the registered session's owner |
| `Game` | `BrawlerSessionGameUImpl` | the two input-sample hooks; the session's `m_game` reference and its `makeStaticData()` (task 24a); `afterVisualization` (task 27a); the six start-up hooks of the session's `beginPlay`, two of them called on `m_game` (task 27b) |
| `Composition` | `BrawlerCompositionUImpl` | the session: its storage, peers, systems executor, integration layer and manager types. Added by task 24a |
| `NetConfig` | `UEBrawlerNetConfig` | no template yet; the session's dt check will read it (a later task). Added by task 24a, as the design's list names it |
| `EngineTraits` | the configuration's `EngineTraits` (`PhysicsBackendUImpl.h`) | the session: the three adapter types. Added by task 24a |
| `BackendWorlds` | `physicsBackendUImpl::BackendWorlds` (`PhysicsBackendHostUImpl.h`) | the session: the type of `m_backendWorlds`. Added by task 24a |
| `Backend<SessionT>` | `physicsBackendUImpl::BackendFor<SessionT>`: `ChaosTickBackendUImpl`, or `TickDriverBackendUImpl` over `BrawlerTickDriverGameUImpl<SessionT>` | the session: the type of `m_backend`, and its friend line. Added by task 24a |
| `TickDriverGame<SessionT>` | `BrawlerTickDriverGameUImpl<SessionT>` | the session: its friend line, and the row that its manager type is this binding's `Manager`. Added by task 24a |
| `InputSampleSource` | `const UOGBrawlerInputCollectionComponent*` | what `inputSampleSource` returns at registration and `inputSampledSeconds` reads per step |

`SessionOwner` stays the actor: the session holds a reference to it and reaches the members that have not moved yet
through `owner()` (`UESimulationSession-rationale.md` §4). The latency member moved into the session in task 24a.

**The `PhysicsBackendHost` assertion.** After the struct, the header asserts that `Backend<UESimulationSession<
BrawlerSessionTraitsUImpl>>`, the type the session holds as `m_backend`, models the brawler concept `PhysicsBackendHost`
(`PhysicsBackendHostUImpl-rationale.md` §2). The concept is the brawler's, so the MPL session cannot name it; the row
was `ASimulationManagerUImpl::compositionContractsHold`'s until task 24a.

---

## §3 A light header

Until task 24a the header included only `OGBrawler/SimulatableBrawlerTypes.h` (for `PlayerInput` and `State`) and
forward-declared the rest: an alias to an incomplete class is legal, and the plugin templates use each type only where
their translation unit has the complete type.

**Since task 24a it includes four more headers,** because a member typedef needs a complete class or a declared template:
`BrawlerCompositionUImpl.h` (`Composition`'s members are named by the session), `UEBrawlerNetConfig.h`,
`PhysicsBackendHostUImpl.h` (the backend selector: `BackendWorlds`, `BackendFor` and the concept the assertion names) and
`BrawlerTickDriverGameUImpl.h` (`TickDriverGame`, in both configurations; the selector includes it only in the Jolt one).
The component's header includes this one, so every file that includes the component or the character now sees the
configuration's backend header too (the Chaos host's or the tick-driver host's, with its `extern template`). No file
includes this header that any of the four includes, so there is no cycle; the four forward-declare
`BrawlerSessionTraitsUImpl` and `UESimulationSession` where they name them. The incomplete types (`SessionOwner`,
`Component`, `Character`, `Game`) are still forward-declared.
