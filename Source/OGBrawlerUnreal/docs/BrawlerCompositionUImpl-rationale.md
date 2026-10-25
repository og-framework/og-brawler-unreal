<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: BrawlerSystemList -- retired member list template: replaced by og-simulation's SystemList in og-ue-simulationcomponentandmanagersplit task 36; named in §2's record of that change -->
<!-- lint-external-ref: BrawlerSimulatables -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a; the table of §2 is task 81's -->
<!-- lint-external-ref: BrawlerNetSync -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a; the table of §2 is task 81's -->
<!-- lint-external-ref: BrawlerIntegrationExecFor_UE -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a; the table of §2 is task 81's -->
<!-- lint-external-ref: BrawlerIntegrationExec -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a; the table of §2 and the history of §1 are task 81's -->
<!-- lint-external-ref: IntegrationLayerType -- retired alias: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24a -->
<!-- lint-external-ref: BrawlerStorage -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24b (it typed one of the feed's const bridges); the table of §2 is task 81's -->
<!-- lint-external-ref: BrawlerInputResolution -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24b (it typed one of the feed's const bridges); the table of §2 is task 81's -->
<!-- lint-external-ref: BrawlerReconciliation -- retired re-export: deleted from AOGBrawlerSimulationManager by og-ue-simulationcomponentandmanagersplit task 24b (it typed one of the feed's const bridges); the table of §2 is task 81's -->
# `BrawlerCompositionUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerCompositionUImpl.h` (og-simulationscheduler-withjolt task
81). Prohibitions: `BrawlerCompositionUImpl-guards.md`. The class that owns the members these aliases
type is `UESimulationSession<BrawlerSessionTraitsUImpl>` since og-ue-simulationcomponentandmanagersplit task 24a, which names them through
`BrawlerSessionTraitsUImpl::Composition` (`UESimulationSession-rationale.md` §2, in OGSimulationUnreal's docs);
`AOGBrawlerSimulationManager` owned them until then (`OGBrawlerSimulationManager-rationale.md` §2).

---

## 1. Why the header exists

The brawler's composition (the simulatable pack, the storage, the three peers, the integration and
systems executors and the manager built from them) was a chain of private aliases inside
`AOGBrawlerSimulationManager`. A class declared outside the UImpl could not name any of them. Three
consumers need to:

* **The task-54 review, finding A1.** The Jolt step driver was then a `SimulationStepDriver` over
  `ManagerType`, `JoltWorld`, `BrawlerIntegrationExec` and the brawler's step hooks (since task 88 over the
  manager, the world and the hooks; since og-tickdriver task 8 it is og-simulation's tick driver's own member,
  over its `TickDriverStepHooks`). Its first
  and third arguments were private to the UImpl, so the driver and the hooks had to stay UImpl members
  instead of moving to the frame host. The review accepted that as a real type cycle.
* **Task 83 (the backend split).** It moves the Jolt host, step driver included, out of the UImpl into
  its own class. That class must spell the driver's type, so it must name the manager and the
  integration executor without the UImpl.
* **og-tickdriver, design D2.** Its tick driver is templated on a game traits type. D2 resolves the same
  A1 cycle by moving the brawler aliases into a traits header; this header is that move, and it
  absorbs og-tickdriver's draft task 10.

## 2. What it holds

`struct BrawlerCompositionUImpl` holds member aliases, two of them alias templates (`IntegrationExecFor` and the systems'
expander `SystemsExecFor`), and one `static_assert` (§4). og-ue-simulationcomponentandmanagersplit task 35 added a member
list template, `BrawlerSystemList`, and a struct expander; task 36 replaced them with og-simulation's `SystemList` and
the alias template. It is a
struct and not a namespace so that the whole composition can be passed as **one template argument**, which is what
a traits type (D2) is. Only the struct's name reaches file scope (`⛔G-01`).

| `AOGBrawlerSimulationManager` re-export | `BrawlerCompositionUImpl` member |
|---|---|
| `BrawlerSimulatables` | `Simulatables` |
| `BrawlerStorage` | `Storage` |
| `BrawlerNetSync` | `NetSync` |
| `BrawlerInputResolution` | `InputResolution` |
| `BrawlerReconciliation` | `Reconciliation` |
| `RenderSnapshot` | `RenderSnapshot` |
| `BrawlerIntegrationExecFor_UE` | `IntegrationExecFor` |
| `BrawlerIntegrationExec` | `IntegrationExec` |
| `BrawlerHitDetectionSystem` | `HitDetectionSystem` |
| `BrawlerSystemsExec` | `SystemsExec` |
| `ManagerType` | `Manager` |

The UImpl kept every old name as a one-line re-export of the member, so none of its member
declarations or uses changed (the table is task 81's). Since og-ue-simulationcomponentandmanagersplit task 24b it kept two, the ones its own code
still named: `BrawlerHitDetectionSystem` and `ManagerType`, which the two role branches of `BeginPlay` named in their emplaces.
Since task 27b it keeps none: the start-up is the session's, which names its own manager type, and
`BrawlerHitDetectionSystem` is the private alias of the hook class `BrawlerSessionGameUImpl`, whose `emplaceSystems` names it.
Task 24a
deleted six with the members they typed, which are the session's, and kept five; task 24b deleted three more, the
storage, input-resolution and reconciliation re-exports, which typed the feed's four const bridges and went with them.
The other nine rows of the table are history. `IntegrationLayerType` was the
UImpl's own alias of `BrawlerIntegrationExec` until task 24a, and the Jolt-only step-driver alias stayed in the UImpl until task 83 moved it
(og-tickdriver task 8 replaced it with `TickDriverBackendUImpl`'s tick-driver alias, over the game binding
`BrawlerTickDriverGameUImpl`).

**`Simulatables` is the single source of truth for the pack.** The storage, the three peers, the
integration executor and the systems executor are all derived from it, and the manager from them;
widen it and they all follow (`⛔G-01`). The OGSim primitives are named unqualified because the whole
OGSim core lives in the global namespace.

**`IntegrationExecFor` is a bind wrapper.** `SimulationIntegrationExecutor`'s leading three parameters
are engine- and game-specific (the static data and the two physics adapters), so `apply_t` cannot
unpack the pack marker into them. The wrapper fixes those three slots first, and `apply_t` then applies
the pack, which keeps `Simulatables` the single source of truth.

**`SystemsExec`'s template order is the systems' firing order**, and since task 35 that order is the order of the
`Systems` list it is built from (§4). Detection must fire before routing in
the same `preIntegrate` pass; the `static_assert` below the alias holds that (§3). The roles each system
fires on are not set here: each system declares its own role affinity, and the executor gates every
hook on the role the manager hands it.

**The 6 in `RenderSnapshot`** is the number of bodies in the brawler's physics composite. The
class-scope `static_assert` in og-simulation's `TickDriverPresenter` checks it: the snapshot's `kMaxBodies` must equal
the render source's `kMaxSimulatableSlots` times the presenter's `kDeclarationsPerSlot` (`declarationCountOf` the
source's `PhysicsComposite`, the brawler's physics composite).

**No backend switch.** The adapter types come from `PhysicsBackendUImpl.h`, which holds the switch, so
this header has no `OG_PHYSICS_BACKEND_CHAOS` of its own. It does not include `OGBrawlerSimulationManager.h`;
the dependency runs the other way.

## 3. The compile-time checks

* **Detection fires before routing.** The `brawlerHitDetection::firesBefore` `static_assert` under
  `SystemsExec`, moved here from the UImpl with its message unchanged. The message still names `BrawlerSystemsExec`, the UImpl's re-export of `SystemsExec`.
  Task 81 swapped the two systems in `SystemsExec` and saw the assert fail.
* **The session's manager is this manager.** Since og-ue-simulationcomponentandmanagersplit task 24a `UESimulationSession::compositionContractsHold`
  asserts that the session's private `ManagerType` (this struct's `Manager`; since task 34 the session spells it as
  og-simulation's `SimulationManager` over its own aliases and asserts it equal to `Manager`, and before that it was its
  traits' `Composition::Manager`) is the
  tick-driver binding's `Manager` (`BrawlerTickDriverGameUImpl`, which names this struct's member). Until task 24a
  `compositionContractsHold()` in `OGBrawlerSimulationManager.h` asserted `std::is_same_v<ManagerType,
  BrawlerCompositionUImpl::Manager>`. It has to be inside the class,
  because `ManagerType` is private. A host outside the UImpl (task 83) names the manager through this
  struct; if `ManagerType` drifted to another `SimulationManager` instantiation, that host would type
  its driver over a different manager than `m_manager`. Task 81 pointed `ManagerType` at another
  instantiation and saw the assert fail.
* **The session's core systems are og-simulation's (task 34).** The session asserts that its `Storage`,
  `Reconciliation`, `InputResolution`, `NetSync` and `ManagerType` are this struct's `Storage`, `Reconciliation`,
  `InputResolution`, `NetSync` and `Manager` (user ruling 2026-10-24: the session requires og-simulation's own core
  systems), and the brawler's traits assert the rows of their core-system map against this struct's aliases
  (`BrawlerSessionTraitsUImpl-rationale.md` §4). An alias here that names another type fails the build.
* **The session's executors are og-simulation's (task 35).** The session spells `IntegrationExec` and `SystemsExec`
  itself, from this struct's `Simulatables` and `Systems` and its traits' `StaticData` and `EngineTraits`, and asserts
  that each is this struct's `IntegrationExec` and `SystemsExec` (`UESimulationSession-rationale.md` §14, in
  OGSimulationUnreal's docs). Because both are built from the same list, a reordered `Systems` list changes both and
  passes that row; the `firesBefore` assert above is what fails it when detection no longer precedes routing (§4).

## 4. The systems list (og-ue-simulationcomponentandmanagersplit tasks 35 and 36)

**What changed.** Until task 35 `SystemsExec` was spelled directly: `SimulationSystemsExecutor<Simulatables,
simulatableBrawler::StaticData, HitDetectionSystem, brawlerHitRouting::System, brawlerRingout::ScoreSystem>`. Since task
35 the struct declares its systems once, in their firing order, as `Systems`, and `SystemsExec` is built from it: the
same `SimulationSystemsExecutor` over the same three systems in the same order. Since task 36 the list is og-simulation's
`SystemList` (`SimulatableList.h`), `Systems = SystemList<HitDetectionSystem, brawlerHitRouting::System,
brawlerRingout::ScoreSystem>`, and `SystemsExec` is `apply_t<SystemsExecFor, Systems>`, the shape of `IntegrationExec`
(§2). Task 35's game-side list template and its partially specialized expander struct are gone; the type is unchanged
(the session's row and the traits' row both compare it). The session spells its own `SystemsExec` from `Systems`
(`UESimulationSession-rationale.md` §14, in OGSimulationUnreal's docs), so a game hands the session its list, not an
executor.

**Why og-simulation's `SystemList`, not `SimulatableList`.** `SimulatableList` is the simulatable pack: the systems
executor takes the simulatables through it, and its `IsSimulatableList` concept gates each system's
`RequiredSimulatables`. A list of systems spelled as a `SimulatableList` would model that concept and read as a list of
simulatables, so task 35 declared a minimal list of the brawler's own that carried the pack and nothing else (lead
ruling, 2026-10-24). Task 36 moved that marker into og-simulation as `SystemList`, beside `SimulatableList` and `apply`,
with its own `apply` specialization (lead ruling, 2026-10-24); `SimulatableList.h` asserts that `SystemList` does not
model `IsSimulatableList`.

**Why inside the struct.** Only the struct's name reaches file scope (`⛔G-01`), so the expander `SystemsExecFor` is a
member alias template, as `IntegrationExecFor` is.

**`SystemsExecFor` is the systems' bind wrapper.** Like `IntegrationExecFor` (§2), it fixes the executor's leading
parameters (the simulatable list and the static data), and `apply_t` applies the list's pack to it. Task 35 could not
use `apply_t`, which then unpacked only a `SimulatableList`; the `apply<F, SystemList<Ts...>>` specialization is what
lets it.

**The order.** Firing order is template order (§2), so the list's order is the order the systems fire in. The
`firesBefore` assert is unchanged and still reads `SystemsExec`; a list that puts routing before detection fails it. Any
other order of the list is the game's choice (the user's ruling, 2026-10-24: a game chooses its systems and their order)
and is checked by no row: the session's row compares two types built from the same list.

**The ruling (user, 2026-10-24).** A game also uses og-simulation's own integration executor and systems executor; it
chooses the systems and their order, its static data, its engine (through its traits) and its physics backend.
