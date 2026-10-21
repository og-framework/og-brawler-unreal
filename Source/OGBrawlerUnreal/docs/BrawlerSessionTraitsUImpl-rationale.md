<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerSessionTraitsUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h`. The source file carries no prose: a two-line docs
pointer and code.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

* **§1** says what the traits are and who reads them.
* **§2** is each member, as of og-ue-simulationcomponentandmanagersplit task 21.
* **§3** is how the header stays light.

---

## §1 What the traits are

`BrawlerSessionTraitsUImpl` is the brawler's type list for OGSimulationUnreal's game-agnostic session templates. A plugin
template takes it as `TraitsT` and names every brawler type through it, so the plugin header names none
(`UELatencyBudgetDiagnostics-rationale.md` §2, in OGSimulationUnreal's docs, is the first reader's list of what it uses).
It holds types only: no data, no function and no behaviour. The behaviour a template needs from the game is
`BrawlerSessionGameUImpl`'s static hooks, named here as `Game`.

Two templates read it: `ASimulationManagerUImpl`'s latency member, `UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>`
(task 19a), and the brawler component's binding base, `UESimulatableOwnerBinding<USimmableUpdateComponent,
BrawlerSessionTraitsUImpl>` (task 21; `UESimulatableOwnerBinding-rationale.md` §2, in OGSimulationUnreal's docs). The
initiative's later tasks add the members the session template needs; the design lists the full set.

---

## §2 The members

| member | brawler type | read by |
|---|---|---|
| `Simulatable` | `SimulatableBrawler` | the latency template: the storage walks and the two input-resolution reads. The binding: the reception coordinator's `receiveInputBundle` |
| `PlayerInput` | `simulatableBrawler::PlayerInput` | the latency template: the input provider's result type; `ASimulationManagerUImpl` asserts that the template's provider type is its own `BrawlerInputProviderFn`. The binding: the bundle and ring element type, the send queue, the remote-move callback and the stage |
| `State` | `simulatableBrawler::State` | the binding: the typed peek of an accepted correction (`onCorrectionStateAccepted`). Added by task 21 |
| `Component` | `USimmableUpdateComponent` | the latency template: the registering component and the delayed-input map's element |
| `Character` | `AOGBrawlerUECharacter` | no template yet. Added by task 21 with `State`, as the design's component list names it; its first reader will be the session template's relay-host hook, in a later task of og-ue-simulationcomponentandmanagersplit |
| `SessionOwner` | `ASimulationManagerUImpl` | the latency template: its `instanceFor`, its `editLatencyBudgetDiagnostics()`, and the four members the template reads through a const reference. The owner befriends the template (`template <typename TraitsT> friend class UELatencyBudgetDiagnostics;`). The binding: `instanceFor`, `getReceptionCoordinator` and `getServerReceptionTick` on the authority, all public |
| `Game` | `BrawlerSessionGameUImpl` | the two input-sample hooks |
| `InputSampleSource` | `const UOGBrawlerInputCollectionComponent*` | what `inputSampleSource` returns at registration and `inputSampledSeconds` reads per step |

`SessionOwner` stays the actor until the session template exists; the design moves the latency member into it then.

---

## §3 A light header

The header includes only `OGBrawler/SimulatableBrawlerTypes.h` (for `PlayerInput` and `State`) and forward-declares the
rest: an
alias to an incomplete class is legal, and the plugin template uses each type only where its translation unit has the
complete type (the owner's header, the component's, and `BrawlerSessionGameUImpl.h` where the wrapper is instantiated).
So `SimulationManagerUImpl.h` can include it where it named the latency class's header, without pulling the component or
the hooks into every file that includes the manager.
