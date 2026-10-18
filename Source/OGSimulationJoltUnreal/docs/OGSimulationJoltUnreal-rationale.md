<!-- SPDX-License-Identifier: MPL-2.0 -->
# `OGSimulationJoltUnreal.h` and `.cpp` — rationale

Companion to `Source/OGSimulationJoltUnreal/OGSimulationJoltUnreal.h` and `.cpp`, the module files of the
OGSimulationJoltUnreal module (og-tickdriver task 15e). Its guards are `OGSimulationJoltUnreal-guards.md` (none live).

> **Licence.** This tier is **MPL-2.0**, matching the code it describes (the user's ruling of 2026-10-18).

## §1 What the module is

The Unreal side of the Jolt engine that does not depend on a game: today the Jolt engine traits,
`JoltEngineTraitsUImpl` (`JoltEngineTraitsUImpl-rationale.md`). The user chose a module of its own for them
(2026-10-18) over adding Jolt to OGSimulationUnreal, so that OGSimulationUnreal stays free of Jolt: its rules name no
Jolt module and its code includes no og-simulation-jolt header. A future engine scheduled by og-simulation's tick
driver would get a module of the same shape.

| dependency | why |
|---|---|
| OGSimulationJolt | the Jolt adapters, the runtime, the fingerprint and the world-host template the traits name |
| OGSimulationUnreal | the Unreal world mutex (`UEFMutexLockableUImpl`), the engine port (`SimulationEngineUImpl`), the static importer's channel table type, and `LogOGSimHost`, the category of the log sink |
| OGSimulation, Core, CoreUObject, Engine | the og-simulation types and the Unreal types the included headers name |

A game that uses Jolt adds this module to its own dependencies and selects `JoltEngineTraitsUImpl` in its seam.

## §2 The module files

* The header includes Unreal's CoreMinimal header and declares nothing; the `.cpp` implements the module with Unreal's
  `FDefaultModuleImpl`. The module has no start-up work.
* The project lists the module in its `.uproject` (Runtime, loading phase Default, beside OGSimulationUnreal) and in
  its targets' extra module names, as it lists OGSimulationUnreal.
* No `#if` on a game's backend switch: the module compiles the same in every configuration of a game, and registers
  no console variable. The Jolt-only console variable `og.Jolt.ContactCounts` stays in the game module, where the
  backend switch is visible (`JoltEngineTraitsUImpl-rationale.md` §3).

## §3 One module for the thread context

og-simulation's thread context (`TickDriverThreadContext-rationale.md` §7 in og-simulation's docs) must compile into
one module, the game module. The traits' header names the world-host template, but an alias template is
instantiated where it is used, which is the game's instantiation of `TickDriverBackendUImpl`. The module's own
translation units (`OGSimulationJoltUnreal.cpp`, `JoltEngineTraitsUImpl.cpp`) instantiate no driver, step scope or
world host. og-tickdriver task 15e checked their objects with `dumpbin`, after a positive control on the game
module's object that holds the instantiation.
