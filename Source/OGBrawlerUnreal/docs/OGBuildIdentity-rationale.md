<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: FCoreDelegates::OnPostEngineInit -- Unreal Engine delegate (CoreDelegates.h), outside every scan root -->
<!-- lint-external-ref: FNetworkVersion::GetLocalNetworkVersion -- Unreal Engine function (NetworkVersion.cpp), outside every scan root -->
<!-- lint-external-ref: FNetworkVersion::GetLocalNetworkVersionOverride -- Unreal Engine delegate (NetworkVersion.h), outside every scan root -->
<!-- lint-external-ref: UEngine::Init -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
<!-- lint-external-ref: UNetConnection::HandleReceiveNetUpgrade -- Unreal Engine method (NetConnection.cpp), outside every scan root -->
<!-- lint-external-ref: UWorld::NotifyControlMessage -- Unreal Engine method (World.cpp), outside every scan root -->
<!-- lint-external-ref: ENetworkFailure::OutdatedClient -- Unreal Engine enumerator (NetEnums.h), outside every scan root -->
# Build identity — rationale

`OGBuildIdentityUImpl.{h,cpp}` give every process one **build label** and make two processes with
different labels refuse to play together (og-brawler-uploadtosteam task 15, following the T10 spike's
Q4 and Q5). The label is shown on the join screen, is part of the network version, and is the first line
a dedicated server writes to its session log. Prohibitions: `OGBuildIdentity-guards.md`.

---

## 1. Where the label comes from, and where it goes

`buildIdentityUImpl::buildIdentity()` resolves the label once, on first use, in this order:

| source | when | `source=` in the log |
|---|---|---|
| `-OGBuildLabel=<label>` on the command line | tests and hand-made runs (§6) | `command-line` |
| the `label=` line of `build_info.txt` in `FPaths::RootDir()` | a build published by og-tools `Publish-OgSteamBuild` (task 14) | `build_info` |
| neither | the editor, `-game`, PIE, an unpublished package | `none` (label `dev`) |

A command-line label wins over the file, so a published build can still be relabelled for a test. A
source that is present but unusable (an `-OGBuildLabel=` value that is not a label, or a
`build_info.txt` with no or an invalid `label=` line) logs an `Error` on `LogOGBuildIdentity` and gives
`dev`; it does not fall through to the next source. One rule for "no usable label" keeps the outcome
predictable: a broken build behaves like an unlabelled one, and the log says why.

The label is used in four places (the physics-backend token beside it is §7):

* the join screen's `Build: <label>` line, through `joinScreenUImpl::ownBuildLabel()`;
* the "different build" failure text, which names the client's own label (the client never learns the
  server's label, T10 flag 6 and the lead's ruling);
* the network version (§3);
* the dedicated server's `OGBrawlerSession: build label=<label>` line (§5).

At engine start each process logs one line:
`LogOGBuildIdentity: Display: OGBuildIdentity: label=<label> source=<source> backend=<token> networkProjectVersion=<v>`.

## 2. Reading `build_info.txt`

**Where.** In a packaged build `FPaths::RootDir()` is the depot's ContentRoot, the folder that holds
`OGBrawlerUnrealClient.exe` / `OGBrawlerUnrealServer.exe` and where task 14 writes the file (T10 spike
Q5: RootDir is the engine directory cut at `/Engine`, and a package's engine directory is
`<ContentRoot>/Engine`). In a non-packaged run RootDir is the engine checkout (`C:/dev/UnrealEngine/`
on this machine), which has no `build_info.txt`, so the label is `dev` without any special case.

**Format.** Task 14 writes UTF-8 without BOM, LF line ends, `label=`, `sha=`, `dirty=`, `created=` in
that order. Only `label=` is read. `labelFromBuildInfo` also accepts a UTF-8 BOM and CR LF line ends,
because task 14 noted that the file may be edited by hand (Notepad adds both). The first `label=` line
wins. The `static_assert`s beside `labelFromBuildInfo` pin all of this.

**The label rule.** 1 to 64 characters of `0-9 A-Z a-z . _ -` (`isValidBuildLabel`). Task 14's labels
(`yyyyMMdd-HHmmss-<sha7>` and a `-dirty` suffix) fit. Excluding the space matters: the label is
printed into a server log line whose fields are separated by spaces (§5).

## 3. The network version

`applyNetworkVersion` runs on `FCoreDelegates::OnPostEngineInit` (⛔G-01) and, for any label but `dev`,
calls `FNetworkVersion::SetProjectVersion("<ProjectVersion>+<label>+<backend>")`, e.g.
`0.1.0+20260929-101500-abc1234+chaos` (the backend token is §7; it was not part of the version before
og-simulationscheduler-withjolt task 51).

* **Why the project version.** The engine hashes "`<ProjectName> <ProjectVersion>, NetCL: <n>`" plus the
  engine and game protocol versions into the network version (`FNetworkVersion::GetLocalNetworkVersion`,
  engine NetworkVersion.cpp:255-291). Appending the label to the project version keeps all the
  engine's own ingredients in the hash. The rejected alternative, binding
  `FNetworkVersion::GetLocalNetworkVersionOverride`, replaces the whole hash (NetworkVersion.cpp:262-271)
  and so drops the engine protocol version (T10 spike Q4).
* **Why after `UEngine::Init`.** `UEngine::Init` sets the project version from the project settings
  (UnrealEngine.cpp:2229), overwriting anything earlier; `OnPostEngineInit` fires after it and before the
  first travel. `SetProjectVersion` clears the cached hash (NetworkVersion.cpp:106-116).
* **`dev` changes nothing.** With no label the engine's default version is untouched, so the editor,
  PIE and every unlabelled run behave exactly as before task 15; `dev` and `dev` connect. A
  `static_assert` pins `!overridesNetworkVersion("dev")`.
* **Case does not count.** The engine lower-cases the version string before hashing it, so labels that
  differ only in case (`A` and `a`) produce the same network version and connect. Task 14's labels are
  digits, lower-case hex and `-dirty`, so a real publish never meets this.

## 4. Who enforces a mismatch — for a Shipping client and a Development server

The shipped pair is a **Shipping** client and a **Development** dedicated server (Backlog user decision
2026-09-26). The T10 spike warned that the engine's handshake check is skipped in Shipping. Reading the
engine (UE 5.6.1) settles which side enforces:

1. **The stateless handshake does not enforce, in any configuration.** It computes the version check
   only when `!UE_BUILD_SHIPPING || net.HandshakeEnforceNetworkCLVersion`, but it rejects a packet only
   when bValidNetCLVersion || !GHandshakeEnforceNetworkCLVersion is false
   (StatelessConnectHandlerComponent.cpp:1704,1743). The console variable defaults to 0
   (`:279-284`), so with engine defaults the handshake lets a mismatched client through in Development
   **and** Shipping alike. This project does not set it.
2. **The server enforces, at `NMT_Hello`.** `UWorld::NotifyControlMessage`, server branch, compares the
   client's version from `NMT_Hello` with its own; on a mismatch it sends `NMT_Upgrade` and closes the
   connection (World.cpp:7039,7055,7061). No build-configuration condition surrounds this code, so it
   runs in the Development server we ship (and would in a Shipping server too).
3. **The client reports it, with no configuration condition either.** The pending net game receives
   `NMT_Upgrade` (PendingNetGame.cpp:254-270); `UNetConnection::HandleReceiveNetUpgrade` finds the
   versions incompatible and broadcasts `ENetworkFailure::OutdatedClient` (NetConnection.cpp:1302,1334),
   which `joinScreenUImpl::networkFailureReason` maps to **different build** (`JoinScreen-rationale.md`
   §13). The client's `NMT_Hello` carries its labelled version because `OnPostEngineInit` precedes the
   first travel in every target.

So the **server** is the enforcing side, and the Shipping caveat does not apply to the shipped pair.
Measured with Development client and server (task 15 impl notes): different labels give
`OutdatedClient` and the "different build" screen; the Shipping client runs the same client-side code
path in (3), none of which is compiled out in Shipping.

## 5. The server's start line

A dedicated server logs `OGBrawlerSession: build label=<label>` from `SessionLog::noteListening`,
immediately before the `listening` line, whenever a gameplay map begins play. It uses the session log's
prefix and category (`SessionLog-rationale.md` §1). It must match neither of the host launcher's
patterns; a `static_assert` in `SessionLogUImpl.h` makes the words after the prefix differ from
`joined`, `left` and `listening`, and a label cannot contain a space. The patterns were run against the
line in both PowerShell 5.1 and pwsh 7 (`SessionLog-rationale.md` §6).

## 6. Testing without a package

`-OGBuildLabel=<label>` works in every configuration (the command line is not filtered,
`UE_COMMAND_LINE_USES_ALLOW_LIST` is 0 by default), including the editor's `-game` and `-server` runs.
The task 15 acceptance used exactly this:

* server `UnrealEditor.exe <uproject> /Game/ThirdPerson/Maps/ThirdPersonMap -server -port=7777 -OGBuildLabel=A`
  and client `UnrealEditor.exe <uproject> -game -OGBuildLabel=B` → "different build" with `Build: B`;
* both `A` → connected;
* neither given (`dev`/`dev`) → connected.

The engine's `-networkversionoverride=<n>` also forces a mismatch, but it replaces the changelist in the
hash rather than testing this code.

## 7. The physics-backend token

Until task 21 of og-simulationscheduler-withjolt, one build of this game runs either the Chaos host or
the Jolt host, chosen at compile time (`SimulationManagerUImpl-rationale.md` §18). The build identity
carries which one: `buildIdentityUImpl::backendToken()` returns `physicsBackendUImpl::kBackendToken`
from `PhysicsBackendUImpl.h`, which is `chaos` in the Chaos configuration. It is read in two places:

* the start line, as `backend=<token>` (§1);
* the network version of a labelled build, as a third `+<token>` part (§3). Two labelled builds of
  different backends therefore get different network versions, and the server refuses the join the
  way it refuses any other version mismatch (§4).

`dev` still changes nothing (§3): an unlabelled run keeps the engine's version whatever its backend,
so the editor and PIE behave as before. Telling two unlabelled builds of different backends apart,
and the Jolt token `jolt:<determinism fingerprint>` with a refusal that names both builds, belong to
task 18. Until then only the Chaos configuration builds, so every peer carries the same token.
