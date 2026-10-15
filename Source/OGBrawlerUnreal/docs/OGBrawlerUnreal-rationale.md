<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: timeBeginPeriod -- Windows API (timeapi.h), outside every scan root -->
<!-- lint-external-ref: SetProcessInformation -- Windows API (processthreadsapi.h), outside every scan root; the module names it only as a string for GetProcAddress -->
<!-- lint-external-ref: GetProcessInformation -- Windows API (processthreadsapi.h), outside every scan root; the module names it only as a string for GetProcAddress -->
<!-- lint-external-ref: GetProcAddress -- Windows API (libloaderapi.h), outside every scan root -->
<!-- lint-external-ref: SetThreadDescription -- Windows API (processthreadsapi.h), outside every scan root -->
<!-- lint-external-ref: FWindowsPlatformMisc::PlatformInit -- Unreal Engine function (WindowsPlatformMisc.cpp), outside every scan root -->
<!-- lint-external-ref: FPlatformProcess::SleepNoStats -- Unreal Engine function (WindowsPlatformProcess.cpp), outside every scan root -->
# The game module — rationale

`Source/OGBrawlerUnreal/OGBrawlerUnreal.cpp` is the primary game module: `FOGBrawlerUEModule`. Its
`StartupModule` runs in every process that loads the game — the editor, `-game`, a client and a dedicated
server, in every configuration — and does three things, in this order:

1. On Windows, asks Windows to keep honouring the process's 1 ms timer request (§1).
2. Registers the build-identity network-version hook, `buildIdentityUImpl::registerNetworkVersionHook`.
   Its reasoning and prohibitions are in `OGBuildIdentity-rationale.md` and `OGBuildIdentity-guards.md`.
3. On Windows, starts the console-command named-pipe server on a detached thread (§2).

`ShutdownModule` does nothing.

---

## 1. Keeping the 1 ms timer resolution: `honourTimerResolution`

### 1.1 Why

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

### 1.2 The call

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

It runs at the start of `StartupModule`. Its position is not load-bearing: the policy is a process
attribute that takes effect when it is set, after the 1 ms request was made. Gate 22 showed this from
outside the process: setting the same policy on a live, minimized server brought its frames back to
16.7 ms without a restart.

**Why `GetProcAddress`, not a direct call.** UE compiles with `_WIN32_WINNT=0x0601` (Windows 7, the
engine's MinimumWindowsX64TargetVersion in Engine/Config/Windows/Windows_SDK.json), and the Windows SDK
declares `SetProcessInformation` and `GetProcessInformation` only for `_WIN32_WINNT >= 0x0602`. The struct,
the enumerator and the constants are declared regardless, so only the two functions are looked up at run
time from kernel32.dll, as the engine does for `SetThreadDescription`
(Runtime/Core/Private/Windows/WindowsPlatformProcess.cpp:2611). No constant is defined locally: the SDK the
engine selects (10.0.22621.0) has all of them. On a Windows without the functions, the lookup fails and the
log line reports it (`err=127`, `ERROR_PROC_NOT_FOUND`); nothing else changes.

### 1.3 The log line

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

### 1.4 `-OGNoTimerHonour`

Skips the call, for A/B measurements and for diagnosing a frame-time problem. The read-back still runs
and the line says `honour=0`, so a log always shows which policy the process ran with.

### 1.5 The measured effect

Measured on 2026-10-14 (og-simulationscheduler-withjolt task 79) with a packaged Shipping dedicated server
(Jolt), no clients, a `-log` console that is shown for 20 s, minimized for 40 s and restored for 20 s. The
values are the `frameMs` p50 of the server's 10-second `[SimHost.Window]` lines.

| run | `[Startup.Timer]` | shown | minimized | restored |
|---|---|---|---|---|
| `-OGNoTimerHonour` | `honour=0 set=0 err=0 readback=1 control=0x0 state=0x0` | 16.7 ms | **30.9–31.1 ms** | 16.7 ms |
| default | `honour=1 set=1 err=0 readback=1 control=0x4 state=0x0` | 16.7 ms | **16.7 ms** | 16.7 ms |

With the policy set, minimizing the console no longer changes the frame time. A packaged Development
client and the editor log the same `honour=1` line.

No automated test covers this: the effect depends on Windows' treatment of a minimized window, which a
test process cannot reproduce. The `static_assert`s hold the values, and the log line shows the outcome in
every process.

---

## 2. The console-command named-pipe server

On Windows, `StartupModule` starts `DAttackPipeServer::NamedPipeServer` on a detached thread. The
server (og-brawler `ConsoleCommandsNamedPipeServer.cpp`) loops forever: it creates the pipe
`\\.\pipe\DAttackPipe`, waits for one client, reads `name=value` lines until the client closes the pipe,
passes each one to the attack machine's runtime tweakables, and starts over. If the pipe cannot be
created (for example because another process already owns it) the thread returns and nothing else
happens. The thread is never joined; it ends with the process.
