<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: BrawlerInputProviderFn -- retired alias: removed from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 36; named in §2's record of the assertion it carried -->
# `BrawlerSessionTraitsUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h`. The source file carries no prose: a two-line docs
pointer and code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the traits are and who reads them.
* **§2** is each member, as of og-ue-simulationcomponentandmanagersplit task 24a.
* **§3** is what the header includes, and why it is no longer light (task 24a).
* **§4** is the core-system map (task 34): six rows spelled with og-simulation's templates, and their checks.
* **§5** is the rename of the backend members to `PhysicsBackend` and `PhysicsBackendWorlds` (task 35).

---

## §1 What the traits are

`BrawlerSessionTraitsUImpl` is the brawler's type list for OGSimulationUnreal's game-agnostic session templates. A plugin
template takes it as `TraitsT` and names every brawler type through it, so the plugin header names none
(`UELatencyBudgetDiagnostics-rationale.md` §2, in OGSimulationUnreal's docs, is the first reader's list of what it uses).
It holds types only: no data, no function and no behaviour. Since task 24a one `static_assert` follows it (§2), and
since task 34 six more, one per row of its core-system map (§4). The behaviour a template needs from the game is
`BrawlerSessionGameUImpl`'s static hooks, named here as `Game`.

Three templates read it: the session, `UESimulationSession<BrawlerSessionTraitsUImpl>`, which `AOGBrawlerSimulationManager`
holds as `m_session` (task 24a; `UESimulationSession-rationale.md`, in OGSimulationUnreal's docs); the session's latency
member, `UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>` (task 19a; the actor's member until task 24a); and the
brawler component's binding base, `UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>`
(task 21; `UESimulatableOwnerBinding-rationale.md` §2, in OGSimulationUnreal's docs). The initiative's later tasks add the
members the rest of the session needs; the design lists the full set.

---

## §2 The members

| member | brawler type | read by |
|---|---|---|
| `Simulatable` | `SimulatableBrawler` | the latency template: the storage walks and the two input-resolution reads. The binding: the reception coordinator's `receiveInputBundle` |
| `PlayerInput` | `simulatableBrawler::PlayerInput` | the latency template: the input provider's result type; the binding's hook check requires the brawler's `makeInputProvider` to return the template's provider type (`AOGBrawlerSimulationManager` also asserted that type was its own `BrawlerInputProviderFn` until og-ue-simulationcomponentandmanagersplit task 36 removed the alias). The binding: the bundle and ring element type, the send queue, the remote-move callback and the stage |
| `State` | `simulatableBrawler::State` | the binding: the typed peek of an accepted correction (`onCorrectionStateAccepted`). Added by task 21 |
| `Component` | `UOGBrawlerSimulatableComponent` | the latency template: the registering component and the delayed-input map's element. The session: that map's element and the two reception sinks' target, since task 26a |
| `Character` | `AOGBrawlerUECharacter` | no template yet. Added by task 21 with `State`, as the design's component list names it; its first reader will be the session template's relay-host hook, in a later task of og-ue-simulationcomponentandmanagersplit |
| `StaticData` | `simulatableBrawler::StaticData` | the session: the type of `m_staticData`. Added by task 24a |
| `SessionOwner` | `AOGBrawlerSimulationManager` | the session: its owner reference, `owner()`, and its friend line (until task 28: task 27b's finding F27b-1, the registration still reads private members). The latency template: its `instanceFor`, its `editLatencyBudgetDiagnostics()` (the actor's, which forwards to the session's), and, until task 26a, `m_delayedInputComponentsById`, read through the session's `owner()` (the session's own since); the actor befriended the template (`template <typename TraitsT> friend class UELatencyBudgetDiagnostics;`) until task 27b deleted the line, which no read needed since task 26a. The binding: `instanceFor` and `session()` on the authority, both public, and through the session its `getReceptionCoordinator` and `getServerReceptionTick` and the session as both sinks (the actor's own until task 26a). Since task 25 the actor's `instanceFor` forwards to the session's registry and returns the registered session's owner |
| `Game` | `BrawlerSessionGameUImpl` | the two input-sample hooks; the session's `m_game` reference and its `makeStaticData()` (task 24a); `afterVisualization` (task 27a); the six start-up hooks of the session's `beginPlay`, two of them called on `m_game` (task 27b) |
| `Composition` | `BrawlerCompositionUImpl` | the session: its simulatable list and, since task 35, its systems list (`Systems`), from which it spells its own executors, and the two executors and five core-system aliases it checks its own against (§4; until task 34 it took its storage, peer and manager types from here too, and until task 35 its systems executor and integration layer types). Added by task 24a |
| `NetConfig` | `UEBrawlerNetConfig` | no template yet; the session's dt check will read it (a later task). Added by task 24a, as the design's list names it |
| `EngineTraits` | the configuration's `EngineTraits` (`PhysicsBackendUImpl.h`) | the session: the three adapter types, its integration executor's adapters (task 35), and the `PhysicsEngineTraits` assertion (task 35). Added by task 24a |
| `PhysicsBackendWorlds` | `physicsBackendUImpl::BackendWorlds` (`PhysicsBackendHostUImpl.h`) | the session: the type of `m_physicsBackendWorlds`. Added by task 24a as `BackendWorlds`; renamed by task 35 (§5) |
| `PhysicsBackend<SessionT>` | `physicsBackendUImpl::BackendFor<SessionT>`: `ChaosTickBackendUImpl`, or `TickDriverBackendUImpl` over `BrawlerTickDriverGameUImpl<SessionT>` | the session: the type of `m_physicsBackend`, and its friend line. Added by task 24a as `Backend<SessionT>`; renamed by task 35 (§5) |
| `TickDriverGame<SessionT>` | `BrawlerTickDriverGameUImpl<SessionT>` | the session: its friend line, and the row that its manager type is this binding's `Manager`. Added by task 24a |
| `InputSampleSource` | `const UOGBrawlerInputCollectionComponent*` | what `inputSampleSource` returns at registration and `inputSampledSeconds` reads per step |

`SessionOwner` stays the actor: the session holds a reference to it and reaches the members that have not moved yet
through `owner()` (`UESimulationSession-rationale.md` §4). The latency member moved into the session in task 24a.

**The `PhysicsBackendHost` assertion.** After the struct, the header asserts that `PhysicsBackend<UESimulationSession<
BrawlerSessionTraitsUImpl>>`, the type the session holds as `m_physicsBackend`, models the brawler concept `PhysicsBackendHost`
(`PhysicsBackendHostUImpl-rationale.md` §2). The concept is the brawler's, so the MPL session cannot name it; the row
was `AOGBrawlerSimulationManager::compositionContractsHold`'s until task 24a.

---

## §3 A light header

Until task 24a the header included only `OGBrawler/SimulatableBrawlerTypes.h` (for `PlayerInput` and `State`) and
forward-declared the rest: an alias to an incomplete class is legal, and the plugin templates use each type only where
their translation unit has the complete type.

**Since task 24a it includes four more headers,** because a member typedef needs a complete class or a declared template:
`BrawlerCompositionUImpl.h` (`Composition`'s members are named by the session, and since task 34 by the map of §4, whose
og-simulation templates it brings too), `UEBrawlerNetConfig.h`,
`PhysicsBackendHostUImpl.h` (the backend selector: `BackendWorlds`, `BackendFor` and the concept the assertion names) and
`BrawlerTickDriverGameUImpl.h` (`TickDriverGame`, in both configurations; the selector includes it only in the Jolt one).
The component's header includes this one, so every file that includes the component or the character now sees the
configuration's backend header too (the Chaos host's or the tick-driver host's, with its `extern template`). No file
includes this header that any of the four includes, so there is no cycle; the four forward-declare
`BrawlerSessionTraitsUImpl` and `UESimulationSession` where they name them. The incomplete types (`SessionOwner`,
`Component`, `Character`, `Game`) are still forward-declared.

---

## §4 The core-system map (task 34)

The struct ends with six rows that name the session's core systems directly, so Go To Definition from the brawler reaches
og-simulation in two steps: F12 on `BrawlerSessionTraitsUImpl` (in the manager's `m_session` type), then F12 on a row's
template. No row changes an existing member.

| row | spelling | asserted equal to |
|---|---|---|
| `Storage` | `SimulationObjectStorage<SimulatableBrawler>` | `BrawlerCompositionUImpl::Storage` |
| `Reconciliation` | `SimulationReconciliation<SimulatableBrawler>` | `BrawlerCompositionUImpl::Reconciliation` |
| `InputResolution` | `SimulationInputResolution<SimulatableBrawler>` | `BrawlerCompositionUImpl::InputResolution` |
| `NetSync` | `SimulationNetSync<SimulatableBrawler>` | `BrawlerCompositionUImpl::NetSync` |
| `SystemsExec` | `BrawlerCompositionUImpl::SystemsExec` | `BrawlerCompositionUImpl::SystemsExec` |
| `SimulationCore` | `BrawlerCompositionUImpl::Manager` | `BrawlerCompositionUImpl::Manager` |

* **A reader's map, read by no template.** The session spells its own core types (`UESimulationSession-rationale.md`
  §13, in OGSimulationUnreal's docs) and reads none of these rows. `SystemsExec` and `SimulationCore` are spelled as the
  composition's own, because the systems are the brawler's (the composition lists them) and the manager is
  og-simulation's `SimulationManager` over the brawler's systems and adapters: F12 on them opens the composition, whose
  `Manager` names `SimulationManager`.
* **The checks.** One `static_assert` per row, after the struct, asserts that the row names the same type as the
  composition's alias. The session asserts its own five derived aliases against the same composition aliases
  (`compositionContractsHold()`), and since task 35 its `SystemsExec` and `IntegrationExec`, which it spells from the
  composition's lists (until then its `SystemsExec` was the composition's by definition), so a row, the composition and
  the session agree. Each message states why: the session requires og-simulation's own core systems (user ruling
  2026-10-24), and a game supplies its systems. *Seen failing (task 34):* `Reconciliation` spelled
  `SimulationInputResolution<SimulatableBrawler>` fails the build with that row's message (`C2338`) and nothing else.
* **Why the checks name the composition, not the session.** Task 34 asked for each row to be asserted against
  `UESimulationSession<BrawlerSessionTraitsUImpl>`'s alias, and allowed the composition where the session is
  incomplete. This header only forward-declares the session template, and it may not include the session's header for
  the sake of a check, so whether the session is complete here depends on what the including file included first. And
  the aliases are private (`UESimulationSession-rationale.md` §4), so only the session and its three friends can name
  them. *Seen failing (task 34):* the row `Storage` asserted against the session's `Storage` here fails the build with
  `C2248` ("cannot access private typedef"; in the game's unity file the session is already complete at this header).
  The chain is therefore closed in two halves: here, each row against the composition; in the session, where the class
  is complete, each alias against the same composition alias.

---

## §5 `PhysicsBackend` and `PhysicsBackendWorlds` (task 35)

**What changed.** The member template `Backend<SessionT>` is `PhysicsBackend<SessionT>`, and `BackendWorlds` is
`PhysicsBackendWorlds` (user, 2026-10-24: "rename Backend so that it is clear that we mean the Physics backend"). Each still
names the selector's type: `physicsBackendUImpl::BackendFor<SessionT>` and `physicsBackendUImpl::BackendWorlds`
(`PhysicsBackendHostUImpl.h`). The session renamed its alias and its two members with them
(`UESimulationSession-rationale.md` §14, in OGSimulationUnreal's docs), and the `PhysicsBackendHost` assertion and its
message name `PhysicsBackend<UESimulationSession<BrawlerSessionTraitsUImpl>>` and `m_physicsBackend`. The first block of
aliases is re-padded so its `=` still lines up after the longer name (whitespace only).

**What did not change.** The selector's own names (`physicsBackendUImpl::Backend`, `BackendFor`, `BackendWorlds`) are
namespace-qualified and already say which backend they are, so they keep their spelling; so do the `[SimHost.Backend]` log
tag, the game hook `backendFingerprint` and every type: the two rows name the same types as before, so no code changed.
