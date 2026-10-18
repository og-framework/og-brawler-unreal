<!-- SPDX-License-Identifier: MPL-2.0 -->
# `ChaosEngineTraitsUImpl` — rationale

Companion to `Source/OGSimulationUnreal/ChaosEngineTraitsUImpl.h` (og-tickdriver task 15a; moved here from the game
module by og-tickdriver task 15e). Its guards are
`ChaosEngineTraitsUImpl-guards.md` (none live).

> **Licence.** This tier is **MPL-2.0**, matching the code it describes. The header and this document were the game
> module's (BUSL-1.1) until og-tickdriver task 15e; the user ruled the move to MPL-2.0 on 2026-10-18. The header's body
> is unchanged by the move.

## §1 What it is

The engine traits of the Chaos engine. It models og-simulation's `PhysicsEngineTraits` (`PhysicsEngineTraits-rationale.md`
§2 in og-simulation's docs, since og-tickdriver task 15f), and the
brawler's `PhysicsBackendUImpl.h` selects it as `EngineTraits` in the Chaos configuration. It is a header only, and names no
game type.

| member | value | where it was before og-tickdriver task 15a |
|---|---|---|
| the six adapter types | `ChaosPhysicsBodyAdapter`, `ChaosPhysicsBodyReaderAdapter`, `ChaosSpatialQueryAdapter` (the query adapter and the visualization query), `ChaosPhysicsFactory` | `PhysicsBackendUImpl.h`'s Chaos block |
| `CategoryTable` | `std::initializer_list<ChaosCategoryMapping>`: `ChaosSpatialQueryAdapter` has only that constructor | new |
| `kName` | `"chaos"` | kBackendName, `PhysicsBackendUImpl.h` |
| `computeDeterminismFingerprint` | returns `std::nullopt`: Chaos has no determinism fingerprint, so `[SimHost.Backend]` prints `fingerprint=none` | `PhysicsBackendUImpl.h`, the same body |
| `kHasWorldHost` | `false` (§3) | new |

## §2 No log sink

The Chaos adapters take no message callback: they log through `UE_LOG` in OGSimulationUnreal. `PhysicsEngineTraits`
requires a log sink only of an engine with a world host (og-tickdriver task 15b; og-simulation's
`PhysicsEngineTraits-rationale.md` §2), so these traits have none. From task 15a until task 15b they held a typed none in its place, a
`static constexpr std::nullptr_t` named logEngineMessage, because the concept then required a sink of every engine; a `std::nullptr_t`
converts to a function pointer, so a host that stored it would have compiled and made a null call. Task 15b deleted
it with the gate.

## §3 No world host

Chaos schedules its own steps: the Chaos host registers a sim callback with the solver and takes its inputs from
the rewind callback (`ChaosTickBackendUImpl-rationale.md` §2). og-simulation's tick driver never steps Chaos, so these
traits have no `WorldHost` template and `kHasWorldHost` is `false`. A `static_assert` after the struct pins the value,
with a message that says why. Task 15a saw it fail with the value set to `true`.

**What it is for.** A driver-scheduled host requires `kHasWorldHost` (og-tickdriver task 15b), so these traits
cannot be put under the tick driver. With the value `true`, `PhysicsEngineTraits` would also fail, because the
template it then requires is missing.
