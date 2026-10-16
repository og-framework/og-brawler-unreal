<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `PhysicsBackendHostUImpl.h` — rationale

Companion to `Source/OGBrawlerUnreal/PhysicsBackendHostUImpl.h` (og-simulationscheduler-withjolt task 83). The header
carries no prose and has no guards document: what could go wrong in it is a compile error.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

## §1 What it is

The **selector** of the physics backend host. `ASimulationManagerUImpl` holds its backend in two members whose
types this header names (`SimulationManagerUImpl-rationale.md` §2):

| name | Chaos configuration | Jolt configuration |
|---|---|---|
| `physicsBackendUImpl::BackendWorlds` | `ChaosBackendWorldsUImpl` (empty) | `JoltBackendWorldsUImpl` |
| `physicsBackendUImpl::Backend` | `ChaosBackendUImpl` | `JoltBackendUImpl` |

* **Only `SimulationManagerUImpl.h` includes it.** It includes `PhysicsBackendUImpl.h` first, so an undefined
  `OG_PHYSICS_BACKEND_CHAOS` stops the build there with that header's error, and then the one backend header of the
  configuration. The backend headers have no `#if` and nothing else includes them, so this header holds the only
  `#if` that picks a host, beside the traits header's alias `#if`.
* **The two names sit in `physicsBackendUImpl`,** the traits header's namespace, beside the adapter aliases they are
  used with. The UImpl's members are typed through them, so the UImpl's header and `.cpp` are the same text in both
  configurations.

## §2 The seam concept

`PhysicsBackendHost<BackendT>` lists every entry point of the seam (`SimulationManagerUImpl-rationale.md` §18, "The
seam") with its parameter types and its exact return type: `void` for the lifecycle, frame and registration entry
points, `bool` for `createBodies_GameThread` (`false` means `TryRegisterStatus::Pending`), the visualization adapter
references (`VizQuery&`, and `const VizReader&` from a `const` backend) and `std::optional<QueryVolumeId>` for the
three visualization doors.

`ASimulationManagerUImpl::compositionContractsHold()` asserts `PhysicsBackendHost<physicsBackendUImpl::Backend>`, so
a backend that loses, renames or changes an entry point fails there, with a message that names the concept, rather
than at whichever call site the compiler reaches first. It checks the configuration being built only: the other
backend's header is not included, so a Chaos build checks `ChaosBackendUImpl` and a Jolt build `JoltBackendUImpl`.
Task 83 saw it fail on a `JoltBackendUImpl` with one entry point removed.

**What it does not check.** Construction (`Backend{ owner, worlds }`) is checked by the UImpl's member initializer
itself, and the declaration order of the two members by the UImpl's `__builtin_offsetof` assertions. The concept
names the parameters through forward declarations (`UWorld`, `AActor`, `USimmableUpdateComponent`,
`AOGBrawlerUECharacter`) and the types the backend headers already bring in (`SimulatableBrawler`,
`QueryVolumeDescriptor`, `QueryVolumeId`).
