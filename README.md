<!-- SPDX-License-Identifier: BUSL-1.1 -->
# OG Brawler Unreal

Unreal Engine 5.x game project integrating the [og-framework](https://github.com/og-framework) simulation and brawler layers. Demonstrates server-authoritative deterministic simulation with client-side prediction in a multiplayer game. Also serves as the canonical host for the UE Low-Level Test targets that exercise both libraries without editor startup.

> **Note:** This repository bundles the project's full `Content/` directory (excluding StarterContent) so the default map loads on first clone. Bundled Epic-licensed assets (Mannequin, ThirdPerson template) remain under the [Unreal Engine EULA](https://www.unrealengine.com/eula); source code is mixed-licensed (BUSL-1.1 / MPL-2.0) — see [`LICENSING.md`](LICENSING.md). See [`Content/LICENSE-NOTICE.md`](Content/LICENSE-NOTICE.md) for the asset/code licensing breakdown.

---

## Position in the og-framework graph

og-brawler-unreal sits at the **top of the dependency graph**, consuming all other og-framework repos.

```
og-simulation ──────────────────────────────────────────┐
og-brawler (depends on og-simulation) ──────────────────┤
og-simulation-ue (wraps og-simulation) ─────────────────┤
og-brawler-ue (wraps og-brawler) ───────────────────────┤
og-simulation-tests ─────────────────────────────────── og-brawler-unreal  (this repo)
og-brawler-tests ───────────────────────────────────────┤
og-tools ───────────────────────────────────────────────┘
```

## Related repos

| Repo | Role |
|---|---|
| [og-simulation](https://github.com/og-framework/og-simulation) | Pure C++ simulation core |
| [og-brawler](https://github.com/og-framework/og-brawler) | Pure C++ brawler core |
| [og-simulation-ue](https://github.com/og-framework/og-simulation-ue) | UE plugin at `Plugins/OGSimulation/` |
| [og-brawler-ue](https://github.com/og-framework/og-brawler-ue) | UE plugin at `Plugins/OGBrawler/` |
| [og-simulation-tests](https://github.com/og-framework/og-simulation-tests) | Test source at `Source/OGSimulationTests/extern/` |
| [og-brawler-tests](https://github.com/og-framework/og-brawler-tests) | Test source at `Source/OGBrawlerTests/extern/` |
| [og-tests-cmake-runner](https://github.com/og-framework/og-tests-cmake-runner) | Standalone CMake test harness (no UE needed) |
| [og-tools](https://github.com/og-framework/og-tools) | PowerShell toolkit at `tools/og-tools/` |

---

## Building

**First time? See [`docs/onboarding.md`](docs/onboarding.md) for an end-to-end setup checklist.**

### Prerequisites

- Unreal Engine 5.x at `C:\dev\UnrealEngine\` (or adapt paths below)
- Visual Studio 2022 with C++ workload
- PowerShell 7.4+ (for og-tools cmdlets)

### Clone with submodules

```bash
git clone --recurse-submodules https://github.com/og-framework/og-brawler-unreal.git
```

If already cloned without `--recurse-submodules`:

```bash
git submodule update --init --recursive
```

### Unreal Engine build (Editor target)

```bat
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat ^
  OGBrawlerUnrealEditor Win64 Development ^
  "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"
```

### Low-Level Test targets (no editor startup — sub-second runs)

```bat
C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat ^
  OGSimulationTests Win64 Development ^
  "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"

.\Binaries\Win64\OGSimulationTests\OGSimulationTests.exe "[@og]"

C:\dev\UnrealEngine\Engine\Build\BatchFiles\Build.bat ^
  OGBrawlerTests Win64 Development ^
  "C:\dev\og-brawler-unreal\OGBrawlerUnreal.uproject"

.\Binaries\Win64\OGBrawlerTests\OGBrawlerTests.exe "[@og]"
```

Or via og-tools:

```powershell
Import-Module .\tools\og-tools\og-framework.psd1 -Force
oglltest simulation   # defaults to [@og] filter
oglltest brawler
```

See [`docs/low-level-tests.md`](docs/low-level-tests.md) for full LLT details and the `[@og]` tag alias rationale.

---

## Build Targets

| Target | Platform | Type | Purpose |
|--------|----------|------|---------|
| `OGBrawlerUnreal` | Win64 | Game | Development / PIE (client+server) |
| `OGBrawlerUnrealEditor` | Win64 | Editor | UE Editor |
| `OGBrawlerUnrealServer` | Win64 | Server | Headless dedicated server |
| `OGBrawlerUnrealClient` | Win64, Android | Client | Dedicated client |
| `OGSimulationTests` | Win64 | LLT | Standalone simulation test exe |
| `OGBrawlerTests` | Win64 | LLT | Standalone brawler test exe |

---

## Project Structure

```
Plugins/
  OGSimulation/        Submodule — og-framework/og-simulation-ue
  OGBrawler/           Submodule — og-framework/og-brawler-ue
Source/
  OGBrawlerUnreal/     Main game module                  [BUSL-1.1]
  OGSimulationUnreal/  UE simulation adapters module     [MPL-2.0]
  dVolume/             Volume simulation module           [MPL-2.0]
  JoltPhysicsModule/   Jolt physics integration          [MPL-2.0]
  ProceduralMountainSide/  Procedural terrain module     [MPL-2.0 → BUSL-1.1 pending C8]
  OGSimulationTests/   LLT target (extern/og-simulation-tests submodule)  [MPL-2.0]
  OGBrawlerTests/      LLT target (extern/og-brawler-tests submodule)     [BUSL-1.1]
tools/
  og-tools/            Submodule — og-framework/og-tools (v1.1.1)
docs/
  onboarding.md        First-clone setup checklist
  cross-repo-dev-loop.md  Multi-repo development workflow
  low-level-tests.md   LLT target details + [@og] tag alias
  standalone-build.md  Standalone CMake build guide
Config/
  DefaultEngine.ini    Engine and platform settings
```

---

## Canonical workflow

See [`docs/cross-repo-dev-loop.md`](docs/cross-repo-dev-loop.md) for the multi-repo dev workflow (submodule push order, recommended git config, og-tools cmdlet usage).

---

## License and contributing

This repository is **mixed-licensed**: brawler-specific code is under BUSL-1.1 and generic simulation-layer code is under MPL-2.0. The authoritative source for the full breakdown — which directories carry which license, what BUSL-1.1 permits, and the Change Date — is [`LICENSING.md`](LICENSING.md).

Bundled Epic assets remain under the UE EULA — see [`Content/LICENSE-NOTICE.md`](Content/LICENSE-NOTICE.md).

Contributions welcome. The inbound license for each contribution is determined by the `SPDX-License-Identifier` header of the file being modified. See [CONTRIBUTING.md](CONTRIBUTING.md) for the decision tree on where to make your change and the CLA requirement.
