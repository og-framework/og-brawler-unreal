<!-- SPDX-License-Identifier: MPL-2.0 -->
<!-- lint-external-ref: timeBeginPeriod -- Windows API (timeapi.h), outside every scan root -->
<!-- lint-external-ref: SetProcessInformation -- Windows API (processthreadsapi.h), outside every scan root; the module names it only as a string for GetProcAddress -->
<!-- lint-external-ref: GetProcessInformation -- Windows API (processthreadsapi.h), outside every scan root; the module names it only as a string for GetProcAddress -->
<!-- lint-external-ref: GetProcAddress -- Windows API (libloaderapi.h), outside every scan root -->
<!-- lint-external-ref: SetThreadDescription -- Windows API (processthreadsapi.h), outside every scan root -->
<!-- lint-external-ref: FWindowsPlatformMisc::PlatformInit -- Unreal Engine function (WindowsPlatformMisc.cpp), outside every scan root -->
<!-- lint-external-ref: FPlatformProcess::SleepNoStats -- Unreal Engine function (WindowsPlatformProcess.cpp), outside every scan root -->
# `OGSimulationUnreal.h` and `.cpp` — rationale

The root files of the `OGSimulationUnreal` module, og-simulation's Unreal adapter. `OGSimulationUnreal.h` declares the
module's log category, `LogOGSimHost` (§1). `OGSimulationUnreal.cpp` defines it, and defines the module class,
`FOGSimulationUnrealModule` (§2), whose `StartupModule` asks Windows to keep honouring the process's 1 ms timer
request and logs one `[Startup.Timer]` line on `LogOGStartup` (§3).

The source holds the licence, a docs pointer, code, and two `static_assert`s with their messages; this file carries the
why. There is no guards file: the one edit that would invert the feature (a non-zero `StateMask`) is a compile error.

**If this file and the source disagree, the source is authoritative and this file is stale.**

---

## 1. `LogOGSimHost`

`OGSimulationUnreal.h` declares the category with `DECLARE_LOG_CATEGORY_EXTERN`, exported (`OGSIMULATIONUNREAL_API`),
with default verbosity `Log`, and `OGSimulationUnreal.cpp` defines it with `DEFINE_LOG_CATEGORY`. It is exported because
the brawler's game module logs on it too.

Who logs on it:

* this module: the tick driver's engine port (`UETickDriverHost.cpp`; see `UETickDriverHost-rationale.md` §1) and the
  render target table (`UERenderTargetTable.cpp`; see `UERenderTargetTable-rationale.md`);
* the brawler: `SimulationManagerUImpl.cpp` (its `[SimHost.Backend]` line, in both physics configurations);
* OGSimulationJoltUnreal: the Jolt engine traits' log sink (`JoltEngineTraitsUImpl.cpp`;
  `JoltEngineTraitsUImpl-rationale.md` §2), the brawler's until og-tickdriver task 15e.

Its name is the one the brawler's host lines have always used, so the `LogOGSimHost=Warning` row of
`Config/DefaultEngine.ini` (line 372) still governs it: in a default run only its `Warning` lines are written.

## 2. The module class, and when it starts

### 2.1 The class

`FOGSimulationUnrealModule` derives from `IModuleInterface` and overrides `StartupModule` only; `ShutdownModule` is the
interface's empty default. It is defined in the `.cpp` and named only by `IMPLEMENT_MODULE`, as the game module's
`FOGBrawlerUEModule` is (`Source/OGBrawlerUnreal/OGBrawlerUnreal.cpp`); nothing else needs to name it, so the header
does not declare it. `FProceduralMountainSideModule` is the project's other `IModuleInterface` subclass.
Before og-tickdriver task 13 the module had no class of its own: it used `FDefaultModuleImpl`, which the engine
declares as an empty subclass of `IModuleInterface` (Runtime/Core/Public/Modules/ModuleManager.h:854-856), so the only
behaviour the new class adds is its `StartupModule`.

On Windows, `StartupModule` calls `honourTimerResolution` (§3). On other platforms it does nothing.

### 2.2 Loading order

OGBrawlerUnreal.uproject lists four project modules, all of type Runtime with loading phase Default, in this order:
`OGBrawlerUnreal`, `OGSimulationUnreal`, `OGSimulationJoltUnreal` (since og-tickdriver task 15e), `DVolumeModule`. The engine loads the startup modules in
FEngineLoop::LoadStartupModules (Runtime/Launch/Private/LaunchEngineLoop.cpp:4412-4437): phase PreDefault, then
Default, then PostDefault, and within each phase the project's modules before the plugins' modules.
FModuleDescriptor::LoadModulesForPhase (Runtime/Projects/Private/ModuleDescriptor.cpp:738-765) walks the descriptors in
file order and loads each one that is not loaded yet, which runs its `StartupModule`. So:

1. the game module's `StartupModule` runs (the build-identity hook and the named-pipe thread; `OGBrawlerUnreal-rationale.md`);
2. then this module's `StartupModule` runs (§3);
3. then OGSimulationJoltUnreal loads, which has no start-up work (`OGSimulationJoltUnreal-rationale.md` §2; three
   modules were listed until og-tickdriver task 15e), then `DVolumeModule`, and later the plugins' modules of the same
   phase.

All of this happens before the first world exists. LoadStartupModules runs inside FEngineLoop's pre-init
(LaunchEngineLoop.cpp:3665), and both places the engine loop calls the engine object's Init (LaunchEngineLoop.cpp:3818
and :4571) come after it. That Init creates the first world: a game engine's Init has its game instance call
InitializeStandalone (Runtime/Engine/Private/GameEngine.cpp:1246), which creates a transient world
(Runtime/Engine/Private/GameInstance.cpp:196), the "Untitled" world of the log below.

Measured on 2026-10-16 (og-tickdriver task 13), an editor `-game -nullrhi` run with the engine's module-manager category at
Verbose (log line numbers in that run):

```
1689  LogModuleManager: Verbose: LoadModuleWithFailureReason OGBrawlerUnreal
1693  LogOGBrawler: Warning: [Movement.cvar] ONE-TIME READ: ...
1694  LogModuleManager: Verbose: LoadModuleWithFailureReason OGSimulationUnreal
1698  LogOGStartup: Display: [Startup.Timer] honour=1 set=1 err=0 readback=1 control=0x4 state=0x0 readErr=0
1699  LogModuleManager: Verbose: LoadModuleWithFailureReason DVolumeModule
2170  LogChaosDD: Creating Chaos Debug Draw Scene for world Untitled
2240  LogLoad: LoadMap: /Game/ThirdPerson/Maps/ThirdPersonMap?Name=Player
```

The order of the log lines is the same as when the call lived in the game module. In the editor `[Startup.Timer]` is
still the line right after the brawler's `[Movement.cvar]` line. In the packaged Shipping server and Development client
it is still the line right after the engine's "MetaSound Engine Initialized", at the same line of the log as in task
79's packages (line 489 of the server's log, line 628 of the client's).

The module is loaded in every target that loads the game: it is in the project's module list, and
`OGBrawlerUnrealTargetCommon.Target.cs` adds it to every target's `ExtraModuleNames`. The low-level test targets do not
depend on it, so a test process never makes the call.

`StartupModule` runs once per process: the line appears exactly once in every process of the editor (`-game`), a packaged
Development client and the packaged Shipping server (§3.5).

## 3. Keeping the 1 ms timer resolution: `honourTimerResolution`

### 3.1 Why

UE asks Windows for a 1 ms timer at startup: `FWindowsPlatformMisc::PlatformInit` calls
`timeBeginPeriod` with 1 ms (engine Runtime/Core/Private/Windows/WindowsPlatformMisc.cpp:947). **Since Windows 11,
Windows may stop honouring that request for a process whose window is minimized, fully occluded or
otherwise not visible** (Microsoft documents this on `timeBeginPeriod` and on `SetProcessInformation`).
Such a process falls back to the default 15.625 ms timer. Gate 22 also saw a server with no window at all
lose it.

A dedicated server paces its frames with one plain sleep toward the 60 Hz target (engine
Runtime/Engine/Private/UnrealEngine.cpp:2784-2787: a dedicated server calls `FPlatformProcess::SleepNoStats`
once, with no spin afterwards). On a 15.625 ms timer that sleep always wakes on the second tick, so
**every frame takes about 31 ms** — the server runs at 32 fps, carrying about two simulation steps per frame.
Clients pace their frames differently (they sleep short and spin), but they are under the same Windows rule.

This was seen twice in gate 22: with the dedicated server's `-log` console minimized, and with a
window-less server that dropped to 31 ms frames when another application started. The Jolt and the Chaos
server behave the same way.
The cause is not in this project: the gate-22 investigation found no project code that sleeps on the
frame's hot path.

### 3.2 The call

`honourTimerResolution` calls `SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, ...)`
with a `PROCESS_POWER_THROTTLING_STATE` of:

| field | value | meaning |
|---|---|---|
| Version | `PROCESS_POWER_THROTTLING_CURRENT_VERSION` | 1 |
| `ControlMask` | `PROCESS_POWER_THROTTLING_IGNORE_TIMER_RESOLUTION` (0x4) | this process takes control of the timer-resolution policy |
| `StateMask` | 0 | the policy is **off**: always honour this process's timer requests |

A set bit in `StateMask` means the opposite — Windows may ignore the timer request — so the state is a
`constexpr` constant, `kHonourTimerResolution`, and two `static_assert`s hold it: `ControlMask` names the
timer bit only (the execution-speed bit would change the process's EcoQoS, which is a different policy),
and `StateMask` is 0.

It runs on **every target** (editor, game, client, server), not only on a dedicated server: a minimized
client is throttled by the same rule, and the call is harmless where Windows was already honouring the
request.

It is the only statement of this module's `StartupModule`, so it runs during the Default loading phase, right after
the game module's `StartupModule` (§2.2). Its position is not load-bearing: the policy is a process attribute that takes
effect when it is set, after the 1 ms request was made. Gate 22 showed this from outside the process: setting the same
policy on a live, minimized server brought its frames back to 16.7 ms without a restart.

**Why `GetProcAddress`, not a direct call.** UE compiles with `_WIN32_WINNT=0x0601` (Windows 7, the
engine's MinimumWindowsX64TargetVersion in Engine/Config/Windows/Windows_SDK.json), and the Windows SDK
declares `SetProcessInformation` and `GetProcessInformation` only for `_WIN32_WINNT >= 0x0602`. The struct,
the enumerator and the constants are declared regardless, so only the two functions are looked up at run
time from kernel32.dll, as the engine does for `SetThreadDescription`
(Runtime/Core/Private/Windows/WindowsPlatformProcess.cpp:2611). No constant is defined locally: the SDK the
engine selects (10.0.22621.0) has all of them. On a Windows without the functions, the lookup fails and the
log line reports it (`err=127`, `ERROR_PROC_NOT_FOUND`); nothing else changes.

### 3.3 The log line

The policy is read back with `GetProcessInformation` and one line is logged, whether or not the call was
made:

```
LogOGStartup: Display: [Startup.Timer] honour=1 set=1 err=0 readback=1 control=0x4 state=0x0 readErr=0
```

| field | meaning |
|---|---|
| `honour` | 1 when the call was made; 0 when `-OGNoTimerHonour` skipped it |
| `set`, `err` | the result of `SetProcessInformation` and `GetLastError` after it (0 when skipped) |
| `readback`, `readErr` | the result of `GetProcessInformation` and its error |
| `control`, `state` | the read-back masks. Honoured: `control=0x4 state=0x0`. Never set: `control=0x0 state=0x0` |

`LogOGStartup` is a file-local category (default verbosity `Log`), and no `[Core.Log]` entry names it, so
a `Display` line is written in every configuration, including the Shipping server, which keeps logging
(`bUseLoggingInShipping` in `OGBrawlerUnrealServer.Target.cs`). `Display` rather than `Warning`: it is an
expected, once-per-process line.

### 3.4 `-OGNoTimerHonour`

Skips the call, for A/B measurements and for diagnosing a frame-time problem. The read-back still runs
and the line says `honour=0`, so a log always shows which policy the process ran with.

### 3.5 The measured effect

Setup: a packaged Shipping dedicated server (Jolt), no clients, a `-log` console that is shown for 20 s, minimized for
40 s and restored for 20 s. The values are the `frameMs` p50 of the server's 10-second `[SimHost.Window]` lines.

Measured on 2026-10-14 (og-simulationscheduler-withjolt task 79), with the call in the game module:

| run | `[Startup.Timer]` | shown | minimized | restored |
|---|---|---|---|---|
| `-OGNoTimerHonour` | `honour=0 set=0 err=0 readback=1 control=0x0 state=0x0` | 16.7 ms | **30.9–31.1 ms** | 16.7 ms |
| default | `honour=1 set=1 err=0 readback=1 control=0x4 state=0x0` | 16.7 ms | **16.7 ms** | 16.7 ms |

Measured again on 2026-10-16 (og-tickdriver task 13), with the call in this module. The minimized column lists the
windows that lie wholly inside the minimized 40 s:

| run | `[Startup.Timer]` | shown | minimized | restored |
|---|---|---|---|---|
| `-OGNoTimerHonour` | `honour=0 set=0 err=0 readback=1 control=0x0 state=0x0` | 30.6–31.1 ms | **31.06, 31.07, 31.09 ms** | 31.0–31.1 ms |
| default | `honour=1 set=1 err=0 readback=1 control=0x4 state=0x0` | 16.9–17.0 ms | **16.93, 16.87, 16.86 ms** | 17.0 ms |
| default, re-run | the same | 16.8–16.9 ms | **16.92, 16.86, 16.94 ms** | 16.9 ms |
| default, task 79's own package, same hour | the same | 16.8–17.0 ms | **16.94, 16.83, 16.92 ms** | 17.0 ms |

That day the shown console was throttled too without the policy (31 ms before the minimize), and every honoured
server, task 79's unchanged package included, ran at about 16.9 ms rather than 16.7 ms; both are the machine's state
on that day, not the move.

With the policy set, minimizing the console no longer changes the frame time. A packaged Development
client and the editor log the same `honour=1` line.

No automated test covers this: the effect depends on Windows' treatment of a minimized window, which a
test process cannot reproduce. The `static_assert`s hold the values, and the log line shows the outcome in
every process.

---

## 4. Provenance

* `LogOGSimHost` moved here from the brawler's simulation manager in og-tickdriver task 11.
* The timer honour was written in the game module (`OGBrawlerUnreal.cpp`) by og-simulationscheduler-withjolt task 79,
  and moved here, unchanged, by og-tickdriver task 13 (approved by the user): it is engine-level, not brawler-level, so
  it belongs to the Unreal adapter (og-tickdriver design §4, "Timer resolution"). §3.1–§3.4 are that task's text, with
  the placement sentences rewritten for this module.
