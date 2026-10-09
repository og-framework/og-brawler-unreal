<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: FCoreDelegates::OnPostEngineInit -- Unreal Engine delegate (CoreDelegates.h), outside every scan root -->
<!-- lint-external-ref: FNetworkVersion::SetProjectVersion -- Unreal Engine function (NetworkVersion.cpp), outside every scan root -->
<!-- lint-external-ref: UEngine::Init -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
# Build identity — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/OGBuildIdentityUImpl.cpp`
(og-brawler-uploadtosteam task 15). Ids are opaque, stable, and retired rather than reused. The
reasoning lives in `OGBuildIdentity-rationale.md`.

The rules the compiler holds need no entry: the `build_info.txt` parse (BOM, CR, missing and invalid
`label=` lines), the label character set, and "the `dev` label never changes the network version" are
`static_assert`s in `OGBuildIdentityUImpl.h` (rationale §2, §3).

The physics-backend token (rationale §7, og-simulationscheduler-withjolt task 51) needs no entry either:
it is read from `PhysicsBackendUImpl.h`, the header that selects the backend, so the token and the
compiled host cannot disagree, and it reaches the network version only inside the labelled-build
branch that the `dev` assertion already covers.

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
