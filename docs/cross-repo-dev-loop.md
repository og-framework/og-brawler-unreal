<!-- SPDX-License-Identifier: MPL-2.0 -->
# Cross-Repo Dev Loop

Reference for developing across the 9-repo og-framework architecture with og-tools as the daily-use front-end. Covers clone scenarios, the iteration workflow, og-tools cmdlets, cascade semantics, branching, PRs, and common pitfalls.

## Contents

1. [The 9-repo graph](#1-the-9-repo-graph)
2. [Three clone scenarios](#2-three-clone-scenarios)
3. [Iteration workflow per scenario](#3-iteration-workflow-per-scenario)
4. [The og-tools toolkit](#4-the-og-tools-toolkit)
5. [Cross-cutting change: the 3-line workflow](#5-cross-cutting-change-the-3-line-workflow)
6. [Pin discipline](#6-pin-discipline)
7. [Branching for feature work](#7-branching-for-feature-work)
8. [PR workflow and merge order](#8-pr-workflow-and-merge-order)
9. [Common pitfalls](#9-common-pitfalls)
10. [Two test paths](#10-two-test-paths)
11. [CI mental model](#11-ci-mental-model)

---

## 1. The 9-repo graph

```
og-simulation ──────────────────────────────────────────────────┐
  └─ og-simulation-ue (wraps og-simulation as UE plugin)        │
  └─ og-simulation-tests (Catch2 test source)                   │
                                                                 │
og-brawler (depends on og-simulation) ──────────────────────────┤
  └─ og-brawler-ue (wraps og-brawler as UE plugin)              ├── og-brawler-unreal
  └─ og-brawler-tests (Catch2 test source)                      │   (apex UE project)
                                                                 │
og-tests-cmake-runner (assembles pure libs + test source) ──────┘
og-tools (PowerShell toolkit) ──────────────────────────────────┘ (submoduled by unreal + cmake-runner)
```

| Repo | Kind | Role |
|---|---|---|
| [og-simulation](https://github.com/og-framework/og-simulation) | pure lib | Deterministic simulation core (C++20) |
| [og-brawler](https://github.com/og-framework/og-brawler) | pure lib | Brawler gameplay layer; links og_simulation |
| [og-simulation-ue](https://github.com/og-framework/og-simulation-ue) | UE shell | `.uplugin` + `Build.cs`; compiles og-simulation via submodule |
| [og-brawler-ue](https://github.com/og-framework/og-brawler-ue) | UE shell | `.uplugin` + `Build.cs`; compiles og-brawler via submodule |
| [og-simulation-tests](https://github.com/og-framework/og-simulation-tests) | test source | Catch2 tests for og-simulation (source dist — not standalone-buildable) |
| [og-brawler-tests](https://github.com/og-framework/og-brawler-tests) | test source | Catch2 tests for og-brawler (source dist) |
| [og-tests-cmake-runner](https://github.com/og-framework/og-tests-cmake-runner) | CMake harness | Assembles all 4 pure-source repos + Catch2 into one CMake tree |
| [og-tools](https://github.com/og-framework/og-tools) | PowerShell module | Cross-repo status, sync, stage, commit, push, diff, LLT |
| [og-brawler-unreal](https://github.com/og-framework/og-brawler-unreal) | UE project | Game + LLT targets; consumes all framework repos via submodules |

### og-brawler-unreal submodule tree

Five direct submodules; UE shells each nest one pure-lib submodule at depth 2:

```
og-brawler-unreal/
├── Plugins/
│   ├── OGSimulation/                        ← og-simulation-ue (depth 1)
│   │   └── Source/OGSimulation/og-simulation/    ← og-simulation (depth 2)
│   └── OGBrawler/                           ← og-brawler-ue (depth 1)
│       └── Source/OGBrawler/og-brawler/          ← og-brawler (depth 2)
├── Source/
│   ├── OGSimulationTests/extern/og-simulation-tests/   ← og-simulation-tests (depth 1)
│   └── OGBrawlerTests/extern/og-brawler-tests/         ← og-brawler-tests (depth 1)
└── tools/og-tools/                          ← og-tools (depth 1)
```

`git submodule status --recursive` shows 7 entries (5 direct + 2 nested inside the UE shells).

---

## 2. Three clone scenarios

### Scenario A — Iterate on pure C++ (simulation or brawler logic, tests)

Clone og-tests-cmake-runner. It submodules all 4 pure-source repos + Catch2 + og-tools.

```powershell
git clone --recurse-submodules https://github.com/og-framework/og-tests-cmake-runner
cd og-tests-cmake-runner
Import-Module .\tools\og-tools\og-framework.psd1 -Force
oggitstatus
```

Edit source in `extern/og-simulation/` or `extern/og-brawler/`; run `ctest` to verify.

### Scenario B — Iterate on UE integration (Plugins/, Build.cs, LLT targets)

Clone og-brawler-unreal. It submodules the UE shells, test sources, and og-tools.

```powershell
git clone --recurse-submodules https://github.com/og-framework/og-brawler-unreal
cd og-brawler-unreal
Import-Module .\tools\og-tools\og-framework.psd1 -Force
oggitstatus
```

Build the Editor target; run LLT exes via `oglltest`.

### Scenario C — Iterate on og-tools cmdlets

Clone og-tools directly. No submodule consumers needed for iteration.

```powershell
git clone https://github.com/og-framework/og-tools
cd og-tools
Import-Module .\og-framework.psd1 -Force
```

Test your cmdlet changes. When done, bump the `tools/og-tools` submodule pin in consumers (og-brawler-unreal + og-tests-cmake-runner) via `oggitcommit`.

---

## 3. Iteration workflow per scenario

### Scenario A (CMake runner)

```
extern/og-simulation/   ← edit simulation source here
extern/og-brawler/      ← edit brawler source here
extern/og-simulation-tests/   ← edit simulation tests here
extern/og-brawler-tests/      ← edit brawler tests here
```

Build and test:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Expected: 218 assertions / 11 cases (og-simulation) + 40 assertions / 20 cases (og-brawler).

### Scenario B (og-brawler-unreal)

```
Plugins/OGSimulation/                    ← og-simulation-ue (UE shell)
  Source/OGSimulation/og-simulation/     ← og-simulation pure source (depth-2 submodule)
Plugins/OGBrawler/                       ← og-brawler-ue (UE shell)
  Source/OGBrawler/og-brawler/           ← og-brawler pure source (depth-2 submodule)
Source/OGSimulationTests/extern/og-simulation-tests/   ← test source
Source/OGBrawlerTests/extern/og-brawler-tests/         ← test source
tools/og-tools/                          ← PowerShell module
```

Build and test:

```bat
REM Build Editor target
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGBrawlerUnrealEditor Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"

REM Build + run LLT targets
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGSimulationTests Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat OGBrawlerTests Win64 Development "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
```

Or via og-tools:

```powershell
oglltest simulation   # builds + runs OGSimulationTests.exe with [@og] filter
oglltest brawler      # builds + runs OGBrawlerTests.exe with [@og] filter
```

See [low-level-tests.md](low-level-tests.md) for the `[@og]` tag alias rationale and the UE LLT footgun details.

---

## 4. The og-tools toolkit

Import from the consumer project root (og-brawler-unreal or og-tests-cmake-runner):

```powershell
Import-Module .\tools\og-tools\og-framework.psd1 -Force
```

### Daily-use cmdlets

| Cmdlet | Alias | Purpose |
|---|---|---|
| `Get-OgRepoStatus` | `oggitstatus` | Read tree state — shows all repos, HEAD, dirty flag, pin status, remote |
| `Get-OgRepoStatus -Fetch` | (same alias) | Like above but fetches first for accurate Ahead/Behind counts |
| `Sync-OgFramework` | `oggitsync` | Fetch + fast-forward merge cascade, deepest-first |
| `Add-OgChange` | `oggitadd` | Stage changes across the tree |
| `New-OgCommit -Message "..."` | `oggitcommit` | Commit staged changes + auto pin-advance cascade |
| `Push-OgFramework` | `oggitpush` | Push deepest-first (fixes the nested-submodule push gap) |
| `Show-OgDiff` | `oggitdiff` | Open git difftool across all dirty repos |
| `Invoke-OgLowLevelTest` | `oglltest` | Build + run an LLT target with `[@og]` default filter |

### Niche cmdlets

| Cmdlet | Purpose |
|---|---|
| `Update-OgLibPin` | Pin a submodule to a specific historical SHA (e.g., bisecting a regression) |
| `New-OgFeatureBranch -Name <branch>` | Cross-repo branch creation in all repos at once |
| `New-OgCloneScenario` | Guided clone for a specific scenario (simulation/brawler/unreal/cmake-runner) |
| `Test-OgPinConsistency` | Validate all submodule pins are in sync; exits 0/1 for CI |

All mutating cmdlets support `-WhatIf` and `-Confirm`. Use `Get-Help <Cmdlet> -Full` for details.

---

## 5. Cross-cutting change: the 3-line workflow

A change touching multiple repos — e.g., a new simulation primitive + brawler hook + a test — follows the same 3 commands from the consumer root:

```powershell
oggitadd                                    # stage across the tree
oggitcommit -Message "feat: new primitive (#42)"  # cascade deepest-first + auto pin-advance
oggitpush                                   # push deepest-first (user only — never agents)
```

### What `oggitcommit` does under the hood

`New-OgCommit` walks the repo tree deepest-first:

1. **Pre-pass:** detects any repo where a child was committed manually outside og-tools (`PinStatus = parent-behind`) and stages the missing pin advance automatically.
2. **Commit loop:** for each repo with staged changes, commits, then stages the new SHA as a pin update in the immediate parent.
3. Result: each repo gets exactly one commit; parent pins chain correctly.

The cascade from a change to og-simulation flows:
```
og-simulation committed  →  pin advance staged in og-simulation-ue
og-simulation-ue committed  →  pin advance staged in og-brawler-unreal (Plugins/OGSimulation)
og-brawler-unreal committed  (one final commit pinning all updated sub-repos)
```

---

## 6. Pin discipline

`oggitstatus` displays a `PinStatus` column for each submodule:

| PinStatus | Meaning |
|---|---|
| `in-sync` | Submodule HEAD matches parent's pinned SHA |
| `parent-behind` | Child has new commits; parent hasn't pinned them yet |
| `submodule-behind` | Parent's pinned SHA is newer than what's checked out locally (run `oggitsync`) |
| `n/a` | Not a submodule (top-level parent repo) |

**When to bump og-tools pin in consumers:**
The `tools/og-tools` submodule pin should be bumped when a new og-tools version is tagged. Use `Update-OgLibPin -Submodule tools/og-tools -Sha <new-sha>` then `oggitcommit -Message "chore: bump og-tools pin to v<X.Y.Z>"`.

Do not bump og-tools on every commit — it is a versioned dependency, not a live tracking branch. Pin to tagged releases.

---

## 7. Branching for feature work

```powershell
# From the consumer root (og-brawler-unreal or og-tests-cmake-runner):
New-OgFeatureBranch -Name "feat/my-feature"
```

This creates matching branches in all repos in the tree simultaneously. Use a common name across repos; reference a common ticket ID in commit messages:

```
Add SphereCollider primitive (#42)   # og-simulation commit
Use SphereCollider in hitboxes (#42) # og-brawler commit
Wire SphereCollider to UE module (#42) # og-brawler-unreal commit
```

For lightweight changes touching only one repo, branching in just that repo is fine.

---

## 8. PR workflow and merge order

Merge in **dependency order — deepest repo first**. Never merge a parent before its child PRs are merged, or the parent's pinned SHA won't exist on the remote.

```
og-simulation PR → og-brawler PR → og-simulation-ue PR → og-brawler-ue PR
    → og-simulation-tests PR → og-brawler-tests PR
        → og-tests-cmake-runner PR → og-brawler-unreal PR (last)
```

Not every change touches all repos. A pure-simulation change only needs PRs in og-simulation and any consumers that pin it. A brawler feature touches og-brawler + og-brawler-ue + og-brawler-tests + og-brawler-unreal at minimum.

The og-brawler-unreal PR should pin submodule pointers to the **merged** SHAs on each child's main branch, not the feature-branch SHAs.

### Opening the PRs

`oggitpush` pushes feature branches but does not open PRs. After it lands, use `gh` (GitHub CLI) per repo:

```powershell
# One per affected repo. Title/body conventionally reference a shared ticket ID.
gh pr create --repo og-framework/og-simulation `
    --base main --head feature/<name> `
    --title "<title>" --body "<body>"

gh pr create --repo og-framework/og-brawler `
    --base main --head feature/<name> `
    --title "<title>" --body "<body>"
# ...repeat for each repo with commits on the feature branch.
```

Or, batch via `oggitstatus` to enumerate just the repos that hold the feature branch:

```powershell
oggitstatus | Where-Object Branch -eq 'feature/<name>' | ForEach-Object {
    gh pr create --repo "og-framework/$($_.Name)" `
        --base main --head feature/<name> `
        --title "<title>" --body "<body>"
}
```

---

## 9. Common pitfalls

| Problem | Symptom | Fix |
|---|---|---|
| Detached HEAD in tools/og-tools | `oggitstatus` shows tools/og-tools at a SHA, not a branch | Expected behavior — og-tools is pinned. `oggitsync` or `git checkout -B main origin/main` inside if you need to branch. |
| Stale origin/main counts | `oggitstatus` shows old Ahead/Behind numbers | Run `oggitstatus -Fetch` to force a fetch before reading A/B counts. |
| Submodule-behind after colleague's push | `oggitstatus` shows `submodule-behind` | Run `oggitsync` — it fetches + fast-forwards the entire tree. |
| Push parent before children | Others' `git submodule update` fails — SHA missing on remote | `oggitpush` walks deepest-first and handles this. If pushing manually: push og-simulation, og-brawler, then og-brawler-unreal. |
| Pre-phase-E artifacts | `og-status.ps1`, `og-push-all.ps1`, `og-feature-start.ps1` not found | These scripts no longer exist. Use the og-tools cmdlets (`oggitstatus`, `oggitpush`, `New-OgFeatureBranch`). |
| Temp-local-remote stale config | After pinning against a local-only commit, `git push` on the parent fails | Restore the canonical HTTPS origin URL: `git remote set-url origin https://github.com/og-framework/<repo>`. |
| `oggitcommit` skips a repo | A child was committed manually; pin not advanced | The pre-pass detects `parent-behind` and stages the missing pin advance before committing. Re-run `oggitcommit` if you see it. |
| LLT exe crashes without filter | Run `OGSimulationTests.exe` bare — crash in Engine test code | Always run with `[@og]` filter (engine-bundled tests require engine init). Use `oglltest simulation` which passes `[@og]` by default. See [low-level-tests.md](low-level-tests.md). |

---

## 10. Two test paths

The same Catch2 test source compiles under two independent toolchains:

| Path | How to invoke | Uses |
|---|---|---|
| **CMake standalone** | `ctest` from og-tests-cmake-runner | Catch2 v3.14.0 (vendored); no UE installed |
| **UE Low-Level Tests** | `oglltest simulation` / `oglltest brawler` | Catch2 v3.4.0 (Epic-bundled via UBT); UE installed |

Expected pass counts (both paths): 218 assertions / 11 cases (og-simulation) + 40 assertions / 20 cases (og-brawler).

Differences:
- Catch2 versions differ (3.14.0 vs 3.4.0) — test source must remain compatible with both.
- LLT exes link engine modules which can pull in Catch2-registered `[SelfTests]`. Always use the `[@og]` filter to exclude them: `oglltest brawler` → `OGBrawlerTests.exe "[@og]"`.
- CMake path is CI-friendly (no UE runner needed). LLT path validates the UE integration layer.

See [low-level-tests.md](low-level-tests.md) for the full LLT setup, `[@og]` tag alias implementation, and the rationale for not patching the engine runner.

---

## 11. CI mental model

> Phase G CI is not yet landed. This section describes the intended model as a forward reference.

| Repo | CI validates |
|---|---|
| og-tests-cmake-runner | CMake configure + build + ctest; expected 218/11 + 40/20. Runs on GitHub-hosted `windows-2022`. |
| og-brawler-unreal | Editor target build + both LLT exes via `[@og]`; runs on self-hosted Windows runner with UE installed. Pin consistency check via `Test-OgPinConsistency` on GitHub-hosted runner (no UE needed). |
| og-simulation, og-brawler, test repos | No own CI (pure source distributions; og-tests-cmake-runner is the canonical build signal). |
| og-tools | No own CI (pure PowerShell; tested via consumer project imports). |

The pin consistency check (`Test-OgPinConsistency`) is lightweight and runs on every push to og-brawler-unreal — it walks `.gitmodules` and emits violations as PSObjects; CI gate: empty output = pass.

---

## See also

- [low-level-tests.md](low-level-tests.md) — UE LLT target setup, `[@og]` tag alias, footgun details
- [og-tools README](https://github.com/og-framework/og-tools/blob/main/README.md) — full cmdlet reference with pipeline examples
- [CONTRIBUTING.md](../CONTRIBUTING.md) — decision tree for which repo to clone for your change
- [checkpoints/pre-ogrepo-refactor-2026-05-11.md](checkpoints/pre-ogrepo-refactor-2026-05-11.md) — pre-refactor snapshot; design rationale for the 9-repo split
