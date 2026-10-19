<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerChaosBindingUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerChaosBindingUImpl.h` and `.cpp` (og-tickdriver task 15a). Its guards are
`BrawlerChaosBindingUImpl-guards.md` (none live).

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

## §1 What it is

The brawler's glue for the Chaos engine. It models `BrawlerEngineBinding<BrawlerChaosBindingUImpl,
ChaosEngineTraitsUImpl>` (`PhysicsBackendUImpl-rationale.md` §3), and `PhysicsBackendUImpl.h` selects it as
`EngineBinding` in the Chaos configuration.

## §2 The four roles, on the Chaos side

Task 15a looked for the Chaos host's brawler glue in the four roles of the binding concept. One role has code, one has
a type, and two have nothing. Since og-tickdriver task 15b the concept asks for those two only of an engine with a world
host, so the binding has no member for them.

| role | on the Chaos side | the member |
|---|---|---|
| the category table | the six rows the Chaos host passed to the query adapter, in the file-local emplaceBrawlerQueryAdapter of ChaosBackendUImpl.cpp (the historical name of `ChaosTickBackendUImpl.cpp`) until task 15a | `categoryChannels()` (below) |
| the simulatable type | `SimulatableBrawler`: the Chaos host's storage and its physics-thread passes name it directly | `Simulatable` |
| the capsule contract check | **none.** The Chaos factory adopts the pawn's own capsule as the root body, and its adopt-root branch compares that capsule with the descriptor itself (`ChaosPhysicsFactory`, OGSimulationUnreal; `OGBrawlerUECharacter-guards.md` G-01). The Chaos host has no check of its own | none |
| the park snap | **none.** Chaos renders the characters through its own interpolation; there is no og-simulation presenter and no park snap to pass | none |

**The table.** `body` → `ECC_GameTraceChannel2`, `guard` → `ECC_GameTraceChannel3`, `queryRouting` →
`ECC_GameTraceChannel4`, `projectile` → `ECC_GameTraceChannel5`, `world` → `ECC_WorldStatic`, `character` →
`ECC_GameTraceChannel6`: the Jolt binding's rows, in the same order (`BrawlerJoltBindingUImpl-rationale.md` §2). It is
in the Chaos engine traits' `CategoryTable` form, `std::initializer_list<ChaosCategoryMapping>`, because
`ChaosSpatialQueryAdapter` has only that constructor. The list is a function-local `static const`, whose backing array
lives as long as the list, so the copy `categoryChannels()` returns stays valid. The Chaos host's `buildWorld`
emplaces the query adapter with it; the adapter's constructor copies each mapping into its own tables, so the values
it holds are the ones the old inline list gave it.

**Why no member, not an invented one.** A Chaos value for the capsule check or the park snap would be invented:
nothing on the Chaos side computes one. Only the tick-driver host reads the two, and since og-tickdriver task 15b the
binding concept requires them only when the engine traits have a world host (`PhysicsBackendUImpl-rationale.md` §3),
so the Chaos binding models the concept without them. From task 15a until task 15b the concept required all four
roles of every binding, and this binding held two typed nones, a `static constexpr std::nullptr_t` checkCapsuleContract
and a `static constexpr std::nullopt_t` kParkSnap. The first converted to `CapsuleContractCheck`, so a host that stored
it would have compiled and made a null call; task 15b deleted both with the gate.

## §3 The file

The `.cpp` is a whole-file `#if OG_PHYSICS_BACKEND_CHAOS`, as the Chaos host's is. The header has no `#if`.
