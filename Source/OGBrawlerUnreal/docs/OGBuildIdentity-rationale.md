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

The label is used in four places (the physics-backend token beside it, and the login check it feeds, are §7):

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
calls `FNetworkVersion::SetProjectVersion("<ProjectVersion>+<label>")`, e.g.
`0.1.0+20260929-101500-abc1234`. The physics backend is not part of the version: it is compared at
login instead (§7). Task 51 of og-simulationscheduler-withjolt had appended it as a third `+<backend>`
part; task 18 took it out again, so that a backend mismatch is refused by one check, for labelled and
`dev` builds alike, with a reason that names both sides.

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

## 7. The physics backend: the token and the login check

Until task 21 of og-simulationscheduler-withjolt, one build of this game runs either the Chaos host or
the Jolt host, chosen at compile time (`SimulationManagerUImpl-rationale.md` §18). Two peers of
different backends cannot play together: they simulate the same inputs differently, so every
prediction would be corrected. The build identity therefore carries a **backend token**, and the server
refuses a login whose backend differs from its own. Two Jolt peers whose Jolt builds step differently (a
Debug and a Development build, a different instruction set, a different Jolt version) are admitted, with
a Warning that names both fingerprints (the policy below).

**The token.** `buildIdentityUImpl::backendToken()`:

| configuration | token | example |
|---|---|---|
| Chaos | `chaos` | `chaos` |
| Jolt | `jolt:<fingerprint>`, the 16 lower-case hex digits of the Jolt determinism fingerprint's value | jolt:35a523518f873707 |

The fingerprint is og-simulation-jolt's `determinismFingerprint`: the Jolt version and feature bits,
the instruction set the library was compiled for, fused multiply-add, the step's FP mode and the
state hash of a short probe simulation, folded into one 64-bit value
(`JoltDeterminismFingerprint-rationale.md`). The traits header computes it
(`physicsBackendUImpl::computeDeterminismFingerprint`, which acquires the Jolt runtime, runs the probe
and releases the runtime; the Chaos configuration has none), and the build identity computes it once,
on first use, and keeps it for the process. The first use is `applyNetworkVersion` at
`FCoreDelegates::OnPostEngineInit`, so the start line (§1) already carries the full token, and the probe
runs before any world exists. It takes about 2 ms; the runtime's two lines
(initialised, shut down) are logged at `Log` on `LogOGBuildIdentity`.
`buildIdentityUImpl::backendFingerprint()` is the hex alone (`none` for Chaos); the manager's
`[SimHost.Backend] backend=<chaos|jolt> fingerprint=<hex|none>` line prints it.

**The check, at login.** The client puts its token in its login URL, and the server compares it in its
game mode's pre-login, before the player is admitted:

* **Client.** `UOGBrawlerLocalPlayer`, the project's local-player class, returns `OGBackend=<token>` as its game login options
  (`buildIdentityUImpl::backendLoginOption`, key `kBackendLoginOptionKey`). The engine's pending net
  game appends a local player's game login options to the URL it sends at login, whatever started the
  travel: the join screen, a command-line address, or PIE. `Config/DefaultEngine.ini` sets
  LocalPlayerClassName to it once, in the engine section. The property is a global config property, so
  every engine class reads it from that section: the game engine of packaged clients and `-game` runs,
  and the editor's engine in PIE.
* **Server.** `AOGBrawlerUEGameMode::PreLogin` runs the engine's own checks first and then, when they
  admitted the player, `buildIdentityUImpl::refuseMismatchedBackendLogin(options, error)`. It reads the
  `OGBackend` option and classifies it against its own token with `classifyBackendLogin`
  (`BackendLoginVerdict`: a match, a client that reports no token, a different backend, or the same
  backend with a different fingerprint; `static_assert`s pin the four). `refusesBackendLogin` decides
  which verdicts refuse: a different backend and a missing token do, a different fingerprint and a match
  do not (a second `static_assert` pins that). A refusal sets the error, which refuses the login, and logs
  one `Warning` on `LogOGBuildIdentity`: `OGBuildIdentity: refused a login: <reason>`. A different
  fingerprint is admitted and logs one `Warning` instead: "OGBuildIdentity: admitted a login with a
  different Jolt determinism fingerprint: this server runs jolt:…, the client runs jolt:…."
* **The reason names both sides**, server first:
  * "Different physics backend: this server runs jolt:35a523518f873707, the client runs chaos."
  * "Different build: the client reports no physics backend, this server runs jolt:…." (a build from
    before this check, or a client whose local-player class is not this project's).
* **What the client sees.** The engine sends the error to the client as its login failure, and the
  client's pending net game reports it as a pending-connection failure carrying the server's text. The
  join screen maps that to "Server refused: <text>" (`JoinScreen-rationale.md` §13), and the engine logs
  the text in the engine's LogNet category, so the client's screen and log name both tokens too.

**Why at login and not in the network version.** The version is a 32-bit hash, so a version mismatch
reaches the client as "different build" and cannot say which builds (§4: the client never learns the
server's label). It is also left unchanged for `dev` (§3), so two unlabelled builds of different
backends, the editor's `-game` and a packaged development client, would have connected. The login URL
carries text, so the check can compare the token exactly and name both, and it applies to every build.
A labelled build still gets the label in its version, and two different labels are still refused at
the version check, before the login.

**A dedicated server on another map.** The check lives in `AOGBrawlerUEGameMode`, the game mode of
every gameplay map (`Config/DefaultEngine.ini` GlobalDefaultGameMode). The front-end game mode hosts no
joins.

**The fingerprint policy: backend only** (the user's ruling, 2026-10-09). A different fingerprint is
admitted, never refused. The instruction set is part of the fingerprint (NEON on Android arm64, an x64
instruction set on Win64), so an Android client always differs from a Win64 server, and refusing it would
rule out the mixed sessions the game is played in. Nothing here needs bitwise-identical simulation across
peers: the server is authoritative, and a client's prediction is corrected when it diverges, so a
different Jolt build costs more corrections, not a broken session. The server's Warning keeps the
difference visible in its log. The client logs nothing about it: a successful login hands the client no
text from the server, so it never learns the server's token (it would need a new replicated field). Its
own start line and `[SimHost.Backend]` still name its fingerprint, so the two logs together show the
pair.
