<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: UWorld::Listen -- Unreal Engine method (World.cpp), outside every scan root -->
<!-- lint-external-ref: UEngine::LoadMap -- Unreal Engine method (UnrealEngine.cpp), outside every scan root -->
<!-- lint-external-ref: ReadyLinePattern -- a settings key of the og-tools host launcher (New-OgDedicatedServerLauncher), outside every scan root -->
<!-- lint-external-ref: AController::Destroyed -- Unreal Engine method (Controller.cpp), outside every scan root -->
# The server's session log — rationale

`SessionLogUImpl.{h,cpp}` write one line when a dedicated server starts listening, one per player who
joins or leaves, and one each time the session grows past the tested size (og-brawler-uploadtosteam
task 13). The host launcher that ships in the server depot parses them. Prohibitions:
`SessionLog-guards.md`.

---

## 1. What is logged, and where

| event | line (after the `LogOGSession: Display: ` log prefix) | called from |
|---|---|---|
| the server starts a map (task 15) | `OGBrawlerSession: build label=<label>`, right before the `listening` line | `AOGBrawlerUEGameMode::BeginPlay` |
| the server listens | `OGBrawlerSession: listening port=<port>` | `AOGBrawlerUEGameMode::BeginPlay` |
| a player joins | `OGBrawlerSession: joined players=<n> tested=<t> local=<l>` | `AOGBrawlerUEGameMode::PostLogin` |
| the join takes players from `<= t` to `> t` | `OGBrawlerSession: above tested size players=<n> tested=<t>`, right after the `joined` line | the same call |
| a player leaves | `OGBrawlerSession: left players=<n> tested=<t>` | `AOGBrawlerUEGameMode::Logout` |

* `players` = the players on the server after the event: player controllers that have a PlayerState
  and are not spectators, across every connection, **including the split (local-player) connections**.
  In this game every such controller gets exactly one character. At `PostLogin` the pawn does not exist
  yet, so counting controllers is the right measure (T10 spike Q6).
* `local` = those of them on the joining player's **client**: the connection's own controller and every
  split player on it. A split player's connection is a `UChildConnection`, and its `Parent` is the
  client's connection, so both sides are compared by that root connection.
* `label` = the build label (`OGBuildIdentity-rationale.md` §1): the published build's label, or `dev`.
* `tested` = `og::brawler::session::maxPlayersPerServer` (3). It is information only. ⛔ Nothing rejects
  a player for being over it (NO HARD PLAYER CAP, user ruling 2026-09-26); the session keeps running
  and the launcher says "above the tested 3, expect degraded performance".
* The engine's own limits are separate and unchanged: 4 local players per client, refused on the
  client (the join screen's notice, `JoinScreen-rationale.md` §14), and 16 players per server.
* Only a **dedicated** server logs (`NM_DedicatedServer`). A client and the front-end are standalone
  or client worlds and log nothing. The user's PIE server is a dedicated server too, so PIE logs these
  lines; that is harmless.
* The category is `LogOGSession` at `Display`, so the lines are in the server log (`-log`, and the
  launcher's `-abslog` file) with no ini change.

Every player-controller join and leave goes through these two GameMode calls. The engine spawns a split
player's controller after `NMT_JoinSplit` through the world's play-actor spawn, which calls Login and `PostLogin`
(T10 spike Q7); a controller's destruction calls `Logout` (`AController::Destroyed`). That
covers a client that connects or disconnects, Tab, and Insert (the server-side leave in
`JoinScreen-rationale.md` §14). Measured: a client closed with 1 player gave `left players=0`.

## 2. The measured sequence

Editor `-game`, a `playtest_server.bat`-shaped server on port 7777, 2026-09-29 (`Saved/T13/c_server.log`,
git-ignored):

```
OGBrawlerSession: listening port=7777
OGBrawlerSession: joined players=1 tested=3 local=1      client 1 joins
OGBrawlerSession: joined players=2 tested=3 local=2      client 1, Tab
OGBrawlerSession: joined players=3 tested=3 local=3      client 1, Tab
OGBrawlerSession: joined players=4 tested=3 local=1      client 2 joins
OGBrawlerSession: above tested size players=4 tested=3
OGBrawlerSession: joined players=5 tested=3 local=2      client 2, Tab
OGBrawlerSession: left players=4 tested=3                client 1, Insert
```

Nobody was rejected or disconnected.

## 3. "Above tested size" — once per upward crossing

`aboveTestedAfter(wasAbove, players, tested)` returns whether the session is now above the tested size,
and whether this event crossed from `<= tested` to `> tested`. The line is logged only on that crossing.
A `left` line that brings the count back to `<= tested` re-arms it, so a session that shrinks to 3 and
grows to 4 again logs the line again (measured: `left players=3`, then `joined players=4` and a second
`above tested size` line). A `static_assert` beside the function pins the four cases.

## 4. Seamless travel would skip the join log

`PostLogin` is not called for players that a seamless travel carries into the new map; the engine calls
`AGameModeBase::HandleSeamlessTravelPlayer` for them instead (T10 spike Q6;
`SimmableUpdateComponent-rationale.md` records the same caveat for another reason). The project has no
server travel today. So `noteListening` stops a dedicated server at start with a `checkf` when the
GameMode's `bUseSeamlessTravel` is set, and its message says what to extend first. A check, not a
comment: the flag is the one edit that would make the counts silently wrong.

## 5. The ready line

The engine calls `UWorld::Listen` in `UEngine::LoadMap` **before** the world's `BeginPlay`, so the
GameMode's `BeginPlay` is the first point where the net driver is listening, and the line is written
there. The port is the net driver's local address port; if the driver has no address, the world URL's
port is used. The engine also writes `IpNetDriver listening on port 7777`, but its wording is the
engine's and can change with an engine upgrade. Task 17's `ReadyLinePattern` uses this line instead.

## 6. The patterns the host launcher uses (for task 17)

Tested on 2026-09-29 with .NET `[regex]` in both Windows PowerShell 5.1 and pwsh 7.6 against the 17
session lines of `Saved/T13/c_server.log` (a run with 2 clients, Tab and Insert). Both shells found 9
`joined`, 5 `left`, 1 ready line, and ignored the 2 `above tested size` lines:

```
JoinLinePattern  = 'OGBrawlerSession: (?:(?<joined>joined)|(?<left>left)) players=(?<players>\d+) tested=(?<tested>\d+)(?: local=(?<local>\d+))?'
ReadyLinePattern = 'OGBrawlerSession: listening port=(?<port>\d+)'
```

The join pattern defines the three groups the launcher requires (`joined`, `left`, `players`) and the
two optional ones (`tested`, `local`). The `above tested size` line matches neither pattern, which is
what the launcher expects: it derives the "above the tested N" wording from `players` and `tested` on
the join line itself.

The `build label` line (task 15) matches neither pattern either. Tested on 2026-09-29 in both shells
against five task 15 server logs (labels `A`, `dev`, `20260929-101500-abc1234` and its `-dirty`
form): all 5 build lines were ignored, while the same run still found the 2 `joined` and 5 ready lines.
A `static_assert` in `SessionLogUImpl.h` keeps the words after the prefix apart: only the `joined`,
`left` and `listening` lines may start with those words.
