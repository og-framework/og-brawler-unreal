<!-- SPDX-License-Identifier: MPL-2.0 -->
# Standalone Build (archived — pre-Phase-Z layout)

> **Archived 2026-05-22.** This document described the standalone CMake build path
> that ran from `Plugins/OGBrawler/CMakeLists.txt` in the 3-repo era. That layout
> no longer exists post-Phase-Z.
>
> The current standalone CMake build lives in
> [og-tests-cmake-runner](https://github.com/og-framework/og-tests-cmake-runner).
> See [cross-repo-dev-loop.md](../cross-repo-dev-loop.md) section 10 for the two
> test paths, and the og-tests-cmake-runner README for the quickstart.

---

## Original content (for historical reference)

`OGSimulation` and `OGBrawler` can be compiled and tested without Unreal Engine
using a standard CMake build. This is the **no-UE-needed** path — useful for
contributors iterating on simulation logic on a machine where UE isn't installed,
and the canonical entry point for CI on a vanilla Windows runner.

For UE-side test runs (Catch2 v3.4.0 via UBT, no editor startup, sub-second
test invocation, supports future UE-coupled tests), see
[`low-level-tests.md`](../low-level-tests.md).

**Platform:** Windows only (this phase). Linux/macOS support is future work.

---

### Prerequisites

- CMake 3.20 or later
- Visual Studio 2022 (the `Visual Studio 17 2022` generator)

---

### Build

From `Plugins\OGBrawler` inside the repo:

```bat
cd Plugins\OGBrawler
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

The test executables are produced at:

```
Plugins\OGBrawler\build\Source\OGBrawlerTests\Debug\og_brawler_tests.exe
Plugins\OGBrawler\build\extern\og-simulation\Source\OGSimulationTests\Debug\og_simulation_tests.exe
```

---

### Run Tests

Run all tests via ctest:

```bat
cd Plugins\OGBrawler\build
ctest -C Debug
```

Run only DAttack tests (Catch2 tag filter):

```bat
Plugins\OGBrawler\build\Source\OGBrawlerTests\Debug\og_brawler_tests.exe "[DAttack]"
```

Run only PCTM tests:

```bat
Plugins\OGBrawler\build\extern\og-simulation\Source\OGSimulationTests\Debug\og_simulation_tests.exe "[PCTM]"
```

Exit code 0 means all selected tests passed.
