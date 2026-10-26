<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: FCoreDelegates::OnPostEngineInit -- Unreal Engine delegate (CoreDelegates.h), outside every scan root -->
<!-- lint-external-ref: FNetworkVersion::SetProjectVersion -- Unreal Engine function (NetworkVersion.cpp), outside every scan root -->
<!-- lint-external-ref: UEngine::Init -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
# Build identity — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/OGBuildIdentityUImpl.cpp`
(og-brawler-uploadtosteam task 15) and `Source/OGBrawlerUnreal/OGBrawlerLocalPlayer.cpp` (og-sim-metres-yup task 15). Ids are opaque, stable, and retired rather than reused. The
reasoning lives in `OGBuildIdentity-rationale.md`.

The rules the compiler holds need no entry: the `build_info.txt` parse (BOM, CR, missing and invalid
`label=` lines), the label character set, and "the `dev` label never changes the network version" are
`static_assert`s in `OGBuildIdentityUImpl.h` (rationale §2, §3).

The physics-backend token and the login check (rationale §7, og-simulationscheduler-withjolt tasks 51
and 18) need no entry either. The token is built from `PhysicsBackendUImpl.h`, the header that selects
the backend, so the token and the compiled host cannot disagree. The admission rule (a different
backend and a missing token are refused; a different Jolt fingerprint is admitted with a Warning) is two
`static_assert`s, over `classifyBackendLogin` and `refusesBackendLogin` in `OGBuildIdentityUImpl.h`. And every way the wiring can break fails closed and says why: a client whose
local-player class does not send the token is refused with "the client reports no physics backend".
The one way it fails open is a game mode that stops calling the check, a deleted call in
`AOGBrawlerUEGameMode::PreLogin`; even then every process names its token on its start line and in
`[SimHost.Backend]`, so the logs show the mismatch.

The wire token and its login check (rationale §8, og-sim-metres-yup task 15) need no entry beyond G-02. The
token is built at compile time from the three codecs' version constants, so it cannot disagree with them.
The token's format, the three verdicts and the refusal of `Missing` and `Different` (the user's ruling Q2) are
`static_assert`s in `OGBuildIdentityUImpl.h`; the start line's `units=m axes=YupRH` is a literal in its format
string. A client
that sends no token is refused with "the client reports no wire format", so a broken wiring fails closed and
says why.

---

## G-01 — the network version is set from `FCoreDelegates::OnPostEngineInit`, never earlier

**Site:** the `FCoreDelegates::OnPostEngineInit.AddStatic(&applyNetworkVersion)` statement in
`buildIdentityUImpl::registerNetworkVersionHook`.

**The prohibition.** ⛔ Do not call `applyNetworkVersion` (or `FNetworkVersion::SetProjectVersion`)
directly from the module's `StartupModule`, from a static initialiser, or from any other point that runs
before `UEngine::Init` has finished. ⛔ Do not move it later than the first connection either (e.g. into a
GameMode, a HUD or a world callback).

**The consequence.** `UEngine::Init` sets the network project version to the project settings'
`ProjectVersion` (engine UnrealEngine.cpp:2229), which overwrites anything set before it. A call from
`StartupModule` runs first, is overwritten, and every labelled build then has the same network version:
two different builds connect to each other and fail later in ways nobody can diagnose, instead of
showing "different build" on the join screen. A call later than the first connection is too late for a
client started with an address: its `NMT_Hello` has already carried the unlabelled version. The engine
broadcasts `OnPostEngineInit` right after `GEngine->Init` and before `GEngine->Start()` starts the game
instance and its first travel (engine LaunchEngineLoop.cpp:4571,4577,4619), in the Client, Server and
Editor targets alike.

**What breaks if the tag moves.** The tag sits on the only statement that binds the version to the
engine's start-up. The module's `StartupModule` only calls `registerNetworkVersionHook`; it is the
first point where the game module can register, and registering there is harmless because nothing runs
until the delegate fires.

## G-02 — the two login options are joined with `?`, never `&`

**Site:** the `return` statement of `UOGBrawlerLocalPlayer::GetGameLoginOptions`, which joins
`buildIdentityUImpl::backendLoginOption()` and `buildIdentityUImpl::wireLoginOption()`.

**The prohibition.** ⛔ Do not join the two options with `&`, a space or anything else but `?`, even though
the engine's own comment on ULocalPlayer::GetGameLoginOptions (LocalPlayer.h:519-527) says that multiple
options are "concatenated together with an & between each key/value pair".

**The consequence.** The engine adds the returned string to the login URL as one option (LocalPlayer.cpp:287-291,
PendingNetGame.cpp:335-339). The URL is serialised with `?` before each option (URL.cpp:582-585); the server
parses it back and splits options at `?` only (URL.cpp:184-214), and UGameplayStatics::ParseOption also
splits at `?` only (GameplayStatics.cpp:3405-3430). With `&`, the server reads one option `OGBackend` whose
value is `<backend token>&OGWire=<wire token>`, and no `OGWire` option. Every login is then refused: a Jolt
server sees the same backend name with a different fingerprint, admits the backend with a misleading
fingerprint Warning, and refuses the login as "the client reports no wire format"; a Chaos server refuses it
as a different physics backend whose name contains the wire token. With `?` the two arrive as two options.

**What breaks if the tag moves.** The tag sits on the only statement that writes the separator. If the two
options are ever built in one function instead, the tag moves with the separator.
