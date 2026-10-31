<!-- SPDX-License-Identifier: BUSL-1.1 -->

# The controls indicator and legend — rationale

Companion to the three `Source/OGBrawlerUnreal` files that gate, gather and draw the controls HUD:
`Source/OGBrawlerUnreal/ControlsHudUImpl.h`, `Source/OGBrawlerUnreal/ControlsHudUImpl.cpp` and
`Source/OGBrawlerUnreal/OGBrawlerUEHUD.cpp`, and to the input handlers in
`Source/OGBrawlerUnreal/OGBrawlerInputCollectionComponent.cpp` that toggle the legend and select a
movement scheme.

⭐ **The authority for every string, ink, row rule and length is the pure og-brawler header**,
`OGBrawler/BrawlerControlsHud.h`, and its rationale, `OGBrawler/docs/BrawlerControlsHud-rationale.md`.
The texts and the key-to-scheme pairs come from the control-mode table,
`OGBrawler/docs/ControlModeTexts-rationale.md`; the key names from
`OGBrawler/docs/KeyGlyphs-rationale.md`. This file covers only what the UE layer adds: the console
variables, the gather, the draw order, and the two input handlers.

The UE layer is modelled on the ring-out scoreboard's (`Source/OGBrawlerUnreal/docs/ScoreboardDisplay-rationale.md`):
a UImpl namespace for the console variables and the gather, and one private draw method on the HUD.

---

## 1. The split

`ControlsHudUImpl.h` :: `enabled`, `scale`, `backgroundAlpha`, `legendOpen`, `toggleLegend`, `mappingContext`, `gatherInput`

The pure header decides what is drawn; this layer only has to answer the questions the pure header
cannot reach: which console values are set, which player's HUD this is, what that player's camera
is, and whether look mode is on. Each answer is one field of `brawlerControlsHud::ControlsHudInput`,
filled once per drawn frame by `controlsHudUImpl::gatherInput`. The HUD then calls
`brawlerControlsHud::indicatorRows` and `brawlerControlsHud::legendLines` and turns their output into
canvas calls.

## 2. The four console variables

| variable | default | read as |
|---|---|---|
| `OGBrawler.ControlsHud` | 1 | `controlsHudUImpl::enabled` |
| `OGBrawler.ControlsHudScale` | 1.0, clamped to [0.25, 4] | `controlsHudUImpl::scale` |
| `OGBrawler.ControlsHudAlpha` | 0.4, clamped to [0, 1] | `controlsHudUImpl::backgroundAlpha` |
| `OGBrawler.ControlsLegend` | 0 (collapsed) | `controlsHudUImpl::legendOpen` |

* **`OGBrawler.ControlsHud` defaults ON.** The indicator is game UI, like the scoreboard, not a
  debug overlay. 0 hides the indicator and the legend; the hotkeys keep working, since nothing in
  the input path reads this variable. Off costs the one bool read in `AOGBrawlerUEHUD::DrawHUD`.
* **The scale and the alpha are clamped when read**, by the pure header's
  `brawlerControlsHud::clampControlsHudScale` and `brawlerControlsHud::clampControlsHudBackgroundAlpha`.
  The console keeps whatever was typed; the HUD draws with the clamped value. A non-number lands on
  the minimum. At alpha 0 no backdrop is drawn at all (an invisible rectangle would still be a
  canvas call every frame), as on the scoreboard.
* **The help strings state the defaults and ranges in words**, and four `static_assert`s in
  `ControlsHudUImpl.cpp` pin each of those numbers to its constant in the pure header. Retuning a
  constant without retuning the help string (and this table) fails the build. The two bool
  defaults have no constant in the pure header; their initialisers in `ControlsHudUImpl.cpp` are
  the only statement of them besides this table.
* **`OGBrawler.ControlsLegend` is the help state: collapsed or expanded** (user ruling R6, task 6).
  0, the default, is the collapsed indicator: the keys column alone, the active scheme's key lit,
  and no legend. 1 expands it: each row also shows its mode name and state, and the legend opens
  below. The pure rationale's §4.1 has the row rules. The help string says this.
* **The key and the console share that one state.** There is no second flag: the legend key
  calls `controlsHudUImpl::toggleLegend`, which flips the very bool the console variable is bound to,
  so typing `OGBrawler.ControlsLegend 1` and pressing the key act on one state and cannot disagree.
  The state is global, not per player (user ruling R1: one shared panel); in couch co-op any local
  player's legend key flips it.

All four variables are read on the game thread only: the console, the input handlers and the HUD
all run there.

## 3. The gather

`ControlsHudUImpl.cpp` :: `gatherInput`

* **The player** is `inputHistoryVisualizationUImpl::firstLocalPlayerController`, the helper the
  input-history displays and the scoreboard use. No second "first local player" helper exists.
* **No input, no draw.** `gatherInput` returns nothing when there is no first local player
  controller, when its pawn is not an `AOGBrawlerUECharacter` (front-end, spawning, spectating), or
  when that character has no input component (it always has one, as a default subobject; the check
  keeps a null from being dereferenced).
* **`scheme`** is `dAttackMachineSimulation::g_movementScheme`, the global the scheme hotkeys and
  the `OGBrawler.MovementScheme` console variable write. It is global, so every local player shares
  it, and the HUD shows it once.
* **`cameraLookOn`** is the first local player's own `UOGBrawlerInputCollectionComponent::getBlockLook`.
  Look mode is per pawn; in the shared view its row is hidden anyway (below).
* **`legacyStickSwap`** is `dAttackMachineSimulation::g_swapMoveAndAimSticks`, so the legend can warn
  that the legacy swap console variable is on.
* **`view`** is Shared exactly when the first local player's view target is an
  `ASharedIsometricCameraActor`, and Solo otherwise.

### 3.1 Why the view target, not a player count, decides Solo or Shared

User ruling R1 says the camera-look row and legend section apply "only when there is only one local
player". The camera, not the player count, is what makes that true: look mode turns the solo
third-person camera, and the shared isometric camera does not turn at all. The view target is the
camera actually on screen, so the HUD describes what the player sees.

A count would have to re-derive `AOGBrawlerPlayerController::computeDesiredViewTarget`'s rule (one
locally controlled brawler: its own camera; two or more: the shared camera), and would disagree with
the screen whenever the two differ. They do differ: a second local player who has joined but whose
brawler has not spawned is a local player without a locally controlled brawler, and between a join
or a leave and `AOGBrawlerPlayerController::refreshViewTarget` the old view is still the one shown.
A count of all players (including remote ones in an online match) would be wrong outright, since
remote players never move a client to the shared camera. Reading the view target cannot drift from
the controller's rule, because it is the result of that rule.

## 4. The mapping context

`ControlsHudUImpl.cpp` :: `mappingContext`

The key names in each row come from the action's bindings in a `dInput::MappingContext`, never from
a second key list. `controlsHudUImpl::mappingContext` builds `dInput::gameMapping::buildDefaultContext`
once, into a function-local static, on the first draw. It is the same context the input component
hands to its translator (`UOGBrawlerInputCollectionComponent::initializeTranslator` calls
`dInput::gameMapping::buildDefaultContext` too). The game has no runtime rebinding, so the two are
equal. If rebinding arrives, the HUD must read the live context instead of this static; the rows
already follow any context they are given (the pure header's rebinding test pins that).

## 5. The draw

`OGBrawlerUEHUD.cpp` :: `AOGBrawlerUEHUD::DrawHUD`, `AOGBrawlerUEHUD::drawControlsHud`

**Order in `AOGBrawlerUEHUD::DrawHUD`.** While the front-end join screen is up, it is drawn alone and
`AOGBrawlerUEHUD::DrawHUD` returns early, so the controls HUD shows only in a match (user ruling R5).
Otherwise: the input-history panel, the frame meter, the scoreboard, then the controls HUD, then the
local-player-limit notice. The controls HUD sits top-left and the scoreboard is flush right. The
input-history displays are debug views, off by default (`OGBrawler.InputHistory` 0): the panel sits
on the left edge, vertically centred, and the frame meter along the bottom. With them on, a tall
open legend can reach the panel on a short screen; the controls HUD is drawn after them, so it is
on top. Fitting both is not a goal of this display.

**The first local player only.** Every local player owns a HUD. Without the check that the owning
controller is `inputHistoryVisualizationUImpl::firstLocalPlayerController`, each couch co-op player's
HUD would stack the same panel on one screen. With it, co-op shows the one shared panel of ruling R1.

**Inside `AOGBrawlerUEHUD::drawControlsHud`**, in this order:

1. gather the input; return if there is none;
2. build the rows and the legend lines from the pure header, with the layout scaled by
   `controlsHudUImpl::scale` (the origin stays top-left);
3. measure every non-empty indicator string at the layout's text scale; the indicator backdrop is
   `brawlerControlsHud::indicatorPanelWidth` wide (the compact width while collapsed, the full panel
   width while expanded), widened only if a measured string (plus the inner padding) reaches past
   it, and the pure header's indicator height; drawn only when the alpha is above 0. Collapsed, the
   model's labels and states are empty, so only the keys are measured;
4. draw each row's keys, and its label and state when they are not empty, at the pure header's
   column positions, in the row's ink
   (`brawlerControlsHud::controlsHudRowInk`), with the engine's small font at the layout's text scale;
5. if the legend is open (its line list is not empty): the same measure-then-backdrop step with the
   layout's legend width, at the pure header's `brawlerControlsHud::legendTopY`, then each line at
   `brawlerControlsHud::legendLineTopY` in the Normal ink. An empty line is the model's section break:
   it keeps its slot and draws nothing.

The measured widths are the only numbers the HUD makes, and they can only widen a backdrop; no
column moves. This follows the pure rationale's rule that the UE layer decides no geometry beyond
measuring text. Whether the columns fit the small font is a G1 check (the pure rationale §6).

The HUD holds only const pointers to what it reads (the controller, the character, its input
component) and calls only const accessors; it writes nothing but canvas calls.

## 6. The input handlers

`OGBrawlerInputCollectionComponent.cpp` :: `setMovementSchemeFromTable`, `UOGBrawlerInputCollectionComponent::onToggleControlsLegend`

* **The legend key.** `ToggleControlsLegend` (H and the gamepad View button) is bound on the Started
  event, one edge per press, like the look-mode toggle. Its handler calls
  `controlsHudUImpl::toggleLegend` and does nothing else. No `PlayerInput` field, no wire field and
  no simulation code reads the legend state: its only reader is `controlsHudUImpl::legendOpen`, whose
  only caller is `controlsHudUImpl::gatherInput`.
* **The scheme hotkeys read the table.** The four `onSetScheme*` handlers used to write a literal
  scheme each. They now call one file-local helper that looks up their own action with
  `dInput::controlModeTexts::findControlModeEntry` and writes that entry's `scheme`. The key and the
  HUD row it lights are therefore the same table entry and cannot disagree. The values are the
  ones the handlers wrote before: the test case "one MovementScheme entry per scheme, pinned to
  today's 7 / 8 / 9 / 0 actions" pins each action's entry to exist, to be a MovementScheme entry,
  and to carry the old scheme. The helper's `ensure` checks the same two facts; outside Shipping a
  missing entry reports, and in every build it leaves the scheme unchanged rather than dereference
  a null.

## 7. What this adds nowhere

No log line and no log tag (the headless equivalence harness compares tag sets), no simulation input,
no wire field, and no change to the pure header or the table.
