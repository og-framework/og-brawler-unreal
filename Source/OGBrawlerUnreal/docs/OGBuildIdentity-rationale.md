<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: PhysicsBackendUImpl.h -- deleted by og-session-runtime task 31 (the brawler's engine seam header: its binding concept and the EngineTraits and EngineBinding aliases moved to the top of BrawlerSessionTraitsUImpl.h, its rationale with it); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent::OnRep_CorrectionState -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
<!-- lint-external-ref: USimulatableNetSyncComponent::ServerReceiveRemoteMove_Implementation -- deleted by og-input-latency task P5-8 (the net-sync component, the pawn's subobject, the bundle builder and the owner binding's net half); kept in the history -->
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

The label is used in four places (the physics-backend token beside it and its login check are §7; the wire token
and its login check are §8):

* the join screen's `Build: <label>` line, through `joinScreenUImpl::ownBuildLabel()`;
* the "different build" failure text, which names the client's own label (the client never learns the
  server's label, T10 flag 6 and the lead's ruling);
* the network version (§3);
* the dedicated server's `OGBrawlerSession: build label=<label>` line (§5).

At engine start each process logs one line:
`LogOGBuildIdentity: Display: OGBuildIdentity: label=<label> source=<source> backend=<token> networkProjectVersion=<v> wire=<token> units=m axes=YupRH`
(the last three fields are §8).

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

Until og-session-runtime task 3 (the retirement og-simulationscheduler-withjolt task 21 had planned), one
build of this game ran either the Chaos host or the Jolt host, chosen at compile time
(`OGBrawlerSimulationManager-rationale.md` §18). Since then every build runs the Jolt host, and the token
and the check below stay generic over the engine traits, for a later engine (Box3D). Two peers of
different backends cannot play together: they simulate the same inputs differently, so every
prediction would be corrected. The build identity therefore carries a **backend token**, and the server
refuses a login whose backend differs from its own. Two Jolt peers whose Jolt builds step differently (a
Debug and a Development build, a different instruction set, a different Jolt version) are admitted, with
a Warning that names both fingerprints (the policy below).

**The token.** `buildIdentityUImpl::backendToken()`:

| configuration | token | example |
|---|---|---|
| Jolt | `jolt:<fingerprint>`, the 16 lower-case hex digits of the Jolt determinism fingerprint's value | jolt:35a523518f873707 |

An engine without a fingerprint has its bare name as its token: the former Chaos configuration (removed
by og-session-runtime tasks 3–6) sent `chaos`, and the classification's `static_assert`s still use that
string as a second backend.

The fingerprint is og-simulation-jolt's `determinismFingerprint`: the Jolt version and feature bits,
the instruction set the library was compiled for, fused multiply-add, the step's FP mode and the
state hash of a short probe simulation, folded into one 64-bit value
(`JoltDeterminismFingerprint-rationale.md`). The configuration's engine traits compute it
(`EngineTraits::computeDeterminismFingerprint`, which the build identity calls directly since og-tickdriver task 15b
and through a forwarder of the same name in `PhysicsBackendUImpl.h` before it: since og-tickdriver task 15a
`JoltEngineTraitsUImpl::computeDeterminismFingerprint` acquires the Jolt runtime, runs the probe and releases the
runtime; the former Chaos configuration had none), and the build identity computes it once,
on first use, and keeps it for the process. The first use is `applyNetworkVersion` at
`FCoreDelegates::OnPostEngineInit`, so the start line (§1) already carries the full token, and the probe
runs before any world exists. It takes about 2 ms; the runtime's two lines
(initialised, shut down) are logged at `Log` on `LogOGBuildIdentity`.
`buildIdentityUImpl::backendFingerprint()` is the hex alone (`none` for an engine without a fingerprint,
as Chaos was); the manager's `[SimHost.Backend] backend=<name> fingerprint=<hex|none>` line prints it
(`backend=jolt` in every build since og-session-runtime task 3).

**The check, at login.** The client puts its token in its login URL, and the server compares it in its
game mode's pre-login, before the player is admitted:

* **Client.** `UOGBrawlerLocalPlayer`, the project's local-player class, returns `OGBackend=<token>` as its game login options
  (`buildIdentityUImpl::backendLoginOption`, key `kBackendLoginOptionKey`), followed by the wire token's option (§8). The engine's pending net
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
  one `Warning` on `LogOGBuildIdentity`: `OGBuildIdentity: refused a login: <reason>`. When the backend check
  admits, the same call checks the wire token next (§8). A different
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

## 8. The wire format: the `OGWire` token and the login check

og-sim-metres-yup task 15 (the user's ruling Q2, 2026-10-25: a login lacking the token is refused). Each of
og-simulation's three payloads carries its own version byte: the correction state
(`correctionStateBuffer::kWireFormatVersion`), the relayed input ring (`relayedInputRing::kWireFormatVersion`)
and the input redundancy bundle (`inputRedundancyBundle::kWireFormatVersion`). They are 15, 3 and 3, so the
token's first three fields are c15.r3.b3 (the next two are the envelope's and the roster's, and the last the og-sim-net
clock's, below).
og-sim-metres-yup moved og-simulation to metres and a Y-up frame, which changes what the bytes
mean without changing their layout, so each step bumped the versions it changed. While the move ran, each version
was a `constexpr` function of `SimulationFrame.h`'s two frame knobs; task 24 deleted the knobs and froze the
versions at the third row, and task 26 (the native planar chart) bumped the correction version once more.
og-ground-aligned-attacks task 10 bumped it again without moving a byte: from that build on the brawler's swing
plane and projectile launch follow its ground normal, so on sloped ground the radial and projectile initial
conditions a peer predicts differ from what a 9 peer predicts for the same input. og-ground-aligned-attacks task 30
bumped it once more, again without moving a byte: the brawler movement now splits its velocity exactly on every
tangent frame, so where the frame's second tangent is not horizontal (a slope that does not face forward or back,
or a rim's tilted normal) a peer predicts a different velocity from a 10 peer. og-ground-aligned-attacks task 14b
bumped it once more, again without moving a byte: on flat ground the brawler's swing initial conditions now take the
quaternion round trip the slope path takes, so a peer's stored angle and axis differ from an 11 peer's by rounding,
and on an aim exactly along forward or back in the axis's sign. og-ground-aligned-attacks task 20 bumped it once
more, again without moving a byte: a melee swing now hits when its attack band overlaps the target's movement
capsule, not only when the capsule's centre is inside the band, so a peer registers hits a 12 peer misses.
og-ground-aligned-attacks task 20b bumped it once more, again without moving a byte: a projectile's block is now
decided by the struck character's guard even when the report lists that character's body first, and every guard
measures the incoming shot in its own plane, so a peer blocks shots a 13 peer lets through.
og-ground-aligned-attacks task 36 bumped it once more, again without moving a byte: the brawler movement's support
law v2 became the default, so a character takes its ground from the exact face, an edge contact and a centre probe,
changes support only on events, writes a knockback once and redirects a velocity into a face along it, and a peer
predicts different movement from a 14 peer on slopes, rims and creases and after every knockback:

| frame | correction | ring | bundle | token |
|---|---|---|---|---|
| cm, Z-up (before the move) | 6 | 2 | 2 | c6.r2.b2 |
| m, Z-up (after flip U) | 7 | 2 | 2 | c7.r2.b2 |
| m, Y-up (after flip X) | 8 | 3 | 3 | c8.r3.b3 |
| m, Y-up, native planar chart (after task 26) | 9 | 3 | 3 | c9.r3.b3 |
| the same, with ground-aligned attacks (after og-ground-aligned-attacks task 10) | 10 | 3 | 3 | c10.r3.b3 |
| the same, with the exact velocity decomposition (after og-ground-aligned-attacks task 30) | 11 | 3 | 3 | c11.r3.b3 |
| the same, with the flat swing frame through the general expression (after og-ground-aligned-attacks task 14b) | 12 | 3 | 3 | c12.r3.b3 |
| the same, with the melee capsule hit test (after og-ground-aligned-attacks task 20) | 13 | 3 | 3 | c13.r3.b3 |
| the same, with the projectile guard rule (after og-ground-aligned-attacks task 20b) | 14 | 3 | 3 | c14.r3.b3 |
| the same, with the movement's support law v2 (after og-ground-aligned-attacks task 36; shipped) | 15 | 3 | 3 | c15.r3.b3 |

The ring and the bundle carry the brawler's input, whose world vectors are directions with no length (the aim
is checked to be a unit vector, the move direction comes from the stick), so only the axis changed them. The
chart change left them alone too: the stick and the world vectors are not chart values. It changed the
correction state only, where the brawler movement's `committedStepDir` is a planar chart value and mirrors.
`WireVersionKnobTest.cpp` in og-simulation-tests pins the three shipped numbers.

**The envelope and the roster (og-input-latency P5-6).** Two more versions follow the three. The sim envelope's
(`kSimEnvelopeVersion`, `OGSimulation/Network/SimEnvelope.h`) is the first byte of every og-sim message sent over
og-net; the og-sim-net roster's (`ogsimnet::kRosterWireVersion`, `OGSimNet/SimNetRoster.h`) is the first byte of
every roster body. Both are 1, so P5-6's token was c15.r3.b3.e1.ro1. Neither describes a payload, so the three
codec versions cannot stand in for them. A peer whose envelope version differs has every message refused by
`simEnvelope::decode` (`DecodeRefusal::Version`), which `ogsimnet::SimNetServer` and `ogsimnet::SimNetClient`
count as bad decodes and drop. A roster of another version is refused (`RosterRefusal::Version`), and the client
then ignores all sim traffic and asks to be disconnected. Either way the player would be admitted into a session
that cannot work, the case the login check exists for (below). A build from before P5-6 presents the three-field
token, which the server classifies as `Different`. `SimEnvelopeTest.cpp` in og-simulation-tests refuses the
envelope version bytes 0 and 2, and `SimNetRosterTest.cpp` in og-sim-net's tests pins the roster's 1.

**The clock (og-time-dilation task 2).** A sixth version follows: og-sim-net's SimClock ping/pong
(`ogsimnet::kClockWireVersion`, `OGSimNet/SimNetClock.h`), 2 since the pong carries its server tick's age (34 bytes; 1
was the 30-byte pong). The pong has no version byte of its own, and og-simulation's envelope version does not change
with it: a pong of the other length is dropped and counted as malformed (`pongsMalformed`), so a client of the other
clock layout would join and never get a clock sample. The token is therefore c15.r3.b3.e1.ro1.ck2, following P5-6's `.ro`
precedent (og-sim-net owns its own versions). A build from before task 2 presents the five-field token c15.r3.b3.e1.ro1,
which the server classifies as `Different` (a `static_assert` row checks both directions). The pong's layout is in
og-sim-net's `SimNetClock-rationale.md` §2.

**The token.** `buildIdentityUImpl::wireToken()` is
`c<correction>.r<ring>.b<bundle>.e<envelope>.ro<roster>.ck<clock>`, each version in decimal without leading zeros.
Since og-session-runtime task 21 each module exports its own fields as one compile-time fragment, and
`OGBuildIdentityUImpl.cpp` joins them with og-simulation's `simWireToken::join` (`SimWireToken.h`): og-simulation's
`simWireToken::kSimFragment`, `c15.r3.b3.e1` from its four versions, then og-sim-net's `ogsimnet::kWireTokenFragment`,
`ro1.ck2` from its roster and clock versions (`OGSimNet-rationale.md` §7). The game has no field of its own. It reaches
og-sim-net only through `UEBrawlerNetConfig.h`, its binding to og-sim-net, which carries that fragment as
`UEBrawlerNetConfig::kSimNetWireTokenFragment`, so `OGBuildIdentityUImpl.cpp` includes neither `SimNetClock.h` nor
`SimNetRoster.h` directly, and names none of their versions. *Not "at all" (corrected by og-session-runtime task 33):* both
still reach the translation unit transitively, through `UEBrawlerNetConfig.h` → og-sim-net's `SimNetVersion.h`, which
includes both to build the fragment (`BrawlerSessionTraitsUImpl.h`, which the file also includes, includes
`UEBrawlerNetConfig.h` too). The token is still built at compile time from the six constants, so it cannot disagree with the
compiled codecs, envelope, roster and clock. A `static_assert` beside it pins the whole token to
`c15.r3.b3.e1.ro1.ck2`: a version bump in any module fails the game's build there until that string and this section
are updated, and the module's own test pins its fragment the same way (`SimWireTokenTest.cpp` in og-simulation-tests,
`SimNetVersionTest.cpp` in og-sim-net's tests). The format and the 32-character buffer are og-simulation's now: its
test writes the longest token, `c255.r255.b255.e255.ro255.ck255` (31 characters), so a buffer too small for it fails
the build; those rows were this header's own format `static_assert` before task 21.

**The check, at login.** The client's `UOGBrawlerLocalPlayer::GetGameLoginOptions` returns the backend option
of §7, then `?`, then `OGWire=<token>` (`buildIdentityUImpl::wireLoginOption`, key `kWireLoginOptionKey`;
the separator is ⛔G-02). `buildIdentityUImpl::refuseMismatchedBackendLogin` runs the backend check of §7
first. When that admits the login (a match, or a different fingerprint), it reads the `OGWire` option and
classifies it with `classifyWireLogin` (`WireLoginVerdict`: `Match`; `Missing` for an absent or empty token;
`Different` for anything else). `refusesWireLogin` refuses `Missing` and `Different`, as ruled;
`static_assert`s pin both functions. A refusal logs the same Warning as §7,
`OGBuildIdentity: refused a login: <reason>`, with the server's token first:

* "Different wire format: this server speaks c15.r3.b3.e1.ro1.ck2, the client c15.r3.b3.e1.ro1." (a build from
  before og-time-dilation task 2)
* "Different build: the client reports no wire format, this server speaks c15.r3.b3.e1.ro1.ck2." (a build from
  before this check, or an empty token)

A backend refusal takes precedence: a client of the other backend is told about the backend. The client sees
the text the way it sees §7's ("Server refused: <text>").

**Why at login, when every payload has a version byte.** The version bytes are compared only when a payload
arrives, after the player was admitted. A client that receives a correction of another version logs an
`Error`, shows a build-mismatch message and ignores every later correction (og-sim-net's `SimNetClient`, through
the Unreal client host's `onWireMismatch`, `UESimNetHost-rationale.md` §8). The server counts every sim message
whose envelope version differs as a bad decode and drops it (`SimNetServer`'s `onMessage`, through
`simEnvelope::decode`). Until og-input-latency task P5-7 the two paths were the net-sync component's
(`USimulatableNetSyncComponent::OnRep_CorrectionState`, and the input RPC
`USimulatableNetSyncComponent::ServerReceiveRemoteMove_Implementation`, which dropped every redundancy bundle of
another version). The player is then in a session that cannot work. The login check refuses before admission, and its text names both tokens.

**The start line.** The `OGBuildIdentity:` Display line (§1) ends with `wire=<token> units=m axes=YupRH`: the
token from the same six constants, then the simulation's length unit and axes as literals in the format string.
The fields are appended after the earlier ones, so a reader of the earlier fields is unaffected. `units=` is each
capture's length-unit marker: a log line that reports simulation state prints its lengths in that unit, and log
tooling reads the marker per capture (a capture with no marker predates it and is in centimetres). While the frame
knobs existed (og-sim-metres-yup tasks 15 to 23) the two values came from them, `units=cm` or `units=m` and
`axes=ZupLH` or `axes=YupRH`; task 24 froze them, because the tools still read the marker.

**Testing a mismatch: `-OGWireTokenOverride=<token>`.** In a non-Shipping build, a client started with
`-OGWireTokenOverride=<token>` presents `<token>` at login instead of its own token. With no value
(`-OGWireTokenOverride=` followed by a space or the end of the command line) it presents an empty token,
which the server refuses as `Missing`. The empty case is detected before FParse::Value is called: that
function skips whitespace after the `=` (engine Parse.cpp:306-308) and would read the next argument as the
token. The process logs one Warning at start-up, "OGBuildIdentity: -OGWireTokenOverride: this process
presents the wire token '<token>' at login instead of c15.r3.b3.e1.ro1.ck2.", and its start line keeps the compiled
token. The switch is compiled out of Shipping (`#if !UE_BUILD_SHIPPING`). It is read from the command line
because the login options are built before any console command a test could send to the client.
