<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `PhysicsBackendHostUImpl.h` — rationale

Companion to `Source/OGBrawlerUnreal/PhysicsBackendHostUImpl.h` (og-simulationscheduler-withjolt task 83). The header
carries no prose and has no guards document: what could go wrong in it is a compile error.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

## §1 What it is

**Seam B** of the physics backend, who schedules the steps: the concept every backend host models, and the
**selector** of the composition. Seam A, what differs between physics engines, is `PhysicsBackendUImpl.h`
(`PhysicsBackendUImpl-rationale.md`). The session holds its backend in two members whose types this header names (`ASimulationManagerUImpl` held them
until og-ue-simulationcomponentandmanagersplit task 24a; `UESimulationSession-rationale.md` §3, in OGSimulationUnreal's docs). The traits name them for
the session as `BrawlerSessionTraitsUImpl::BackendWorlds` and `Backend<SessionT>` (§5):

| name | Chaos configuration | Jolt configuration |
|---|---|---|
| `physicsBackendUImpl::Backend` | `ChaosTickBackendUImpl`: Chaos's own tick schedules | `TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>` (OGSimulationUnreal's host since og-tickdriver task 15e; §4): og-simulation's `SimulationTickDriver` schedules |
| `physicsBackendUImpl::BackendWorlds` | `ChaosTickBackendWorldsUImpl` (empty) | `Backend::Worlds`, that is `TickDriverBackendWorldsUImpl` over the host's world host |

* **`SimulationManagerUImpl.h` and, since task 24a, `BrawlerSessionTraitsUImpl.h` include it** (the traits name its
  types; every file that includes the component sees it through them). It includes `PhysicsBackendUImpl.h` first, so an undefined
  `OG_PHYSICS_BACKEND_CHAOS` stops the build there with that header's error, and then the one host header of the
  configuration. The host headers have no `#if` and nothing else includes them, so this header holds the only
  `#if` that picks a host, beside `PhysicsBackendUImpl.h`'s selector of the engine traits and the binding
  (`PhysicsBackendUImpl-rationale.md` §4).
* **The two selectors agree by assertion.** Each arm of this header's `#if` opens with a `static_assert` that
  `EngineTraits` and `EngineBinding` are the pair its host is built over: `ChaosEngineTraitsUImpl` and
  `BrawlerChaosBindingUImpl` beside `ChaosTickBackendUImpl`, `JoltEngineTraitsUImpl` and `BrawlerJoltBindingUImpl`
  beside the tick-driver host. The session types its adapters through `EngineTraits` and its backend through
  `Backend`, so a selector that picked a different pair would hand the host adapters of another engine.
* **The Jolt arm names its game binding, engine traits and engine binding,**
  `TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>` since og-tickdriver
  task 15e (the engine traits and the binding only, from task 15b), as the composition is written in og-tickdriver task 15's plan; a later driver-scheduled
  engine (Box3D) is one more arm, `TickDriverBackendUImpl<BrawlerTickDriverGame, Box3DEngineTraitsUImpl, BrawlerBox3DBindingUImpl>`. The host
  template refuses any pair whose engine has no world host (`TickDriverBackendUImpl-rationale.md` §10), so the Chaos
  traits cannot be put on it.
* **The names sit in namespace `physicsBackendUImpl`,** where task 83 put them. Since og-tickdriver task 15b the
  namespace holds only seam B's three names (the concept, `Backend`, `BackendWorlds`): the seam-A adapter names it also
  held, kept in `PhysicsBackendUImpl.h` by task 15a for the files outside that task, are gone, and every user names
  `EngineTraits::` instead (`PhysicsBackendUImpl-rationale.md` §5). The UImpl's members are typed through these names,
  so the UImpl's header and `.cpp` are the same text in both configurations.

**Why the selector is here and not in `BrawlerCompositionUImpl.h`.** og-tickdriver task 15's plan names the
composition header as the selector's home. It cannot hold it: the tick-driver host's type names
`BrawlerCompositionUImpl`'s types (its storage and render snapshot, through `BrawlerTickDriverGameUImpl`, whose header
includes the composition header; directly as well until og-tickdriver task 15e), so the composition header cannot
include the host in turn to name `Backend`. This header is included
after both, by the session only, so task 15b left the selector here.

## §2 The seam concept

`PhysicsBackendHost<BackendT>` lists every entry point of the seam (`SimulationManagerUImpl-rationale.md` §18, "The
seam") with its parameter types and its exact return type: `void` for the lifecycle, frame and registration entry
points, `bool` for `createBodies_GameThread` (`false` means `TryRegisterStatus::Pending`), the visualization adapter
references (`EngineTraits::VizQuery&`, and `const EngineTraits::VizReader&` from a `const` backend) and
`std::optional<QueryVolumeId>` for the three visualization doors.

It is asserted twice, on the configuration being built only (the other host's header is not included): once here,
after the selector, for the composition the arm picked; and once in `BrawlerSessionTraitsUImpl.h`, on the
traits' `Backend<UESimulationSession<BrawlerSessionTraitsUImpl>>`, the type the session holds (in the actor's
`compositionContractsHold()` until og-ue-simulationcomponentandmanagersplit task 24a; the MPL session cannot name the brawler concept). So a host that loses, renames or changes an entry point fails with a message
that names the concept, rather than at whichever call site the compiler reaches first. A Chaos build checks
`ChaosTickBackendUImpl` and a Jolt build the tick-driver host. Task 83 saw it fail on the Jolt host
(JoltBackendUImpl, its historical name) with one entry point removed.

**What it does not check.** Construction (`Backend{ owner, worlds }`) is checked by the session's member initializer
itself, and the declaration order of the two members by the session's `__builtin_offsetof` assertions (the UImpl's
until task 24a). The concept
names the parameters through forward declarations (`UWorld`, `AActor`, `USimmableUpdateComponent`,
`AOGBrawlerUECharacter`) and the types the host headers already bring in (`SimulatableBrawler`,
`QueryVolumeDescriptor`, `QueryVolumeId`).

## §3 History

* **og-simulationscheduler-withjolt task 83** created the header: the concept, and the two names per configuration
  (then ChaosBackendWorldsUImpl/ChaosBackendUImpl and JoltBackendWorldsUImpl/JoltBackendUImpl, historical names).
* **og-tickdriver task 15b** renamed the hosts by who schedules (`ChaosTickBackendUImpl`, `TickDriverBackendUImpl`),
  made the tick-driver host a template over the engine traits and the binding, added the two agreement asserts and the
  namespace-scope concept assert, and switched the concept's adapter types to `EngineTraits::`.
* **og-tickdriver task 15e** moved the tick-driver host to OGSimulationUnreal over three parameters, and gave the
  Jolt arm the game binding's include, the `extern template` and the three-parameter `Backend` (§4).
* **og-ue-simulationcomponentandmanagersplit task 24a** added `BackendFor<SessionT>` per arm and the two forward declarations, made `Backend` the session's,
  and moved the actor's second concept assertion beside the traits (§2, §5).

## §4 The Jolt arm's includes and its `extern template` (og-tickdriver task 15e)

Since og-tickdriver task 15e the tick-driver host is OGSimulationUnreal's header template, which names no brawler
type, so the Jolt arm includes two headers: `OGSimulationUnreal/TickDriverBackendUImpl.h` (the host) and
`OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h` (the game binding and its `BrawlerTickDriverGame` alias, which the host
header included until then). The arm then declares
`extern template class TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>;`.

* **Why `extern`.** The host's member definitions are in its header since task 15e. Without the declaration every
  game translation unit that calls the backend (the session's `.cpp`) would instantiate the members it calls,
  besides the one explicit instantiation. With it, only `BrawlerJoltBindingUImpl.cpp` instantiates them
  (`BrawlerJoltBindingUImpl-rationale.md` §7), and the others instantiate only the members defined inside the class,
  which is where the definitions were compiled before task 15e. Task 15e checked it with `dumpbin`.
* **Why here.** An explicit instantiation declaration must be at the template's own namespace scope (global), so it
  sits in the arm before namespace `physicsBackendUImpl` opens, and it is in the Jolt arm only, as the host is.

## §5 `BackendFor<SessionT>` and the session (og-ue-simulationcomponentandmanagersplit task 24a)

Since task 24a the composition lives in OGSimulationUnreal's `UESimulationSession<BrawlerSessionTraitsUImpl>`, which holds
the backend over itself, so the backend's type depends on the session's: each arm defines an alias template
`BackendFor<SessionT>` (the Chaos host, which ignores `SessionT`; or `TickDriverBackendUImpl` over
`BrawlerTickDriverGameUImpl<SessionT>`), and `Backend` is `BackendFor` over the brawler's session. The traits'
`Backend<SessionT>` is `BackendFor<SessionT>`, so the session's friend line and its `m_backend` name the same type the
`extern template` and the explicit instantiation do.

* **The header forward-declares `BrawlerSessionTraitsUImpl` and `UESimulationSession`.** `BrawlerSessionTraitsUImpl.h`
  includes this header before it defines the struct, and `Backend` only names the session's specialization: the host's
  class is instantiated with the session incomplete, as it was with the actor incomplete before (it holds the session
  by reference; the game binding too). The members that read the session are instantiated where it is complete: the
  explicit instantiation in `BrawlerJoltBindingUImpl.cpp`, which includes `SimulationManagerUImpl.h`, and so the
  session's header and the complete actor.
* **The Jolt arm's `extern template`** still names the host over `BrawlerTickDriverGame`, which is
  `BrawlerTickDriverGameUImpl<UESimulationSession<BrawlerSessionTraitsUImpl>>` since task 24a: the same type as
  `Backend`.
