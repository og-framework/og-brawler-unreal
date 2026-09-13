// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/ScoreboardVisualizationUImpl.h"

#include "HAL/IConsoleManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "OGBrawler/BrawlerRingoutSimulation.h"

#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"

namespace
{

// ⭐⭐ THE MASTER, AND IT IS THE ONE VIZ CVAR IN THIS PROJECT THAT DEFAULTS **ON**
// (user ruling, 2026-09-13). ⛔ THAT IS DELIBERATE AND IT CONTRADICTS THE HOUSE PATTERN, so
// the reason is written here rather than left for a reader to supply.
//
// Every other visualization toggle in this tree defaults OFF, and several of them say why
// at their own site -- "DEFAULT 0, AND OFF MUST COST ONE BOOL READ" -- because a debug draw
// nobody asked for should cost nothing. ⛔ THE SCOREBOARD IS NOT A DEBUG VISUALIZATION. It
// is GAME-MODE UI: a ring-out match whose score is invisible is not a playable match, it is
// a match nobody can tell they are winning. A feature that the mode does not work without
// has the opposite natural default to an overlay you switch on to investigate something.
// ⛔ DO NOT "FIX" THIS BACK TO `false` TO MATCH THE NEIGHBOURING PATTERN. The pattern is
// about debug draws and this is not one; the divergence is the ruling, not a slip.
//
// ⭐ THE "OFF COSTS ONE BOOL READ" PROPERTY IS UNCHANGED AND STILL WORTH STATING -- only
// which side of it is the default moved. `DrawHUD` still reaches exactly one bool before
// deciding, so a player or a profiler who types `OGBrawler.Scoreboard 0` pays one read per
// frame and nothing else: no actor iteration, no storage lookup, no geometry, no backdrop.
// That is now the cost of turning the board OFF rather than of leaving it off.
// ⛔ THE `DrawHUD` BRANCH ITSELF DID NOT CHANGE, and must not. Only this initialiser did.
//
// ⚠ AND UNLIKE THE INPUT-HISTORY MASTER this one has no child toggles to fold into, because
// the initiative ships exactly one scoreboard. See the header's note at `enabled()` before
// adding a second.
bool GScoreboard = true;

static FAutoConsoleVariableRef CVarScoreboard(
	TEXT("OGBrawler.Scoreboard"),
	GScoreboard,
	TEXT("1 = on (DEFAULT -- this is game-mode UI, not a debug overlay), 0 = off. Draws ")
	TEXT("the ring-out scoreboard: one row per character, ")
	TEXT("flush against the RIGHT edge and vertically centred, ordered by character id so ")
	TEXT("every peer shows the same player in the same row. Each row is that brawler's own ")
	TEXT("COLOUR SWATCH -- the tint its mesh is wearing -- then its ring-out score, then ")
	TEXT("-- only while that fighter is out -- the ticks remaining until its respawn. The ")
	TEXT("swatch does not change while a fighter is down; being out shows in the row's ink ")
	TEXT("and in the countdown column. Correct on a client as well as the authority: the ")
	TEXT("score and the colour both arrive by replication and the dead flag rides the ")
	TEXT("correction wire. OFF COSTS ONE BOOL READ PER FRAME ")
	TEXT("-- no actor is iterated, no simulation state is looked up, no geometry is ")
	TEXT("computed and no backdrop is drawn until this reads 1."),
	ECVF_Default);

// Both look knobs default to the PURE HEADER'S OWN CONSTANT, so the shipped look and the
// code's idea of the shipped look cannot drift apart.
// ⭐ TUNING THE BOARD IS A CONSOLE LINE, NOT A REBUILD.
float GScoreboardScale = brawlerScoreboardVisualization::kScoreboardDefaultScale;

static FAutoConsoleVariableRef CVarScoreboardScale(
	TEXT("OGBrawler.ScoreboardScale"),
	GScoreboardScale,
	TEXT("Size multiplier for the ring-out scoreboard. Default 1.0; CLAMPED to [0.25, 4] ")
	TEXT("when read, so a value outside it is pulled to the nearer end rather than ")
	TEXT("rejected, and a non-number lands on the minimum instead of blanking the board. ")
	TEXT("It scales the row geometry AND the text by the same factor, and the board ")
	TEXT("re-flushes to the right edge and re-centres on the scaled height, so nothing is ")
	TEXT("reallocated and no row is dropped. Requires OGBrawler.Scoreboard 1."),
	ECVF_Default);

float GScoreboardAlpha =
	brawlerScoreboardVisualization::kScoreboardDefaultBackgroundAlpha;

static FAutoConsoleVariableRef CVarScoreboardAlpha(
	TEXT("OGBrawler.ScoreboardAlpha"),
	GScoreboardAlpha,
	TEXT("Opacity of the ring-out scoreboard's BACKGROUND only. Default 0; CLAMPED to ")
	TEXT("[0, 1] when read. At 0 -- the default -- the backdrop is not drawn at all and ")
	TEXT("the rows sit straight over the scene, because an invisible rectangle is still a ")
	TEXT("canvas call every frame; at 1 the board hides what is behind it. The rows' own ")
	TEXT("colours are unaffected. Requires OGBrawler.Scoreboard 1."),
	ECVF_Default);

} // namespace

namespace scoreboardVisualizationUImpl
{

// ⛔ THE ACCESSOR BLOCK -- THE ONLY PLACE ANY `G*` SCOREBOARD VALUE IS READ, and the only
// place either clamp is applied. Every one of these is read per drawn frame; see the
// header for why that is deliberate and not an oversight.
bool enabled()
{
	return GScoreboard;
}

float scale()
{
	// Clamped at READ, so the console still echoes whatever the user typed.
	// ⛔ THE PURE CLAMP IS CALLED, NOT COPIED. Re-spelling the range here would put the
	//   shipped bound somewhere no Catch2 case can reach -- and the pure one is written
	//   NEGATED on purpose, to land a NaN on the minimum. See its own comment.
	return brawlerScoreboardVisualization::clampScoreboardScale(GScoreboardScale);
}

float backgroundAlpha()
{
	return brawlerScoreboardVisualization::clampScoreboardBackgroundAlpha(GScoreboardAlpha);
}
// ⛔ END OF THE ACCESSOR BLOCK.

std::optional<uint32_t> displayTick(const ASimulationManagerUImpl* manager)
{
	if (manager == nullptr)
		return std::nullopt;

	// ⛔ NOT A DEFENSIVE GUARD: getClientClock() std::terminates on a role that does not
	//   predict. The same test pollInputHistoryLanes makes at the same accessor.
	if (manager->runsPrediction())
		return static_cast<uint32_t>(manager->getClientClock().getPredictionTick());

	// A listen server and a dedicated server draw the tick they are actually simulating.
	return static_cast<uint32_t>(
		manager->getServerClock().getSimulationStep().getTick());
}

std::vector<brawlerScoreboardVisualization::ScoreboardRow> gatherScoreboardRows(
	const UWorld* world, const ASimulationManagerUImpl* manager)
{
	using brawlerScoreboardVisualization::ScoreboardRow;

	std::vector<ScoreboardRow> rows;

	if (world == nullptr || manager == nullptr)
		return rows;

	const std::optional<uint32_t> nowTick = displayTick(manager);

	// ⛔ CONST ITERATION OVER A NON-CONST WORLD HANDLE, because TActorIterator takes one.
	//   Nothing below calls a non-const member on any actor it visits; the two reads are
	//   `GetRingoutScore()` and `GetSimCharacterId()`, both const and both accessors over
	//   a replicated property and a component id.
	UWorld* iterableWorld = const_cast<UWorld*>(world);

	for (TActorIterator<AOGBrawlerUECharacter> it(iterableWorld); it; ++it)
	{
		const AOGBrawlerUECharacter* character = *it;
		if (character == nullptr)
			continue;

		ScoreboardRow row;

		// ⭐ THE JOIN KEY: the SIM id, which is the SimmableUpdateComponent's unique id and
		//   NOT this pawn's. See the header -- joining on the pawn's silently matches
		//   nothing and draws a board of zeroes that looks entirely healthy.
		row.characterId = character->GetSimCharacterId();

		// ⛔ THE SAME DOOR ON EVERY ROLE. On a client this is whatever last replicated in;
		//   on the authority it is whatever the per-pass push last wrote. Reading
		//   brawlerRingout::ScoreSystem on the authority instead would be a second code
		//   path that only a listen server ever exercises.
		// ⚠ NEGATIVE IS UNREACHABLE -- the property mirrors a uint32_t counter that only
		//   ever increments -- but the cast is written rather than assumed, so a future
		//   penalty scoring a fighter below zero cannot wrap the column into billions.
		const int32 replicatedScore = character->GetRingoutScore();
		row.score = (replicatedScore > 0) ? static_cast<uint32_t>(replicatedScore) : 0u;

		// ⭐⭐ [ringout task 10] THE SWATCH, AND IT IS BOUND TO *THIS* CHARACTER.
		// ⛔ IT MUST BE READ OFF `character`, INSIDE THIS LOOP, AND FROM NOWHERE ELSE.
		//   Every other spelling that compiles is a mis-binding with no symptom the
		//   compiler or this project's suites can see: a constant gives every row the same
		//   colour, a value hoisted above the loop gives every row the FIRST character's,
		//   and either one draws a board that looks entirely healthy while telling the
		//   player nothing. The row's tint and the row's id come from the same
		//   `character` in the same iteration, which is the whole of the binding.
		// ⛔ THE SAME DOOR ON EVERY ROLE, exactly like the score above: `BrawlerColor` is
		//   assigned on the authority and replicated PLAIN (not `COND_OwnerOnly`), so a
		//   client reads the real tint here and needs no role branch.
		// ⚠ THE ALPHA IS DROPPED DELIBERATELY -- `ScoreboardInk` is three channels and the
		//   swatch is opaque. See the note on `AOGBrawlerUECharacter::GetBrawlerColor`.
		const FLinearColor tint = character->GetBrawlerColor();
		row.swatch = brawlerScoreboardVisualization::ScoreboardInk{
			tint.R, tint.G, tint.B
		};

		// Dead and the countdown, from the simulation's own viz snapshot. A character the
		// storage does not know keeps `isDead = false` and draws no status column.
		if (const std::optional<brawlerRingout::State> ringout =
				manager->getRingoutVizState(row.characterId))
		{
			// ⚠ THE FLAG IS READ FIRST, AND THAT ORDER IS LOAD-BEARING.
			//   `respawnAtTick` is meaningful only while the dead bit is set and is
			//   deliberately left at its last value once cleared, so a countdown computed
			//   for a living fighter would be a stale number drawn as a live one.
			row.isDead = brawlerRingout::isDead(*ringout);

			if (row.isDead && nowTick.has_value())
			{
				// ⛔ THE SUBTRACTION IS THE PURE HEADER'S, NOT THIS FILE'S -- and its zero
				//   clamp, which is the reason it is worth a call at all. `respawnAtTick`
				//   is ABSOLUTE and the display tick MOVES BACKWARDS on a hard resync, so
				//   an unguarded difference of two uint32_t reads about four billion ticks
				//   on exactly the frames an investigation is looking at. The CALL is made
				//   once, here; the arithmetic lives where a Catch2 case can probe it,
				//   because nothing in this translation unit can be reached from one.
				row.ticksUntilRespawn =
					brawlerScoreboardVisualization::scoreboardTicksUntilRespawn(
						ringout->respawnAtTick, *nowTick);
			}
		}

		rows.push_back(row);
	}

	// ⛔ THE ORDERED FORM IS WHAT LEAVES THIS FUNCTION. Actor-iteration order is no more
	//   specified than the storage sweep's, so an unsorted board shows two peers the same
	//   scores in different rows. The sort is the pure header's; this only guarantees it
	//   has run before anything can draw.
	return brawlerScoreboardVisualization::orderedScoreboardRows(std::move(rows));
}

} // namespace scoreboardVisualizationUImpl
