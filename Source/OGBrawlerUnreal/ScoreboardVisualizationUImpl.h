// SPDX-License-Identifier: BUSL-1.1

#pragma once

// =============================================================================
// THE UE SIDE OF THE RING-OUT SCOREBOARD -- the three CVars and the gather.
// =============================================================================
// Modelled, deliberately and name for name, on `InputHistoryVisualizationUImpl.h`: that
// file is this project's shipped precedent for a display's UE layer, and the rules stated
// in its banner are inherited here rather than re-argued. The layout, the ordering, the
// clamps and the ink all live in `OGBrawler/BrawlerScoreboardVisualization.h`, which a
// Catch2 case can reach; this layer supplies only what UE knows -- which characters exist,
// what each one's replicated score is, and which tick the countdown is measured against.
//
// ⛔ NO GEOMETRY AND NO SORTING IN THIS FILE. `gatherScoreboardRows` returns rows that
// have already been through `orderedScoreboardRows`, and the only reason it returns the
// ordered form is that there is then no way to reach a draw site holding unsorted rows.
// Nothing here computes an origin, a row height or a column edge.
//
// ---------------------------------------------------------------------------
// ⭐ THE JOIN, AND THE THREE PLACES ITS INPUTS COME FROM.
//
// A row needs three facts that live in three different places, and the key that ties them
// together is NOT the one an actor iteration hands you first:
//
//   | fact                | source                                        | present on |
//   |---------------------|-----------------------------------------------|------------|
//   | score               | `AOGBrawlerUECharacter::GetRingoutScore()`     | every peer |
//   | ⭐ swatch           | `AOGBrawlerUECharacter::GetBrawlerColor()`     | every peer |
//   | dead                | `brawlerRingout::State::flags`, via the sim    | every peer |
//   | respawn-at-tick     | `brawlerRingout::State::respawnAtTick`         | every peer |
//
// ⭐ THE SWATCH IS TASK 10'S, AND IT CHANGED ONLY WHAT COLUMN ONE DRAWS. `characterId` is
// still the key this table is joined on and still the key the rows are sorted by; the id
// simply stopped being the thing the player reads, because `42353` names no fighter they
// can see. `BrawlerColor` is the tint already on that fighter's mesh, assigned server-side
// from `kBrawlerPalette` and replicated PLAIN -- deliberately not `COND_OwnerOnly` -- so it
// is on every peer for the same reason the score is, and needs no role branch either.
//
// ⛔ THE KEY IS `AOGBrawlerUECharacter::GetSimCharacterId()`, NOT `AActor::GetUniqueID()`
// ON THE PAWN. Every id in the simulation -- storage's key, the score roster's key, the
// spawn-slot table, every `id=%u` in every ring-out log line -- is the
// `USimmableUpdateComponent`'s `GetUniqueID()`, because that is the value
// `tryRegisterWithNewFramework` passes to `tryRegister`. The pawn's own id is a DIFFERENT
// number, so a board joined on it would match nothing and draw a column of zeroes while
// looking entirely healthy.
//
// ⛔ ONE DOOR PER FACT, ON BOTH ROLES. The score is read from the replicated property on
// the authority too, never from `brawlerRingout::ScoreSystem`. Two code paths would mean
// one of them is exercised only by a listen server and the other only by a client, and
// neither would be the one a bug report describes.
//
// ⛔ DEAD AND RESPAWN ARE NOT REPLICATED AGAIN, and adding a property for either would be
// a defect rather than a convenience: they are `brawlerRingout::State`, which already
// rides the correction wire inside `simulatableBrawler::State` and is reproduced on every
// peer by the ring-out sub-simulation.
//
// ---------------------------------------------------------------------------
// ⭐ KEYED ON CHARACTER ID, NEVER ON THE CONNECTION -- inherited verbatim from the model
// file. Couch co-op shares one `UNetConnection` across local players and a listen-server
// host's local player has none at all, so a connection-keyed gather gives every sibling
// the same data or fails outright on the host.
//
// ⚠ UNLIKE THE INPUT-HISTORY PANE, THIS PANEL IS NOT SINGLE-CHARACTER. That pane picks one
// character by selection (`firstLocalCharacterId`); a scoreboard draws everyone, so no
// filter of that kind appears below. The OTHER guard the model file makes -- that only ONE
// local player's HUD draws, or every couch-co-op sibling stacks the same panel on one
// screen -- is still needed and still made, at the draw site, through the same
// `firstLocalPlayerController` this file does not duplicate.
//
// ---------------------------------------------------------------------------
// READS ONLY, STRUCTURALLY. The manager arrives as a pointer to const and its ring-out
// door (`getRingoutVizState`) returns a value; nothing in this file or its `.cpp` names a
// non-const simulation accessor. Nothing gathered here is replicated, enters a correction
// payload, or reaches `compute_checksum`.
// =============================================================================

#include "CoreMinimal.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "OGBrawler/BrawlerScoreboardVisualization.h"

class ASimulationManagerUImpl;
class UWorld;

namespace scoreboardVisualizationUImpl
{

// ---------------------------------------------------------------------------
// THE CVARS. All three are read PER DRAWN FRAME, deliberately.
//
// ⚠ READ PER FRAME -- DO NOT CONVERT ANY OF THESE TO A ONE-TIME READ. The reasoning is
// `SimmableUpdateComponent.cpp`'s, at `THE MOVEMENT DEBUG DRAW'S MASTER SWITCH`, and it is
// two opposing disciplines that coexist on purpose:
//   * a `StaticData` CVar is read ONCE, at construction, so a tunable cannot move under a
//     running session and put two peers on different numbers. ⛔ THE RING-OUT KILL PLANE
//     AND RESPAWN DELAY ARE THIS KIND and are nowhere in this file.
//   * a VIZ CVar is the opposite case: it feeds nothing simulated, and its whole value is
//     that a tuner can type `OGBrawler.ScoreboardScale 2` mid-session and see the NEXT
//     frame change. ⭐ ALL THREE BELOW ARE THIS KIND.
// The two are repeated at both sites so neither can be "fixed" in isolation.
// ---------------------------------------------------------------------------

// ⭐⭐ THE MASTER GATE -- `OGBrawler.Scoreboard`, **DEFAULT ON** (user ruling, 2026-09-13),
// which makes it the ONE viz CVar in this project that does not default off. ⛔ THE
// DIVERGENCE IS THE RULING, NOT A SLIP: every other toggle here gates a DEBUG DRAW, and a
// debug draw nobody asked for should cost nothing; the scoreboard is GAME-MODE UI, and a
// ring-out match whose score is invisible is not a playable match. The full argument is at
// `GScoreboard`'s initialiser in the `.cpp`, where the value actually lives.
//
// Read alone by `DrawHUD`'s early-out branch, so a board that is off costs exactly one bool
// read per frame: no actor iteration, no storage lookup, no geometry, no backdrop. ⭐ THAT
// PROPERTY IS UNCHANGED -- it is now what turning the board OFF costs, rather than what
// leaving it off costs.
//
// ⛔ THE MODEL FILE'S FOLD DEGENERATES HERE, AND THAT IS WORTH STATING RATHER THAN
// IMITATING. `InputHistoryVisualizationUImpl.h` folds its master into four CHILD BOOLS
// (`displayEnabled`, `provenanceEnabled`, ...) because it gates four separate displays.
// This initiative ships ONE panel, so the master IS its toggle and there is no second bool
// to fold it into; and a bool cannot be folded into either float below, because "off" is
// not a scale and not an opacity. The discipline the fold exists to buy -- no call site
// can forget the gate -- is therefore kept where it CAN be expressed: there is exactly one
// branch, in `DrawHUD`, and every other entry point in this namespace is reached only from
// behind it. ⛔ A SECOND SCOREBOARD TOGGLE OWES THE FOLD.
bool enabled();

// `OGBrawler.ScoreboardScale`, DEFAULT 1.0, CLAMPED to [0.25, 4] at READ. The ONE factor
// behind both the board's geometry and its text.
// ⛔ CLAMPED IN THE ACCESSOR, NEVER AT THE CONSOLE, and by calling the pure
//   `clampScoreboardScale` rather than re-implementing its range: the console keeps
//   echoing whatever the user typed, the drawn frame is bounded whatever that was, and the
//   clamp stays in the one place a Catch2 case can reach it. A non-number lands on the
//   minimum rather than multiplying the whole layout into NaN.
float scale();

// `OGBrawler.ScoreboardAlpha`, DEFAULT 0, CLAMPED to [0, 1] at READ, through the pure
// `clampScoreboardBackgroundAlpha` for the same reasons as the scale above.
// ⛔ THE BACKDROP'S OPACITY ONLY -- the row ink is opaque and unaffected.
// ⛔ AT 0, THE SHIPPED DEFAULT, NO BACKDROP IS DRAWN AT ALL. An invisible rectangle is
//   still a canvas call on every frame the board is up.
float backgroundAlpha();

// ---------------------------------------------------------------------------
// THE GATHER.
// ---------------------------------------------------------------------------

// The tick a countdown is measured against on THIS role, or nullopt when there is none to
// have (no manager yet).
//
// ⛔ GATED ON `runsPrediction()`, AND THE GATE IS NOT DEFENSIVE. `getClientClock()` does
//   not return a wrong number on a role that does not predict -- it calls
//   `std::terminate`. This is the same guard `pollInputHistoryLanes` makes, at the same
//   accessor, for the same reason. A predicting role reads its prediction tick because
//   that is the tick its own drawn world is at; every other role reads the server clock.
//
// ⚠ THIS TICK MOVES BACKWARDS. A hard resync rewinds the prediction clock, so the
//   subtraction against an absolute `respawnAtTick` is clamped at zero by `gather` below
//   rather than being allowed to wrap a `uint32_t` into roughly four billion ticks.
std::optional<uint32_t> displayTick(const ASimulationManagerUImpl* manager);

// One row per `AOGBrawlerUECharacter` in `world`, ALREADY ORDERED by character id, or an
// empty vector when there is no world, no manager or no character.
//
// The score comes from each actor; dead and the countdown come from `manager`'s ring-out
// door, looked up by that actor's SIM id. A character the simulation does not know -- one
// mid-registration, or a proxy on a peer that has not received it yet -- still gets a row,
// carrying its replicated score with `isDead` false: a fighter missing from the board is
// worse than one whose status column is briefly blank, because nothing about it looks
// wrong.
//
// ⛔ THE RETURN IS THE ORDERED FORM. `SystemsExecutor.h` item 81 states character order
//   within a sweep as a LIBRARY CONTRACT -- unordered-map order, unspecified and varying
//   with registration history -- and an actor iteration is no better. Unsorted, two peers
//   watching one match see the same scores in DIFFERENT ROWS. The sort itself is the pure
//   header's; this only guarantees it has happened.
std::vector<brawlerScoreboardVisualization::ScoreboardRow> gatherScoreboardRows(
	const UWorld* world, const ASimulationManagerUImpl* manager);

} // namespace scoreboardVisualizationUImpl
