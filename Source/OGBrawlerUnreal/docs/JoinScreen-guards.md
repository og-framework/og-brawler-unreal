<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: UEngine::OnNetworkFailure -- Unreal Engine delegate accessor (Engine.h), outside every scan root -->
<!-- lint-external-ref: UEngine::OnTravelFailure -- Unreal Engine delegate accessor (Engine.h), outside every scan root -->
<!-- lint-external-ref: ETravelFailure::InvalidURL -- Unreal Engine enumerator (EngineBaseTypes.h), outside every scan root -->
<!-- lint-external-ref: UGameViewportClient::InputChar -- Unreal Engine method (GameViewportClient.cpp), outside every scan root -->
<!-- lint-external-ref: UGameViewportClient::InputKey -- Unreal Engine method (GameViewportClient.cpp), outside every scan root -->
# The join screen's UE layer — guards

Prohibitions with a tagged site in the join-screen files of `Source/OGBrawlerUnreal`:
`JoinScreenUImpl.cpp`, `OGBrawlerJoinSessionSubsystem.cpp`, `OGBrawlerFrontEndGameMode.cpp` and
`OGBrawlerGameViewportClient.cpp` (og-brawler-uploadtosteam task 12). Ids are opaque, stable, and
retired rather than reused. The reasoning lives in `JoinScreen-rationale.md`.

The rules the compiler holds need no entry: the front-end's truth table (`frontEndApplies`, a
`static_assert` in `OGBrawlerFrontEndGameMode.h`, rationale §2) and the numbers the scale console
variable's help text states (rationale §6). A local co-op key name that is not an engine key fails a
`checkf` at the first game start (rationale §7).

---

## G-01 — the front-end gate asks "launched by the editor?" through `IsPlayInPreview`, never `GIsEditor`

**Site:** the `m_frontEndActive = ... frontEndApplies(GetNetMode(), world->IsPlayInPreview())`
statement in `AOGBrawlerFrontEndGameMode::InitGame`.

**The prohibition.** ⛔ Do not drop the second argument, and do not replace it with `GIsEditor` (or any
other "is this the editor" test). ⛔ Do not add a condition that turns the front-end on for a world that
is not standalone.

**The consequence.** The user's daily workflow is new-process PIE: a separate dedicated server and two
clients launched as `127.0.0.1:17777 -game -PIEVIACONSOLE`. When that server never comes up, the
client's connection times out and the engine loads the default map with `LocalMapOptions`, which
selects this GameMode. Today that path lands in a standalone gameplay world with a pawn. `IsPlayInPreview`
(true under `-PIEVIACONSOLE`) keeps it so. `GIsEditor` is **false** in those client processes and in
`UnrealEditor.exe -game`, so it would show the join screen to PIE clients, and a gate that used it the
other way round would hide the screen from the hand-launched `-game` test. A world that is not
standalone (dedicated server, listen server, client) has a real game to run; showing the join screen
there would replace it.

**What breaks if the tag moves.** The tag sits on the only statement that decides the gate. Deleting
the statement takes the tag with it and orphans this entry. The truth table of `frontEndApplies` itself
is pinned by a `static_assert`, so an edit inside the function fails to compile.

---

## G-02 — a front-end reached through a failure return never auto-joins

**Site:** the `if (reachedViaFailureReturn)` branch in
`UOGBrawlerJoinSessionSubsystem::noteFrontEndShown`.

**The prohibition.** ⛔ Do not let the pending command-line auto-join (or any other automatic travel)
run when the front-end was loaded with the `closed` or `failed` URL option. ⛔ Do not move this branch
below the auto-join branch.

**The consequence.** A failed connection sends the engine back to the default map with `?closed`,
which is the front-end again. If the front-end then re-issued the command-line join, a server that is
down would be retried forever and the player would never see the failure or the field. The model
already ignores the token on a failure return at `start`; this branch is the same rule for the one
case the model cannot see: a Shipping auto-join whose travel was issued by the adapter.

**What breaks if the tag moves.** The tag is on the branch that must come first. Deleting it orphans
this entry.

---

## G-03 — the recent list is never saved from an editor-launched session

**Site:** the `if (editorLaunched() || GConfig == nullptr)` early return in
`joinScreenUImpl::saveRecentAddresses`.

**The prohibition.** ⛔ Do not remove the `editorLaunched()` test, and do not add a second write of the
`[JoinScreen]` section that bypasses this function.

**The consequence.** Every PIE client and every in-editor session would write its address
(`127.0.0.1:17777` for the user's PIE settings) into the player's recent list, so the list a real
playtest shows would fill with editor ports. The backlog requires that PIE leaves the list unchanged.
(New-process PIE clients also get a per-instance PIEGameUserSettingsN.ini from the editor, but the
editor-launched test is what keeps every editor path out, including in-process PIE.)

**What breaks if the tag moves.** This function is the one writer; the tag sits on its refusal.

---

## G-04 — the failure delegates are bound in `Initialize`, not in a GameMode or HUD

**Site:** the `GEngine->OnNetworkFailure().AddUObject(...)` statement in
`UOGBrawlerJoinSessionSubsystem::Initialize`.

**The prohibition.** ⛔ Do not move the binding of `UEngine::OnNetworkFailure` /
`UEngine::OnTravelFailure` into the front-end GameMode, the HUD or any other per-world object.

**The consequence.** The T10 spike bound them in a GameMode's `InitGame` and **missed the first
failure** of a client started with an address: that failure fires on the pending net driver before any
front-end world exists. A per-world binding also dies with the world at every map load, while the join
attempt (and its first failure, which the model latches) spans the front-end map, the pending
connection and the return. The game-instance subsystem lives for the whole process.

**What breaks if the tag moves.** A move to another object deletes this statement, which takes the
tag with it and orphans the entry.

---

## G-05 — the join screen yields every key and character to an open console

**Site:** the `if (ViewportConsole != nullptr && ViewportConsole->ConsoleActive())` test in
`UOGBrawlerGameViewportClient::joinSessionTakingInput`.

**The prohibition.** ⛔ Do not remove the console test, and do not let `InputKey`/`InputChar` consume
input before asking `joinSessionTakingInput`.

**The consequence.** `UGameViewportClient::InputChar` routes characters only to the console, and
`UGameViewportClient::InputKey` is where the console key opens it. Without the test, once the console
is open every character typed into it would also be typed into the address field, and Enter, Backspace
and the arrow keys would edit the address instead of the console line. The console is the
development tool for every other diagnostic in this project.

**What breaks if the tag moves.** The tag is on the one test both handlers go through.

---

## G-06 — the travel URL names its protocol: `unreal://` + the canonical address

**Site:** the `const FString travelUrl = FString(TEXT("unreal://")) + ...` statement in
`UOGBrawlerJoinSessionSubsystem::travelTo`.

**The prohibition.** ⛔ Do not travel to the bare canonical address (`host:port`), and do not strip the
prefix to "simplify" the URL.

**The consequence.** The engine's URL parser reads `word:rest` as a protocol and a remainder whenever
the part before the first `:` has no dot (engine URL.cpp:359). A single-label host name with a port,
such as `localhost:7777` or a LAN machine name, then becomes protocol `localhost` and map `7777`: the
travel fails at once with `ETravelFailure::InvalidURL` ("Invalid URL: /Game/ThirdPerson/Maps/
ThirdPersonMap"). Measured on 2026-09-29 before this prefix existed. IPv4 addresses and dotted host names
parse either way, so only a test with a dot-less host name shows the defect. An explicit `unreal://`
is parsed as the protocol first, and everything `parseServerAddress` accepts then reaches the host and
port fields.

**What breaks if the tag moves.** The tag sits on the only statement that builds the travel URL.
