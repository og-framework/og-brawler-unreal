<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerCompositionUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerCompositionUImpl.h` (og-simulationscheduler-withjolt task
81). Prohibitions: `BrawlerCompositionUImpl-guards.md`. The class that owns the members these aliases
type is `ASimulationManagerUImpl` (`SimulationManagerUImpl-rationale.md` §2).

---

## 1. Why the header exists

The brawler's composition (the simulatable pack, the storage, the three peers, the integration and
systems executors and the manager built from them) was a chain of private aliases inside
`ASimulationManagerUImpl`. A class declared outside the UImpl could not name any of them. Three
consumers need to:

* **The task-54 review, finding A1.** The Jolt step driver was then
  `SimulationStepDriver<ManagerType, JoltWorld, BrawlerIntegrationExec, BrawlerStepHooksUImpl>` (since task 88
  `SimulationStepDriver<Manager, JoltWorld, BrawlerStepHooksUImpl>`). Its first
  and third arguments were private to the UImpl, so the driver and the hooks had to stay UImpl members
  instead of moving to the frame host. The review accepted that as a real type cycle.
* **Task 83 (the backend split).** It moves the Jolt host, step driver included, out of the UImpl into
  its own class. That class must spell the driver's type, so it must name the manager and the
  integration executor without the UImpl.
* **og-tickdriver, design D2.** Its tick driver is templated on a game traits type. D2 resolves the same
  A1 cycle by moving the brawler aliases into a traits header; this header is that move, and it
  absorbs og-tickdriver's draft task 10.

## 2. What it holds

`struct BrawlerCompositionUImpl` holds only member aliases and one `static_assert`. It is a struct and
not a namespace so that the whole composition can be passed as **one template argument**, which is what
a traits type (D2) is. Only the struct's name reaches file scope (`⛔G-01`).

| `ASimulationManagerUImpl` re-export | `BrawlerCompositionUImpl` member |
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

The UImpl keeps every old name as a one-line re-export of the member, so none of its member
declarations or uses changed. `IntegrationLayerType` is still the UImpl's own alias of
`BrawlerIntegrationExec`, and the Jolt-only `BrawlerStepDriver` stays in the UImpl until task 83 moves
it.

**`Simulatables` is the single source of truth for the pack.** The storage, the three peers, the
integration executor and the systems executor are all derived from it, and the manager from them;
widen it and they all follow (`⛔G-01`). The OGSim primitives are named unqualified because the whole
OGSim core lives in the global namespace.

**`IntegrationExecFor` is a bind wrapper.** `SimulationIntegrationExecutor`'s leading three parameters
are engine- and game-specific (the static data and the two physics adapters), so `apply_t` cannot
unpack the pack marker into them. The wrapper fixes those three slots first, and `apply_t` then applies
the pack, which keeps `Simulatables` the single source of truth.

**`SystemsExec`'s template order is the systems' firing order.** Detection must fire before routing in
the same `preIntegrate` pass; the `static_assert` below the alias holds that (§3). The roles each system
fires on are not set here: each system declares its own role affinity, and the executor gates every
hook on the role the manager hands it.

**The 6 in `RenderSnapshot`** is the number of bodies in the brawler's physics composite. The
`static_assert` at the top of `JoltBackendUImpl::publishRenderSnapshot_Step` checks it against
`physicsDeclarationCountOf<BrawlerPhysicsComposite>`.

**No backend switch.** The adapter types come from `PhysicsBackendUImpl.h`, which holds the switch, so
this header has no `OG_PHYSICS_BACKEND_CHAOS` of its own. It does not include `SimulationManagerUImpl.h`;
the dependency runs the other way.

## 3. The compile-time checks

* **Detection fires before routing.** The `brawlerHitDetection::firesBefore` `static_assert` under
  `SystemsExec`, moved here from the UImpl with its message unchanged. The message still names `BrawlerSystemsExec`, the UImpl's re-export of `SystemsExec`.
  Task 81 swapped the two systems in `SystemsExec` and saw the assert fail.
* **The UImpl's manager is this manager.** `compositionContractsHold()` in `SimulationManagerUImpl.h`
  asserts `std::is_same_v<ManagerType, BrawlerCompositionUImpl::Manager>`. It has to be inside the class,
  because `ManagerType` is private. A host outside the UImpl (task 83) names the manager through this
  struct; if `ManagerType` drifted to another `SimulationManager` instantiation, that host would type
  its driver over a different manager than `m_manager`. Task 81 pointed `ManagerType` at another
  instantiation and saw the assert fail.
