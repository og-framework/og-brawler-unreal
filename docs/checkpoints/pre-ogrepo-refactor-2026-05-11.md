# Pre-OGRepoRefactor checkpoint — 2026-05-11

Snapshot of the og-framework's state immediately before the **OGRepoRefactor**
initiative starts mutating repos in place. The refactor splits the current
3-repo extraction into 7 repos; if it goes sideways, this is what you'd be
rolling back to.

Reading this doc cold: skim §1 ("State at a glance") and §6 ("If rolling back").
Everything else is reference detail.

---

## 1. State at a glance

**Three repos under `og-framework`**, all private:

| Repo | HEAD SHA | Tag | Branch |
|---|---|---|---|
| `og-simulation` | `b6253cb` | `v1.1.0` (this checkpoint) | `main`, pushed |
| `og-brawler` | `a6b2b0d` | `v1.1.0` (this checkpoint) | `main`, pushed |
| `og-brawler-unreal` | `caf5869` | `v1.1.0` (this checkpoint) | `main`, pushed |

All three are clean against `origin/main`. The parent's submodule pointers
match the actual submodule HEADs:
- `Plugins/OGSimulation` → `b6253cb`
- `Plugins/OGBrawler` → `a6b2b0d`

**Four newly-created empty repos** (also under `og-framework`, private) are
the Phase A landing pads for the refactor — they currently contain no commits
and would be deleted on rollback:
- `og-simulation-ue`
- `og-brawler-ue`
- `og-simulation-tests`
- `og-brawler-tests`

---

## 2. Architecture at this checkpoint

```
og-brawler-unreal/                       UE project (the game)
├── OGBrawlerUnreal.uproject
├── Source/
│   ├── OGBrawlerUnreal/                 game module
│   ├── OGSimulationTests/               LLT target — TestTargetRules
│   │   └── OGSimulationTests.Target.cs
│   ├── OGBrawlerTests/                  LLT target — TestTargetRules
│   │   └── OGBrawlerTests.Target.cs
│   ├── OGBrawlerUnreal.Target.cs        game/editor/server/client targets
│   ├── OGBrawlerUnrealEditor.Target.cs
│   ├── OGBrawlerUnrealServer.Target.cs
│   ├── OGBrawlerUnrealClient.Target.cs
│   └── (DVolumeModule, JoltPhysicsModule, ProceduralMountainSide, …)
└── Plugins/
    ├── OGSimulation/   →  submodule og-simulation
    │   └── Source/
    │       ├── OGSimulation/            pure simulation source
    │       ├── glm/                     glm UE module
    │       ├── OGSimulationTests/       test source + LLT Build.cs (TestModuleRules)
    │       │   ├── OGSimulationTests.Build.cs
    │       │   ├── CMakeLists.txt       (standalone CMake build path)
    │       │   ├── Catch2BridgeSmokeTest.cpp
    │       │   └── PCTimeManagement/
    │       │       ├── ClientPredictionClockTest.cpp
    │       │       ├── NetworkTimeEstimatorTest.cpp
    │       │       └── ServerTickClockTest.cpp
    │       └── ThirdParty/Catch2/       vendored amalgamation (CMake-only consumer)
    └── OGBrawler/      →  submodule og-brawler
        ├── Source/OGBrawler/            pure brawler source
        ├── Source/OGBrawlerTests/       test source + LLT Build.cs (TestModuleRules)
        │   ├── OGBrawlerTests.Build.cs
        │   ├── CMakeLists.txt
        │   ├── HumanoidVisualizationTest.cpp
        │   ├── SimulatableCharacterTest.cpp
        │   ├── SimulationCompositeForEachTest.cpp
        │   ├── SimulationIntegrationExecutorTest.cpp
        │   ├── SimulationNetSyncTest.cpp
        │   └── SimulationReconciliationTest.cpp
        └── extern/og-simulation/        nested submodule (CMake build only)
            └── OGSimulation.uplugin.disabled    ← RENAME HACK — see §4
```

**Key properties of this checkpoint:**

- Test runs happen via **UE Low-Level Tests** (no Catch2-to-UE-Automation
  bridge). The bridge code was deleted in the LLT migration immediately
  preceding this checkpoint.
- og-brawler-unreal has **separate `Program` targets** for each test suite
  (`Source/<X>Tests/<X>Tests.Target.cs`); no two-target setup,
  no `DisablePlugins`/`EnablePlugins` gating.
- og-brawler still has its own `extern/og-simulation` nested submodule
  (used only by the standalone CMake build). UE-side discovers OGSimulation
  via `Plugins/OGSimulation/` directly.

---

## 3. How to verify "this build is healthy" after a rollback

### UE Low-Level Tests (the canonical UE-side path)

```powershell
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGSimulationTests Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGBrawlerTests    Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"

.\Binaries\Win64\OGSimulationTests\OGSimulationTests.exe "[PCTM]"
.\Binaries\Win64\OGBrawlerTests\OGBrawlerTests.exe "[DAttack],[CharacterViz]"
```

**Expected results:**

| Command | Expected |
|---|---|
| `OGSimulationTests.exe "[PCTM]"` | All tests passed (**217 assertions in 10 test cases**) |
| `OGBrawlerTests.exe "[DAttack],[CharacterViz]"` | All tests passed (**40 assertions in 20 test cases**) |

**Always pass a tag filter.** Unfiltered runs pull in Engine's
`WITH_LOW_LEVEL_TESTS`-gated implicit tests (from `Core/Tests/`) which crash
because the LLT targets don't initialize engine subsystems. See
`docs/low-level-tests.md` §"Known issue: Engine implicit-test noise".

### Standalone CMake (the no-UE-needed path)

```powershell
cd C:\dev\og-brawler-unreal\Plugins\OGBrawler
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
cd build
ctest -C Debug
```

**Expected results:** **31 tests pass total**:
- `og_simulation_tests.exe` → 218 assertions, 11 test cases (10 PCTM + 1 Catch2 smoke)
- `og_brawler_tests.exe` → 40 assertions, 20 test cases

### Editor target (smoke-test the game compiles)

```powershell
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGBrawlerUnrealEditor Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
```

Should report `Result: Succeeded`.

---

## 4. The `.uplugin.disabled` rename hack (NOT captured by tags)

`Plugins/OGBrawler/extern/og-simulation/` is a nested submodule used by the
standalone CMake build. Its working tree contains an `OGSimulation.uplugin`
at the root. When UE's plugin scanner walks `Plugins/`, it would discover
**two** OGSimulation plugins — the legitimate one at `Plugins/OGSimulation/`
and the duplicate at `Plugins/OGBrawler/extern/og-simulation/`. UE refuses to
load with a "duplicate plugin name" error.

**Workaround applied in working tree:** rename the nested submodule's
`.uplugin` to `.uplugin.disabled` so UE skips it.

This rename is **not in any commit**. It's a working-tree-only modification
of the nested submodule. After a fresh clone, you must re-apply it:

```powershell
cd C:\dev\og-brawler-unreal\Plugins\OGBrawler\extern\og-simulation
Move-Item OGSimulation.uplugin OGSimulation.uplugin.disabled
```

After applying, `git status` inside that submodule will show:
```
deleted:    OGSimulation.uplugin
Untracked files:
  OGSimulation.uplugin.disabled
```

This dirty state will propagate up as ` m extern/og-simulation` in OGBrawler's
status and ` m Plugins/OGBrawler` in the parent. **That's expected.** Leave it
unstaged; never commit it.

This hack is eliminated by the OGRepoRefactor's Phase C (removes
`extern/og-simulation` from og-brawler entirely). Until the refactor lands,
the rename must be in place for the UE Editor to load.

---

## 5. Working-tree state to be aware of

Right after applying the §4 rename, `git status` at each location should
show exactly this (no more, no less):

**Parent `og-brawler-unreal`:**
```
On branch main
Your branch is up to date with 'origin/main'.

Changes not staged for commit:
        modified:   Plugins/OGBrawler (modified content)

no changes added to commit
```

**`Plugins/OGSimulation`:** clean.

**`Plugins/OGBrawler`:**
```
On branch main
Your branch is up to date with 'origin/main'.

Changes not staged for commit:
        modified:   extern/og-simulation (modified content, untracked content)

no changes added to commit
```

**`Plugins/OGBrawler/extern/og-simulation`:**
```
HEAD detached at 33f5016
Changes not staged for commit:
        deleted:    OGSimulation.uplugin
Untracked files:
        OGSimulation.uplugin.disabled
```

Any deviation from this means something other than the rename hack is dirty.

---

## 6. If rolling back

### Scenario A: Phase B/C completed but we want the pre-refactor state back

Each repo's `main` will have been force-rewritten by the refactor. To restore:

```powershell
# For each of the 3 existing repos:
cd C:\dev\og-brawler-unreal\Plugins\OGSimulation
git fetch --tags
git reset --hard v1.1.0
git push --force-with-lease origin main

cd ..\OGBrawler
git fetch --tags
git reset --hard v1.1.0
git push --force-with-lease origin main

cd C:\dev\og-brawler-unreal
git fetch --tags
git reset --hard v1.1.0
git push --force-with-lease origin main
```

Then **delete the four Phase A repos** (they'd be unused post-rollback):
```powershell
gh repo delete og-framework/og-simulation-ue --yes
gh repo delete og-framework/og-brawler-ue --yes
gh repo delete og-framework/og-simulation-tests --yes
gh repo delete og-framework/og-brawler-tests --yes
```

Re-apply the §4 `.uplugin.disabled` rename in a fresh clone.

Verify health via §3.

### Scenario B: Phase A succeeded but we abandon Phase B/C

No rollback needed on the existing 3 repos — they're untouched. Just delete
the 4 empty Phase A repos as above.

### Scenario C: Mid-refactor abort

Depends on how far Phase B got. If `og-simulation` has been force-pushed:
follow Scenario A's reset for that repo. If only Task 2 (new repo init) has
happened: only delete the new repos as in Scenario B. The OGRepoRefactor's
`current_state.md` should be consulted for what's been done so far.

---

## 7. Reference: what the refactor will change

The refactor splits each existing repo:

| Existing repo → | Becomes |
|---|---|
| `og-simulation` | `og-simulation` (pure lib, no .uplugin, no tests) + `og-simulation-ue` (UE shell) + `og-simulation-tests` (test source) |
| `og-brawler` | `og-brawler` (pure lib, no .uplugin, no tests, no extern submodule) + `og-brawler-ue` (UE shell) + `og-brawler-tests` (test source) |
| `og-brawler-unreal` | unchanged in *role* — but its `Plugins/OGSimulation/` and `Plugins/OGBrawler/` submodule URLs are repointed at the new `-ue` repos, and the LLT targets in `Source/<X>Tests/` gain new `extern/<test-repo>/` submodules |

Tagged with `v2.0.0` on completion (per `Initiatives/OGRepoRefactor/Backlog.md`
Phase I). The `v1.1.0` tag this doc references is the pre-refactor state.

---

## 8. Companion docs

- `docs/low-level-tests.md` — the UE-side test setup (this checkpoint's
  test-runner architecture)
- `docs/standalone-build.md` — the no-UE CMake path
- `docs/onboarding.md` — end-to-end clone/build for new contributors
- `docs/cross-repo-dev-loop.md` — multi-repo development workflow (was the
  current 3-repo version at checkpoint time; subsequently rewritten for the
  post-refactor 9-repo architecture)
