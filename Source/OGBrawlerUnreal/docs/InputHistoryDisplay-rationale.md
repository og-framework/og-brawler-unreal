<!-- SPDX-License-Identifier: BUSL-1.1 -->
<!-- lint-external-ref: AHUD::DrawText -- the engine's own HUD draw call; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: AHUD::GetTextSize -- the engine's own HUD text measure; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: FRotationTranslationMatrix -- the engine's own rotation matrix, whose Y row IS the right vector; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: USceneComponent::GetRightVector -- the engine's own right-vector accessor; owned by the host engine, not by this repository, and must not resolve here -->
<!-- lint-external-ref: OGBrawler.InputHistoryFrameMeter -- RETIRED NAME (§7.10): the meter's single toggle before the three-bar split; the CVar is deleted and must not resolve -->
<!-- lint-external-ref: GInputHistoryFrameMeter -- RETIRED NAME (§7.10): the CVar-backed bool behind the name above; deleted alongside it -->
<!-- lint-external-ref: InputHistoryFrameMeter -- RETIRED NAME (§7.10): the bare stem both retired names above share; must not resolve on its own either -->
<!-- lint-external-ref: frameMeterEnabled -- RETIRED NAME (§7.10): the accessor for the CVar above; deleted, its callers now read anyBarEnabled() -->
# The input-history display — rationale

Companion to the three pure `og-brawler` headers that hold the display's logic —
`BrawlerInputHistoryVisualization.h`, `BrawlerInputHistoryVisualizationPoll.h` and
`BrawlerInputHistoryVisualizationPanel.h` — and to the four `Source/OGBrawlerUnreal`
files that own, feed, gate and draw them: `InputHistoryVisualizationUImpl.h`,
`InputHistoryVisualizationUImpl.cpp`, `OGBrawlerUEHUD.h` and `OGBrawlerUEHUD.cpp`.
**The source files carry the guards; this file carries the reasoning.**

<!-- ================= WHY THIS FILE LIVES HERE ==============================
     Recorded as a decision, not left to be re-derived. See §0.
     ========================================================================= -->

## 0. Why the record sits in this tier and not in the other two

The display spans three licence subtrees, so "beside the code" does not name one place.
The ruling, and its reasons:

* **Not the `og-simulation` docs tier.** That tier is MPL-2.0 and travels with that
  submodule. Every load-bearing claim below is about a **consumer** of og-simulation, and
  a BUSL-1.1 argument cannot be distributed inside the MPL-2.0 core. This is decision D9's
  reasoning applied unchanged. og-simulation gets exactly one pointer instead: fence 3 of
  `SlotStateProvenance.h` now names the external reader, so a person holding only that
  header learns a downstream consumer exists.
* **Not beside the `og-brawler` headers.** That would match `VISUALIZATION_DISCIPLINE.md`'s
  own placement rule, but `og-brawler` is **not** one of the two tiers
  `doc_anchor_lint.ps1` discovers, so every anchor in it would go unchecked — and this
  document exists precisely to assert machine-checkable facts about names in three repos.
  og-brawler gets a pointer too: the discipline document's scope list now names the three
  new units.
* **Here.** `Source/OGBrawlerUnreal/docs` is the BUSL-1.1 tier decision D9 created so that
  BUSL documents need not live in the MPL core; it is linted by default; and it already
  holds this initiative's other rationale (`SimulationManagerUImpl-rationale.md`, whose §1
  carries the threading half of the same feature). One feature, one tier, three pointers.

---

## 1. Why new state was justified at all — the depth argument

**This section is the entire justification for writing any new state. Without it the row
ring reads as a reinvention of two buffers that already exist.**

Two resident buffers could plausibly have served the display, and **both hold about one
second**:

| Buffer | Capacity | Where the number is |
|---|---|---|
| `LocalInputCache` | `kLocalInputCacheCapacityTicks` = **64** capture ticks | `LocalInputCache.h` |
| `StateCorrectionCache` | `StateCorrectionCache::StateBufferSize` = **60** slots | `CorrectionCache.h` |

At `TimeConfig`'s `tickFrequency` of **60 Hz** that is **1.07 s** and **1.00 s**
respectively.

Now the requirement. The reference display's visible rows carry these hold counts:

```
37, 16, 99, 3, 1, 4, 5, 8, 4, 2, 6, 4, 5, 5, 5, 3, 24, 10, 10
```

Sum = **251 ticks = 4.2 s**, and **one single row reads 99 ticks (1.65 s)** — on its own
longer than either buffer's whole capacity.

Two consequences, and the second is the sharper one:

1. A reader restricted to resident state reconstructs at most the newest ~64 ticks —
   roughly the top quarter of one reference screenshot.
2. It could **never** report that 99-tick row correctly. It would print a truncated `64`
   and be **silently wrong**, which is worse than being visibly short.

A third, independent reason: `LocalInputCache`'s `clear()` runs from `wipeAllForResync`, so
the buffer **blanks on a hard resync** — precisely the event the display exists to explain.

**Therefore** the new state is a bounded ring of **run-length-compressed rows**
(`InputHistoryRow`, `InputHistoryRowRing`, `kInputHistoryRowCapacity` = 64 rows), one row
per held input state, fed by *polling* what `LocalInputCache` already records. Nothing
re-records an input and no capture site was added. `tickCount` is deliberately
**unclamped**, which is what lets one row outlive the cache that produced it.

---

## 2. `AppliedCaptureRefKind` has FOUR arms, and `Replayed` reaches a reader through exactly one

This is the fold's structural trap, and it is invisible to inspection: the wrong version
**looks correct and works for the common case.**

`SimulationReconciliation`'s `getAppliedCaptureTickRef` answers with four kinds, not the
three every earlier draft of this design described:

| Kind | What it means |
|---|---|
| `NoSlot` | no cache slot for that tick at all |
| `NoRef` | the slot **exists** and no correction ever landed on it — **the ordinary client steady state** |
| `Sentinel` | a correction landed but named no capture (`kNoInputCaptureTick`, documented ambiguous) |
| `Ref` | a correction landed and names a real capture tick |

`m_appliedCaptureTickBuffer` has exactly **one** writer — `tryInsertingCorrectState`, i.e.
a landing correction. `pushPredictionTick` writes `kNoInputCaptureTick`. So on a healthy
client with no corrections the inverted map is **empty**, and every capture is answered by
one of the three arms that name no capture.

⭐ **Now the trap.** Item 47's protect-all-corrected rule means
`tryInsertingResimulatedState` never writes a slot whose `containsCorrectTick` bit is set,
and that bit is set at both authority-grade stamp sites. A replay can therefore only ever
write a slot that has **not** been corrected — so a slot carrying
`SlotStateProvenance::Replayed` is answered by `NoRef`, never by `Ref`.

**A fold that consulted lineage only on the `Ref` arm makes `Resimulated` structurally
unreachable while `Corrected` keeps working.** It passes every hand test a reviewer is
likely to run, because corrections are the case anyone thinks to produce. That is why
`captureSummaryOf` takes both the kind **and** the optional lineage and branches on all
four arms, and why `summaryOfSlotProvenance` is consulted on `NoRef` and on `Ref` but
deliberately **not** on `Sentinel`: a sentinel slot always carries an authority-grade
lineage, so refining it would render every sentinel `Corrected` or `Confirmed` and delete
the arm outright.

The fold itself is `worseRowProvenance`, a max over `RowProvenanceSummary`'s **declaration
order** (`Unknown` … `ProvenanceLie`, `kRowProvenanceSummaryCount` = 9), and
`residentRowSummary` reduces one tick span through it. Any reader that CARRIES a summary
forward must merge it **monotonically** — the 60-slot correction window scrolls, and a plain
assignment would erase a correction the instant its slot aged out.

### 2.1 ⛔ The join no longer reaches the input row, and that is a correction to this design

The first shipped panel put the summary **on the input row**, and exercising it exposed a
design error this file is as responsible for as the code was. **Input identity and lineage
vary on different axes.** Input changes when the player changes what is pressed; lineage
changes per tick from network events. Run-length compressing on input identity therefore
destroys lineage resolution — and because the merge above ratchets worst-case-wins and never
recovers, a long hold **saturates**: one resimulated tick inside a 99-tick row makes all 99
read as resimulated. The column's information content decayed as rows lengthened, which is
inverted from what was wanted.

The join and the fold are **unchanged and kept**, with their tests, because the four-arm
analysis above is the hard part and none of it was wrong. What was removed is the
`InputHistoryRow` field, the panel's rendering of it, and the poll's call into the join. A
tick-keyed lane is the successor; until one exists these functions' readers are their tests.

---

## 3. The display is a DIAGNOSTIC READ, and the fence list is exhaustive

Nothing about this feature reaches production:

* **Nothing is replicated.** No wire format changed; no `USTRUCT` gained a field.
* **Nothing enters the correction payload.**
* **Nothing enters `compute_checksum` or the determinism comparison.** Two peers replaying
  identical inputs from identical state must agree on the *state* and may legitimately
  disagree on how each arrived at it.
* **There is no production reader.** The seams are read-only views —
  `localInputCache` on `SimulationInputResolution`'s diagnostics view and
  `slotStateProvenance` on `SimulationReconciliation`'s — and both are const-only. The
  machine-checked half of that guarantee is og-simulation's own fence test,
  `CorrectionCache.ResimGate.TheProvenanceColumnCannotReachAnyProductionOutput`, which this
  initiative left **byte-identical**.
* **The feature is default OFF, at the MASTER.** `OGBrawler.InputHistory` (`false` by
  default) is the one gate every display below folds through; `OGBrawler.InputHistoryDisplay`
  itself now defaults ON, like every other per-feature CVar — see §7.10. At the poll site the
  master read is an **early-out placed above** the `firstLocalCharacterId` actor walk rather
  than a conjunct inside the condition below it — with the master off the whole per-frame
  cost is one `bool` read in `AOGBrawlerUEHUD::DrawHUD` and one per simulated character in
  `USimmableUpdateComponent::TickComponent`. No allocation, no actor walk, no ring access.

The one thing this feature *did* add to og-simulation is two const read seams, and
`DiagnosticsConventions.md` §5 records both against the class that **owns** the data — the
reason a single facade on `SimulationInputResolution` was considered and rejected even
though it does hold reconciliation and already reaches through it for
`getAppliedCaptureTickRef`.

---

## 4. The ring's toggle invariant

⛔ **The correct statement is: _no row may count a tick that was never observed._** Record
that form and no other.

The weaker gloss — *"a resumed poll is an ordinary poll after a gap, and a gap opens a new
row rather than overstating a hold"* — describes only the **long-gap** regime, and reading
it literally would fail a correct implementation. A poll gap has two regimes:

* **Gap ≤ the source cache's 64 ticks.** The simulation kept capturing while the display
  was off and those captures are still resident. The resumed sweep spans the whole window,
  re-presents everything and recovers every missed tick. A row here **legitimately spans
  the off window**, because the ring observed every tick in it. This is `appendCapture`'s
  idempotence doing its job, and it is the same property that makes render-rate polling of
  a 64-slot ring safe in the first place.
* **Gap > 64 ticks.** Ticks were evicted unseen. The resumed sweep's oldest resident tick
  is not the successor of the newest folded row's last tick, so contiguity fails and a row
  opens.

Both regimes satisfy the invariant as stated; only the second satisfies the weaker gloss.
The criterion is pinned by **counting** the violations rather than by asserting a shape, and
the long-gap case holds the input **identical across the gap on purpose**, so that only the
contiguity test — and not a field change — can prevent the fold.

---

## 5. Where each piece lives

| Layer | Unit | What it holds |
|---|---|---|
| pure, engine-free (BUSL-1.1) | `BrawlerInputHistoryVisualization.h` | `directionBucketOf` / `nearestNamedDirection` (built on `aimRelativeAngle` + `angularDistance`, no trigonometry of its own), `InputHistoryRow`, `InputHistoryRowRing`, and the whole four-arm join (`captureSummaryOf`, `summaryOfSlotProvenance`, `worseRowProvenance`, `residentRowSummary`, `AppliedCaptureInversion`, `kAppliedCaptureInversionCapacity`) |
| pure, engine-free (BUSL-1.1) | `BrawlerInputHistoryVisualizationPoll.h` | `pollWindowEndingAt`, `kCapturePollWindowTicks`, `kAppliedPollWindowTicks`, `captureRowFieldsOf`, `mergeResidentCaptures`, `rebuildAppliedCaptureInversion`, `pollInputHistory` |
| pure, engine-free (BUSL-1.1) | `BrawlerInputHistoryVisualizationPanel.h` | `directionArrowAxis`, `directionGlyphOf` (`DirectionGlyph`, `ArrowSegment`), `buttonMaskGlyph`, `kPanelVisibleRows`, `panelDrawnRowCount`, `panelRingIndexForSlot`, `panelRowTopY`, `panelHeight`; and the size/placement surface — `scaledPanelLayout`, `panelWindowHeight`, `panelCenteredOriginY`, `placedPanelLayout`, `kPanelLeftEdgeX`, `clampPanelScale`, `clampPanelBackgroundAlpha`, `clampPanelVisibleRows` — §5.2 |
| pure, engine-free (BUSL-1.1) | `BrawlerInputHistoryVisualizationLanes.h` | `MachineStateCell`, `machineStateCellOf`, `TickLane`, `InputHistoryTickLanes`, `kTickLaneCapacity`, `clampRetainedLaneTicks` — the per-tick storage the bars read; and the idle gate — `laneTickIsInactive`, `LaneIdleGate`, `LaneAdmission`, `LaneAxisEvent`, `LaneAxisEventKind`, `kLanePauseEngageTicks`, `kLaneElisionLedgerCapacity` — §7.7; and the clock reading — `ClockDriftReading`, `noteClockDriftReading`, `clockDriftReading`, `authorityStaticSimTicks` — §7.12 |
| pure, engine-free (BUSL-1.1) | `BrawlerInputHistoryVisualizationBars.h` | `provenanceCellStyleOf`, `machineCellStyleOf`, `frameMeterGeometryFor`, `frameMeterCellX`, `frameMeterBarTopY`, `collectLaneRuns`, `runLabelFits`, `frameMeterHorizonOf` — §7; and the axis-event markers — `FrameMeterAxisEvent`, `FrameMeterAxisEventList`, `collectFrameMeterAxisEvents`, `frameMeterElisionLabelTopY`, `kLaneElisionColor`, `kLaneResyncColor` — §7.7; and the clock readout — `ClockDriftReadout`, `buildClockDriftReadout` — §7.12 |
| read seams (MPL-2.0) | `SimulationInputResolution.h`, `SimulationReconciliation.h` | `localInputCache`, `slotStateProvenance`; the join `getAppliedCaptureTickRef` already existed |
| UE (BUSL-1.1) | `InputHistoryVisualizationUImpl.*` | the id-keyed rings and lanes, `firstLocalCharacterId`, `firstLocalPlayerController`, `masterEnabled`, `displayEnabled`, `provenanceEnabled`, `inputDelayEnabled`, `characterStateEnabled`, `barSelection`, `anyBarEnabled`, `retainedLaneTicks`, `panelScale`, `panelBackgroundAlpha`, `panelVisibleRows`, `pauseLanesWhileIdle` — §7.10 |
| UE (BUSL-1.1) | `OGBrawlerUEHUD.*` | draw calls only; every number, endpoint and sign comes from the pure header |

Two rules the split exists to serve. First, **no test target links `OGBrawlerUnreal`**, so
anything that must be swept by a Catch2 case has to be pure — that is why the presentation
table and the poll sweep are headers in og-brawler rather than methods on the HUD. Second,
the pure headers are named `*Visualization*` **on purpose**: that is the filename glob
`visualization_hitbox_isolation.ps1` scans.

That gap is now closed: the lint takes two roots and two globs — `*Visualization*` and
`*UEHUD*` — so the `Source/OGBrawlerUnreal` files in the table above are scanned too, 25
files in all. ⛔ It still sees DIRECT includes only, so a transitive reach through
`SimulationManagerUImpl.h` is structurally invisible to it and is ruled on in
`VISUALIZATION_DISCIPLINE.md` §4.1 instead.

### 5.1 Why the direction glyph is DRAWN and not typed

A Unicode arrow would have been one character in a `DrawText` call and was rejected: a font
missing that codepoint renders a tofu box, which is a **runtime** failure no test in this
tree can reach — `Source/OGBrawlerTests` cannot link a canvas or a font. Three `DrawLine`
calls have no such failure mode.

The consequence for the split is that the glyph's whole geometry had to move into the pure
header. `directionArrowAxis` turns a bucket into a unit screen-space direction and
`directionGlyphOf` turns that into a shaft and two barbs in absolute pixels, so the HUD
rotates nothing and decides no sign. `Neutral` is the zero vector and draws a dot; an arrow
for a direction nobody pressed would point somewhere false.

⛔ THE COMPASS IS AIM-RELATIVE, AND UP MEANS "MOVING WHERE I AM AIMING". `Forward` is
aim-relative angle `0` and draws straight UP, so on a canvas that counts y DOWNWARD its axis
is `(0, -1)` and the other seven rotate clockwise from it. The axis is read straight off the
enumerator value, which is the compass ORDINAL — `Neutral` at 0, then eight clockwise steps
from `Forward` — so a bucket indexes its own arrow and there is no second ordering to drift.

⚠ An earlier version mapped the Street Fighter numpad GRID onto the screen, which drew
`Forward` pointing RIGHT. That convention encodes a fighter who faces screen-right; this game
is isometric with a free aim vector, so there is no screen-right "forward" and the assumption
did not hold. The two differ by one rotation, and `Panel.EachOfTheEightAimRelativeAnglesDrawsItsOwnCompassArrow`
pins all eight individually because a rotation applied to only part of a derivation is right
on the axes and wrong on the diagonals.

⚠ The bucket names are the DISPLAY's and deliberately disagree with `inputSequence`'s. That
header labels its constants on the numpad, so what it calls `angle::Down` (`+pi/2`) the
display calls `Left`. The angles themselves are unchanged;
`Panel.EveryArrowMirrorsTheAngleTheMatcherTestedAcrossTheAimAxis` sweeps `kNamedDirections`
and asserts each arrow is `(-sin, -cos)` of the angle the motion matcher actually tested.

⛔ THE SIDE, AND A STALE SENTENCE IN `og-brawler`'s OWN `InputSequence.h`. That header
documents `+pi/2` as "right-of-aim", from `right = (fwd.y, -fwd.x)` annotated "right-hand
z-up". Every vector reaching it is an Unreal world vector copied component-for-component,
and Unreal's right vector is the other perpendicular — `FRotationTranslationMatrix`'s `Y`
row, which `USceneComponent::GetRightVector` returns, is `(-sin yaw, cos yaw, 0)`. So
`(fwd.y, -fwd.x)` is the character's LEFT and `+pi/2` is **left-of-aim**: pressing left
produced `+pi/2` and the display drew a RIGHT arrow, which is what the user reported. The
display's table was mirrored to match reality. **`InputSequence.h`'s comment is wrong and
was deliberately left alone** — the motion matcher's behaviour is baked into the shipped
motion definitions, so correcting the vocabulary there is a gameplay decision, not a
presentation one.

---

### 5.2 The panel's size and placement are computed, not chosen

The panel used to be a fixed 84 × 432 box at a fixed `(16, 72)`. It is now a **scale**, a
**background opacity** and a **visible row count**, each a console variable clamped at read,
and a placement derived from the viewport. Three things about that are worth recording,
because each of them is a mistake that would otherwise have been easy to make.

**The scale must reach the TEXT, not only the geometry.** The rows are drawn with
`GEngine->GetSmallFont()`, which is fixed-size. Multiplying `rowHeight`, `rowWidth` and the
column offsets alone yields a larger box holding the same tiny glyphs floating in dead space
— a change that looks like a layout bug rather than like a scale. `AHUD::DrawText` and
`AHUD::GetTextSize` both take a scale parameter, so `PanelLayout` carries a `textScale` field
that `scaledPanelLayout` writes **in the same statement list** as every positional number.
One function, one factor; there is no second place where the two could be given different
values. `Panel.OneFactorScalesBothTheGeometryAndTheTextAndTheyCannotDriftApart` pins it, and
the measure takes the scale too — right-aligning the tick count against an unscaled width
would drift the column as the panel grew.

**The centring is computed AFTER the scale, and that ordering is structural.**
`originY = (viewportHeight - panelWindowHeight) * 0.5`, where `panelWindowHeight` reads
`rowHeight` off the layout it is given. `placedPanelLayout` scales first and then centres the
result, so the height being centred is necessarily the scaled one. Centring a pre-scale
height is exactly right at scale 1 and visibly wrong at every other scale — the classic bug
that ships because only the default was ever looked at. The order is not a caller's to get
wrong here, because callers never see the two steps.

**The window, not the drawn rows, is what gets centred.** `panelWindowHeight` is
`visibleRows × rowHeight` whether or not the ring has that many rows yet. Centring the drawn
height instead would let the panel's top edge creep upward for the first minute of play as
rows accumulate. The background rectangle still hugs the rows that exist, so an empty panel
does not paint a large black box.

⚠ **The panel and the frame meter can overlap, and at the ruled defaults they do not — in
either axis.** At scale 1.0 the panel is 84 px wide and, at 24 rows on a 720p viewport, spans
y = 144..576, while the bottom-anchored meter's backdrop begins at y ≈ 621. Horizontally the
meter's backdrop begins at x = 157 against a panel ending at x = 84, a 73 px gap, and every
wider aspect moves the meter further right. **The vertical clearance is the first to go**: the
panel re-enters the meter's band at a scale of about 1.21, above which x alone separates them,
and that x gap closes in turn at panel scale ≈ 1.87 on a 720p viewport — or at any scale above
≈ 0.73 if `OGBrawler.InputHistoryLaneTicks` is raised to 240, since the meter widens toward
both edges as its window grows.
`Panel.ThePanelClearsTheFrameMeterAtTheRuledDefaultsInBothAxes` asserts both clearances as
numbers **and** asserts the overlap at the settings that lose each one, so the limits are
pinned facts rather than something to rediscover in play.

## 6. Reading order

1. This file, §1 — why the ring exists at all.
2. This file, §2 — why the fold has four arms.
3. This file, §7 — the second display, and why a run is a reading rather than a storage shape.
4. This file, §7.7 — why the lanes pause, what a paused lane stops recording, and the
   second cut in the axis a hard resync makes.
5. This file, §7.12 — the clock line under the bars, and what `auth static` diagnoses.
6. `SimulationManagerUImpl-rationale.md` §1 — the two game/physics thread crossings the
   render-rate poll opens, and the tear argument for each.
7. `VISUALIZATION_DISCIPLINE.md` — the mesh-only invariant the pure units live under.
8. `DiagnosticsConventions.md` §5 — how the two read seams were allowed to be added.

---

## 7. The frame meter — two bars, one tick axis

The second display is a fighting game's frame meter: two stacked horizontal bars near the
bottom middle of the screen, **one cell per capture tick**, newest at the right. The upper bar
is the provenance lane, the lower one the attack machine's state. Both are read out of
`InputHistoryTickLanes`, which stores per tick and folds nothing.

### 7.1 Why a run is a READING and not a shape

`collectLaneRuns` walks the retained window comparing neighbours and emits a `LaneRun` per
maximal stretch of one value; the length is drawn on `lastOffset`, the run's right-hand cell.
⛔ **Nothing upstream may pre-fold this.** The retired row model keyed lineage on *input*
identity while lineage varies per tick from network events, so a worst-case-wins merge over a
run saturated — one resimulated tick inside a 99-tick hold made all 99 read as resimulated and
never recovered. Detecting the run at draw time is what makes a late correction repaint exactly
one cell, and it is the reason the storage in §5's lanes row is per tick.

### 7.2 Tick alignment is structural, not asserted

There is **one** `FrameMeterGeometry` per frame and **one** window from `retainedLaneWindow`.
Both bars take their column from `frameMeterCellX` and differ only in the index they hand
`frameMeterBarTopY`. A vertical slice is therefore one capture tick by construction: there is no
second derivation available to drift. `frameMeterGeometryFor` centres the bar horizontally and
anchors it a fraction of the viewport height above the bottom edge — cell sizes are absolute
because legibility is, and the margin is fractional because placement in the frame is relative
to the frame. A retained count too wide for the viewport shrinks the cells rather than clipping.

### 7.3 A hole must not read as a state

`MachineStateCell::NotSampled` maps to `LaneCellFill::Hole`, which draws nothing at all, so the
bar's own dark ground shows through. The provenance bar treats a tick no observation named the
same way — a missing cell is a hole, never a defaulted `Unknown`. ⛔ **A hole also ends the run
it interrupts**, so a two-tick gap inside a hold reports two runs and not one six-tick hold
nobody observed. A third fill, `LaneCellFill::Unnamed`, exists so that an enumerator the style
table stopped covering renders as `kUnnamedLaneColor` rather than falling back on the hole; an
absent cell and an unmapped one are opposite failures.

⭐ **THE NO-CACHE ROLE IS ALSO HOLES, NOT A COLOUR.** A role with no correction cache at all —
the authority, a listen-server host, standalone — writes nothing to the provenance lane on any
poll, so its bar is holes end to end rather than a wall of `Pending`. `frameMeterHorizonOf`
answers `NoCache` for that reading, the horizon anchors the right edge, and the residency
readout says so in words (§7.5).

### 7.4 Two palettes, one colour language

The nine provenance colours are the input panel's own, kept value for value — one datum, one
colour language. The four machine colours sit in hues the nine leave free. Distinctness is swept
rather than asserted by eye: every pair inside a palette clears `kLanePaletteMinPairGap`, every
cross-palette pair clears the stricter `kLanePaletteMinCrossGap`, and the measured cross-palette
minimum is required to **exceed the provenance palette's own closest pair**, which is the
property that stops the lower bar reading as one more spelling of the upper one. Both sweeps
walk to the enumerator count and one past it, so a state added without a table entry fails.

### 7.5 The frozen horizon — a residency reading, not a lane-cell count

⚠ **THE OLD `retained − 60` SENTENCE WAS WRONG IN LANE SPACE.** The provenance lane is only
written across the correction cache's own 60 SIM-tick resident window
(`kAppliedPollWindowTicks`), and the earlier rule placed the rule `retained − 60` **lane**
cells from the left — correct only when a lane tick is a sim tick. With
`OGBrawler.InputHistoryPauseIdle` on (the default), an elided span costs one lane cell however
many sim ticks it removed, so the writable window is FEWER than 60 lane cells once anything has
been paused, and the old rule drew cells as live that were already frozen.

`frameMeterHorizonOf` reads the lanes' own residency reading — `oldestResident`, a SIM tick
the poll files beside the axis every tick, paused or not, so a paused display's horizon keeps
tracking the live tick instead of freezing with it — and converts it through
`placeFrameMeterSimTick`, the SAME helper the authority marker uses, so the two markers can
never derive a column two different ways.

**What the rule means.** Everything LEFT of it is frozen: correct, finished observations that
neither diagnostic seam can speak for any more — a different failure from §7.3's holes, where
nothing was ever learned; a frozen cell learned something once and can no longer be asked
again. Everything RIGHT of it is still live. A sky-blue cell RIGHT of the rule is the frontier
itself, `Pending` — pressed, not yet run. A sky-blue cell LEFT of the rule would mean this
design missed a case — exactly the shape of the bug this classifier was built to close, where
an evicted tick's re-ask overwrote a real observation with `Pending` instead of filing nothing.

⛔ **THE FROZEN CELLS ARE CORRECT OBSERVATIONS AND ARE NEVER DIMMED OR HATCHED.** Position is
the only signal a reader needs — a second colour or a per-cell modifier would put a second axis
on an already-swept palette, the same drift §7.4's distinctness sweep exists to prevent, where
one rule's position cannot drift at all.

**It sits AT the frontier the frame after a resync.** A hard resync (`wipeCache`) collapses the
resident window to one tick, so `oldestResident == newestResident`, and the horizon and the
sky-blue frontier cell land on the SAME column — that is the correct picture of "everything
just got wiped", not a gap between two markers that failed to meet.

**A `Skip` step moves it two columns in one frame.** A `Skip` step pushes the correction cache
twice — once to backfill the tick it skipped, once for its own advance — so the residency edge
advances two sim ticks between polls and the rule steps two lane columns at once. That is
correct, not a flicker; report it as a bug and you have found the wrong thing.

**On a listen host — or standalone, or the dedicated server — it sits at the RIGHT edge, with
the no-cache readout line, never a sky-blue bar.** `hasCorrectionCache() == false` for every id
on a role with no correction cache, so the reading answers `NoCache`, the horizon anchors
`RightEdge`, and the provenance bar draws as pure holes (§7.3) rather than a wall of `Pending`
claiming every tick that role ran was merely pressed.

### 7.6 The toggle, and why it costs nothing

⚠ **SUPERSEDED BY THE MASTER GATE — see §7.10.** As shipped by this task, the meter is no
longer one toggle: it is three independent bar CVars behind a master, and
`OGBrawler.InputHistoryFrameMeter` no longer exists. What follows is kept for the
**shape** of the argument — an early-out above the actor walk, gated flags inside the
block — which §7.10 extends rather than replaces.

`OGBrawler.InputHistoryDisplay`, default **on**, independent of the three bar CVars so any
of the four can be shown alone. In `USimmableUpdateComponent::TickComponent` the flags are
read into locals and the early-out above the `firstLocalCharacterId` actor walk fires when
**none** is set; inside the block each poll is gated on its own flag (or, for the three bars,
on whether any is on). In `AOGBrawlerUEHUD::DrawHUD` each display is one branch.

### 7.7 The pause, the elision marker, and the cost that was accepted

A fixed window spent on a player standing still holds no signal, so both lanes stop
recording while nothing is happening. `laneTickIsInactive` is the predicate — the panel's
own `DirectionBucket::Neutral`, an empty `motionButtonMask`, and `DAttackState::Idle`,
all three — and `LaneIdleGate::admit` is the single place it is asked.

⛔ **ONE EVALUATION DRIVES BOTH LANES.** `pollInputHistoryLanes` calls `admit` once and
both lane writes obey the `LaneAdmission` it returns. Two lanes deciding for themselves
could disagree by a tick, and a column would then be one capture tick in the upper bar and
a different one in the lower — the failure §7.2 exists to prevent, arriving through a door
geometry alone cannot close. `Pause.OneGateDecidesForBothLanesSoAColumnCannotMeanTwoTicks`
runs a sequence built so that pausing on either conjunct alone stops in a different place.

**Storage moves onto a LANE tick.** Eliding a tick without remapping would leave the
retained window spanning the elided span, so a long idle would still scroll the activity
either side of it away — which is the whole thing the pause is for. `LaneIdleGate` therefore
owns the sim-tick to lane-tick mapping: `laneTickOf` answers `nullopt` for an elided tick and
subtracts the accumulated skip for every other, and the lanes store, evict and read entirely
in lane ticks. Nothing downstream changed, because `retainedLaneWindow` already read the
axis rather than the clock.

⚠ **HYSTERESIS IS ONE-SIDED.** The pause engages only after `kLanePauseEngageTicks`
consecutive inactive ticks — 15, a quarter-second at 60 Hz, far longer than the one- to
three-tick chatter a stick resting on the deadzone produces and an eighth of the default
window, so a false pause costs one cell. It resumes on the **first** active tick, with no run
of its own to serve first: being late to record something interesting is the one failure this
display cannot absorb.

⭐ **AN ELISION IS MARKED AND CARRIES ITS COUNT.** A collapsed span costs exactly one lane
tick, empty in both lanes, filed in the gate's ledger as a `LaneAxisEvent`.
`collectFrameMeterAxisEvents` hands the renderer its bar offset and its skipped-tick count, and
the HUD draws `kLaneElisionColor` across **every enabled bar** — a cell is one bar tall and this
deliberately is not — with the count above the backdrop. The marker cell is a hole, so §7.3's
rule applies unchanged and the runs either side keep their own lengths. Without it the bar
would imply the two runs were neighbours, which is the same fabrication a carried-forward
hole would be.

⛔ **PROVENANCE IS NOT IN THE PREDICATE, AND THAT COSTS SOMETHING.** A correction or a
resimulation landing while the player is idle is elided with everything else, so a rollback
while standing still is invisible. That was weighed and taken. The answer to it is that the
pause is a setting: `OGBrawler.InputHistoryPauseIdle`, **default on**, and setting it to 0
restores full-fidelity recording. Its help text says so, because a documented cost with an
undocumented remedy is not a documented cost. Unlike the two display toggles this one is a
behaviour knob rather than a developer overlay, which is why its default is the other way up.

⭐ **THE AXIS HAS A SECOND KIND OF CUT, AND IT IS NOT AN ELISION.** A hard resync ASSIGNS
the client's prediction tick — the only assignment `ClientPredictionClock` ever makes to it;
every other step is monotone — so the same tick numbers are simulated twice (a backward
resync) or never at all (a forward one). That is a discontinuity of the axis, not time the
display chose to remove, and `LaneAxisEventKind` is the word for the difference:
`Elision` and `Resync` share one ledger, one collection and one marker cell, and differ in
exactly two places — the colour (`kLaneElisionColor` against `kLaneResyncColor`, §7.11) and
the label. ⛔ **THE RESYNC'S LABEL IS SIGNED AND THE SIGN IS THE INFORMATION**: `−22` says the
client re-ran twenty-two ticks it had already run, `+22` says it never ran them. An elision's
count is unsigned because time going missing has no direction.

⛔ **THE SEAM IS THE DETECTOR, AND THE POLL'S OWN READING IS A CROSS-CHECK ON IT.**
`pollInputHistoryLanes` reads two things. The first is the clock's own `hardResyncCount`,
through `getDiagnostics()`, differenced against the reading the previous poll filed: a count
that moved is a resync in EITHER direction, and beside it the clock hands over the exact tick
it left (`lastHardResyncFromTick`) and the tick it landed on (`lastHardResyncToTick`), so the
label is the signed difference of two known numbers rather than a bound on one. The second is
`LaneIdleGate::lastPolledSimTick`, against the tick this poll was handed. A hard resync is the
only assignment the clock makes to that tick, so a tick behind the last polled one is certain
rather than likely.

⛔ **THE BACKWARD ARM IS HELD ONLY ON THE CLAIM BOTH ARMS CAN SPEAK TO.** It is
structurally blind forward — a tick the clock jumped ahead to is indistinguishable from a tick
that simply advanced — so it is asked about the seam's answer only when the seam itself reports a
BACKWARD pair, `lastHardResyncToTick < lastHardResyncFromTick`, and only on a poll where
`axisBreaksFromSeam` is non-zero. Holding it on the forward case too would have made it read as
a disagreement on every forward resync, which is the one thing it cannot see.
⛔ **A DISAGREEMENT IS COUNTED, NEVER RESOLVED.** When only one of the two fires,
`axisBreakDisagreements` records it and ONE break is still filed, from the seam's own numbers.
It is a finding about the two readings, not a tie for the display to break, and there is
deliberately no rule anywhere that prefers one arm over the other.

There is still deliberately **no** derived test on the residency edges, and the one that
existed was removed rather than narrowed. `WindowResidency::newestResident` is the newest
resident tick *inside the poll window*, clamped at the tick the game thread read;
`oldestResident` is the ring's own eviction edge, which is not clamped by anything.
The physics thread owns that ring and keeps pushing while the sweep runs, so between two
polls the old edge can move when the clamped new edge
cannot — and a difference of those two deltas therefore says *a step landed mid-sweep* exactly
as often as it says *the cache was wiped*. Separating them needs a magnitude tolerance, and
this meter allows none: a threshold a hitch can cross is a marker that lies about the clock.

**So a forward resync draws its own signed marker, and the derived arm stays deleted.** The two
facts are independent, and that is the point: the clock supplies the event, so no tolerance on
the ring's edges is needed to infer one. ⛔ **DO NOT REINTRODUCE THAT ARM.** It fired
several times a second on a healthy client, and each false fire also closed the open idle span
and zeroed the pause hysteresis, so the pause could never re-engage: one wrong marker cost the
whole of the idle elision this section is about. What the residency edges are honest about is a
STATE — the horizon jumps to the frontier and the residency line collapses to a one-tick span,
§7.5's own picture of everything having just been wiped — and that is what they still say. The
event is the clock's to report.

⛔ **BOTH CHOICES ARE MADE PER MARK.** `drawFrameMeterAxisEvents` asks `kind` inside its own
loop, so a `Resync` and an `Elision` at adjacent columns draw two cells in two colours — the
arrangement `FrameMeter.AResyncEndingAPausedSpanDrawsTwoAdjacentCellsOfDifferentKinds` pins,
because it is the one a renderer deciding once per ledger would get wrong.

**THE EPOCH MODEL — why a lane tick outruns its sim tick.** The gate's offset is what converts
between the two axes, and a backward resync pushes the LANE tick *ahead* of the sim tick: the
new epoch starts at the marker's next lane tick whatever number the clock assigned. So the
offset is signed (`int64_t`), and `laneTickOf` walks the ledger newest-first with one extra
rule per `Resync` entry — a tick at or after the new epoch's first belongs to the new epoch, a
tick strictly between the previous epoch's last polled tick and it belongs to a forward
resync's never-simulated range and has no cell at all, and anything older falls through to the
entries behind it, where the OLD epoch's mapping is untouched. ⛔ **OLD-EPOCH TICKS AFTER A
BACKWARD RESYNC ARE NOT REACHABLE BY NUMBER.** They are history: frozen left of the horizon,
which is exactly where §7.5's residency edge already puts them. A test expecting tick 6200 to
answer its FIRST lane tick "because it already has a cell" is asserting the defect.

⭐ **THE WORKED EXAMPLE IS THE USER'S OWN STORM, AND IT IS WHY THIS MARKER EXISTS.** A PIE
session run while this display was being built logged 128 identical lines — `hard resync oldTick=6219 -> newTick=6197 drift=-22`, at
about two a second for a full minute — and the frame meter showed **nothing at all**. The
reason is the gate's own early return: `admit` answers with the previous decision for
`simTick <= m_lastSimTick`, so after each backward resync the twenty-two re-run ticks mapped
onto lane ticks they already held. The lane axis did not advance for the whole minute;
provenance overwrote its history in place, and the machine lane and the delay lane's client
half — both first-sample-wins — kept their first-epoch samples. **The bar was not stale; it
was being rewritten under the reader.** With the axis event, the break bypasses that early
return, and the same minute draws a `−22` cell about every twenty-fourth column with the
horizon riding just right of each one: 23 recorded ticks per cycle plus the marker's own, so
the retained window scrolls through the loop at its true rate. The count of markers a bar can
hold at once is bounded by `kLaneElisionLedgerCapacity`, which a storm this long overruns —
the ledger drops its oldest, and `laneTickOf` answers `nullopt` rather than guessing once it
has, so an overrun costs reach and never fabricates a column.

### 7.8 The authority marker, and the three places its target can hide

⚠ **The offset is the estimator's, and there is no second derivation of it anywhere.**
`NetworkTimeEstimator::getPredictionOffsetTicks` is the number of ticks the target tick sits
above the authority tick — the estimator forms its target as `authorityTick` plus exactly
that — so subtracting it back off the client's own prediction tick names the authority tick.
`authorityTickOf` is the one place that subtraction happens, and it floors at tick 0 so an
early-session offset larger than the tick count cannot wrap into the top of the tick space.
The value moves with RTT and jitter every time a timing buffer lands, so nothing about the
marker is cached: `frameMeterAuthorityMarkerOf` re-resolves it on every draw from the
reading the most recent poll filed.

⛔ **ONE SNAPSHOT PLACES THE MARKER, AND IT IS THE POLL'S.** This is the defect the first
version of this display shipped, so it is worth stating as a rule rather than as a detail.
The lane axis is built inside `pollInputHistoryLanes` from that poll's own `liveSimTick`;
the marker used to be built from a *second*, independent read of the same physics-written
prediction tick, taken later in the frame at `drawInputHistoryFrameMeter`. On any frame
where a physics step landed between the two, the rule sat one column away from the number
printed under it — a marker seen flickering between two cells while its label held still.
`noteAuthorityReading` now files the estimator's offset against the very tick the axis is
noted from, and `frameMeterAuthorityMarkerOf` takes **no reading parameter at all**, so a
draw-time read has nowhere to enter.
⛔ A tolerance, a clamp or a smoothed position would have hidden the disagreement instead
of removing it, on the one display whose whole claim is that the server is *here*.

⚠ **The reading is filed BEFORE the gate can end the poll early**, so an elided poll still
moves it. A reading frozen at the last recorded tick would leave the rule on a column long
after authority had passed every cell on the bar — which is exactly the idle steady state
`InsideOpenSpan` below describes.

⛔ **THE DISPLAY HOLDS NO `edit*` HANDLE.** The offset is read through `getNetworkEstimator`,
which is `const`, at the poll rather than at the draw. The adapter's two `editNetworkEstimator`
call sites are the game-thread WRITE path in `onTimingInfoReceived` and are untouched by this.
The read is guarded on `runsPrediction`, because a role with no client clock has no estimator
either — and a dedicated server, a listen-server host and a standalone session are all such
roles. ⛔ The draw holds the lanes as a pointer to `const`, so `noteAuthorityReading` is
  unreachable from it: only a poll can file a reading.
⭐ **`AuthorityMarkerKind::NoEstimate` is what those roles get, and it draws nothing at all.**
There is no prediction offset on an authority manager, so inventing one would be a fiction in
the one display built to say where authority is.

**THREADING.** `getPredictionOffsetTicks` reads `m_smoothedRTT`, `m_smoothedJitter` and
`m_hasFirstSample`, all of which the estimator's own contract declares GAME-THREAD-written
by `updateRTT`. The poll runs on the game thread, so this is a same-thread read and nothing
can tear. ⭐ **The prediction tick is no longer read here at all**: the marker is paired with
`liveSimTick`, the value `USimmableUpdateComponent::TickComponent` had already read once for
the whole visualization block, so this display now takes **one** physics-written read per
frame where it used to take two. That single read is still the accepted tear argued at the
CROSSING block in `SimulationManagerUImpl.h` — a four-byte aligned load that cannot tear and
at worst is one tick stale — but a stale tick now moves the axis and the marker together.

⛔ **THE MARKER IS NEVER CLAMPED AND NEVER HIDDEN.** The lane axis is compacted (§7.7), so
the authority tick can name time that has no cell — and a player who stood still while the
connection lagged is exactly that case rather than an exotic one. Both obvious repairs are
lies: clamping to the nearest recorded tick points at a tick the server is demonstrably not
on, and omitting the marker reads as "no data" when the truth is "inside that collapsed
span". `frameMeterAuthorityMarkerOf` therefore answers with a KIND and an ANCHOR, and the
kinds are exhaustive:

| kind | what is true | anchor |
|---|---|---|
| `NoEstimate` | the role does not predict | `None` — nothing is drawn |
| `OnCell` | the tick has a lane cell of its own | its column, or the edge the window left it beyond |
| `OnElidedSpan` | it is inside a span the gate has closed | that span's own marker cell |
| `InsideOpenSpan` | it is inside the span being collapsed right now | `RightEdge` — no cell exists yet |
| `TooOldToPlace` | the gate's `LaneAxisEvent` ledger no longer reaches it | `LeftEdge` |

⭐ **`InsideOpenSpan` is the steady state while standing still, not a corner case.** The
offset floor is `predOffsetFloorTicks` = 4 and the pause engages after `kLanePauseEngageTicks`
= 15 consecutive inactive ticks, so from the sixteenth idle tick onward the authority tick is
inside a span that has not been filed yet and has no cell to land on. The gate's own
`m_offset` is private and the span's future lane tick is therefore underivable from outside,
which is why this anchors on the bar's right edge rather than on a column: the column that
span will occupy has not been created.

⚠ **`TooOldToPlace` and `InsideOpenSpan` are told apart WITHOUT reaching into the gate.**
`laneTickOf` answers `nullopt` for both, so the discriminator is whether the target lies past
every span already in the ledger. A tick behind the oldest retained `LaneAxisEvent` cannot be
idle time in progress no matter what `paused` says, and reading it as such would have put an
"off the left edge" marker at the right edge instead.

⛔ **AN OFF-BAR MARKER MUST NOT LOOK LIKE A COLUMN MARKER**, because a target at column 0 and
a target older than column 0 share an x exactly. `authorityMarkerStyleOf` returns
`kFrameMeterAuthorityStyle` for a column and `kFrameMeterAuthorityOffBarStyle` otherwise —
half-lit and thinner — so the two cannot draw the same rule.

**And it must not look like the frozen horizon** (§7.5). The two vertical markers mean
opposite things: one says the correction cache can no longer answer for anything left of it,
the other says the server is there. A reader who swapped them would reach the opposite
conclusion about a desync, which is worse than seeing neither. Both styles are
`FrameMeterMarkerStyle` values in the pure header — colour, alpha, thickness and shape — and
each authority style differs from `kFrameMeterHorizonStyle` in **all four**, so no single edit
can collapse the pair. The authority colour additionally clears `kLanePaletteMinPairGap`
against every colour either bar can put beneath it, because it is a rule drawn over cells
rather than beside them.

⭐ **A THIRD KIND OF MARKER SHARES THIS VOCABULARY, AND ALL FOUR STYLES ARE SWEPT AGAINST
EACH OTHER.** The rate marks (§7.12) are the clock's own corrections to the rate time arrives
at — a skip inserted a tick, a stall withheld one — and they are neither a rule across the
bars nor a cell. Every marker style is a `FrameMeterMarkerStyle` value in the pure header, and
`Authority.NoTwoOfTheFourMarkerStylesCanRenderIdentically` sweeps all six pairs among them:

| style | what it claims | colour | alpha | thickness | shape |
|---|---|---|---|---|---|
| `kFrameMeterHorizonStyle` | the cache answers for nothing left of here (§7.5) | grey `0.55,0.55,0.58` | 0.45 | 1 | `PlainRule` |
| `kFrameMeterAuthorityStyle` | the server is on this column | white | 1.00 | 3 | `LabelledRule` |
| `kFrameMeterAuthorityOffBarStyle` | the server is off the bar, that way | white | 0.55 | 2 | `LabelledRule` |
| `kFrameMeterRateMarkStyle` | the clock corrected its rate at this boundary | `kLaneResyncColor` | 0.80 | 4 | `SignedTickMark` |

⛔ **ONE PAIR IS EXCEPTED, AND IT IS THE PAIR THAT IS ONE MARKER IN TWO STATES.** The
authority style and its off-bar form share colour and shape deliberately and are told apart by
alpha and thickness alone; every other pair differs in all four fields, and each swept colour
pair clears `kLanePaletteMinPairGap`. The sweep asserts its own pair count, so a fifth style
left out of the array fails it rather than passing quietly.

⭐ **THE BETWEEN-SLOT GRAMMAR — WHERE A THING IS DRAWN SAYS WHAT KIND OF THING IT IS.**

| drawn as | grammar | what it is |
|---|---|---|
| a cell one column wide, across every enabled bar | **ON** a column | that lane tick was a cut in the axis — an elision or a resync (§7.7) |
| a rule the full height of the backdrop | **ON** a column, or on the bar's own edge | where the server is, or where the cache's reach ends |
| a short mark in the label band, on a column EDGE, with a glyph beside it | **BETWEEN** two columns | the clock inserted or withheld a tick there |

A skip and a stall consume no lane tick — both ticks of a skip are recorded and a stall repeats
a frame — so a rate mark has no column of its own to claim, and drawing it on one would be the
fabrication §7.3 forbids for a hole. The two kinds sit on OPPOSITE edges of their column for a
physical reason: a skip at `P` arrived together with `P−1`, which is a backfilled copy, so the
jump is on the LEFT edge of `P−1`'s column; a stall at `T` is a step that passed with no tick at
all, so it is on the RIGHT edge of `T`'s column. `rateMarkX` is `frameMeterCellX` of that column
plus one `cellStride` when `rightEdge`, which puts the two kinds exactly one stride apart on one
column and is the whole reason neither can be read as the other. The glyph is the sign of the
tick DISPLACEMENT — `+` inserted, `−` withheld — which is the resync label's own convention, and
it is DERIVED from `kind` at the draw rather than stored beside it.
⛔ **THE COLUMN COMES OFF `placeFrameMeterSimTick`, NEVER OFF A LANE TICK MINUS ONE.** A
closed elided span takes that span's own marker cell, on its left edge whichever kind landed
there; the span still being collapsed, and a tick the `LaneAxisEvent` ledger no longer reaches,
are **not drawn at all**. Those marks survive only in the clock line's counts, which is a stated
loss and not an oversight — a mark placed on a column that is not its own would be worse.
⚠ **The mark crosses no cell, and that is what buys it its colour.** It names
`kLaneResyncColor`, because a rate mark and a resync are one clock's two grades of correction,
and it needs no palette clearance of its own against the cell colours below it precisely because
it never sits over one. The two are told apart by KIND rather than hue: a cell across every bar
carrying a signed *number*, against a mark above the bars carrying a signed *glyph* — the same
distinction the elision cell and the horizon rule already have from each other.

⚠ **The offset value is printed BELOW the bars.** An elision count is printed above them
(§7.7). Both are bare numbers beside the same meter and they mean entirely different things,
so `frameMeterAuthorityLabelTopY` and `frameMeterElisionLabelTopY` put them on opposite sides
rather than near each other.

⚠ **ONE OPEN QUESTION FOR THE LEAD, recorded rather than decided.** The marker is placed at
`prediction − offset`, which is what was asked for and what the acceptance criterion states.
`NetworkTimeEstimator::getLastAuthorityTick` is a second, independent answer to "where is the
server", and the two differ by exactly the clock's current drift — the estimator's identity
holds between its own target and its own authority tick, not between the clock's live
prediction tick and that authority tick. Marking `prediction − offset` keeps the printed
number and the visible cell gap in agreement, which is what a reader counts on; marking
`getLastAuthorityTick` would instead make the gap show the drift. Both are defensible and
only one can be the marker. This shipped as specified; the alternative is one accessor away.

### 7.9 The delay verdict bar, the readout, and where the CVar's cost really goes

⚠ **The CVar default and the DrawHUD branch below are SUPERSEDED BY §7.10** — the delay bar
is now one of three independently-toggled bars behind a master, and its own CVar defaults
**on** rather than off. The field sources, the verdict rules and the threading argument below
are unchanged by that and still hold as written.

The third piece is `OGBrawler.InputHistoryInputDelay`. It draws no display of its own — it is
a THIRD BAR on the existing frame meter plus one line of text under it — so turning it on
alone still opens the meter branch even with the other two bars off, and
`AOGBrawlerUEHUD::DrawHUD`'s second branch is `anyBarEnabled()` (§7.10), which folds all
three bar toggles and the master into one call.

**The source of every field, and why none of it is a new read.** All three inputs the
decomposition needs — the replicated tier consumer, the shared `TimeConfig`, and the
resolution peer's published effective-delay atomic — are members `pollInputHistoryLanes`
already runs on, on the GAME THREAD, for `predictionOffsetTicks` and the machine-state
sample. `decomposeInputDelay(consumer.hasReceivedTier(), consumer.currentTierIndex(),
m_manager->getTimeConfig(), consumer.effectiveInputDelayTicks(),
m_inputResolution.getClientEffectiveInputDelayTicks())` runs **inside** that same passthrough,
as its own local `std::optional`, guarded on `includeDelay && m_replicatedTierConsumer &&
m_manager` exactly like the offset read beside it. ⛔ **NO NEW PUBLIC ACCESSOR AND NO NEW
PASSTHROUGH NAME.** The NARROW PASSTHROUGHS list in `SimulationManagerUImpl.h` is therefore
**unchanged** — `pollInputHistoryLanes` was already the one name on it that this display
reaches through, and three more same-thread reads inside an existing entry point widen what
that one entry point does, not the set of entry points.

**The same-thread argument, stated once rather than per field.** `ReplicatedTierConsumer`'s
tier is GAME-THREAD-written (`applyReplicatedConnectionTier`, §5 of the manager rationale);
the shared `TimeConfig` is GAME-THREAD-bound at `BeginPlay` and mutated only by the OnRep
listeners, also GAME THREAD; and `SimulationInputResolution::getClientEffectiveInputDelayTicks()`
reads the atomic that `recomputeAndPublishEffectiveInputDelay` — GAME THREAD — writes, and
that the PHYSICS thread only ever *loads*. `pollInputHistoryLanes` itself runs on the game
thread, driven from `USimmableUpdateComponent::TickComponent`. Three GAME-THREAD readers of
three GAME-THREAD writers is not a crossing at all, let alone a tear — it is the same
same-thread shape §1 already argues for the offset. **The CROSSING block's read clause in
`SimulationManagerUImpl.h` now says so explicitly**, and `SimulationManagerUImpl-rationale.md`
§1 states why the passthrough list did not need a fourth name.

**The verdict, total over one cell** (`InputDelayVerdict`, `delayVerdictOf` —
`BrawlerInputHistoryVisualizationLanes.h`), rules applied in this order:

| order | condition | verdict | reads as |
|---|---|---|---|
| 1 | `serverNamedNoCapture` | `NoCaptureNamed` | the server substituted; checked FIRST so a Sentinel join never falls through |
| 2 | no server half | `NoVerdict` | a hole — nothing claimed yet |
| 3 | no client half | `LagUnverified` | a lag is shown, no comparison possible |
| 4 | `lag == D` | `Agree` | |
| 5 | `lag == D − 1` | `LagShortByOne` | its OWN class — §below |
| 6 | `lag > D` | `ServerLater` | a late release, or the server's delay is larger |
| 7 | otherwise (`lag < D − 1`) | `ServerEarlier` | the server's delay reads SMALLER — impossible from lateness alone |

**Why `LagShortByOne` is its own class and is never folded into `Agree`.**
`m_lastUsedCaptureTicks` — the server's record of which capture it applied, which
`serverLagTicks` is derived from — is written on the PHYSICS thread and read on the GAME
thread by the send, and its own declaration documents a torn schedule carrying the previous
tick's value for one tick, healed by the next correction. A correction can therefore pair the
state of tick `T` with the capture consumed at `T + 1`, reading one tick short of the client's
own delay. Folding that into `Agree` would be a tolerance hiding a documented slip; a distinct,
quiet class shows it without treating it as a fault. If PIE shows `LagShortByOne` as the
*common* verdict rather than the rare one, that is itself the finding — see T24.

⚠ **THE EIGHT-ROW `(panel, meter, delay)` TABLE THIS SUBSECTION USED TO CARRY IS RETIRED —
see §7.10 for the current (panel / any-bar / delay) shape and the master-off row.** `meter`
stopped naming one flag the moment the frame meter split into three independently-toggled
bars; the historical two-bar table and its RED probe belong to T23's shipped state and are
kept only in this repository's history, not restated here to avoid a second, competing table.

⛔ **THE TIER AND THE BASE DELAY ARE TWO LABELLED FIELDS, NEVER AN ARROW.** The tier arm
renders `tier 2  base 3` and the no-tier arm `no tier  fallback base 4` — ONE state either
way: the tier that arrived, and the unfloored base delay that tier implies. It used to render
`tier 2 → 3`, and the first reader to meet it in a PIE session took that for a TRANSITION from
tier 2 to tier 3.
⛔ **THAT ARROW COULD NEVER HAVE DISAMBIGUATED ITSELF.** `rttTierInputDelays` is
`{ 1, 2, 3, 4 }` indexed by tier 0..3, so the base is the tier index plus one at every shipped
tier without exception — `tier 0 → 1`, `tier 1 → 2`, `tier 2 → 3`, `tier 3 → 4` — and no value
the line could take would have separated "tier 2, base 3" from "moved from 2 to 3". The only
tell was that tiers stop at 3, so `tier 3 → 4` named a tier that does not exist.
⛔ **SO DO NOT RESTORE THE ARROW AS A TIDIER RENDERING.** A glyph that reads as movement over
data that never moves is the exact failure class this display exists to stop telling, and it
shipped because everyone who reviewed it read it as its authors, already knowing what it meant.
⭐ **THE BARE WORD `arrived` WENT WITH IT.** It restated `tierKnown`, which the two arms
already say — `tier N` against `no tier` — and standing as a peer field with no subject it read
as a verb on whichever number happened to sit beside it.

⚠ **A DIVERGENCE THIS BAR WILL SHOW ON DAY ONE, AND IT IS NOT NEW.**
`ConnectionTierTable-rationale.md` §8: *"the core never publishes tier 0 as a FIRST value...
the client sits on the pre-arrival fallback while the server parks at tier-0 delay. That
divergence is today's behaviour, is preserved deliberately."* On any wire that never leaves
tier 0 — the ordinary shape of a short LAN session — every corrected tick will therefore read
`ServerEarlier` (or `ServerLater`, depending on which side is larger at the shipped floor) for
the whole session, and the readout will say `no tier  fallback base N` throughout. **This is
EXPECTED on LAN, not a regression the delay bar introduced** — it is the same standing
divergence the tier system has carried since before this display existed, now visible instead
of silent. T24's PIE session confirms it is present and reads correctly.

### 7.10 The gate hierarchy — one master, four children, and the compaction it buys

**The shape.** `OGBrawler.InputHistory` (`GInputHistory`), DEFAULT **OFF**, is the one CVar
every other input-history CVar folds through. Four children sit under it, and — unlike every
toggle this display has shipped before — all four default **ON**:
`OGBrawler.InputHistoryDisplay` (the row panel), `OGBrawler.InputHistoryProvenance`,
`OGBrawler.InputHistoryInputDelay` and `OGBrawler.InputHistoryCharacterState` (the frame
meter's three independently-toggled bars). `OGBrawler.InputHistoryFrameMeter` and
`frameMeterEnabled()` are **retired outright** — the meter is no longer one flag, it is
three bars, each gated on its own kind.

⭐ **THE NET DEFAULT IS UNCHANGED: nothing displays until the master is set.** Inverting four
children to `true` while the fifth CVar (the master) stays `false` looks alarming read in
isolation, and this initiative has shipped exactly that alarm for real once before — see the
`G*` census discipline below. `GInputHistory && GInputHistoryDisplay` is `false` at every
shipped default; the same holds for all three bars. A fresh install shows nothing, exactly
as every prior version of this display did.

**Why the master is folded INSIDE the accessors, never checked again at a call site.**
`displayEnabled()` returns `GInputHistory && GInputHistoryDisplay`, and `provenanceEnabled()`,
`inputDelayEnabled()` and `characterStateEnabled()` each fold it in the same way.
`barSelection()` builds a `brawlerInputHistoryVisualization::FrameMeterBarSelection` from the
three bar accessors, and `anyBarEnabled()` asks the pure header's own
`frameMeterEnabledBarCount` whether that selection is non-empty rather than OR-ing three
locals at the call site. Folding once, in the accessor, makes "every site looks at the
master" true **by construction** — a call site cannot forget a check it never makes, because
it never reads `GInputHistory` at all. `masterEnabled()` is the one deliberate exception: it
reads the raw master alone, and only the poll's own early-out calls it, so that path costs
exactly one `bool` read rather than one per feature.

⛔ **A CHECKER PROVES THIS RATHER THAN ASSERTING IT.** `tools_task26_houserules.py`'s
zero-direct-read arm scans every `G*` display bool
(`GInputHistory`, `GInputHistoryDisplay`, `GInputHistoryProvenance`,
`GInputHistoryInputDelay`, `GInputHistoryCharacterState`) across every file this task
touches and requires every occurrence to be either that variable's own declaration, its
`FAutoConsoleVariableRef` registration, or inside the accessor block bounded by the
`⛔ THE ACCESSOR BLOCK` / `⛔ END OF THE ACCESSOR BLOCK.` markers in
`InputHistoryVisualizationUImpl.cpp` — **0 violations**, run against the shipped tree and
proved to discriminate by its checker control (a seeded read inside `panelScale()`, outside
the block, fires exactly one violation).

**Retiring `InputHistoryFrameMeter` properly.** The CVar, `GInputHistoryFrameMeter` and
`frameMeterEnabled()` are all deleted, not merely unused — a surviving accessor nothing calls
would be a second, silent source of truth about whether the meter draws. The checker's
retired-symbol sweep confirms zero remaining references to any of the three, anywhere in this
task's files.

**The gate** (`SimmableUpdateComponent.cpp`). The FIRST statement in the input-history poll
block is now `if (!inputHistoryVisualizationUImpl::masterEnabled()) return;` — one `bool`
read on the default path. Only past that does the block read `displayEnabled()`,
`anyBarEnabled()` and `inputDelayEnabled()` into locals and apply the same early-out shape
this section always has: `if (!feedRowPanel && !feedAnyBar) return;`, still above the
character-id walk. The row poll is gated on `feedRowPanel` alone; the lane poll — both the
provenance/machine-state halves AND the delay decomposition — is gated on `feedAnyBar`, and
carries `feedInputDelay` through to `includeDelay` exactly as T23 shipped it.

**The cost table**, read mechanically from the gate, not measured at runtime (no LLT target
links this file):

| master | panel | any bar | delay | reachable | cost |
|---|---|---|---|---|---|
| off | — | — | — | yes | **one** `bool` read (`masterEnabled()`) and `return` — no further CVar read, no walk, no poll |
| on | off | off | off | yes | the master read + three feature reads + `return` — no walk, no poll |
| on | on | off | off | yes | the walk + the row poll only |
| on | off | on | off | yes | the walk + the lane poll, `includeDelay=false` |
| on | off | on | on | yes | the walk + the lane poll, `includeDelay=true` — the row poll is NOT run |
| on | on | on | off | yes | the walk + both polls |
| on | on | on | on | yes | the walk + both polls, decomposition included |
| on | off | off | on | **no** | unreachable by construction — `inputDelayEnabled()` is one of the three flags OR'd into `anyBarEnabled()`, so delay on forces any bar on |
| on | on | off | on | **no** | unreachable, same reason |

The two unreachable rows are not a gap in the table, they are the same fact `anyBarEnabled()`
proves structurally: the delay bar is one of the three bars, not a display of its own, so it
cannot be on while every bar reads off.

**Compaction, and where it lives.** `AOGBrawlerUEHUD::drawInputHistoryFrameMeter` asks
`inputHistoryVisualizationUImpl::barSelection()` once, computes
`barCount = frameMeterEnabledBarCount(selection)`, and for each of the three
`FrameMeterBarKind`s asks `frameMeterBarSlotOf(selection, kind)` for that bar's slot among
the *enabled* bars — `nullopt` draws nothing, a slot draws at that row. That is the entire
compaction rule, and it is T25's, reused verbatim; nothing here recomputes a slot with an
if-chain or a local counter. Traced against three of the reachable rows above:

* **CharacterState only.** `barSelection() == {false, false, true}`,
  `frameMeterEnabledBarCount == 1`. `frameMeterBarSlotOf(…, Provenance)` and
  `(…, InputDelay)` both answer `nullopt` — neither bar nor the delay readout draws.
  `frameMeterBarSlotOf(…, CharacterState) == 0`, so the machine-state palette draws at the
  meter's only row, slot 0.
* **Provenance + CharacterState, delay off.** `{true, false, true}`, count `2`.
  `frameMeterBarSlotOf(…, InputDelay) == nullopt` — the delay bar and its readout are
  absent. `frameMeterBarSlotOf(…, Provenance) == 0`, `frameMeterBarSlotOf(…, CharacterState)
  == 1` — CharacterState sits at slot 1, one row up from where it would sit with delay
  present.
* **All three off.** `anyBarEnabled()` is `false`, so `AOGBrawlerUEHUD::DrawHUD` never calls
  `drawInputHistoryFrameMeter` at all — no geometry is computed, matching T25's zero-bar
  guard from the *caller's* side rather than only inside `frameMeterGeometryFor`. The method
  also re-checks `barCount == 0u` on entry and returns before any geometry call, so a direct
  call (were one ever added) stays safe too.

**The declaration order in `FrameMeterBarKind` is still the only place the display order
lives** — `Provenance, InputDelay, CharacterState`, top to bottom, per the user's Phase 4
request. This task adds no second copy of that ordering; it only supplies the CVar-backed
selection the pure header's own rule consumes.

⭐ **THE `G*` CENSUS, RE-PROVED FOR THE INVERTED DEFAULTS.** This initiative once shipped
`GInputHistoryDisplay` and `GInputHistoryFrameMeter` both defaulting `true` while every guard
and doc said OFF — caught late, by accident, and it is why a `G*` initialiser census exists
at all (§7.6's history, and T16's must-fix). Inverting the four children to `true` here is
precisely the moment that census must be **re-proved**, not quietly relaxed because the rule
changed. `tools_task26_houserules.py`'s census asserts exactly: `GInputHistory = false`,
`GInputHistoryDisplay = true`, `GInputHistoryProvenance = true`,
`GInputHistoryInputDelay = true`, `GInputHistoryCharacterState = true`, and that
`GInputHistoryFrameMeter`'s initialiser no longer exists. A RED probe flipped the master's
default to `true` in place on the shipped file and reran the census: it failed on exactly
`GInputHistory` (`true`, expected `false`) and nothing else, predicted before the run and
matched exactly; the file was restored and its SHA-256 matched the pre-probe hash; both the
Editor and Server targets were rebuilt green afterward.

**"How do I turn this on" — for T24's reader.** One console command is enough:
`OGBrawler.InputHistory 1`. Every child defaults on, so the master alone turns on the row
panel and all three frame-meter bars; a reader who wants fewer than all four turns the
unwanted children off individually afterward. Turning the master back to `0` costs one `bool`
read again and hides everything, whatever the children are set to.

### 7.11 The colour legend — what each cell means, and what this table does NOT prove

Nothing else in this document maps a colour to its meaning. §7.4 gives the palettes'
distinctness PROPERTIES and §7.9 gives the verdict RULES, but the only place a name is bound
to an RGB triple is the `switch` arms of `provenanceCellStyleOf`, `machineCellStyleOf` and
`delayVerdictStyleOf` in `BrawlerInputHistoryVisualizationBars.h`. Reading a bar on screen —
with `OGBrawler.InputHistory` on — has meant reading that header. The four tables below are
that binding, written down.

⭐ **THIS TABLE IS CHECKED, NOT TRUSTED.** Palette values are each implementer's own choice —
the suite asserts RELATIONS on them (pairwise gaps, cross-palette floors, `ServerEarlier`'s
strict-max isolation), never literals, so a prose copy of today's colours would go stale
silently the first time anyone retunes a hue. `tools/lint/palette_legend_lint.ps1` parses both
this section and the three switches above and fails on any disagreement between them — an
enumerator with no row, a row naming no enumerator, an RGB mismatch, or a hole documented as a
colour (or the reverse). Its row counts are asserted against `kRowProvenanceSummaryCount`,
`kMachineStateCellCount` and `kInputDelayVerdictCount` — read from their own declaring
headers, never a literal — so an enumerator added without a table row fails the lint the same
way the existing palette sweeps fail the suite.

⭐ **THE RGB COLUMN IS LINEAR, NOT WHAT THE SCREEN SHOWS — READ THE HEX COLUMN INSTEAD.**
`meterCellColor` (`OGBrawlerUEHUD.cpp`) builds `FLinearColor(color.r, color.g, color.b, 1.f)`
straight from the floats below and hands it to Unreal, which gamma-encodes it for display.
Reading a linear float as if it were an sRGB byte gives a materially wrong, and always
DARKER, colour: `LagShortByOne`'s `0.00, 0.14, 0.52` looks like a dark navy under a naive
`× 255` (`#002485`) but actually renders as a MEDIUM AZURE (`#0069BF`). Every row in this
section is darker on paper than it is on screen. The **On screen (sRGB hex)** column below
is the byte value that actually renders, computed per channel by `s = 12.92·L` when
`L ≤ 0.0031308`, else `s = 1.055·L^(1/2.4) − 0.055`, then `round(s × 255)` clamped to `0..255`
— the standard linear-to-sRGB transfer function. It is GENERATED by
`palette_legend_lint.ps1 -EmitHex` from this section's own RGB floats, never hand-computed,
and re-derived and checked against this table on every ordinary lint run, so a float retuned
without regenerating its hex fails the same way an RGB mismatch does.
⚠ **Honest limit**: this hex is the colour AS SUBMITTED — opaque, alpha 1.0. It does not model
the translucent meter backdrop a cell is actually composited over, nor any monitor calibration
or colour-profile difference; it is only what `FLinearColor(color.r, color.g, color.b, 1.f)`
becomes once Unreal's own pipeline gamma-encodes it, nothing more. Do not "simplify" this
table by deleting the hex column as redundant with the floats — the two numbers answer
different questions, and only the hex answers "what does this look like on screen."

⛔ **WHAT THE LINT DOES NOT CHECK.** It can verify that a name, an RGB triple, the sRGB hex
derived from it, and a `LaneCellFill` kind agree between the header and this table. It
**cannot** verify that a MEANING sentence, or the plain-English **Colour** word beside it,
are true — both are prose, and nothing mechanical reads either of them. A clean lint run is
not a claim that any description or colour name here is accurate, only that the names, RGB
values, sRGB hex and fill kinds it can parse agree with the shipped header.

#### Provenance palette

Nine states, `RowProvenanceSummary`, the input panel's own colours reused value for value
(§7.4). All nine are drawn — none is a hole.

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `RowProvenanceSummary::Unknown` | 0.42f, 0.42f, 0.42f | #ADADAD | medium grey | Nothing ever joined this tick — never observed, or the join hit the documented-ambiguous sentinel. |
| `RowProvenanceSummary::Pending` | 0.20f, 0.45f, 0.95f | #7CB3F9 | sky blue | No correction-cache slot for this tick ABOVE the frontier — pressed, not yet run; normal inside the input-delay window. A missing slot BELOW the frontier files nothing at all, never this colour — see §7.5. |
| `RowProvenanceSummary::NoStateWritten` | 0.35f, 0.30f, 0.55f | #A095C4 | dusty lavender | A slot exists for this tick, but nothing has written its state yet. |
| `RowProvenanceSummary::RanUnconfirmed` | 0.88f, 0.88f, 0.88f | #F1F1F1 | near-white grey | Ran as a prediction and no authority has spoken on it yet — the ordinary client steady state. |
| `RowProvenanceSummary::LineageUnavailable` | 0.70f, 0.60f, 0.18f | #DACB76 | dull gold | The authority named this capture, but its lineage could not be read. |
| `RowProvenanceSummary::Confirmed` | 0.15f, 0.80f, 0.30f | #6CE795 | mint green | The authority certified the prediction, so the state copy was skipped. |
| `RowProvenanceSummary::Corrected` | 1.00f, 0.55f, 0.05f | #FFC43F | amber | The authority disagreed with the prediction and its state was adopted instead. |
| `RowProvenanceSummary::Resimulated` | 0.10f, 0.82f, 0.95f | #59EAF9 | bright cyan | Re-run during a rollback. |
| `RowProvenanceSummary::ProvenanceLie` | 1.00f, 0.05f, 0.75f | #FF3FE1 | hot pink | Must never appear on a real bar — its zero-occurrence count is the guarantee `SlotStateProvenance.h`'s fence exists to prove, not a state a reader should expect to see. |

#### Machine-state palette

`MachineStateCell` — the four `DAttackState` values plus `NotSampled`, the lane's own hole for
a tick the poll never sampled at all (§7.3, §7.4). Four colours drawn, one hole.

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `MachineStateCell::NotSampled` | — (hole; draws nothing) | — (hole; draws nothing) | — (hole) | The poll never captured a machine-state sample for this tick. A hole, not a state — this lane is append-only at the live tick and has no per-tick history to back-fill from. |
| `MachineStateCell::Attacking` | 0.85f, 0.10f, 0.10f | #ED5959 | coral red | The character is mid-attack — a radial swing or a cast is committed. |
| `MachineStateCell::Idle` | 0.05f, 0.30f, 0.25f | #3F9589 | dark teal | Not attacking and not flinching — the default resting state. |
| `MachineStateCell::GuardFlinch` | 0.50f, 1.00f, 0.05f | #BCFF3F | lime green | Knocked into guard-flinch by a blocked hit. |
| `MachineStateCell::HitFlinch` | 0.55f, 0.00f, 0.85f | #C400ED | vivid violet | Knocked into hit-flinch by a confirmed inbound hit. |

#### Input-delay verdict palette

`InputDelayVerdict` — the six verdicts `delayVerdictOf` can reach plus `NoVerdict`, the hole
for a cell nothing has written yet (§7.9's rule table gives the ORDER these are decided in;
this table gives what each one looks like and means in isolation).

| Enumerator | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `InputDelayVerdict::NoVerdict` | — (hole; draws nothing) | — (hole; draws nothing) | — (hole) | Nothing claimed yet — there is no server half to compare against. |
| `InputDelayVerdict::Agree` | 0.00f, 0.84f, 0.00f | #00EC00 | bright green | The server's reported lag equals the client's own delay. |
| `InputDelayVerdict::LagShortByOne` | 0.00f, 0.14f, 0.52f | #0069BF | medium azure | The server's reported lag reads exactly one tick short of the client's delay. Two different causes reach exactly this verdict: the documented send-side tear (§7.9), and — independently — the standing tier-0 connection divergence at the shipped ini floor of 3, where the client resolves `max(3,4) = 4` and the server resolves `max(3,1) = 3`, so `lag == d − 1` on every corrected tick without the tear ever firing. The two are indistinguishable from this colour alone; see the floor-dependence table in this initiative's workspace current_state.md document for which ini floors mask, collide with, or cleanly separate them. |
| `InputDelayVerdict::ServerLater` | 0.76f, 0.50f, 0.50f | #E2BCBC | dusty pink | The server's reported lag is larger than the client's delay — a late release, or the server itself running behind. |
| `InputDelayVerdict::ServerEarlier` | 0.36f, 1.00f, 1.00f | #A2FFFF | pale cyan | The server's reported lag reads SMALLER than the client's delay by more than one tick — impossible from lateness alone, so it always names a real divergence. |
| `InputDelayVerdict::LagUnverified` | 0.80f, 0.08f, 1.00f | #E750FF | bright magenta | The server names a lag, but there is no client-side delay reading to compare it against. |
| `InputDelayVerdict::NoCaptureNamed` | 0.47f, 0.02f, 0.12f | #B62761 | dark crimson | The server substituted — it named no capture for this tick at all. Checked FIRST, so a Sentinel join never falls through to a weaker verdict. |

#### Non-enumerator markers

Three colours drawn on the bars that belong to no enumerator at all — module-level constants,
not switch arms, so they are checked by value but are deliberately **not** counted against any
of the three constants above.

| Name | RGB (as written in the header) | On screen (sRGB hex) | Colour | Meaning |
|---|---|---|---|---|
| `kLaneElisionColor` | 0.62f, 0.32f, 0.00f | #CE9900 | burnt amber | Marks a collapsed idle span, one cell wide on both bars regardless of how many ticks it swallowed — missing time, not a state, so it clears the cross-palette floor against every real colour in both palettes (§7.7). |
| `kLaneResyncColor` | 0.05f, 0.00f, 1.00f | #3F00FF | electric indigo | Marks a HARD RESYNC — the one cell where the lane axis was cut, and the same tick numbers were simulated twice (a backward resync) or never at all (a forward one). BOTH DIRECTIONS ARE ANSWERED BY THE CLOCK'S OWN `hardResyncCount`, never by the correction ring's edges: a derived test cannot tell a wipe from a push that landed mid-sweep, and rather than keep the forward mark on one, the mark was withdrawn until the clock could be asked. The poll's own tick still cross-checks the backward case, and the two disagreeing is counted rather than resolved. Because the count arrives beside the exact tick the clock left, the label is the SIGNED difference of the new tick and that one — an exact number, not a bound — and that opposite claim to an elision's is why it does not share the elision's colour: burnt amber says the display took time away, this says the client did. Its own cross-palette floor is cleared against every real colour in both palettes AND against `kLaneElisionColor`, since the two markers sit one cell apart on the same axis. |
| `kUnnamedLaneColor` | 1.00f, 1.00f, 0.35f | #FFFFA0 | pale yellow | The fallback when a cell's stored ordinal matches no enumerator this table covers. On screen this is a soft, pale yellow, not the loud warning colour the raw floats suggest — itself a small demonstration of why this section needed the hex column at all. Still further from every real palette colour than any two of them are from each other, so a fallback cell is never mistaken for a real state — the loudness argument is about distance in the palette, not about how bright it renders. |

⚠ **A correction to this table's own arithmetic.** An earlier hand check of `kUnnamedLaneColor`
read its hex as `#FFFF9E`. That was itself exactly the class of manual-conversion mistake this
section exists to eliminate: `#FFFFA0` above is what `-EmitHex` actually generates from the
shipped floats via the formula stated earlier in this section, and is the value
`palette_legend_lint.ps1` checks. The three other hand-checkable spot values quoted when this
column was designed — `0.00,0.84,0.00 → #00EC00`, `0.00,0.14,0.52 → #0069BF`,
`0.36,1.00,1.00 → #A2FFFF` — all agree with the generated column exactly.

### 7.12 The clock line — what the markers alone cannot say

A marker says the axis was cut. It does not say **why**, and during a storm that is the only
question worth asking. The clock line is one row of text under the bars that answers it from
state the client already holds:

```
clock drift -22  target 6197 = auth 6193 +4  next HARDRESYNC  debt 0  auth static 2816 ticks  skips 0  stalls 6  resyncs 128
clock drift +1   target 6412 = auth 6408 +4  next NONE        debt 0  auth static 0 ticks  skips 3  stalls 2  resyncs 0
clock drift -5   target 6500 = auth 6496 +4  next STALL       debt 2  auth static 1 ticks  skips 3  stalls 6  resyncs 0
```

⚠ The numbers illustrate the FORMAT. Only the first line's `auth static` 2,816 and its
`resyncs` 128 are from the recorded session this section ends with; the rest are made up to
show the shape of the line.

**Where every token comes from.** The line is built UE-side by `buildClockDriftReadoutText`
from `ClockDriftReadout` and nothing else; the readout is `buildClockDriftReadout`'s
restatement of the ONE `ClockDriftReading` the poll filed, and the HUD never re-reads a clock
at draw time. ⛔ **THE HUD FETCHES NOTHING** — a second read at draw time is exactly the defect
§7.8 records for the authority marker, arriving through a different door.

| token | field | read at the poll from |
|---|---|---|
| `drift` | `driftTicks` | `int32(targetTick) − int32(predictionTick)`, cast BEFORE the subtraction |
| `target` | `targetTick` | `NetworkTimeEstimator::getTargetPredictionTick` |
| `auth` | `authorityTick` | `NetworkTimeEstimator::getLastAuthorityTick` |
| the `+N` after `auth` | derived | `int32(targetTick) − int32(authorityTick)` — the estimator's own prediction offset, restated so the two ticks visibly account for each other |
| `next` | `pendingAction` | `ClientPredictionClock::evaluateDrift` — what `advancePrediction` WOULD do next |
| `debt` | `stallDebtTicks` | `ClientPredictionClock::getRequiredInputDelayIncreaseStallTicks` |
| `auth static` | `authorityStaticTicks` | the lanes' `authorityStaticSimTicks`, accumulated across polls |
| `skips` `stalls` `resyncs` | `skips` / `stalls` / `resyncs` | the lanes' `clockEventCounts`, accumulated from the DIFFERENCES of successive readings of `skipCount`, `stallCount` and `hardResyncCount` |

⛔ **THE ACTION WORD IS THE ENUMERATOR'S OWN NAME, UPPER-CASED** — `NONE`, `SKIP`, `STALL`,
`HARDRESYNC` — the same rule `floorClassWord` follows for the delay readout. A prettier
`HARD RESYNC` would be a second name for one enumerator, free to drift from it silently.

**The threading split, and why it needs no new argument.** The reading is built inside
`pollInputHistoryLanes`, guarded on `runsPrediction()`, and it is the same PAIR of postures
`SimulationManagerUImpl-rationale.md` §1 already argues for the prediction-offset read beside
it. The estimator's two ticks are GAME-THREAD-written and read here on the game thread —
same-thread, nothing can tear. `ClientPredictionClock::getPredictionTick` is a plain
`unsigned int` written on the PHYSICS thread: a naturally-aligned four-byte load on the x64
target cannot tear, so the worst case is a value one tick stale, on a display that decides
nothing. **That is the accepted tear, not a new crossing class.**
⛔ **THE GUARD IS NOT COSMETIC.** `getClientClock()` calls `std::terminate` on a role that has
no client clock, so an unguarded read is a hard crash on the dedicated server, not a wrong
number on screen. On the authority the reading is absent, the readout answers
`present == false`, and the line is simply not drawn —
`Clock.NoReadingDrawsNothingRatherThanAPlausibleZero` pins that a plausible zero is never
substituted for it. ⛔ **AND NO RATE MARK EITHER.** A mark is filed only from a DIFFERENCE of
two readings, so a role that never files one leaves the label band empty rather than at zero,
and the same case pins that too. With every bar switched off nothing is drawn at all — the
frame meter's whole branch included, marks and clock line with it.

**Placement.** It rides `anyBarEnabled()` rather than any one bar's toggle, because it
describes the AXIS and not a lane, and it takes the next `frameMeterReadoutLineTopY` index
after whichever of the delay and residency readouts this selection drew. With all bars off
nothing draws at all, the frame meter's branch included.

⭐ **`auth static` IS THE FIELD THAT DIAGNOSES THE STORM, AND HERE IS HOW TO READ IT.** It
counts **sim ticks the client simulated since the authority tick last changed** — ⛔ never
polls, which are render frames. On a healthy connection the server's tick arrives every few
ticks, so the number sits at 0–3 and flickers. **A number that climbs and keeps climbing means
the server's tick stopped arriving**: `recordAuthorityTick` has received nothing new, so
`targetTick` is frozen, so the client drifts away from a stale target until `abs(drift)`
crosses the hard-resync threshold, resyncs to that same stale target, and does it again. That
is the whole storm, and the line says it in one glance: `auth static` in the thousands beside
a `drift` sawtoothing to `−22` and `next HARDRESYNC`. ⛔ **THE CAUSE IS THEN OUTSIDE THIS
DISPLAY'S FENCE** — a static authority tick is a timing-relay or server-tick matter, and the
`[Verbose] PCTM NTE: authorityTick=` line settles which. What the reader should do with it is
report it there, not look for a display bug.
⚠ The counter ACCUMULATES rather than subtracting two ticks, and that is deliberate: during
the storm the client's own tick number never leaves the twenty-two-tick loop, so
subtracting the run's first sim tick from the poll's own is bounded by 22, and the specified
subtraction would have read **healthy** for the entire minute. The accumulated run reached
2,816 for the same session. On a clock that advances normally the two agree exactly, as
`Clock.AnAdvancingClockUnderAFrozenAuthorityRunsOneStaticTickPerSimTick` pins.
⚠ A reading arriving after a gap in which none was filed starts a NEW run at zero: the lanes
hold no predecessor for it to have been static against. In production that transition happens
only at startup, since `runsPrediction()` does not change within a session.

⭐ **THE THREE COUNTS ARE THIS DISPLAY'S, NEVER THE CLOCK'S OWN TOTALS.** The clock counts
from the moment it was built, which is before this display existed, so only the DIFFERENCE
between two successive readings says anything about a session being watched: the first reading
of all contributes nothing, and a gap in the readings is counted neither as a quiet stretch nor
as a busy one. ⛔ **A COUNTER THAT READ LOWER THAN THE LAST ONE ANSWERS ZERO** rather than
wrapping a subtraction into four billion — `clockEventDelta` is the one place that is decided.
⭐ **THE COUNTS OUTLIVE THE MARKS.** A rate mark (§7.8) scrolls off the bar, is lost inside a
span the gate is still collapsing, or falls behind the ledger's reach; the count keeps it. So
`stalls` climbing while no `−` is on screen is the display working, not failing — it means the
corrections happened somewhere the bar can no longer point at.

⚠ **THE SEVEN SEAM READS ARE THE SAME ACCEPTED TEAR, AND THE ORDER MAKES IT ONE-SIDED.**
They are taken inside `pollInputHistoryLanes` under the SAME `runsPrediction()` guard as the
drift fields above, through `getDiagnostics()` — the clock publishes no bare accessor for any of
them — and each is a plain physics-written `unsigned int`, so a naturally-aligned four-byte load
cannot tear on the x64 target. What CAN straddle a write is a PAIR, and the two orders are
deliberately opposed: **the clock writes the tick and then the count; the poll reads the count**
**and then the tick.**

Work the interleaving through and one side of it closes:

* If the poll's count-read already sees the **incremented** count, then by the clock's own
  program order the count-write landed after the tick-write — so the tick-write landed too, and
  the poll's tick-read comes strictly after its count-read. ⛔ **A FRESH COUNT BESIDE A STALE**
  **TICK IS THEREFORE IMPOSSIBLE**, and that is the case worth closing: it is the one that would
  file a mark — the count moved, so a delta exists — at the *previous* event's tick, putting a
  purple mark on a column where nothing happened.
* The straddle that CAN happen is the mirror: the count-read lands before the increment while
  the tick-read lands after the tick-write, giving a **stale count beside a fresh tick**. The
  delta is then zero, so **no mark is filed and no token moves**. The event is not lost — the
  next poll differences against that same stale count, sees the delta, and files the mark from
  the tick the clock still holds, which is that event's own tick.

⭐ **SO THE WORST CASE IS A MARK ONE POLL LATE, AT THE RIGHT TICK** — never a mark at the wrong
one. A frame of latency on a diagnostic that decides nothing is the cheap failure; a mark
sitting on a column the clock never touched is the expensive one, and the read order is what
buys the first at the price of the second.
⛔ **DO NOT REVERSE THAT READ ORDER.** Reading tick-then-count swaps which of those two is
reachable, and the misplaced mark is exactly the class of defect this display was rebuilt to
stop telling.

⛔ **WHAT THIS LINE DOES NOT SHOW.** `AdvanceResult` is consumed and dropped inside the
prediction step, so no per-tick record of a skip or a stall exists here — what the seam gives is
a COUNT and the LAST tick of each kind, which is why a poll that saw three skips files one mark
saying three rather than three marks it cannot place. What `next` shows is the drift STATE that
decides them, sampled once per poll: a poll landing between two decisions can miss a `SKIP`
entirely, and the line makes no claim that it did not. The `skips` count still sees it.
