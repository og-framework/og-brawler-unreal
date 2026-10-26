<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `BrawlerJoltBindingUImpl` — rationale

Companion to `Source/OGBrawlerUnreal/BrawlerJoltBindingUImpl.h` and `.cpp` (og-tickdriver task 15a). Its guards are
`BrawlerJoltBindingUImpl-guards.md` (none live).

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

## §1 What it is

The brawler's glue for the Jolt engine. It models `BrawlerEngineBinding<BrawlerJoltBindingUImpl,
JoltEngineTraitsUImpl>` (`PhysicsBackendUImpl-rationale.md` §3), and `PhysicsBackendUImpl.h` selects it as
`EngineBinding` in the Jolt configuration. The Jolt host (`TickDriverBackendUImpl`) calls it. Every member was in the Jolt
host until og-tickdriver task 15a, and moved without a change to what it computes or logs.

| member | what it is | was |
|---|---|---|
| `Simulatable` | `SimulatableBrawler`, the type the world host is instantiated with (`TickDriverBackendUImpl::WorldHost`) | the template argument of the alias JoltTickWorldHostUImpl in JoltBackendUImpl.h (historical names) |
| `categoryChannels()` | the six-row category-to-channel table (§2) | brawlerCategoryChannels, JoltBackendUImpl.cpp (the historical name of TickDriverBackendUImpl.cpp, itself deleted by og-tickdriver task 15e) |
| `mappedCategories()` | the bit mask of the table's categories | brawlerMappedCategories, JoltBackendUImpl.cpp (historical name) |
| `checkCapsuleContract(character, id)` | the capsule-to-descriptor check (§3) | the first block of the Jolt host's `createBodies_GameThread` (today `TickDriverBackendUImpl::createBodies_GameThread`) |
| `kParkSnap` | the render park snap (§4) | kRenderParkSnapCm, JoltBackendUImpl.cpp (historical name) |
| `contactCountsEnabled()` | reads `og.Jolt.ContactCounts` (§6) | since og-tickdriver task 15e; the Jolt engine traits' from task 15a, the Jolt host's before |

## §2 The category table

Six rows, one og-simulation collision category to one Unreal collision channel: `body` → `ECC_GameTraceChannel2`,
`guard` → `ECC_GameTraceChannel3`, `queryRouting` → `ECC_GameTraceChannel4`, `projectile` →
`ECC_GameTraceChannel5`, `world` → `ECC_WorldStatic`, `character` → `ECC_GameTraceChannel6`. They are the rows of
the Chaos binding's table (`BrawlerChaosBindingUImpl-rationale.md` §2), in the same order.

* The table is a function-local `static const std::vector`, built at the first call. The first call is the Jolt
  host's `buildWorld`, which hands it to `UEStaticGeometryImporter` (on the game thread, at `BeginPlay`), as before.
  It is in the engine traits' `CategoryTable` form, which the binding concept checks.
* `mappedCategories()` ORs one bit per row; the host puts it in the world host's configuration
  (`TickDriverBackendUImpl-rationale.md` §3).

## §3 The capsule contract

`checkCapsuleContract` `checkf`s the pawn's unscaled capsule radius and half-height against the movement
descriptor's `CapsuleGeometry` (`brawlerMovementSimulation::PhysicsDeclaration`'s first shape), with
`FMath::IsNearlyEqual`. The Jolt slot body is built from the descriptor, while the UE capsule stays the aim plane and
the camera anchor, so a size that disagrees would make them disagree with the simulated body
(`OGBrawlerUECharacter-guards.md` G-01; design D14). The message is unchanged, including its `tryRegister:` prefix:
the check runs first in the Jolt host's `createBodies_GameThread`, which `tryRegister`'s first pass calls.

The comparison is in UE units (og-sim-metres-yup task 8): the descriptor's radius and half-height are og-simulation
lengths, so each is converted with `ueFrame::lengthToUe` and narrowed to the engine's `float` before
`FMath::IsNearlyEqual` compares it with the pawn's centimetres, as the Chaos factory's adopt-root check does. The
message prints those converted values, so both pairs read in cm. At the shipped knobs (centimetres) `lengthToUe` is a
float-to-double copy, so the check and its text are as before; in metres it gives 42 and 96 again. From task 8 until
task 7b this file did not include `UEFrameConversion.h` and multiplied by `simFrame::kUeCentimetresPerUnit` instead,
because the header's optimize-off helpers then changed the simulation code this translation unit instantiates
(`BrawlerJoltBindingUImpl-guards.md` §R, G-01; `UEFrameConversion-guards.md` G-05).
The pawn's `InitCapsuleSize(42, 96)` stays a UE value (`OGBrawlerUECharacter-guards.md` G-01).

The function sits between `OGSIM_OPTIMIZE_OFF` and `OGSIM_OPTIMIZE_ON`, as the host's function it came from did; the
table and the mask sat outside that pair in the host and sit outside it here.

## §4 The park snap

`kParkSnap` is `-0.5f * brawlerProjectileSimulation::kParkHeight`: half the projectile pool's park depth, 500
og-simulation lengths (500 m; it was 50,000 while the simulation ran in centimetres). The Jolt host passes it as the presenter's
`PresentControls::parkSnap` every frame: a body whose two bracketing poses lie farther apart than this is a pooled
projectile being parked or unparked, and is snapped rather than blended (`TickDriverBackendUImpl-rationale.md` §7). It is
game configuration in og-simulation lengths, so its name carries no unit (design K9); the old name ended in `Cm`.

## §5 The file

The `.cpp` is a whole-file `#if !OG_PHYSICS_BACKEND_CHAOS`, as the Jolt host's was until og-tickdriver task 15e made
the host a header template in OGSimulationUnreal. The header has no `#if`. Since task 15e the `.cpp` also holds the
console variable (§6) and the host's one explicit instantiation (§7).

## §6 `og.Jolt.ContactCounts` (og-tickdriver task 15e)

The console variable object is in this file's `.cpp`, with its name, default (0), help text and flags unchanged, and
`contactCountsEnabled()` is the old read, `GetValueOnGameThread() == 1`. The Jolt host calls it once, on the game
thread, when it fills the world host's configuration in `buildWorld` (`TickDriverBackendUImpl-rationale.md` §9).

* **Why it is here** (lead ruling L4). From og-tickdriver task 15a until task 15e the variable and its read were the
  Jolt engine traits'. Task 15e moved the traits to the OGSimulationJoltUnreal module, which cannot see the game's
  backend switch: the variable would have registered in the Chaos configuration too. Here, in a whole-file Jolt arm
  of the game module, it registers in the Jolt configuration only, at the game module's static initialization, as
  before. The engine traits keep the log sink (`JoltEngineTraitsUImpl-rationale.md` §2).
* **The host does not require it** of a binding through a concept: a binding without it fails inside the host's
  `buildWorld` at the instantiation (og-simulation's `PhysicsEngineTraits-rationale.md` §3).
* Task 15e compared the console-variable dumps of both configurations before and after the move: unchanged, and an
  engine-ini override still reaches the variable (the dump's LastSetBy reads SystemSettingsIni).

## §7 The host's instantiation (og-tickdriver task 15e)

The `.cpp` ends with `template class TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl,
BrawlerJoltBindingUImpl>;`, inside its `OGSIM_OPTIMIZE_OFF` span, after `checkCapsuleContract`. It is the one place the
Jolt configuration's host is instantiated: the header is OGSimulationUnreal's and game-agnostic, and
`PhysicsBackendHostUImpl.h` declares the same instantiation `extern` (`PhysicsBackendHostUImpl-rationale.md` §4). So the
driver, its step scope and the world host's thread-context calls compile into the game module, as og-simulation's
one-module rule requires (`TickDriverThreadContext-rationale.md` §7 in og-simulation's docs), and the host's order
rows are evaluated here: task 15e saw a member swap in the host's header fail this file's compile
(`TickDriverBackendUImpl-rationale.md` §12).

**Why here.** The instantiation needs the complete session, component and character types, so it includes
`OGBrawlerSimulationManager.h` and `OGBrawlerSimulatableComponent.h`; it belongs to the Jolt configuration only, and this file is
the game module's Jolt arm. The host's own `.cpp` held it until task 15e.
