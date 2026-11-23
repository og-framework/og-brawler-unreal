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

* **Engine traits** are engine-generic: any Unreal game on that engine could use them. One per engine; the one left is
  `JoltEngineTraitsUImpl` (`JoltEngineTraitsUImpl-rationale.md`; in the OGSimulationJoltUnreal module since
  og-tickdriver task 15e). The Chaos engine traits (in OGSimulationUnreal since task 15e) lost their last selector in
  og-session-runtime task 3 and were deleted with their two documents by its task 4. The concept is
  `PhysicsEngineTraits` (og-simulation's since task 15f).
* **The game's per-engine binding** is the brawler's glue for one engine. One per engine: `BrawlerJoltBindingUImpl`
  (`BrawlerJoltBindingUImpl-rationale.md`). The Chaos binding was deleted with its two documents by og-session-runtime task 3.
  The concept is `BrawlerEngineBinding`, over the binding and the engine traits it binds to; since og-tickdriver
  task 15e it refines the generic `PhysicsEngineBinding`, og-simulation's since task 15f (§3).

Seam B, who schedules the steps, is `PhysicsBackendHostUImpl.h` (`PhysicsBackendHostUImpl-rationale.md`). Until
task 15a this header was the rough form of both seam-A layers: two `#if` blocks of aliases and functions in namespace
`physicsBackendUImpl` (§7).

Until og-session-runtime task 3 the header also stopped the build when the backend macro `OG_PHYSICS_BACKEND_CHAOS` was
undefined (an `#error` that named `OGBrawlerSimulationManager-rationale.md` §18, which describes the switch). Task 3
removed the macro (the constant and its definition in the game module's build rules), the `#error` and the selector's
Chaos branch: the header names the Jolt pair unconditionally (§4). Chaos was off in every build before it.

## §2 `PhysicsEngineTraits` (in og-simulation since og-tickdriver task 15f)

The concept of an engine's traits, its helpers `EngineLogSink` and `PhysicsEngineWorldHostTemplate`, and the
traits half of the self-check moved, unchanged, to `PhysicsEngineTraits.h` in OGSimulationUnreal in og-tickdriver
task 15e (lead ruling L3; MPL-2.0 by the user's ruling of 2026-10-18), and on to og-simulation in task 15f, because the
header names only STL (§7). What each member is, and why the log sink and the world-host template are required of
every engine (since og-session-runtime task 6; until then only of a driver-scheduled engine), in engine-free words, is
og-simulation's `PhysicsEngineTraits-rationale.md` §2. This header includes `OGSimulation/PhysicsEngineTraits.h` and
still asserts the concept on the configuration's traits (§4).

**This project's engine traits** (moved back here from that document by task 15f, which keeps no game or engine
names):

| member | Jolt |
|---|---|
| `BodyAdapter`, `ReaderAdapter`, `QueryAdapter`, `Factory`, `VizQuery`, `VizReader` | `JoltPhysicsBodyAdapter`, `JoltPhysicsBodyReaderAdapter`, `JoltSpatialQueryAdapter`, `JoltPhysicsFactory`, `JoltSpatialQueryAdapter`, `JoltPhysicsBodyReaderAdapter` |
| `CategoryTable` | `const std::vector<UEStaticCategoryChannel>&`, for `UEStaticGeometryImporter` |
| `kName` | `"jolt"` |
| `computeDeterminismFingerprint(logger)` | og-simulation-jolt's `determinismFingerprint` |
| `WorldHost<SimulatableT>` | `JoltTickWorldHost<SimulatableT, UEFMutexLockableUImpl, SimulationEngineUImpl>` |
| `logEngineMessage` | a function onto `LogOGSimHost` |

Until og-session-runtime task 4 the table had a Chaos column: OGSimulationUnreal's Chaos body adapter, reader adapter,
spatial query adapter and factory, an `std::initializer_list` category table, kName `"chaos"`, no fingerprint, no world
host and no log sink. Until its task 6 it also had a row for og-simulation's world-host flag (Jolt true, Chaos false).

`kName` is the backend name of `[SimHost.Backend]` and the build identity's token.

* **The table form.** The static importer has a `std::vector` constructor, hence `CategoryTable` is an engine member:
  the former Chaos query adapter (removed by og-session-runtime task 4) had only an `std::initializer_list` one.
* **The log sink.** The tick-driver host puts the Jolt traits' sink in the world host's configuration as
  og-simulation-jolt's `JoltRuntime::LogFn` (`JoltEngineTraitsUImpl-rationale.md` §2). The former Chaos adapters took
  no message callback (their lines went through `UE_LOG`), so the Chaos traits had none; task 15b gated the sink on the
  traits' world-host flag (§7), and og-session-runtime task 6 removed the flag: the sink is required of every engine.
* **Members the tick-driver host reads that the concept does not require:** the traits' `splitStatics` and its
  `Statics` result type (og-tickdriver task 15e). Traits without them fail inside the host's `buildWorld` at the
  instantiation, not at the requirement (`TickDriverBackendUImpl-rationale.md` §12). That the world-host instance
  models og-simulation's `TickDriverWorldHost` is the tick-driver host's own requirement (§12 there).
* **Who includes the header:** `TickDriverBackendUImpl.h` in OGSimulationUnreal (the driver-scheduled host's
  requirement, `TickDriverBackendUImpl-rationale.md` §13) and this header.

## §3 `BrawlerEngineBinding`

**Since og-tickdriver task 15e** the concept is the generic `PhysicsEngineBinding` (og-simulation's since task 15f;
the simulatable, the category table, the world-host instance and the park snap, the last two of every engine since
og-session-runtime task 6 and until then only of a driver-scheduled engine;
og-simulation's `PhysicsEngineTraits-rationale.md` §3) plus the one requirement that names a brawler type: the
capsule check, taken by address as a `CapsuleContractCheck` (`void (*)(const AOGBrawlerUECharacter&,
unsigned int)`). Since og-session-runtime task 5 this header requires the capsule check of every binding; until then
only of a driver-scheduled engine's (below). When task 15e split the concept the conjunction was the same set of
requirements as the concept had before, so both bindings and every self-check shape modelled or failed it as before;
task 15e saw the capsule part and the park-snap part fail through the refinement. The table below is the whole of it.

| member | what it is | Jolt binding |
|---|---|---|
| `Simulatable` | the simulatable type the world host is instantiated with | `SimulatableBrawler` |
| `categoryChannels()` | the brawler's six-row category table, in the engine's `CategoryTable` form | a `const std::vector<UEStaticCategoryChannel>&` |
| `checkCapsuleContract` | the capsule-to-descriptor contract check, a function whose address converts to `CapsuleContractCheck` (of every binding since og-session-runtime task 5; until then only when the engine had a world host) | a function |
| `kParkSnap` | the render park-snap distance, convertible to `float` (of every binding since og-session-runtime task 6; until then only when the engine had a world host) | `-0.5f * brawlerProjectileSimulation::kParkHeight` |

Until og-session-runtime task 3 a Chaos binding modelled the concept too: the same simulatable, the same six rows as an
`std::initializer_list<ChaosCategoryMapping>`, and neither of the last two members.

The binding concept also requires `EngineTraitsT::WorldHost<BindingT::Simulatable>` to name a type (of every engine
since og-session-runtime task 6; until then when the engine traits had a world host).

**The split** (og-tickdriver task 15e, lead ruling L3). Until task 15e the generic concept's requirements were three of
the four requirements of `BrawlerEngineBinding`, in this header. They name no game type, so they moved out as
`PhysicsEngineBinding`, and the capsule check, which names the brawler's character, stayed here.

**Members the tick-driver host reads that no concept requires:** the binding's `mappedCategories` and
`contactCountsEnabled` (`TickDriverBackendUImpl-rationale.md` §12). The capsule check is required of the host's
composition instead (`TickDriverBackendComposition`), as a call with the game binding's character type.

**Why two members were gated** (og-tickdriver task 15b). Only the tick-driver host reads the capsule check and the park
snap: the check before each bind, the snap in the presenter's controls. Chaos's factory checked the capsule itself and
Chaos rendered through its own interpolation, so the Chaos binding had neither (until og-session-runtime task 3 deleted it),
and the concept asked for neither of an engine without a world host, as it asks for no `WorldHost`. og-session-runtime
task 5 removed this header's gate: the one binding left, Jolt's, is a driver-scheduled engine's, so the capsule check is
required unconditionally; the park snap's gate was og-simulation's, in `PhysicsEngineBinding`, until og-session-runtime
task 6 removed it with the world-host flag. The capsule check is taken
by address and the park snap must
convert to `float`, so neither a `std::nullptr_t` nor a `std::nullopt_t` stand-in models the concept for a
driver-scheduled engine. Until task 15b the concept required both of every binding, and the Chaos binding held two
such typed nones; a `std::nullptr_t` converts to `CapsuleContractCheck`, so a host that stored it would have compiled
and made a null call.

## §4 The selector

Since og-session-runtime task 3 the selector has no `#if`: it includes the Jolt engine traits and the Jolt binding
(`OGSimulationJoltUnreal/JoltEngineTraitsUImpl.h`, `OGBrawlerUnreal/BrawlerJoltBindingUImpl.h`) and names them:

| alias | names |
|---|---|
| `EngineTraits` | `JoltEngineTraitsUImpl` |
| `EngineBinding` | `BrawlerJoltBindingUImpl` |

Until task 3 one `#if OG_PHYSICS_BACKEND_CHAOS` picked the pair per configuration, and the Chaos configuration named
the Chaos engine traits (ChaosEngineTraitsUImpl, deleted by og-session-runtime task 4) and the Chaos binding. Chaos was off in
every build, so the Jolt pair is the one every build compiled before the task too.

Two `static_assert`s follow it: `PhysicsEngineTraits<EngineTraits>` and
`BrawlerEngineBinding<EngineBinding, EngineTraits>`, on the one pair.

* **Who names them.** `AOGBrawlerSimulationManager` types its three adapter optionals and its two visualization doors
  through `EngineTraits`, and prints `EngineTraits::kName` in `[SimHost.Backend]`. Since og-tickdriver task 15b the
  composition (`BrawlerCompositionUImpl`'s integration executor and hit-detection system), the build identity, the
  session's concept test and the seam concept (`PhysicsBackendHostUImpl.h`) name them too. The tick-driver host
  takes them as its template parameters, and `PhysicsBackendHostUImpl.h` asserts that its pair is this header's
  (`PhysicsBackendHostUImpl-rationale.md` §1). Until og-session-runtime task 3 the Chaos host named its own engine's traits
  and binding directly.
* **The two aliases are at global scope,** as the session's members name them (`EngineTraits::BodyAdapter`). No
  header in the engine's Runtime source, this project's Source or its Plugins directory declared either name when task 15a
  added them.
* **No seam-A file has an `#if` since og-session-runtime task 3.** Until then the binding `.cpp` files were whole-file
  arms (`OGBrawlerSimulationManager-rationale.md` §18, rule 2), so the Jolt console variable registered in the Jolt
  configuration only. Task 3 removed the Jolt binding's arm with the switch, and the variable registers in every
  build, which is the build it registered in before. Since og-tickdriver task 15e that variable is the Jolt binding's
  (`BrawlerJoltBindingUImpl-rationale.md` §6); the Jolt traits' `.cpp` is in a module that never saw the switch, and
  registers nothing.

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

**Since og-tickdriver task 15e the namespace is shared.** The traits shapes (`TraitsCore` and three traits since
og-session-runtime task 6, which removed the fourth, the self-scheduled one), `BindingCore` (named SelfScheduledBinding
until task 6), the three `PhysicsEngineTraits` asserts and the two `PhysicsEngineBinding` asserts are
in `PhysicsEngineTraits.h`, OGSimulationUnreal's in task 15e and og-simulation's since task 15f (og-simulation's
`PhysicsEngineTraits-rationale.md` §4). This header reopens
`physicsEngineSeamSelfCheck` for the binding shapes that name the brawler's character (`BindingWithoutParkSnap`,
`Binding`, `BindingWithoutCapsuleContract`, `BindingWithNullCapsuleContract`, `BindingWithNoneParkSnap`) and the
five `BrawlerEngineBinding` asserts; they use the moved traits shapes. Until og-session-runtime task 5 the binding shapes
derived from og-simulation's self-scheduled binding shape; since then they derive from this header's
`BrawlerBindingCore` (the same two members: og-simulation's `Simulatable` shape and a category table), and the positive
assert names only the driver-scheduled traits.

Namespace `physicsEngineSeamSelfCheck` holds reference shapes, as og-simulation's concepts hold theirs. This
header's positive assert: a binding with every member (`Binding`) models `BrawlerEngineBinding` over the
driver-scheduled engine (`DriverScheduledTraits`). Until og-session-runtime task 5 it also asserted `Binding`, and
og-simulation's binding with only a simulatable and a category table, over og-simulation's self-scheduled engine; the
shared namespace's own positive shapes are og-simulation's (`PhysicsEngineTraits-rationale.md` §4). These must not
model the concepts, each with one member missing or wrong:

| shape | what is wrong | the concept part it controls | since |
|---|---|---|---|
| `DriverScheduledTraitsWithoutWorldHost` | no `WorldHost` (until og-session-runtime task 6 also the world-host flag true) | the world-host requirement of `PhysicsEngineTraits` | task 15a |
| `DriverScheduledTraitsWithNullLogSink` | a `std::nullptr_t` log sink | the log sink taken by address | task 15b |
| `BindingWithoutParkSnap` over the driver-scheduled engine | no `kParkSnap` | the park snap | task 15a |
| `BindingWithoutCapsuleContract` over the driver-scheduled engine | no `checkCapsuleContract` | the capsule check | task 15b |
| `BindingWithNullCapsuleContract` over the driver-scheduled engine | a `std::nullptr_t` capsule check | the capsule check taken by address | task 15b |
| `BindingWithNoneParkSnap` over the driver-scheduled engine | a `std::nullopt_t` park snap | the park snap converting to `float` | task 15b |
| `BindingCore` (SelfScheduledBinding until og-session-runtime task 6) over the driver-scheduled engine | no `kParkSnap` | the park snap of `PhysicsEngineBinding` | task 15e |

Each negative `static_assert` is a control that its part can fail. Task 15a saw its two fail with the member put back;
task 15b saw each of its four fail with the member corrected, and saw the selector's `static_assert`s fail on a Jolt
binding without `checkCapsuleContract` and on Jolt traits whose log sink is a `std::nullptr_t`. Both tasks' notes
quote the compiler. Task 15e saw each of its two new `PhysicsEngineBinding` asserts fail with its shape corrected.
Task 15f changed only the messages of the five asserts it moved to og-simulation: each lost its task references and
its game wording and still states its rule (the third still names what it replaced, the typed-none log sink).
og-session-runtime task 5 changed no negative shape's members, only their base: over `DriverScheduledTraits` the
removed gate was already closed (the traits have a world host), so each of the four negative asserts tests the same
requirement as before, and the build is their check that they still hold.

## §7 History

* **og-simulationscheduler-withjolt task 51** created the header as the backend switch: the `#error`, and the
  adapter aliases per configuration. Task 83 moved the hosts to `PhysicsBackendHostUImpl.h`.
* **Until og-tickdriver task 15a** the header held, per configuration, the six adapter aliases, kChaosBackend,
  kBackendName and `computeDeterminismFingerprint` in namespace `physicsBackendUImpl`. Task 15a moved the aliases,
  the name and the fingerprint into the engine traits (§2), and replaced the two `#if` blocks with the selector
  (§4). kChaosBackend had no reader in the project's Source or Plugins directories and was not carried; `kName` names the engine.
  `ReaderAdapter` and `Factory` stayed out of §5's list because nothing outside the session named them.
* **og-tickdriver task 15b** gated the log sink, the capsule check and the park snap on the traits' world-host flag, took the two
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
* **og-session-runtime task 3** (2026-11-23) removed the Chaos backend arm: the `#error` on an undefined backend macro,
  the selector's Chaos branch (§1, §4) and the Chaos binding, with the macro itself in the game module's build rules
  and the Chaos host. Chaos was off in every build, so the code every build compiles is unchanged. The world-host
  gate of the binding concept and the self-scheduled self-check shapes stayed until its task 5.
* **og-session-runtime task 5** (2026-11-23) removed the binding concept's world-host gate (the capsule check is
  required of every binding), the positive self-check's two self-scheduled conjuncts, and the binding shapes' base on
  og-simulation's self-scheduled binding shape, which `BrawlerBindingCore` replaces (§3, §6). The one binding, Jolt's,
  has a world host, so the concept's verdict on it is unchanged.
* **og-session-runtime task 4** (2026-11-23) deleted the Chaos engine traits, their adapters and factory, and their two
  documents from OGSimulationUnreal (§1, §2).
* **og-session-runtime task 6** (2026-11-23) removed og-simulation's world-host flag and its self-scheduled shapes: the
  world-host template, the log sink and the park snap are required of every engine and binding, and the self-scheduled
  binding shape is `BindingCore` (§2, §3, §6).
