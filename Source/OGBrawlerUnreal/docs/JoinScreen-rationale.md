<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: AHUD::DrawText -- the engine's own HUD draw call; owned by the host engine, not by this repository -->
<!-- lint-external-ref: AHUD::DrawRect -- the engine's own HUD rectangle call; owned by the host engine, not by this repository -->
<!-- lint-external-ref: AHUD::GetTextSize -- the engine's own HUD text measure; owned by the host engine, not by this repository -->
<!-- lint-external-ref: UEngine::GetSmallFont -- the engine's own fixed-size debug font; owned by the host engine, not by this repository -->
<!-- lint-external-ref: UGameViewportClient::InputChar -- Unreal Engine method (GameViewportClient.cpp), outside every scan root -->
<!-- lint-external-ref: UGameViewportClient::InputKey -- Unreal Engine method (GameViewportClient.cpp), outside every scan root -->
<!-- lint-external-ref: UGameInstance::GetMapOverrideName -- Unreal Engine method (GameInstance.cpp), outside every scan root -->
<!-- lint-external-ref: UWorld::IsPlayInPreview -- Unreal Engine method (World.cpp), outside every scan root -->
<!-- lint-external-ref: UEngine::SetClientTravel -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
<!-- lint-external-ref: FPlatformApplicationMisc::ClipboardPaste -- Unreal Engine ApplicationCore function, outside every scan root -->
<!-- lint-external-ref: FCoreUObjectDelegates::PostLoadMapWithWorld -- Unreal Engine delegate (UObjectGlobals.h), outside every scan root -->
<!-- lint-external-ref: UEngine::OnNetworkFailure -- Unreal Engine delegate accessor (Engine.h), outside every scan root -->
<!-- lint-external-ref: UEngine::OnTravelFailure -- Unreal Engine delegate accessor (Engine.h), outside every scan root -->
<!-- lint-external-ref: FKey::IsValid -- Unreal Engine InputCore method, outside every scan root -->
<!-- lint-external-ref: UEngine::CancelPending -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
<!-- lint-external-ref: UGameViewportClient::MaxSplitscreenPlayers -- Unreal Engine config property (GameViewportClient.h), outside every scan root -->
<!-- lint-external-ref: UGameInstance::RemoveLocalPlayer -- Unreal Engine method (GameInstance.cpp), outside every scan root -->
<!-- lint-external-ref: APlayerController::OnNetCleanup -- Unreal Engine method (PlayerController.cpp), outside every scan root -->
<!-- lint-external-ref: UChildConnection::CleanUp -- Unreal Engine method (NetConnection.cpp), outside every scan root -->
<!-- lint-external-ref: UChildConnection::HandleClientPlayer -- Unreal Engine method (NetConnection.cpp), outside every scan root -->
<!-- lint-external-ref: NetPlayerIndex -- Unreal Engine APlayerController member (PlayerController.h), outside every scan root -->
<!-- lint-external-ref: BadChildConnectionIndex -- Unreal Engine ENetCloseResult enumerator (NetCloseResult.h), outside every scan root -->
# The join screen's UE layer — rationale

Companion to the pure `og-brawler` header that holds the join screen's logic, texts and layout
(`OGBrawler/BrawlerJoinScreen.h`, with its own `BrawlerJoinScreen-rationale.md` in the og-brawler docs
tier), and to the `Source/OGBrawlerUnreal` files that turn it into a front-end
(og-brawler-uploadtosteam task 12):

| file | role |
|---|---|
| `JoinScreenUImpl.h` / `JoinScreenUImpl.cpp` | the adapter: console variable, string conversion, command-line address, persistence, the build label, the local co-op keys |
| `OGBrawlerFrontEndGameMode.h` / `OGBrawlerFrontEndGameMode.cpp` | decides whether this world is the front-end |
| `OGBrawlerJoinSessionSubsystem.h` / `OGBrawlerJoinSessionSubsystem.cpp` | holds the model for the whole process, travels, hears success and failure |
| `OGBrawlerGameViewportClient.h` / `OGBrawlerGameViewportClient.cpp` | typed characters, keys and gamepad buttons while the screen is up |
| `OGBrawlerUEHUD.cpp` (`drawJoinScreen`) | the canvas calls |
| `Config/DefaultEngine.ini` | three lines that select the GameMode and the viewport client |

**The source files carry the guards (`JoinScreen-guards.md`); this file carries the reasoning.** The
design is the T10 spike's, ruled binding by the user on 2026-09-29; the section numbers below are cited
by `static_assert` messages in the source.

---

## 1. The split

Everything a test can reach is in the pure header: the edit buffer, `parseServerAddress`, the
recent-address list and its one-line form, the command-line token rule, the phases
(Editing/Connecting/Joined/Failed), every text, the focus model, the inks and the placed layout. It is
covered by the `[BrawlerJoinScreen]` Catch2 cases in og-brawler-tests.

The UE layer holds only what needs the engine: when the screen exists, where input comes from, the
travel call, the success and failure signals, the ini file and the canvas. The scoreboard
(`ScoreboardDisplay-rationale.md`) has the same split, and the HUD draws both in the same style
(`AHUD::DrawText`, `AHUD::DrawRect`, `AHUD::GetTextSize`, `UEngine::GetSmallFont`). There is no UMG,
no Slate widget and no new binary asset.

## 2. When the front-end exists

Three `DefaultEngine.ini` lines, and no new map:

* `[/Script/EngineSettings.GameMapsSettings] LocalMapOptions=?game=OGFrontEnd`. The engine appends
  `LocalMapOptions` to the default map only when the command line names no map or address (engine
  GameInstance.cpp:671, `UGameInstance::GetMapOverrideName`), and again on every return to the
  default map after a failure (engine UnrealEngine.cpp:15278, "Connection failed; returning to
  Entry").
* `+GameModeClassAliases=(Name="OGFrontEnd",GameMode="/Script/OGBrawlerUnreal.OGBrawlerFrontEndGameMode")`,
  so `?game=OGFrontEnd` selects `AOGBrawlerFrontEndGameMode`.
* `[/Script/Engine.Engine] GameViewportClientClassName=/Script/OGBrawlerUnreal.OGBrawlerGameViewportClient`
  (§3). The engine default is `/Script/Engine.GameViewportClient` (engine BaseEngine.ini:126).

`AOGBrawlerFrontEndGameMode` **is** an `AOGBrawlerUEGameMode`, so a world that selected it but is not
the front-end behaves exactly like the gameplay GameMode. It becomes the front-end only when
`frontEndApplies(GetNetMode(), IsPlayInPreview())` holds: a standalone world that was not launched by
the editor. The whole truth table is a `static_assert` beside the function. When active, it spawns no
pawn and uses a plain `APlayerController`, so the gameplay controller's Tab/Insert bindings are not
live on the join screen. The HUD class stays `AOGBrawlerUEHUD`.

Who never sees it:

* **A dedicated server** has a map on its command line (`playtest_server.bat`), and even without one
  it is not standalone.
* **A client launched with an address** (`playtest_client.bat`, the Android client, PIE clients)
  browses to the address directly. `LocalMapOptions` is not appended, so the join screen never loads
  in Development or Editor builds.
* **New-process PIE** (`-PIEVIACONSOLE`): `UWorld::IsPlayInPreview` is true. Even when the separate
  server never comes up and the engine falls back to the default map with `?game=OGFrontEnd`, the
  GameMode stays inert and the client lands in a standalone gameplay world with a pawn, as it did
  before this task (measured, impl notes). ⛔G-01.

## 3. Input — a game viewport client, not a widget

Typed characters reach `UGameViewportClient::InputChar` only; the engine forwards them to the console
and nowhere else, so no player controller or Enhanced Input action ever sees them, and a key-down
event cannot tell `:` from `;`. `UOGBrawlerGameViewportClient` overrides `InputChar` and
`UGameViewportClient::InputKey`. While the front-end is shown and the console is closed (⛔G-05), it
hands characters and keys to the session subsystem. Everything it does not handle goes to `Super`,
unchanged: the console key, Alt+Enter, and every key in every non-front-end world, PIE included.

| input | effect |
|---|---|
| a character | typed into the field if the model allows it (`[0-9A-Za-z.:-]`) |
| Backspace, Delete, Left, Right, Home, End | edit the field / move the caret |
| Ctrl+V | paste the clipboard, trimmed, all or nothing |
| Up/Down, gamepad D-pad or left stick up/down | move focus list → field → Join |
| Enter, gamepad A (`Gamepad_FaceButton_Bottom`) | join the focused list entry, or the field |
| Escape, gamepad B (`Gamepad_FaceButton_Right`) | while Connecting only: cancel the attempt (§13); otherwise not handled |

Keys act on press and on auto-repeat. Ctrl is read from the viewport's key state, so no Slate
dependency is needed. `FPlatformApplicationMisc::ClipboardPaste` lives in the `ApplicationCore`
module. The monolithic Client target links it through the engine anyway, but the modular Editor target
does not, so `ApplicationCore` is a private dependency in `OGBrawlerUnreal.Build.cs` (the spike
measured the link error without it).

## 4. The session subsystem

`UOGBrawlerJoinSessionSubsystem` is a game-instance subsystem, created on every process except a
dedicated server. It owns the one `JoinScreenModel` for the process, because the join attempt spans
map loads: front-end → pending connection → the server's map, or → failure → the front-end again.
Anything per-world would forget it at each load. It needs no change to the game instance class setting.

* **Start.** `Initialize` loads the recent list (§8) and builds the model with
  `JoinScreenModel::start(recents, commandLineAddress, false)`. With a valid address on the command
  line the model starts in Connecting.
* **Auto-join (Shipping).** A Shipping client drops the command-line address before the engine
  browses, so it loads the front-end instead. `m_autoJoinPending` remembers that the model began in
  Connecting with no travel yet; when the front-end is shown **without** a failure option it travels
  to the model's target. In Development the engine has already travelled and the front-end never
  loads first, so the flag is cleared by the success or failure that follows. A front-end reached
  through a failure return (`closed`/`failed`) never auto-joins (⛔G-02).
* **Failure.** `UEngine::OnNetworkFailure` and `UEngine::OnTravelFailure` are bound in `Initialize`
  (⛔G-04). Every failure is handed to `noteJoinFailed`, which keeps the **first** failure of the
  attempt, with the reason that `joinScreenUImpl::networkFailureReason` or
  `joinScreenUImpl::travelFailureReason` gives for the engine's failure type (§13), and the engine's
  error text. A failure return with no failure event before it (not seen in any run) is recorded as
  Unknown, so the screen never sits in Connecting.
* **Success.** `FCoreUObjectDelegates::PostLoadMapWithWorld` for a world of this game instance whose
  net mode is `NM_Client` means the server accepted the login and its map loaded.
  `noteJoinSucceeded(remember)` moves the model to Joined and, unless the session was launched by the
  editor, adds the address to the recent list, which is then saved.

Every step logs one `LogOGJoinScreen` line starting with `OGJoinScreen:` (session start, front-end
shown, joining, failure, joined). The impl notes quote them as evidence.

## 5. Travel

`GEngine->SetClientTravel(world, "unreal://" + address, TRAVEL_Absolute)`, via `UEngine::SetClientTravel`.

* **Absolute, not Partial:** a partial travel copies the front-end URL's options (`?game=OGFrontEnd`)
  into the login URL.
* **The explicit protocol** is ⛔G-06: without it a dot-less host name with a port (`localhost:7777`)
  is parsed as protocol `localhost`, and the travel fails as an invalid URL.

The engine's own command-line path has the same limit: `-game localhost:7777` in a Development build
does not connect. Use an IPv4 address or a dotted name on the command line. The join screen and the
Shipping auto-join are not affected, because both travel through `travelTo`.

## 6. The console variable

`OGBrawler.JoinScreenScale` (default 1.0, clamped to [0.5, 4] when read) multiplies the model's
layout. The model already scales with the window height (1.0 is sized for a 720-pixel-high window)
and then shrinks the panel to fit the window, so the first screen a player sees is legible at 1080p
and 4K without touching the variable. `static_assert`s tie the help text's numbers to the header's
constants. The screen has no on/off variable: it is the game's front door, not a diagnostic.

## 7. The local co-op keys — one source for the hint and the binding

The join screen's hint ("After joining: Tab adds a local player, Insert removes one") is written from
`brawlerJoinScreen::kLocalCoopKeyNames`. `AOGBrawlerPlayerController::SetupInputComponent` now binds
`JoinLocalPlayer` and `LeaveLocalPlayer` to `joinScreenUImpl::localCoopAddPlayerKey()` and
`joinScreenUImpl::localCoopRemovePlayerKey()`, which build the `FKey` from those same names. The hint
and the binding therefore cannot name different keys. A name that is not an engine key (wording such as
"the Tab key") fails `FKey::IsValid` in a `checkf` at the first gameplay player controller's input setup, so the
og-brawler guard G-01 is now checked mechanically in every Development and Editor run. `checkf` is
compiled out of Shipping, where an invalid name would leave the key unbound, but that build is only
made after Development runs have passed.

## 8. The recent list on disk

One line, `RecentAddresses=<a>,<b>,...` (the model's own serialisation), in the `[JoinScreen]` section
of the user-scope GameUserSettings.ini: `Saved/Config/WindowsEditor/` for `UnrealEditor.exe -game`,
and the per-user saved folder for a packaged client. It is read once at session start, written only
on a successful join that changed the list, and flushed at once. Hand edits are loaded through the same
filter as saves: invalid entries, duplicates and "This PC" are dropped, and at most five are kept.

`saveRecentAddresses` refuses in editor-launched sessions (`GIsEditor`, or `-PIEVIACONSOLE`, ⛔G-03).
The PIE regression therefore leaves the list byte-for-byte unchanged. New-process PIE also points each
process at its own PIEGameUserSettingsN.ini (engine PlayLevelNewProcess.cpp:103), a second, weaker
barrier.

## 9. The build label

`joinScreenUImpl::ownBuildLabel()` returns `buildIdentityUImpl::buildLabel()` (task 15): the
`-OGBuildLabel=` value, else the `label=` of `build_info.txt` in the ContentRoot, else `dev`
(`OGBuildIdentity-rationale.md` §1-2). It is the one source for the "Build:" line and for the
different-build failure text. The same label is part of the network version, so a client and a server
with different labels fail with `OutdatedClient` and this screen says "Different build"
(`OGBuildIdentity-rationale.md` §3-4).

## 10. Simulating a Shipping client without a Shipping build

In Development and Editor builds the engine consumes the command-line address, so the Shipping
auto-join path (§4) cannot be reached by passing an address. `-OGFrontEndAddress=<host:port>` is read
by `joinScreenUImpl::commandLineAddress` only when the command line has no address token, and only in
non-Shipping builds (`#if !UE_BUILD_SHIPPING`). The engine then loads the front-end, and the adapter
behaves exactly as a Shipping client whose address the engine dropped: front-end shown → Connecting →
travel. Example: `OGBrawlerUnrealClient.exe -OGFrontEndAddress=127.0.0.1:7777`.

## 11. The draw

`AOGBrawlerUEHUD::DrawHUD` asks `UOGBrawlerJoinSessionSubsystem::forShownFrontEnd` first. While the
front-end is shown it draws only the join screen and returns: the front-end has no character, no
simulation manager and no match. In every other world the cost is the null check. `drawJoinScreen`
places everything from `placedJoinScreenLayout`: the backdrop, the title, the build line, one row per
list entry ("This PC" shows its address beside the label), the field with a caret measured from
`textBeforeCursor`, the Join button, the two status lines in the tone's ink and the hint. The focused
row gets a bar at its left edge as well as the focus ink, so focus does not rest on colour alone.

## 12. What later tasks extend

Nothing is pending. Task 15 filled `ownBuildLabel()` (§9).

## 13. Failure reasons and cancelling a join (task 13)

**The mapping** lives in two `constexpr` functions in `JoinScreenUImpl.h`. Four `static_assert`s
beside them pin every row below, so an edit that moves a row fails to compile, with the row's origin
in the message. The subsystem's `handleNetworkFailure` passes whether the failing driver is the
pending net driver (`NetDriverName == NAME_PendingNetDriver`); `handleTravelFailure` needs no driver.

| engine event | reason |
|---|---|
| `ENetworkFailure::OutdatedClient`, `ENetworkFailure::OutdatedServer`, `ETravelFailure::PackageVersion` | DifferentBuild |
| `ENetworkFailure::ConnectionTimeout` or `ENetworkFailure::ConnectionLost` on the **pending** driver | CannotReachServer |
| the same two on the **game** driver (the client was in the match) | ConnectionLost |
| `ENetworkFailure::FailureReceived`, `ENetworkFailure::PendingConnectionFailure` | ServerRefused (the engine text is the server's) |
| the three net-driver creation failures; `ETravelFailure::PendingNetGameCreateFailure` (unresolvable host), `ETravelFailure::InvalidURL` | CannotReachServer |
| the two net GUID/checksum mismatches, every other travel failure | Unknown |

The model keeps the **first** failure of an attempt, and the order in which the engine reports
failures is what makes the table right:

* **A version mismatch** reports `OutdatedClient` first and then `PendingConnectionFailure` with the
  same text (T10 spike, measured). The first one wins, so the screen says "Different build".
* **A refusal during the login** is the one row where the T10 spike was wrong. The spike put
  `PendingConnectionFailure` under "can't reach server" and expected a refusal to arrive as
  `FailureReceived`. Measured on 2026-09-29 with a server started as `ThirdPersonMap?MaxPlayers=1`:
  the second client logs `PendingConnectionFailure, ErrorString = Server full.` and no failure before
  it, and the screen shows "Server refused: Server full.". The engine's pending net game stores the
  server's `NMT_Failure` text as its connection error, and the engine's travel tick reports that error
  as `PendingConnectionFailure`. `FailureReceived` is only reported for an `NMT_Failure` that arrives
  **after** the login, on the game driver (for example a refused split join). Every other pending cause
  (timeout, version, driver creation, an unresolvable host) reports its own failure first, and that one
  is latched before the `PendingConnectionFailure` that follows. One engine source of a first
  `PendingConnectionFailure` remains: a server that closes the pending connection without a text
  ("Your connection to the host has been lost."). The screen then shows it as a refusal, which is
  acceptable, because the server did end the attempt.
* **A timeout** is the same failure type while connecting and while playing; only the driver differs
  (T10 spike r6/r7). Measured again: `Driver: Name:PendingNetDriver` after 20 s for a black-hole
  address, and `Name:GameNetDriver` after 60 s when the server was killed.

**Cancelling.** Escape or gamepad B (`Gamepad_FaceButton_Right`) while the model is Connecting calls
`JoinScreenModel::cancel`. It then clears the world context's pending travel URL (a join issued this
frame has not browsed yet) and calls `UEngine::CancelPending`, which closes the pending connection and
destroys the pending net driver without reporting a failure. A wrong address therefore no longer waits
out the engine's 20 s connect timeout. Measured: Escape sent at 08:30:26.059 UTC, `OGJoinScreen:
cancelled` logged at 08:30:26.061, and no failure or map load in the 30 s after it. In any other phase
the key is not handled and goes to the engine as before.

## 14. Local players on a network client (task 13)

**Adding: the limit notice.** The engine refuses a client's 5th local player on the client itself
(`UGameViewportClient::MaxSplitscreenPlayers`, 4). `AOGBrawlerPlayerController::JoinLocalPlayer`
compares the local-player count before and after `UGameplayStatics::CreatePlayer`. An unchanged count
at the limit means the player was refused, and the subsystem then records the time and the limit.
`AOGBrawlerUEHUD` draws `localPlayerLimitNoticeText(limit)` near the top of the screen while
`localPlayerLimitNoticeVisible` holds (4 s), from the first local player's HUD only.
⚠ `CreatePlayer`'s return value cannot decide it. On a network client it returns null **even on
success**, because the server spawns the new player's controller and it arrives later. The first
version tested the return value and showed the notice for the successful 4th player (measured).

**Removing: Insert.** User ruling (2026-09-29): Insert removes the highest-numbered local player, and
local player 0 is never removed. Keyboard input goes to local player 0, so Insert always arrives on
its controller. Two engine facts shape the code:

* **The engine does not tell the server.** `UGameplayStatics::RemovePlayer` ends in
  `UGameInstance::RemoveLocalPlayer`, which destroys the controller only where it is the authority.
  On a network client it drops the local player and nothing else, so the server kept the character.
  Measured before this change: Insert produced no `left` line on the server. The engine itself notes
  that split players leaving a match is not supported (a comment in `APlayerController::OnNetCleanup`).
  So the leaving controller first sends `ServerLeaveLocalPlayer`, a reliable server RPC. The server
  removes that split connection the way a disconnect removes each child connection: out of the parent
  connection's `Children`, then `UChildConnection::CleanUp`, which destroys the controller and its pawn
  and calls the GameMode's `Logout`. The `left` line and the ring-out slot release follow from that.
  The engine's replay driver removes split viewers the same way.
* **Only the LAST local player can go.** Client and server pair a split player's controller with its
  local player by array position. The server gives each split controller a `NetPlayerIndex` equal to
  its position in the parent connection's `Children`. The client hands a received controller to the
  local player at that position (the engine's `UChildConnection::HandleClientPlayer`). Removing a
  middle player would pair the next joiner with the wrong local player, and the client then closes the
  whole connection (`BadChildConnectionIndex`). So Insert always removes the last local player,
  whichever local controller received it, and the server refuses a leave from any child that is not
  the last of its connection. The earlier "a local player other than 0 removes itself" path is gone:
  no key reaches it (gamepads have no Insert key, and the console runs on local player 0), and over
  the network it would have broken the pairing.

Measured (editor `-game`, two clients against a `playtest_server.bat`-shaped server): Insert on client
1 gave server `left players=4`; Tab again gave `joined players=5 local=3`, with spawn slot 2 reused.
Client 2 went from 4 local players to 1 with three Inserts (a `left` line each), and a fourth Insert
logged `no other local player to remove`; a Tab after that joined cleanly. Neither client logged
`BadChildConnectionIndex` or a network failure. After each leave the server logs a few
`[InputDrop] ... never released` warnings for the removed slot's parked input. They stop after about
ten ticks; they were not investigated further.
