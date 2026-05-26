<!-- SPDX-License-Identifier: MPL-2.0 -->
# OG Brawler Unreal — Onboarding

End-to-end setup checklist for cloning, building, and running the project in UE PIE.

---

## 1. Prerequisites

- **Unreal Engine 5.x** installed at `C:\dev\UnrealEngine\` (or adapt paths below).
- **Visual Studio 2022** with the "Desktop development with C++" and ".NET desktop development" workloads.
- **CMake 3.20+** — only needed for standalone framework tests (step 8).
- **Git 2.13+** with submodule support.
- **GitHub access** to the `og-framework` org (to clone submodules).

## 2. One-time git config

Run once on your machine so submodule operations behave correctly:

```bash
git config --global submodule.recurse true
git config --global push.recurseSubmodules on-demand
git config --global diff.submodule log
git config --global status.submodulesummary 1
```

See [`docs/cross-repo-dev-loop.md` §3](cross-repo-dev-loop.md#3-recommended-git-config) for rationale.

## 3. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/og-framework/og-brawler-unreal.git C:\dev\og-brawler-unreal
```

If you already cloned without `--recurse-submodules`, recover with:

```bash
git submodule update --init --recursive
```

## 4. Put submodules on `main`

After clone, submodules are at the pinned SHA in detached-HEAD state. Switch to `main` so any commits you make in those submodules don't get dropped.

```bash
cd C:\dev\og-brawler-unreal
cd Plugins/OGSimulation && git checkout main && cd ../..
cd Plugins/OGBrawler && git checkout main && cd ../..
```

See [`docs/cross-repo-dev-loop.md` §1](cross-repo-dev-loop.md#1-submodules-and-detached-head) for details.

## 5. Generate the Visual Studio solution

Right-click `OGBrawlerUnreal.uproject` in Explorer and choose **"Generate Visual Studio project files"**, OR run from a command prompt:

```bat
C:\dev\UnrealEngine\Engine\Build\BatchFiles\GenerateProjectFiles.bat ^
  C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject -game
```

The solution lands at `C:\dev\og-brawler-unreal\OGBrawlerUnreal.sln`.

## 6. Build the Editor target

```bat
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat ^
  OGBrawlerUnrealEditor Win64 Development ^
  "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
```

Alternatively, open `OGBrawlerUnreal.sln` in Visual Studio and build the `Development Editor` configuration.

## 7. Launch and test

Double-click `OGBrawlerUnreal.uproject` or press **F5** in Visual Studio to launch the UE Editor. The default map (`ThirdPersonMap`) loads automatically on first open.

For multiplayer testing, use the recommended PIE config: **1 dedicated server + 2 clients** (not Standalone).

## 8. Run framework Catch2 tests

Two ways to run the same tests, pick whichever is convenient:

### Via UE Low-Level Tests (uses UBT, requires UE installed)

```bat
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat ^
  OGSimulationTests Win64 Development ^
  "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"

.\Binaries\Win64\OGSimulationTests\OGSimulationTests.exe "[PCTM]"
```

Same for `OGBrawlerTests` with `"[DAttack],[CharacterViz]"`. Always pass a
tag filter — see [`low-level-tests.md`](low-level-tests.md) for why and the
full setup.

### Via standalone CMake (no UE needed)

```bash
cd C:\dev\og-brawler-unreal\Plugins\OGBrawler
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
cd build && ctest -C Debug
```

Expect **31 passing cases** (20 og_brawler_tests + 11 og_simulation_tests).

See [`docs/standalone-build.md`](standalone-build.md) for full details on the
CMake path.

---

## Next steps for active development

Once the project builds and runs, see [`docs/cross-repo-dev-loop.md`](cross-repo-dev-loop.md) for the multi-repo development workflow: submodule push order, handling pin bumps, recommended git config, and the "three commits, three PRs" pattern.
