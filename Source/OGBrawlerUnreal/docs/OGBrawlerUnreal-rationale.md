<!-- SPDX-License-Identifier: BUSL-1.1 -->
# The game module — rationale

`Source/OGBrawlerUnreal/OGBrawlerUnreal.cpp` is the primary game module: `FOGBrawlerUEModule`. Its
`StartupModule` runs in every process that loads the game — the editor, `-game`, a client and a dedicated
server, in every configuration — and does two things, in this order:

1. Registers the build-identity network-version hook, `buildIdentityUImpl::registerNetworkVersionHook`.
   Its reasoning and prohibitions are in `OGBuildIdentity-rationale.md` and `OGBuildIdentity-guards.md`.
2. On Windows, starts the console-command named-pipe server on a detached thread (§1).

`ShutdownModule` does nothing.

The startup timer-resolution honour (`[Startup.Timer]`, `-OGNoTimerHonour`) used to be this module's first
statement. og-tickdriver task 13 moved it, unchanged, into the `OGSimulationUnreal` module, whose own `StartupModule`
runs right after this one: see `OGSimulationUnreal-rationale.md` §2 and §3.

---

## 1. The console-command named-pipe server

On Windows, `StartupModule` starts `DAttackPipeServer::NamedPipeServer` on a detached thread. The
server (og-brawler `ConsoleCommandsNamedPipeServer.cpp`) loops forever: it creates the pipe
`\\.\pipe\DAttackPipe`, waits for one client, reads `name=value` lines until the client closes the pipe,
passes each one to the attack machine's runtime tweakables, and starts over. If the pipe cannot be
created (for example because another process already owns it) the thread returns and nothing else
happens. The thread is never joined; it ends with the process.
