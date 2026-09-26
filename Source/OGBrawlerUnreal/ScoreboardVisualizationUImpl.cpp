// SPDX-License-Identifier: BUSL-1.1
// docs/ScoreboardDisplay-rationale.md

#include "OGBrawlerUnreal/ScoreboardVisualizationUImpl.h"

#include "HAL/IConsoleManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"

#include "OGBrawler/BrawlerRingoutSimulation.h"

#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"

namespace
{

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

float GScoreboardScale = brawlerScoreboardVisualization::kScoreboardDefaultScale;

static_assert(brawlerScoreboardVisualization::kScoreboardDefaultScale == 1.0f,
              "Was prose: the CVar help string below says `Default 1.0`, and so does "
              "docs/ScoreboardDisplay-rationale.md section 4. "
              "Retune the sentences too, or retune this line -- never only the constant.");
static_assert(brawlerScoreboardVisualization::kScoreboardMinScale == 0.25f
                  && brawlerScoreboardVisualization::kScoreboardMaxScale == 4.f,
              "Was prose: the CVar help string below says `CLAMPED to [0.25, 4]`, and "
              "docs/ScoreboardDisplay-rationale.md section 4 repeats the same range. "
              "Both are sentences a reader trusts and nothing else verifies.");

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

static_assert(brawlerScoreboardVisualization::kScoreboardDefaultBackgroundAlpha == 0.f,
              "Was prose: the CVar help string below says `Default 0` and `At 0 -- the "
              "default -- the backdrop is not drawn at all`.");
static_assert(brawlerScoreboardVisualization::kScoreboardMinBackgroundAlpha == 0.f
                  && brawlerScoreboardVisualization::kScoreboardMaxBackgroundAlpha == 1.f,
              "Was prose: the CVar help string below says `CLAMPED to [0, 1]`, and "
              "docs/ScoreboardDisplay-rationale.md section 4 repeats it.");

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

bool enabled()
{
	return GScoreboard;
}

float scale()
{
	return brawlerScoreboardVisualization::clampScoreboardScale(GScoreboardScale);
}

float backgroundAlpha()
{
	return brawlerScoreboardVisualization::clampScoreboardBackgroundAlpha(GScoreboardAlpha);
}

std::optional<uint32_t> displayTick(const ASimulationManagerUImpl* manager)
{
	if (manager == nullptr)
		return std::nullopt;

	if (manager->runsPrediction())
		return static_cast<uint32_t>(manager->getClientClock().getPredictionTick());

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

	UWorld* iterableWorld = const_cast<UWorld*>(world);

	for (TActorIterator<AOGBrawlerUECharacter> it(iterableWorld); it; ++it)
	{
		const AOGBrawlerUECharacter* character = *it;
		if (character == nullptr)
			continue;

		ScoreboardRow row;

		row.characterId = toStorageKey(character->GetSimCharacterId());

		const int32 replicatedScore = character->GetRingoutScore();
		row.score = (replicatedScore > 0) ? static_cast<uint32_t>(replicatedScore) : 0u;

		const FLinearColor tint = character->GetBrawlerColor();
		row.swatch = brawlerScoreboardVisualization::ScoreboardInk{
			tint.R, tint.G, tint.B
		};

		if (const std::optional<brawlerRingout::State> ringout =
				manager->getRingoutVizState(row.characterId))
		{
			row.isDead = brawlerRingout::isDead(*ringout);

			if (row.isDead && nowTick.has_value())
			{
				row.ticksUntilRespawn =
					brawlerScoreboardVisualization::scoreboardTicksUntilRespawn(
						ringout->respawnAtTick, *nowTick);
			}
		}

		rows.push_back(row);
	}

	return brawlerScoreboardVisualization::orderedScoreboardRows(std::move(rows));
}

} // namespace scoreboardVisualizationUImpl
