<!-- SPDX-License-Identifier: BUSL-1.1 -->
# Low-Level Tests (UE-side test runners)

This document describes the **UE Low-Level Tests** setup that compiles
`OGSimulationTests.exe` and `OGBrawlerTests.exe` via UBT against Epic's
bundled Catch2 v3.4.0. For the standalone CMake build path (no UE installed)
see [standalone-build.md](standalone-build.md).

---

## Overview

Unreal Engine 5 ships a first-class Catch2 testing framework called **Low-Level
Tests** (LLT): prebuilt Catch2 v3.4.0 at `Engine/Source/ThirdParty/Catch2/`,
runner infrastructure at `Engine/Source/Developer/LowLevelTestsRunner/`, and a
`TestTargetRules` / `TestModuleRules` API for declaring test programs.

Each LLT target produces a standalone `.exe` that runs Catch2's `main()`
directly — no UE editor startup, no automation framework, no DLL boundary.
Tests start in well under a second.

We use LLT for **all UE-side test runs**. There is **no** UE Automation /
Session Frontend integration. The "DAttack / PCTM tests via the editor"
workflow that used to exist has been retired in favor of LLT.

---

## Targets

Two LLT targets exist:

| Target | Tests | Binary |
|---|---|---|
| `OGSimulationTests` | PCTM clock tests | `Binaries/Win64/OGSimulationTests/OGSimulationTests.exe` |
| `OGBrawlerTests` | DAttack + CharacterViz | `Binaries/Win64/OGBrawlerTests/OGBrawlerTests.exe` |

Both are `Program`-type, monolithic, `bCompileAgainstEngine = false`,
`bCompileAgainstCoreUObject = false` — pure-C++ tests with no engine init.

---

## File layout

### `OGSimulationTests`

```
Source/OGSimulationTests/
└── OGSimulationTests.Target.cs            ← TestTargetRules (the LLT exe entry)

Plugins/OGSimulation/Source/OGSimulationTests/
├── OGSimulationTests.Build.cs             ← TestModuleRules (the launch module)
├── CMakeLists.txt                         ← CMake-only (the standalone .exe)
├── Catch2BridgeSmokeTest.cpp              ← TEST_CASE source
└── PCTimeManagement/
    ├── ClientPredictionClockTest.cpp
    ├── NetworkTimeEstimatorTest.cpp
    └── ServerTickClockTest.cpp

Plugins/OGSimulation/Source/ThirdParty/Catch2/
├── catch_amalgamated.hpp                  ← vendored v3.14.0 (CMake only)
├── catch_amalgamated.cpp                  ← vendored v3.14.0 (CMake only)
└── NOTICE.md
```

The Target.cs lives in the project's `Source/` tree because that's where UBT
expects target entry points. The Build.cs lives alongside the test `.cpp`
files inside the plugin — same physical location as before, just with its
base class changed from `ModuleRules` to `TestModuleRules`.

### `OGBrawlerTests` — symmetric

```
Source/OGBrawlerTests/
└── OGBrawlerTests.Target.cs

Plugins/OGBrawler/Source/OGBrawlerTests/
├── OGBrawlerTests.Build.cs
├── CMakeLists.txt
├── HumanoidVisualizationTest.cpp
├── SimulatableBrawlerTest.cpp
├── SimulationCompositeForEachTest.cpp
├── SimulationIntegrationExecutorTest.cpp
├── SimulationNetSyncTest.cpp
└── SimulationReconciliationTest.cpp
```

`OGBrawlerTests` doesn't carry its own Catch2 vendor — `Plugins/OGSimulation/Source/ThirdParty/Catch2/` is reached by the standalone CMake build via the
`extern/og-simulation/` submodule chain.

---

## Test source conventions

Every test `.cpp` file is wrapped in:

```cpp
#if WITH_LOW_LEVEL_TESTS

#include "catch_amalgamated.hpp"
// ...test source...

TEST_CASE("Tag.Class.WhatItDoes", "[Tag][SubTag]")
{
    REQUIRE(...);
}

#endif // WITH_LOW_LEVEL_TESTS
```

- The `WITH_LOW_LEVEL_TESTS` macro is **auto-defined by UBT** for any target
  with `bWithLowLevelTestsOverride = true` in its `TestTargetRules`. The CMake
  build defines it explicitly via `target_compile_definitions(... WITH_LOW_LEVEL_TESTS=1)`.
- The `#include "catch_amalgamated.hpp"` resolves to:
  - UE/UBT side: Epic's bundled `Engine/Source/ThirdParty/Catch2/v3.4.0/extras/catch_amalgamated.hpp`
  - CMake side: our vendored `Plugins/OGSimulation/Source/ThirdParty/Catch2/catch_amalgamated.hpp`
- Test tags must be one of `[PCTM]`, `[DAttack]`, `[CharacterViz]`, or a
  sub-tag scoped under those. The top-level tags are what CI invocations
  filter on (see [Known issue](#known-issue-engine-implicit-test-noise) below).

---

## Build and run

### Build

```powershell
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat `
    OGSimulationTests Win64 Development `
    "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
```

Replace `OGSimulationTests` with `OGBrawlerTests` for the brawler exe. Both
build incrementally; a no-op rebuild is <2 seconds.

### Run

```powershell
.\Binaries\Win64\OGSimulationTests\OGSimulationTests.exe "[PCTM]"
.\Binaries\Win64\OGBrawlerTests\OGBrawlerTests.exe "[DAttack],[CharacterViz]"
```

**Always pass a tag filter.** Without one, Catch2 also tries to run the
Engine's `WITH_LOW_LEVEL_TESTS`-gated tests (Core/Async, Core/Containers,
ColorManagement, etc.) which crash because we don't initialize engine
subsystems. See [Known issue](#known-issue-engine-implicit-test-noise).

Exit code 0 = all selected tests passed. List discoverable tests with
`--list-tests`. The full Catch2 v3 CLI is available — `--help` for the menu.

### CI invocation

```powershell
# Returns 0 on full pass, non-zero on any failure.
.\Binaries\Win64\OGSimulationTests\OGSimulationTests.exe "[PCTM]"
.\Binaries\Win64\OGBrawlerTests\OGBrawlerTests.exe "[DAttack],[CharacterViz]"
```

For JUnit-style reports (so CI dashboards ingest them):

```powershell
.\OGSimulationTests.exe "[PCTM]" --reporter junit::out=pctm-results.xml
```

---

## Known issue: Engine implicit-test noise

### What happens

When a target has `bWithLowLevelTestsOverride = true`, UBT globally defines
`WITH_LOW_LEVEL_TESTS` across the compilation. Every Engine module we depend
on transitively (Core, in our case) has `WITH_LOW_LEVEL_TESTS`-gated test
`.cpp` files inside its own source tree — `Engine/Source/Runtime/Core/Tests/`
and `Engine/Source/Runtime/Core/Private/.../Tests/`. **All of those compile
into our `.exe` and register their `TEST_CASE`s alongside ours.**

`--list-tests` shows ~100+ tests interleaved with ours. Running unfiltered
crashes on the first Async test with `Assertion failed: GThreadPool != nullptr`
because those tests assume engine subsystems (`FCommandLine`, `GThreadPool`,
malloc) have been initialized via `InitAll(...)` — which we don't do.

### Why Epic's own `FoundationTests` doesn't have this problem

Their `TestTargetRules` sets `bCompileAgainstCoreUObject = true` and their
`TestGroupEvents.cpp` calls `InitAll(true, true)` in a `GROUP_BEFORE_GLOBAL`
hook. Engine subsystems exist; Core's implicit tests pass alongside theirs.

Our targets opt out of `Engine` and `CoreUObject` for fast pure-C++ builds.
The trade-off is the noise.

### Current mitigation (shipped 2026-05-20)

**Two-part solution that lives in our code (no engine patches):**

1. **`CATCH_REGISTER_TAG_ALIAS("[@og]", "<top-level tag list>")` in each test module.**
   - File: `Source/OGSimulationTests/OgTagAliases.cpp` and the og-brawler-tests equivalent.
   - The alias is a Catch2 v3 tag-aliasing macro that expands at filter-parse time.
   - Source of truth for "which tests `[@og]` selects" lives next to the tests in C++.
   - **Both modules ship a whitelist of top-level tags, not the `~[SelfTests]` blacklist
     this section described until 2026-08-25.** UE's framework tests are excluded by being
     absent from the list. So is any *new* top-level tag of your own: such a test still runs
     on a direct `exe "[MyTag]"` call but is invisible to `[@og]`, so the suite total stays
     green while it never executes.
   - Extend by **appending your new top-level tag** to the alias spec - not by adding
     `~[<UE-tag>]` exclusions, which the whitelist form does not need.
   - To check a module's list is still complete, compare the case count `[@og]` reports
     against the number of `TEST_CASE` definitions in that module. og-simulation-tests
     additionally holds `[Determinism][DevTest]` and `[Determinism][KU1CrossArch]` out of
     `[@og]` on purpose, through an AND-spec its `OgTagAliases.cpp` documents; those are
     opt-in, not a gap.

2. **`oglltest` wrapper cmdlet in og-tools** (`Invoke-OgLowLevelTest`).
   - Calls the exe with `[@og]` as the default Catch2 positional filter.
   - Invocation: `oglltest brawler`, `oglltest simulation`, or with overrides
     (`-Filter`, pass-through Catch2 args).

**Caveat: the bare exe still crashes if run without a filter.** The wrapper is
still required for the no-arg case. See [Why we couldn't make the exe self-filter](#why-we-couldnt-make-the-exe-self-filter) below.

### Why we couldn't make the exe self-filter

The original version of this doc proposed an Option C: "a small
`GROUP_BEFORE_GLOBAL` hook in each test module pushes a default Catch2 tag filter
into the session config, ~5 LoC per target." **This turned out not to be
implementable as written.** Investigated 2026-05-20:

- Catch2 v3.4's `Session::run(argc, argv)` is implemented inline as one statement
  (`applyCommandLine(argc, argv); if (rc == 0) run();`). The config is locked
  between those two calls and there's no hook in between.
- `GROUP_BEFORE_GLOBAL` fires via `RunContext::testsStarting()` (catch_run_context.cpp
  line ~500). By then the filtered group set is already passed in — the test spec
  is locked. The hook can run code (`InitAll(...)` for engine init, as Epic's
  `FoundationTests` does) but it cannot mutate the spec.
- UE's `FTestRunner` constructs the Session inline (`Catch::Session().run(...)`)
  with `CatchArgs` as a private member populated by `ProcessArgs`. No external
  injection point.

The only way to genuinely make the exes self-filter is an **engine modification**:
~5 LoC in `Engine/Source/Developer/LowLevelTestsRunner/Private/TestRunner.cpp` to
inject a default arg from a build-time define if `CatchArgs` ends up empty after
parsing. That patch has to be re-applied after every Engine `git pull` (Epic is
unlikely to upstream-merge a feature that benefits one project). Decided not worth
the upkeep — the tag alias + wrapper covers ~95% of the value.

### Other options (still relevant)

**Option B — Adopt Epic's pattern: full engine init.** Flip
`bCompileAgainstCoreUObject = true` (and possibly `bCompileAgainstEngine = true`),
add a `GROUP_BEFORE_GLOBAL` calling `InitAll(true, true)`. Core's implicit
tests now pass alongside ours; bonus coverage of Engine subsystems.
Pros: unfiltered runs work; clean transition story when we want UE-coupled
tests (the harness is already there). Cons: binary grows from ~30 MB to
hundreds of MB; build time from seconds to minutes; broader dependency
surface; blurs the "no UE needed for pure logic tests" property.
**Right move IF and WHEN we add UE-coupled tests.** Not before.

**Option D — Module-local gate (split `WITH_LOW_LEVEL_TESTS` in two).**
Bespoke gates that diverge from UE convention. Avoid — fights the framework,
costs more in maintenance than it saves in cleanliness.

### Decision matrix

| Trigger | Action |
|---|---|
| Today, small team, everyone uses `oglltest` | Stay with tag alias + wrapper (shipped 2026-05-20) |
| Bare-exe footgun keeps biting (someone double-clicks the exe, debugger F5, etc.) | Engine patch + re-apply discipline. Or Option B. |
| We start writing UE-coupled tests (touching `UObject`, `FString`, actors) | Option B |

---

## Relationship to the standalone CMake build

The standalone CMake build at `Plugins/OGBrawler/CMakeLists.txt` is **completely
independent** of the LLT setup:

| | LLT (UE-side) | Standalone (CMake) |
|---|---|---|
| Build system | UBT (`Build.bat`) | CMake + MSVC |
| Requires UE installed | Yes | No |
| Catch2 source | Epic's bundled v3.4.0 | Our vendored v3.14.0 |
| Test exe | `OGSimulationTests.exe`, `OGBrawlerTests.exe` | `og_simulation_tests.exe`, `og_brawler_tests.exe` |
| Startup time | <1 s | <1 s |

The same test `.cpp` files compile under both. Both paths must stay green —
breaking one doesn't block work on the other.

---

## Adding new tests

1. **Pure-logic test on simulation code** → add a new `.cpp` under
   `Plugins/OGSimulation/Source/OGSimulationTests/`. Add the file path to
   `Plugins/OGSimulation/Source/OGSimulationTests/CMakeLists.txt`'s
   `add_executable(...)` list. UBT picks it up automatically.

2. **Pure-logic test on brawler code** → same, under
   `Plugins/OGBrawler/Source/OGBrawlerTests/` + its `CMakeLists.txt`.

3. **Choose a tag** — `[PCTM]` (simulation prediction-clock-related),
   `[DAttack]` (brawler combat / sim integration), `[CharacterViz]`
   (procedural visualization). Add a sub-tag for the system you're testing.

4. **Test body** — vanilla Catch2: `TEST_CASE("Tag.Class.Behavior", "[Tag][Sub]") { REQUIRE(...); }`.
   `SECTION`s work. `Catch::Approx` works. Surface kept small on purpose;
   if you find yourself reaching for `GENERATE` / `TEMPLATE_TEST_CASE` /
   `SCENARIO`, raise it — those are also fine but we should agree as a team
   before scope-creeping the Catch2 usage.

---

## Current structure: test-source submodules

The og-framework repo refactor (completed) splits the test source into dedicated
repositories: [`og-simulation-tests`](https://github.com/og-framework/og-simulation-tests)
and [`og-brawler-tests`](https://github.com/og-framework/og-brawler-tests).
The LLT targets in `og-brawler-unreal` consume test source from those repos via
submodules rather than from inside the plugin trees:

```
og-brawler-unreal/
├── Plugins/
│   ├── OGSimulation/ → og-simulation-ue submodule (UE shell + extern pure lib)
│   └── OGBrawler/    → og-brawler-ue    submodule (UE shell + extern pure lib)
└── Source/
    ├── OGSimulationTests/
    │   ├── OGSimulationTests.Target.cs
    │   ├── OGSimulationTests.Build.cs
    │   └── extern/og-simulation-tests/ → submodule (test .cpp files)
    └── OGBrawlerTests/
        ├── OGBrawlerTests.Target.cs
        ├── OGBrawlerTests.Build.cs
        └── extern/og-brawler-tests/    → submodule (test .cpp files)
```

The Build.cs at each `Source/<X>Tests/` directory uses UBT's source-folder
customization (e.g., `ConditionalAddModuleDirectory`) to compile test
`.cpp` files from the submodule path into the LLT exe. Same pattern as the
plugin shells in the existing refactor backlog.

---

## Future: testing UE-coupled code

If we ever want to test code that needs UE types (`UObject`, `FString`,
`AActor`, etc. — anything that requires the engine's compilation pipeline),
add a **third** LLT target:

```csharp
// Source/OGBrawlerUnrealTests/OGBrawlerUnrealTests.Target.cs
public class OGBrawlerUnrealTestsTarget : TestTargetRules
{
    public OGBrawlerUnrealTestsTarget(TargetInfo Target) : base(Target)
    {
        bWithLowLevelTestsOverride = true;
        bCompileAgainstEngine = true;        // ← flip these on
        bCompileAgainstCoreUObject = true;
        // GROUP_BEFORE_GLOBAL(InitAll) in a TestGroupEvents.cpp in the module
        LaunchModuleName = "OGBrawlerUnrealTests";
    }
}
```

This target would mirror Epic's `FoundationTests` pattern — full engine init,
bigger binary, supports UE types. Pure-logic tests stay in the existing two
targets, untouched.

---

## References

- [Epic's Low-Level Tests documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/low-level-tests-in-unreal-engine)
- Engine examples: `Engine/Source/Programs/LowLevelTests/FoundationTests/` (canonical pure-test target),
  `Engine/Source/Programs/AutoRTFMTests/` (more elaborate setup with engine init)
- Catch2 v3 docs: <https://github.com/catchorg/Catch2/tree/devel/docs>
