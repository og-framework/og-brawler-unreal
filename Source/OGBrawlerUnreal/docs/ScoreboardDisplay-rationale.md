<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: AHUD::DrawText -- the engine's own HUD draw call; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: AHUD::GetTextSize -- the engine's own HUD text measure; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: AHUD::DrawRect -- the engine's own HUD rectangle call; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: UEngine::GetSmallFont -- the engine's own fixed-size debug font; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: TActorIterator -- the engine's own actor iteration template; owned by the host engine, not by this repository, and must not resolve here -->

# The ring-out scoreboard — rationale

Companion to the pure `og-brawler` header that holds the board's logic —
`OGBrawler/BrawlerScoreboardVisualization.h` — and to the three
`Source/OGBrawlerUnreal` files that gate, gather and draw it:
`Source/OGBrawlerUnreal/ScoreboardVisualizationUImpl.h`,
`Source/OGBrawlerUnreal/ScoreboardVisualizationUImpl.cpp` and
`Source/OGBrawlerUnreal/OGBrawlerUEHUD.cpp`.
**The source files carry the guards; this file carries the reasoning.**

Sibling document: `Source/OGBrawlerUnreal/docs/InputHistoryDisplay-rationale.md`, which
this board's UE layer is modelled on name for name. Where the two displays differ, the
difference is stated here rather than left to be inferred from two files side by side.

---

## 0. Why the record sits in this tier

Three docs tiers exist and `doc_anchor_lint.ps1` now discovers all three, so "which tier is
linted" — decision D9's discriminator when the input-history display chose this one — no
longer settles it. Two things do:

* **Not the `og-brawler` docs tier**, though it is the same BUSL-1.1 licence and would
  otherwise be the natural home for a document about a pure header. That tier travels with
  the `og-brawler` submodule, and **every load-bearing claim below is about files that are
  not in it**: three console variables registered in `Source/OGBrawlerUnreal`, a join
  across a UE actor and a UE-side manager, and a draw method on `AOGBrawlerUEHUD`. A reader
  who checked out only `og-brawler` would be holding a document whose subject is absent.
  The pure header keeps its own reasoning in its own banner, where it belongs.
* **Here.** `Source/OGBrawlerUnreal/docs` already holds this initiative's other rationale
  (`SimulationManagerUImpl-rationale.md`, whose §1 carries the score push's threading
  argument) and the sibling display's. One feature, one tier.

⛔ **The document is not split across both tiers.** The backlog offered that option
explicitly; a fact asserted in two places is the broken join `doc_anchor_lint.ps1` was
built to catch, and this document would have been the first to demonstrate it.

---

## 1. The split — what is pure and what could not be

`Source/OGBrawlerTests` links `{ Core, OGSimulation, OGBrawler }` and **not**
`OGBrawlerUnreal`, so every line in the two `ScoreboardVisualizationUImpl` files is
unreachable from the low-level test target by construction. That is not a reason to write
untested logic; it is the reason the split falls exactly where it does.

| decision | where it lives | reachable by a Catch2 case |
|---|---|---|
| row layout, column edges, row height | `BrawlerScoreboardVisualization.h` | yes |
| the ordering | `orderedScoreboardRows` | yes |
| both console clamps | `clampScoreboardScale`, `clampScoreboardBackgroundAlpha` | yes |
| right-flush and vertical centring | `placedScoreboardLayout` | yes |
| which ink a row draws in | `scoreboardRowInk` | yes |
| whether column three says anything | `scoreboardRowDrawsCountdown` | yes |
| **which colour a row's swatch draws** | `scoreboardRowSwatch` | yes |
| **column one's rectangle** | `scoreboardSwatchRect` | yes |
| **which characters exist** | `gatherScoreboardRows` | no |
| **each character's actual tint** | `gatherScoreboardRows` | no |
| **which tick the countdown is measured against** | `displayTick` | no |
| **three CVar registrations** | `ScoreboardVisualizationUImpl.cpp` | no |
| **canvas calls** | `AOGBrawlerUEHUD::drawScoreboard` | no |

Everything in the bottom four rows is a question only UE can answer. Nothing in them
decides a number: the UE layer produces five plain fields per row and a tick, and hands
them to the pure header.

⭐ **Task 10 added the last two `yes` rows deliberately, and finding F26 is the reason.**
The obvious place to write "column one is a rectangle at this x, this y, this wide and this
tall, filled with this row's colour" is the draw site — and the draw site is a
`Source/OGBrawlerUnreal` file, where no mechanical gate in this tree can read it. The
colour selection and the rectangle both moved into the pure header instead, and task 10's
red probes (recorded in the initiative workspace, not in this repository) show the
difference: a defect injected into either of them turns the suite red, while the same class
of defect typed into `gatherScoreboardRows` or `AOGBrawlerUEHUD::drawScoreboard` leaves
every gate green.

---

## 2. The join, and the id that would have been wrong

A row carries three facts that live in three different places.

| fact | read from | present on |
|---|---|---|
| score | `AOGBrawlerUECharacter::GetRingoutScore` | every peer |
| ⭐ swatch | `AOGBrawlerUECharacter::GetBrawlerColor` | every peer |
| dead | `brawlerRingout::State` flags, via the manager | every peer |
| respawn-at-tick | `respawnAtTick` in the same slice | every peer |

### 2.1 ⛔ The key is the SIM id, not the pawn's

The join key is `AOGBrawlerUECharacter::GetSimCharacterId`, which returns the
`USimmableUpdateComponent`'s unique id. **Every id inside the simulation is that one** —
the storage map's key, `brawlerRingout::ScoreSystem`'s roster key, the spawn-slot table,
every `id=%u` in every ring-out log line — because that is the value the component passes
when it registers. The pawn's own unique id is a **different number** — and it is the one a
`TActorIterator` walk puts in front of you first.

⚠ **This is the defect the layer was most likely to ship, and it would not have looked like
one.** Joining on the pawn's id matches nothing, every lookup misses, and the board draws a
full set of rows with correct scores and a permanently blank status column. Nothing logs,
nothing asserts, and the board looks healthy. It was caught in advance — task 5 added the
accessor specifically so this layer would have one obvious door — rather than in play.

### 2.2 ⛔ One door per fact, on every role

The score is read from the replicated property **on the authority too**, never from
`brawlerRingout::ScoreSystem`, even though the authority is holding that roster in memory
and could answer faster. Two code paths would mean the authority's is exercised only by a
listen server and the client's only by a client, and a bug report would never say which one
it came from. Reading the replicated value everywhere also makes the board a live test of
the replication itself: if the property stops arriving, the board stops moving.

### 2.3 ⛔ Dead and the countdown are not replicated again

They are `brawlerRingout::State`, which already rides the correction wire inside
`simulatableBrawler::State` and is reproduced on every peer by the ring-out
sub-simulation. Adding a `UPROPERTY` for either would be a second copy of a number the
wire already carries, and the two would disagree on exactly the frames that matter — the
ones around a correction.

### 2.4 The read is off the VIZ snapshot, not live state

`ASimulationManagerUImpl::getRingoutVizState` reads `getVizState`, **not** `getAllState`.
`updateVisualizationAll` takes the whole-state copy into `m_vizState` once per game-thread
pass, on the line immediately above the score push, and that copy is the sanctioned
physics→game handoff. A HUD calling `getAllState` would be a fresh, unargued cross-thread
read of live simulation state, so the accessor deliberately offers no route to one.

⛔ The accessor returns **by value and `const`**. The slice is five bytes of plain data;
returning a reference would hand a drawing surface a pointer into live storage for as long
as it cared to keep it. Together with the `const ASimulationManagerUImpl*` the HUD holds,
"the scoreboard writes no simulation state" is a compile error to break rather than a
promise made in a comment.

⚠ `m_delayedInputComponentsById` — the map the score push walks — is private and
authority-only, so it is **not** this layer's route. A gather built on it would work
perfectly on a listen server and return nothing on a client.

---

## 3. The display tick — three hazards in one subtraction

`respawnAtTick` is an **absolute** tick, so a countdown is `respawnAtTick - displayTick`.
Each of the three things that can go wrong here is guarded at the site.

**1. Asking the wrong clock terminates the process.** `getClientClock` does not return a
wrong number on a role that does not predict — it calls `std::terminate`. `displayTick`
therefore gates on `runsPrediction` before reading `getPredictionTick`, and takes
`getServerClock` otherwise. This is the same guard `pollInputHistoryLanes` makes at the
same accessor, and it is not defensive coding: without it, turning the board on during a
listen-server session would close the editor.

**2. The display tick moves BACKWARDS.** A hard resync rewinds the prediction clock. Both
ticks are `uint32_t`, so an unguarded subtraction wraps to roughly four billion and the
status column reads a nine-digit number. The subtraction is clamped at zero.

**3. The countdown is meaningless while alive.** `respawnAtTick` is left at its last value
once the dead bit clears — deliberately, so the field records what happened rather than
being zeroed. The flag is therefore read *first*, and the countdown is computed only when
`isDead` is true. Column three is gated on the same flag through
`scoreboardRowDrawsCountdown`, never on the countdown being non-zero.

The zero clamp also covers an ordinary, non-pathological frame: the respawn tick has
arrived but the sub-simulation has not yet run the step that clears the flag. The board
shows `OUT 0` for that frame rather than a wrapped number.

---

## 4. The CVars — why all three are read every frame

Two opposing disciplines coexist in this project, and the reasoning is written at both
sites so neither can be "fixed" in isolation. The original statement is in
`SimmableUpdateComponent.cpp`, at the movement debug draw's master switch.

* A **`StaticData`** CVar is read **once, at construction**, so a tunable cannot move under
  a running session and put two peers on different numbers. ⛔ **The ring-out kill plane and
  respawn delay are this kind**, and neither appears anywhere in this display's files.
* A **VIZ** CVar is the opposite case. It feeds nothing simulated, and its whole value is
  that a tuner can type `OGBrawler.ScoreboardScale 2` mid-session and see the **next** frame
  change. ⭐ **All three below are this kind.**

| CVar | default | clamp | kind |
|---|---|---|---|
| `OGBrawler.Scoreboard` | ⭐ **1 (ON)** | — | master gate |
| `OGBrawler.ScoreboardScale` | 1.0 | [0.25, 4] | look |
| `OGBrawler.ScoreboardAlpha` | 0 | [0, 1] | look |

### 4.1 ⛔ The clamps live in the pure header, not at the console

Both accessors call `clampScoreboardScale` / `clampScoreboardBackgroundAlpha` rather than
re-spelling a range. Three things follow, and all three are lost if the range is copied:
the console keeps echoing whatever the user actually typed; the shipped bound stays
somewhere a Catch2 case can reach it; and the NaN behaviour — the pure clamp's first test
is negated on purpose, so a non-number lands on the minimum instead of multiplying every
geometric field into NaN and drawing the board nowhere — comes along for free.

### 4.2 ⚠ The model file's "fold" degenerates here, and imitating it would be worse

`InputHistoryVisualizationUImpl.h` folds its master into four child booleans, so that no
call site can read a child toggle and forget the master. This board has **one** display:
the master *is* its toggle, there is no second boolean to fold it into, and a boolean
cannot be folded into either float — "off" is not a scale and not an opacity.

The discipline the fold exists to buy is kept where it can actually be expressed: **there
is exactly one branch**, in `AOGBrawlerUEHUD::DrawHUD`, and every other entry point in the
namespace is reached only from behind it. ⛔ A second scoreboard toggle owes the fold.

### 4.3 ⭐ The master defaults **ON**, which no other viz CVar here does

User ruling, 2026-09-13, and it is a deliberate departure from the house pattern rather
than an oversight — so it is stated in three places that move together: the initialiser in
`ScoreboardVisualizationUImpl.cpp`, the declaration comment in the header, and here.

Every other visualization toggle in this tree defaults off, and the reason is written at
several of those sites: a debug draw nobody asked for should cost nothing. ⛔ **The
scoreboard is not a debug visualization.** It is game-mode UI. A ring-out match whose score
is invisible is not a playable match — it is a match in which nobody can tell they are
winning — so a feature the mode does not work without has the opposite natural default to
an overlay you switch on to investigate something.

⛔ **Do not "fix" it back to `0` to match the neighbouring pattern.** The pattern is about
debug draws, and this is not one.

⚠ **Only the initialiser moved.** The branch in `DrawHUD` is unchanged, both look knobs are
unchanged, and `OGBrawler.ScoreboardAlpha` in particular stays at `0`: a transparent
backdrop is still not drawn at all, and that early-out has nothing to do with the master.

### 4.4 Off costs one boolean read

The gate in `DrawHUD` is a plain early-out, matching the shape that file already uses:
nothing below it runs. No actor is iterated, no simulation slice is looked up, no geometry
is computed, no backdrop is drawn. ⭐ **That property is unchanged by the default flip —
only which side of it is the default moved.** It is now what a player or a profiler pays
for typing `OGBrawler.Scoreboard 0`, rather than what every shipped session pays for
leaving the board off.

⛔ **A fully transparent backdrop is not drawn either.** Zero is the shipped alpha default,
and an invisible rectangle is still a canvas call on every frame.

---

## 5. What this panel does *not* inherit from the input-history pane

Three differences, and each one is a place where copying the precedent would be a defect.

**1. It is not single-character.** The input-history pane picks one character by selection
through `firstLocalCharacterId`. A scoreboard draws everyone, so no filter of that kind
appears. ⛔ The *other* guard is still needed and still made: `firstLocalPlayerController`
is asked at the draw site so **one** local player's HUD draws the board. Without it every
couch-co-op sibling stacks the same panel on the same screen — and unlike a per-character
pane, four identical boards at the same coordinates are not obviously four.

**2. It is right-flush, so it needs the viewport WIDTH.** `placedScoreboardLayout` takes
one; the pane's equivalent never did, because flush-left needs no measurement. Hardcoding
1920 would put the board 640 px off-screen on an ultrawide.

**3. It centres the DRAWN height, not a reserved window.** The pane reserves its full row
window because rows arrive while you watch and a creeping top edge is unreadable. A
scoreboard's row count is the player count: constant for the match except at a join or a
leave. Reserving eight rows for a two-player match would draw the board visibly above
centre for a whole session, so `placedScoreboardLayout` takes `rowCount`.

---

## 6. The draw — canvas calls and one thing that drifts

`AOGBrawlerUEHUD::drawScoreboard` iterates nothing, sorts nothing, and decides no origin,
row height or column edge. Per row it makes **one `AHUD::DrawRect` — the colour swatch —
and at most two `AHUD::DrawText` calls**, and the whole board makes at most one further
`AHUD::DrawRect` for the backdrop.

⚠ **That count changed with task 10, and so did its shape**: column one used to be a third
`AHUD::DrawText` printing the raw character id. It is now a rectangle, so the per-row call
count is unchanged, but one text call became a rect call and the board's total rect calls
went from at most one to at most one per drawn row plus one.

⛔ **`textScale` reaches every `AHUD::DrawText` AND every `AHUD::GetTextSize`.** The board
draws with `UEngine::GetSmallFont`, which is fixed-size, so a scale that reached only the
geometry would give a bigger box holding the same tiny glyphs. The measure matters just as
much and is the half that actually drifts: a right-aligned column's left edge is its
`scoreRightX` or `statusRightX` minus the width the measure returned, so a measure taken
without the scale places the column further out of alignment the larger the board gets — and **it is exact at scale 1**, which is the only scale most runs use. The
sibling document marks this as the thing that drifts if missed; it is repeated here because
this panel has two right-aligned columns rather than one.

### 6.0 ⭐⭐ Column one is a colour swatch, and what it does when a fighter dies

**User request, 2026-09-13, from playing the mode.** The column used to print
`characterId` — a number like `42353`, which is the `USimmableUpdateComponent`'s unique id.
It is the correct key and it is unreadable: nothing on screen carries that number, so a
player could not connect a row to a body. Every brawler already wears a unique tint, so the
column now draws that tint and the player reads the row the way they read the fight.

⛔ **The id did not leave the row, and only the DISPLAYED column changed.** `characterId`
is still what `gatherScoreboardRows` joins each row's other facts on, and still what
`scoreboardRowPrecedes` orders the board by, so two peers continue to show the same player
in the same row. Nothing on this board is ordered by, keyed on or compared by colour — a
future edit that sorted by the swatch would put two peers back out of step the moment one
of them assigned tints in a different order, because the assignment order is a
possession-counter artefact and not a shared fact.

#### ⛔ The swatch does not change while a fighter is counting down. That is a choice.

`scoreboardRowSwatch` is deliberately independent of `isDead`: the same row draws the same
three floats whether its owner is in play or waiting to respawn. Four reasons, in the order
that decided it:

1. **A dead row is exactly when identity is needed most.** A player who has just been rung
   out is looking at the board to find their own row and read how long they are out for. A
   column that changed appearance at the moment it is read hardest is the wrong column to
   carry status in.
2. **Status is already stated twice, on two channels that are not the identity channel.**
   The row's *text* ink switches to `kScoreboardDeadRowInk` — dimmer and warmer — and
   column three appears and reads `OUT 120`. A third cue bought nothing the first two do
   not already say, and it would have been the only one to cost identity.
3. ⛔ **Any "dim while dead" rule is a many-to-one map on the one axis that must stay
   one-to-one.** `kBrawlerPalette` is hand-picked so neighbouring tints stay distinguishable
   for a colour-blind reader and against the level's grey. Pulling every dead swatch toward
   a common colour compresses exactly that separation, and two fighters can be down at once.
   This is measured rather than asserted:
   `Scoreboard.TheSwatchIsBoundToItsOwnRowAndSurvivesTheOrderingFold` builds a full
   eight-row board on which **every** fighter is dead, and shows that the mildest imaginable
   dim — a half-blend toward the dead ink — costs exactly half the minimum pairwise
   separation, while the shipped rule preserves it exactly.
4. **The fighter's body does not change colour when it dies.** It respawns wearing the same
   tint, so a swatch that dimmed would be showing something the world does not.

⛔ **If a future task wants a dead-state cue in column one, the honest form is a SECOND mark
beside the tint — an outline, a strike — never a transform applied to it.**

#### Two brawlers never share a swatch in a session this mode is built for

The palette holds ten hand-picked entries and `PossessedBy` hands them out round-robin, one
per possession, on the authority only. ⭐ A `static_assert` at the palette's own declaration
now pins its cardinality against `kScoreboardMaxRows`, so shrinking it below the number of
rows the board can draw is a **compile error** — and the compiler is one of only three
mechanisms that reach a `Source/OGBrawlerUnreal` file at all.

⚠ **The honest bound is per-possession, not per-concurrent-fighter.** The counter never
reclaims a leaver's index, so what is guaranteed is "the first ten possessions of a run get
distinct tints", not "no two live brawlers ever share one". A run with enough joins and
rejoins to wrap past ten can hand a newcomer the tint of a fighter who never left. That is
pre-existing palette behaviour, unchanged by this task, and far outside the four-player
session the mode targets — but it is what the assert actually promises.

### 6.1 The status column says a word, and that is not decoration

Columns two and three are both right-aligned integers. A bare number in column three would
be indistinguishable from a score that had simply moved right — the reader has no second
cue, because the dead row's ink differs from a live row's by hue and not by shape. Column
three therefore reads `OUT 120`, so the quantity names itself.

### 6.2 ⛔ Why this panel adds no legend table

`palette_legend_lint.ps1` is scoped to the frame meter's three **enum-keyed** palettes in
`BrawlerInputHistoryVisualizationBars.h`, each of which must keep a legend table in the
sibling rationale in step with its switch arms. This board has **no enumerated state to
colour**. It has one boolean — out, or not — so `scoreboardRowInk` is a two-armed function
of a `bool`, there is no switch, and there are no enumerator names for a table to list. The
lint's scope does not reach it, and a table here would be a fourth palette that nothing
checks: exactly the stale table that lint exists to prevent.

⭐⭐ **And task 10's swatch did not become one either — stated explicitly rather than left
to be inferred, because it is the first colour on this board that is not a constant.**
`scoreboardRowSwatch` is a **per-character runtime tint**: it returns a value carried in on
the row, assigned at possession time from a palette that lives in a different module, and
this display neither names an entry of that palette nor selects between entries. The lint's
subject is *a function that switches on an enumerator*, because that is the shape whose arms
can silently fall out of step with a table listing those enumerator names. A function with
no arms at all has nothing to list — a legend for this column would have to enumerate
`kBrawlerPalette`, which is indexed by a possession counter rather than by any named state,
and would go stale the day somebody appends an eleventh colour. **No legend table is owed,
and none was added.**

⛔ **If a future task gives this board an enum-keyed colour, it owes the legend table AND
the lint arm, in the same change.** The draw method names no colour of its own — every ink
it uses arrives from the pure header's selector — which is a property of the *shape* of
that code, not of its current contents.

---

## 7. Reading order

1. `OGBrawler/BrawlerScoreboardVisualization.h` — the banner states the three ways this
   board deliberately differs from the input pane, then the layout and clamps themselves.
   The swatch's policy is at `scoreboardRowSwatch`, its geometry at `scoreboardSwatchRect`.
2. `Source/OGBrawlerUnreal/ScoreboardVisualizationUImpl.h` — the join table, the CVar
   read-cadence argument, and the display-tick contract.
3. `Source/OGBrawlerUnreal/ScoreboardVisualizationUImpl.cpp` — the three registrations and
   the gather.
4. `Source/OGBrawlerUnreal/OGBrawlerUEHUD.cpp`, `drawScoreboard` — the canvas calls.
5. This document, for anything that reads as a choice rather than a consequence.
