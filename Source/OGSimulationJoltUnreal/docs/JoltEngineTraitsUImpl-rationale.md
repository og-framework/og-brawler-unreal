<!-- SPDX-License-Identifier: MPL-2.0 -->
# `JoltEngineTraitsUImpl` — rationale

Companion to `Source/OGSimulationJoltUnreal/JoltEngineTraitsUImpl.h` and `.cpp` (og-tickdriver task 15a; moved to the
OGSimulationJoltUnreal module by og-tickdriver task 15e). Its guards are `JoltEngineTraitsUImpl-guards.md` (none live).

> **Licence.** This tier is **MPL-2.0**, matching the code it describes. The traits and this document were the game
> module's (BUSL-1.1) until og-tickdriver task 15e; the user ruled the move to MPL-2.0 on 2026-10-18.

## §1 What it is

The engine traits of the Jolt engine: everything an Unreal game needs to know about Jolt that does not depend on the
game. It models og-simulation's `PhysicsEngineTraits` (`PhysicsEngineTraits-rationale.md` §2 in og-simulation's docs,
since og-tickdriver task 15f), and a game's seam selects it as its
engine traits in its Jolt configuration (the brawler's is `PhysicsBackendUImpl.h`). Nothing in it names a game type:
a game's own Jolt glue is its binding's (og-simulation's `PhysicsEngineTraits-rationale.md` §3).

| member | what it is | where it was before og-tickdriver task 15a |
|---|---|---|
| the six adapter types | `JoltPhysicsBodyAdapter`, `JoltPhysicsBodyReaderAdapter`, `JoltSpatialQueryAdapter` (the query adapter and the visualization query), `JoltPhysicsFactory` | `PhysicsBackendUImpl.h`'s Jolt block |
| `CategoryTable` | `const std::vector<UEStaticCategoryChannel>&`, the form `UEStaticGeometryImporter`'s constructor takes | new: the binding concept checks the game's table against it |
| `kName` | `"jolt"` | kBackendName, `PhysicsBackendUImpl.h` |
| `computeDeterminismFingerprint` | acquires the Jolt runtime with the caller's logger, takes og-simulation-jolt's `determinismFingerprint`, releases the runtime | `PhysicsBackendUImpl.h`, the same statements |
| `logEngineMessage` | the Jolt log sink (§2) | logJoltToSimHost, in JoltBackendUImpl.cpp (the historical name of the host's `.cpp`, deleted by og-tickdriver task 15e) |
| `kHasWorldHost`, `WorldHost` | `true`, and the world-host template (§4) | the alias JoltTickWorldHostUImpl in JoltBackendUImpl.h (historical names) named the template directly |
| `Statics`, `splitStatics` | og-simulation-jolt's `JoltTickStatics`, and a forward of its `joltTickWorldHost::splitStatics` (§5) | new in og-tickdriver task 15e: the host's `buildWorld` called the function directly |

## §2 The log sink

`logEngineMessage` is the `JoltRuntime::LogFn` the tick-driver host puts in the world host's configuration
(`config.log`, `TickDriverBackendUImpl::buildWorld`). It writes one `LogOGSimHost` line per message: at `Warning`
when the message starts with `[Warning]`, otherwise at `Log`, with the text unchanged. The body is the old
logJoltToSimHost, statement for statement.

* **It is exported** (`OGSIMULATIONJOLTUNREAL_API`) since og-tickdriver task 15e: the game module, which instantiates
  the host, takes its address, and in a modular build that address crosses a module boundary. The struct is not
  exported as a whole: its other members are inline or compile-time constants.
* **It is compiled in every configuration** since task 15e: the module cannot see a game's backend switch. It
  registers nothing, so compiling it where nothing calls it changes nothing at run time.

A game's build identity passes its own logger to the fingerprint call, not this one; that is unchanged.

## §3 `og.Jolt.ContactCounts` is not here

From og-tickdriver task 15a until task 15e the console variable object and its read (`contactCountsEnabled`) were in
these traits' `.cpp`, as a whole-file arm on the game's backend switch, so the variable registered in the game's Jolt
configuration only. This module cannot see that switch (a game's define), so task 15e moved the variable and its read
into the game's Jolt binding (lead ruling L4: no console variable newly registers in the Chaos configuration). Its
name, default, help text, flags and read are unchanged; the host reads it through the binding
(`TickDriverBackendUImpl-rationale.md` §9 and §12). Task 15e compared the console-variable dumps of both
configurations before and after the move.

## §4 The world-host template

`WorldHost<SimulatableT>` is an alias template over og-simulation-jolt's
`JoltTickWorldHost<SimulatableT, UEFMutexLockableUImpl, SimulationEngineUImpl>`: the world mutex is Unreal's
(`UEFMutexLockableUImpl`) and the diagnostics engine is OGSimulationUnreal's (`SimulationEngineUImpl`). The game
supplies the simulatable type through its binding. The tick-driver host names the instance once, as
`TickDriverBackendUImpl::WorldHost`, from the binding's `Simulatable`; it is the type the host named before (as
JoltTickWorldHostUImpl, its historical name until og-tickdriver task 15b).

**The one-module rule.** An alias template is instantiated where it is used, not where it is declared. The use is the
game's instantiation of `TickDriverBackendUImpl`, in the game module, so the thread-context masks the world host reads
and writes still compile into one module (`TickDriverThreadContext-rationale.md` §7 in og-simulation's docs;
`OGSimulationJoltUnreal-rationale.md` §3).

## §5 The statics split

`splitStatics(description, physicsOnly)` returns og-simulation-jolt's `joltTickWorldHost::splitStatics` of the same
arguments: the imported static description split into the queryable statics and the physics-only statics, by the
importer's per-body flags. `Statics` names its result type. The tick-driver host calls it once per world build, on the
game thread, between the static import and the world host's `build`, and passes the two halves on.

It moved here in og-tickdriver task 15e (lead ruling L2, which carried over task 15d's R6): until then the host's
`.cpp` named the og-simulation-jolt function and its result type, so the host was the one piece of the Jolt
configuration that was not generic. The forward returns the function's result directly (no copy), so the split, its
thread and its moment are unchanged.

## §6 What it does not hold

* The game's category table, mapped categories, capsule check, park snap, simulatable type and contact-count switch:
  the game binding's.
* The `og.Sim.*` and `og.Render.*` variables, the step-task priority and the block mode: OGSimulationUnreal's since
  og-tickdriver task 15c (`UETickDriverHost.cpp`, `UEPresentControls.cpp`).
* No `#if`: a game's seam includes the header in its Jolt configuration only.
