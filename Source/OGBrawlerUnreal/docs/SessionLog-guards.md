<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: JoinLinePattern -- a settings key of the og-tools host launcher (New-OgDedicatedServerLauncher), outside every scan root -->
<!-- lint-external-ref: ReadyLinePattern -- a settings key of the og-tools host launcher (New-OgDedicatedServerLauncher), outside every scan root -->
<!-- lint-external-ref: AGameModeBase::GetNumPlayers -- Unreal Engine method (GameModeBase.cpp), outside every scan root -->
<!-- lint-external-ref: AController::Destroyed -- Unreal Engine method (Controller.cpp), outside every scan root -->
# The server's session log — guards

Prohibitions with a tagged site in `Source/OGBrawlerUnreal/SessionLogUImpl.h` and
`SessionLogUImpl.cpp` (og-brawler-uploadtosteam task 13). Ids are opaque, stable, and retired rather
than reused. The reasoning lives in `SessionLog-rationale.md`.

The rules the compiler holds need no entry: every line starts with the shared prefix, only the
`joined`, `left` and `listening` lines start with those words after it, and the "above tested size"
line is logged once per upward crossing. All three are `static_assert`s in `SessionLogUImpl.h`
(rationale §3, §6). A seamless travel, which would skip the join log, stops a dedicated
server at start with a `checkf` (rationale §4).

---

## G-01 — the session lines are a parsed interface: keep their words, order and spacing

**Site:** the `inline constexpr SessionLineFormats kSessionLineFormats{...}` statement in
`SessionLogUImpl.h`.

The exact lines, as a dedicated server writes them (the prefix is the log category and verbosity):

```
LogOGSession: Display: OGBrawlerSession: build label=20260929-101500-abc1234
LogOGSession: Display: OGBrawlerSession: listening port=7777
LogOGSession: Display: OGBrawlerSession: joined players=4 tested=3 local=1
LogOGSession: Display: OGBrawlerSession: above tested size players=4 tested=3
LogOGSession: Display: OGBrawlerSession: left players=3 tested=3
```

**The prohibition.** ⛔ Do not reword, reorder, rename a key of, or add a field in the middle of the
`joined`, `left` or `listening` formats. ⛔ Do not change the `OGBrawlerSession: ` prefix. A new field may only be appended
at the end of a line, and only after the host launcher's patterns have been checked against it.

**The consequence.** These lines are read by a program, not only by people. The host launcher that ships
in the server depot (og-tools `New-OgDedicatedServerLauncher`, configured in this repo's
`tools/steam/steam-publish.psd1` by og-brawler-uploadtosteam task 17) tails the server log and matches
two regular expressions against it:

* `JoinLinePattern`, which must define the named groups `joined`, `left` and `players`, and may define
  `tested` and `local`. It prints "Player joined (4 players - above the tested 3, expect degraded
  performance)" from `players` and `tested`.
* `ReadyLinePattern`, whose first match means "the server is listening"; only then does the launcher
  offer to start the game on the host PC. If it never matches, the offer never appears (there is no
  timeout).

A changed word makes the join echo or the start offer silently disappear on every host PC. Nothing in
this repository fails: the patterns live in a config file and run in a PowerShell script. The
`above tested size` line must match neither `joined` nor `left`, or the launcher would count one join
twice; the `build label` line (task 15) must match neither pattern either. Both are now held by a
`static_assert`. The patterns tested against these lines are in `SessionLog-rationale.md` §6.

**What breaks if the tag moves.** The tag sits on the one statement that holds all five formats. The
prefix is pinned by a `static_assert`, so a prefix edit fails to compile, but the words after the prefix
are not.

---

## G-02 — the leave count skips the controller that is leaving

**Site:** the `if (controller == nullptr || controller == exiting || ...)` test in
`sessionLogUImpl::countPlayers`.

**The prohibition.** ⛔ Do not remove the `controller == exiting` test, and do not count players in
`Logout` with `AGameModeBase::GetNumPlayers` instead.

**The consequence.** The GameMode's `Logout` runs from `AController::Destroyed` **before** the world
removes the controller from its controller list (engine Controller.cpp). The leaving controller is therefore still in the iterator, still has its PlayerState, and would be counted. Every
`left` line would then report one player too many, and the "above tested size" line would re-arm one
leave late. `AGameModeBase::GetNumPlayers` has the same problem because it walks the same list.

**What breaks if the tag moves.** The tag sits on the one test every count goes through; `noteJoined`
passes no exiting controller, so the test is inert there.
