<!-- SPDX-License-Identifier: BUSL-1.1 -->
# `PhysicsBackendUImpl.h` — rationale

Companion to `Source/OGBrawlerUnreal/PhysicsBackendUImpl.h`. The header carries no prose and has no guards document:
what could go wrong in it is a compile error.

> **Licence.** This tier is **BUSL-1.1**, matching the code it describes.

## §1 What it is

Since og-tickdriver task 15a the header is **seam A** of the physics backend: what differs between physics engines.
Seam A has two layers, and this header holds the concept of each and the one selector that picks them. Since
og-tickdriver task 15f the engine-generic concepts are og-simulation's (`PhysicsEngineTraits.h`, included as
`OGSimulation/PhysicsEngineTraits.h`; `PhysicsEngineTraits-rationale.md` in og-simulation's docs); in task 15e they
were OGSimulationUnreal's (§2). This header includes them; it keeps the brawler's binding concept, which refines the
generic one, and the selector.

* **Engine traits** are engine-generic: any Unreal game on that engine could use them. One per engine,
  `JoltEngineTraitsUImpl` (`JoltEngineTraitsUImpl-rationale.md`; in the OGSimulationJoltUnreal module since
  og-tickdriver task 15e) and `ChaosEngineTraitsUImpl` (`ChaosEngineTraitsUImpl-rationale.md`; in OGSimulationUnreal
  since task 15e). The concept is `PhysicsEngineTraits` (og-simulation's since task 15f).
* **The game's per-engine binding** is the brawler's glue for one engine. One per engine, `BrawlerJoltBindingUImpl`
  (`BrawlerJoltBindingUImpl-rationale.md`) and `BrawlerChaosBindingUImpl` (`BrawlerChaosBindingUImpl-rationale.md`).
  The concept is `BrawlerEngineBinding`, over the binding and the engine traits it binds to; since og-tickdriver
  task 15e it refines the generic `PhysicsEngineBinding`, og-simulation's since task 15f (§3).

Seam B, who schedules the steps, is `PhysicsBackendHostUImpl.h` (`PhysicsBackendHostUImpl-rationale.md`). Until
task 15a this header was the rough form of both seam-A layers: two `#if` blocks of aliases and functions in namespace
`physicsBackendUImpl` (§7).

The header still stops the build when `OG_PHYSICS_BACKEND_CHAOS` is undefined. Its `#error` text is unchanged
(`OGBrawlerSimulationManager-rationale.md` §18 owns the macro).

## §2 `PhysicsEngineTraits` (in og-simulation since og-tickdriver task 15f)

The concept of an engine's traits, its helpers `EngineLogSink` and `PhysicsEngineWorldHostTemplate`, and the
traits half of the self-check moved, unchanged, to `PhysicsEngineTraits.h` in OGSimulationUnreal in og-tickdriver
task 15e (lead ruling L3; MPL-2.0 by the user's ruling of 2026-10-18), and on to og-simulation in task 15f, because the
header names only STL (§7). What each member is, and why the log sink and the world-host template are required only
of a driver-scheduled engine, in engine-free words, is og-simulation's `PhysicsEngineTraits-rationale.md` §2. This
header includes `OGSimulation/PhysicsEngineTraits.h` and still asserts the concept on the configuration's traits (§4).

**This project's two engine traits** (moved back here from that document by task 15f, which keeps no game or engine
names):

| member | Jolt | Chaos |
|---|---|---|
| `BodyAdapter`, `ReaderAdapter`, `QueryAdapter`, `Factory`, `VizQuery`, `VizReader` | `JoltPhysicsBodyAdapter`, `JoltPhysicsBodyReaderAdapter`, `JoltSpatialQueryAdapter`, `JoltPhysicsFactory`, `JoltSpatialQueryAdapter`, `JoltPhysicsBodyReaderAdapter` | `ChaosPhysicsBodyAdapter`, `ChaosPhysicsBodyReaderAdapter`, `ChaosSpatialQueryAdapter`, `ChaosPhysicsFactory`, `ChaosSpatialQueryAdapter`, `ChaosPhysicsBodyReaderAdapter` |
| `CategoryTable` | `const std::vector<UEStaticCategoryChannel>&`, for `UEStaticGeometryImporter` | `std::initializer_list<ChaosCategoryMapping>`, for `ChaosSpatialQueryAdapter` |
| `kName` | `"jolt"` | `"chaos"` |
| `computeDeterminismFingerprint(logger)` | og-simulation-jolt's `determinismFingerprint` | `std::nullopt` |
| `kHasWorldHost` | `true` | `false` |
| `WorldHost<SimulatableT>` | `JoltTickWorldHost<SimulatableT, UEFMutexLockableUImpl, SimulationEngineUImpl>` | none |
| `logEngineMessage` | a function onto `LogOGSimHost` | none |

`kName` is the backend name of `[SimHost.Backend]` and the build identity's token.

* **The two table forms.** The static importer has a `std::vector` constructor, and the Chaos query adapter has only
  an `std::initializer_list` constructor; hence the engine member.
* **The log sink.** The tick-driver host puts the Jolt traits' sink in the world host's configuration as
  og-simulation-jolt's `JoltRuntime::LogFn` (`JoltEngineTraitsUImpl-rationale.md` §2). The Chaos adapters take no
  message callback (their lines go through `UE_LOG` in OGSimulationUnreal), so the Chaos traits have none
  (`ChaosEngineTraitsUImpl-rationale.md` §2); task 15b gated the sink on `kHasWorldHost` (§7).
* **Members the tick-driver host reads that the concept does not require:** the traits' `splitStatics` and its
  `Statics` result type (og-tickdriver task 15e). Traits without them fail inside the host's `buildWorld` at the
  instantiation, not at the requirement (`TickDriverBackendUImpl-rationale.md` §12). That the world-host instance
  models og-simulation's `TickDriverWorldHost` is the tick-driver host's own requirement (§12 there).
* **Who includes the header:** `TickDriverBackendUImpl.h` in OGSimulationUnreal (the driver-scheduled host's
  requirement, `TickDriverBackendUImpl-rationale.md` §13) and this header.

## §3 `BrawlerEngineBinding`

**Since og-tickdriver task 15e** the concept is the generic `PhysicsEngineBinding` (og-simulation's since task 15f;
the simulatable, the category table, and, for a driver-scheduled engine, the world-host instance and the park snap;
og-simulation's `PhysicsEngineTraits-rationale.md` §3) plus the one requirement that names a brawler type: for a driver-scheduled
engine, the capsule check, taken by address as a `CapsuleContractCheck` (`void (*)(const AOGBrawlerUECharacter&,
unsigned int)`). The conjunction is the same set of requirements as the concept had before, so both bindings and
every self-check shape model or fail it as before; task 15e saw the capsule part and the park-snap part fail through
the refinement. The table below is the whole of it.

| member | what it is | Jolt binding | Chaos binding |
|---|---|---|---|
| `Simulatable` | the simulatable type the world host is instantiated with | `SimulatableBrawler` | `SimulatableBrawler` |
| `categoryChannels()` | the brawler's six-row category table, in the engine's `CategoryTable` form | a `const std::vector<UEStaticCategoryChannel>&` | an `std::initializer_list<ChaosCategoryMapping>` |
| `checkCapsuleContract` | only when the engine has a world host: the capsule-to-descriptor contract check, a function whose address converts to `CapsuleContractCheck` | a function | none |
| `kParkSnap` | only when the engine has a world host: the render park-snap distance, convertible to `float` | `-0.5f * brawlerProjectileSimulation::kParkZ` | none |

When the engine traits have a world host, the binding concept also requires
`EngineTraitsT::WorldHost<BindingT::Simulatable>` to name a type.

**The split** (og-tickdriver task 15e, lead ruling L3). Until task 15e the generic concept's requirements were three of
the four requirements of `BrawlerEngineBinding`, in this header. They name no game type, so they moved out as
`PhysicsEngineBinding`, and the capsule check, which names the brawler's character, stayed here.

**Members the tick-driver host reads that no concept requires:** the binding's `mappedCategories` and
`contactCountsEnabled` (`TickDriverBackendUImpl-rationale.md` §12). The capsule check is required of the host's
composition instead (`TickDriverBackendComposition`), as a call with the game binding's character type.

**Why two members are gated** (og-tickdriver task 15b). Only the tick-driver host reads the capsule check and the park
snap: the check before each bind, the snap in the presenter's controls. Chaos's factory checks the capsule itself and
Chaos renders through its own interpolation, so the Chaos binding has neither, and the concept asks for neither when
`kHasWorldHost` is false, as it asks for no `WorldHost`. The capsule check is taken by address and the park snap must
convert to `float`, so neither a `std::nullptr_t` nor a `std::nullopt_t` stand-in models the concept for a
driver-scheduled engine. Until task 15b the concept required both of every binding, and the Chaos binding held two
such typed nones; a `std::nullptr_t` converts to `CapsuleContractCheck`, so a host that stored it would have compiled
and made a null call.

## §4 The selector

One `#if OG_PHYSICS_BACKEND_CHAOS` includes the configuration's engine traits and binding and names them (since
og-tickdriver task 15e the Chaos traits from OGSimulationUnreal and the Jolt traits from OGSimulationJoltUnreal, with
the include paths `OGSimulationUnreal/ChaosEngineTraitsUImpl.h` and `OGSimulationJoltUnreal/JoltEngineTraitsUImpl.h`):

| alias | Chaos configuration | Jolt configuration |
|---|---|---|
| `EngineTraits` | `ChaosEngineTraitsUImpl` | `JoltEngineTraitsUImpl` |
| `EngineBinding` | `BrawlerChaosBindingUImpl` | `BrawlerJoltBindingUImpl` |

Two `static_assert`s follow it: `PhysicsEngineTraits<EngineTraits>` and
`BrawlerEngineBinding<EngineBinding, EngineTraits>`. They check the configuration being built only, as
`PhysicsBackendHost` does: the other configuration's headers are not included.

* **Who names them.** `AOGBrawlerSimulationManager` types its three adapter optionals and its two visualization doors
  through `EngineTraits`, and prints `EngineTraits::kName` in `[SimHost.Backend]`. Since og-tickdriver task 15b the
  composition (`BrawlerCompositionUImpl`'s integration executor and hit-detection system), the build identity, the
  session's concept test and the seam concept (`PhysicsBackendHostUImpl.h`) name them too. The Chaos host names its
  own engine's traits and binding directly (`ChaosEngineTraitsUImpl`, `BrawlerChaosBindingUImpl`), because it exists
  in one configuration only; the tick-driver host takes them as its template parameters, and the seam selector
  asserts that its pair is this selector's (`PhysicsBackendHostUImpl-rationale.md` §1).
* **The two aliases are at global scope,** as the session's members name them (`EngineTraits::BodyAdapter`). No
  header in the engine's Runtime source, this project's Source or its Plugins directory declared either name when task 15a
  added them.
* **The four seam-A headers have no `#if`.** This header includes the configuration's pair, and each host includes
  its own engine's headers. The `.cpp` files of the traits and bindings are whole-file arms
  (`OGBrawlerSimulationManager-rationale.md` §18, rule 2), so the Jolt console variable registers in the Jolt
  configuration only, as before. Since og-tickdriver task 15e that variable is the Jolt binding's
  (`BrawlerJoltBindingUImpl-rationale.md` §6): the Jolt traits' `.cpp` is in a module that cannot see the switch and
  is no arm, and registers nothing.

## §5 The names that were kept in `physicsBackendUImpl` (og-tickdriver tasks 15a and 15b)

Task 15a kept six names in namespace `physicsBackendUImpl`, each defined from `EngineTraits`, for the four files
outside its ownership that still named them: `BodyAdapter`, `QueryAdapter`, `VizQuery`, `VizReader`, kBackendName
and `computeDeterminismFingerprint`. Task 15b switched those files to `EngineTraits::` and deleted the names:

| file | was | now |
|---|---|---|
| `BrawlerCompositionUImpl.h` | the namespace's `BodyAdapter`, `QueryAdapter` | `EngineTraits::BodyAdapter`, `EngineTraits::QueryAdapter` |
| `PhysicsBackendHostUImpl.h` | the namespace's `VizQuery`, `VizReader`, unqualified inside the namespace | `EngineTraits::VizQuery`, `EngineTraits::VizReader` |
| `OGBrawlerSimulationManagerConceptTest.cpp` | the namespace's `BodyAdapter`, `QueryAdapter` | `EngineTraits::BodyAdapter`, `EngineTraits::QueryAdapter` |
| `OGBuildIdentityUImpl.cpp` | the namespace's kBackendName, `computeDeterminismFingerprint` | `EngineTraits::kName`, `EngineTraits::computeDeterminismFingerprint` |

They name the same types and call the same function as before. This header no longer opens the namespace; seam B's
three names (`PhysicsBackendHost`, `Backend`, `BackendWorlds`) are the only ones left in it, in
`PhysicsBackendHostUImpl.h`.

## §6 The self-check

**Since og-tickdriver task 15e the namespace is shared.** The traits shapes (`TraitsCore`, the four traits),
`SelfScheduledBinding`, the three `PhysicsEngineTraits` asserts and the two new `PhysicsEngineBinding` asserts are
in `PhysicsEngineTraits.h`, OGSimulationUnreal's in task 15e and og-simulation's since task 15f (og-simulation's
`PhysicsEngineTraits-rationale.md` §4). This header reopens
`physicsEngineSeamSelfCheck` for the binding shapes that name the brawler's character (`BindingWithoutParkSnap`,
`Binding`, `BindingWithoutCapsuleContract`, `BindingWithNullCapsuleContract`, `BindingWithNoneParkSnap`) and the
five `BrawlerEngineBinding` asserts, unchanged; they use the moved traits shapes and `SelfScheduledBinding`.

Namespace `physicsEngineSeamSelfCheck` holds reference shapes, as og-simulation's concepts hold theirs. These must
model the concepts: a driver-scheduled engine (`DriverScheduledTraits`), a self-scheduled engine
(`SelfScheduledTraits`), a binding with every member (`Binding`), and a binding with only a simulatable and a category
table (`SelfScheduledBinding`) over the self-scheduled engine. These must not, each with one member missing or wrong:

| shape | what is wrong | the concept part it controls | since |
|---|---|---|---|
| `DriverScheduledTraitsWithoutWorldHost` | `kHasWorldHost` true, no `WorldHost` | the world-host half of `PhysicsEngineTraits` | task 15a |
| `DriverScheduledTraitsWithNullLogSink` | a `std::nullptr_t` log sink | the log sink taken by address | task 15b |
| `BindingWithoutParkSnap` over the driver-scheduled engine | no `kParkSnap` | the park snap | task 15a |
| `BindingWithoutCapsuleContract` over the driver-scheduled engine | no `checkCapsuleContract` | the capsule check | task 15b |
| `BindingWithNullCapsuleContract` over the driver-scheduled engine | a `std::nullptr_t` capsule check | the capsule check taken by address | task 15b |
| `BindingWithNoneParkSnap` over the driver-scheduled engine | a `std::nullopt_t` park snap | the park snap converting to `float` | task 15b |
| `SelfScheduledBinding` over the driver-scheduled engine | no `kParkSnap` | the park snap of `PhysicsEngineBinding` | task 15e |

Each negative `static_assert` is a control that its part can fail. Task 15a saw its two fail with the member put back;
task 15b saw each of its four fail with the member corrected, and saw the selector's `static_assert`s fail on a Jolt
binding without `checkCapsuleContract` and on Jolt traits whose log sink is a `std::nullptr_t`. Both tasks' notes
quote the compiler. Task 15e saw each of its two new `PhysicsEngineBinding` asserts fail with its shape corrected.
Task 15f changed only the messages of the five asserts it moved to og-simulation: each lost its task references and
its game wording and still states its rule (the third still names what it replaced, the typed-none log sink).

## §7 History

* **og-simulationscheduler-withjolt task 51** created the header as the backend switch: the `#error`, and the
  adapter aliases per configuration. Task 83 moved the hosts to `PhysicsBackendHostUImpl.h`.
* **Until og-tickdriver task 15a** the header held, per configuration, the six adapter aliases, kChaosBackend,
  kBackendName and `computeDeterminismFingerprint` in namespace `physicsBackendUImpl`. Task 15a moved the aliases,
  the name and the fingerprint into the engine traits (§2), and replaced the two `#if` blocks with the selector
  (§4). kChaosBackend had no reader in the project's Source or Plugins directories and was not carried; `kName` names the engine.
  `ReaderAdapter` and `Factory` stayed out of §5's list because nothing outside the session named them.
* **og-tickdriver task 15b** gated the log sink, the capsule check and the park snap on `kHasWorldHost`, took the two
  functions by address, deleted the Chaos side's three typed nones, and deleted the namespace's kept names (§5).
* **og-tickdriver task 15e** moved `PhysicsEngineTraits`, `EngineLogSink`, `PhysicsEngineWorldHostTemplate` and the
  traits half of the self-check to OGSimulationUnreal (`PhysicsEngineTraits.h`), split `PhysicsEngineBinding` out of
  `BrawlerEngineBinding`, and pointed the selector at the traits' new homes (§2, §3, §4, §6). Until then the concepts
  and the self-check were BUSL-1.1 in this header; the user ruled the move to MPL-2.0 on 2026-10-18.
* **og-tickdriver task 15f** moved `PhysicsEngineTraits.h` and its docs on from OGSimulationUnreal to og-simulation
  (user, 2026-10-18: "move it to separate small task and dispatch it"), because it names only STL: code goes in the
  lowest layer it can live in. The code is unchanged; the five assert messages lost their task references and game
  wording (§6). The two includes (this header and `TickDriverBackendUImpl.h`) changed their path to
  `OGSimulation/PhysicsEngineTraits.h` and nothing else. The moved rationale keeps the engine-free narrative; the
  passages that named this project's engines, modules or tasks came back to this document (§2, §3, §6) and to
  `TickDriverBackendUImpl-rationale.md` §13.
