// SPDX-License-Identifier: BUSL-1.1
// docs/InputHistoryDisplay-rationale.md · docs/InputHistoryVisualizationUImpl-guards.md

#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"

#include "HAL/IConsoleManager.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "OGBrawler/BrawlerInputHistoryVisualizationPanel.h"

#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"

namespace
{

bool GInputHistory = false;

static FAutoConsoleVariableRef CVarInputHistory(
	TEXT("OGBrawler.InputHistory"),
	GInputHistory,
	TEXT("0 = off (default). Master gate for every input-history visualiser; each one also ")
	TEXT("has its own CVar, all of which default on, so setting this to 1 turns them all on."),
	ECVF_Default);

bool GInputHistoryDisplay = true;

static FAutoConsoleVariableRef CVarInputHistoryDisplay(
	TEXT("OGBrawler.InputHistoryDisplay"),
	GInputHistoryDisplay,
	TEXT("1 = on (default), 0 = off. Draws the per-tick input-history panel for the first ")
	TEXT("local player's character and runs the render-rate poll that feeds it, when the ")
	TEXT("master OGBrawler.InputHistory is also on."),
	ECVF_Default);

bool GInputHistoryProvenance = true;

static FAutoConsoleVariableRef CVarInputHistoryProvenance(
	TEXT("OGBrawler.InputHistoryProvenance"),
	GInputHistoryProvenance,
	TEXT("1 = on (default), 0 = off. Draws the capture-provenance frame-meter bar and runs ")
	TEXT("the lane poll that feeds it, when the master OGBrawler.InputHistory is also on."),
	ECVF_Default);

bool GInputHistoryCharacterState = true;

static FAutoConsoleVariableRef CVarInputHistoryCharacterState(
	TEXT("OGBrawler.InputHistoryCharacterState"),
	GInputHistoryCharacterState,
	TEXT("1 = on (default), 0 = off. Draws the attack-machine-state frame-meter bar and ")
	TEXT("runs the lane poll that feeds it, when the master OGBrawler.InputHistory is also on."),
	ECVF_Default);

bool GInputHistoryRelayHealth = true;

static FAutoConsoleVariableRef CVarInputHistoryRelayHealth(
	TEXT("OGBrawler.InputHistoryRelayHealth"),
	GInputHistoryRelayHealth,
	TEXT("1 = on (default), 0 = off. Draws the relay-health frame-meter bar -- per tick, ")
	TEXT("whether the scheduled relayed read found the capture it wanted or fell back, and ")
	TEXT("whether that capture ever arrived. It is drawn on the NEAREST stack only: the ")
	TEXT("primary follows a character this client controls, which resolves no relayed input ")
	TEXT("at all. Needs the master OGBrawler.InputHistory and OGBrawler.InputHistoryNearest."),
	ECVF_Default);

bool GInputHistoryNearest = true;

static FAutoConsoleVariableRef CVarInputHistoryNearest(
	TEXT("OGBrawler.InputHistoryNearest"),
	GInputHistoryNearest,
	TEXT("1 = on (default), 0 = off. Draws a SECOND frame-meter stack for the brawler ")
	TEXT("nearest the first local one -- the only way a remote proxy's corrections, ")
	TEXT("machine state and relayed-input delay are visible at all. The nearest stack ")
	TEXT("takes the bottom anchor and the primary is lifted one stack above it; at 0 the ")
	TEXT("primary returns to the anchor and nothing else is drawn. Every other ")
	TEXT("input-history CVar keeps its meaning and gates BOTH stacks, when the master ")
	TEXT("OGBrawler.InputHistory is also on."),
	ECVF_Default);

int32 GInputHistoryLaneTicks = static_cast<int32>(
	brawlerInputHistoryVisualization::kTickLaneDefaultRetainedTicks);

static FAutoConsoleVariableRef CVarInputHistoryLaneTicks(
	TEXT("OGBrawler.InputHistoryLaneTicks"),
	GInputHistoryLaneTicks,
	TEXT("How many capture ticks of the per-tick frame-meter lanes are retained for ")
	TEXT("reading. Default 120; CLAMPED to [1, 240] when read, so a smaller or larger ")
	TEXT("value is pulled to the nearer end rather than rejected. The lanes always ")
	TEXT("allocate 240 ticks, so changing this reallocates nothing and drops no cell."),
	ECVF_Default);

bool GInputHistoryPauseIdle = true;

static FAutoConsoleVariableRef CVarInputHistoryPauseIdle(
	TEXT("OGBrawler.InputHistoryPauseIdle"),
	GInputHistoryPauseIdle,
	TEXT("1 = on (default), 0 = off. While on, the two frame-meter lanes stop recording ")
	TEXT("once the stick has been neutral, no attack button held and the attack machine ")
	TEXT("Idle for a short run of consecutive ticks, so the retained window holds activity ")
	TEXT("instead of a wall of Idle. Each collapsed span shows as one marked cell across ")
	TEXT("both bars carrying the number of ticks it removed. COST: a correction or a ")
	TEXT("resimulation landing while idle is elided with everything else, so a rollback ")
	TEXT("while standing still becomes invisible. SET THIS TO 0 to restore full-fidelity ")
	TEXT("recording -- that is how a desync that only happens while idle is investigated."),
	ECVF_Default);

bool GInputHistoryInputDelay = true;

static FAutoConsoleVariableRef CVarInputHistoryInputDelay(
	TEXT("OGBrawler.InputHistoryInputDelay"),
	GInputHistoryInputDelay,
	TEXT("1 = on (default), 0 = off. Adds a third frame-meter bar (the per-tick server-lag ")
	TEXT("verdict) and a decomposition line under the bars: effective delay, floor ")
	TEXT("(requested/clamped/cap) and what it is doing, the tier arm and whether a tier has ")
	TEXT("arrived, and whether the published value agrees -- when the master ")
	TEXT("OGBrawler.InputHistory is also on."),
	ECVF_Default);

float GInputHistoryPanelScale = brawlerInputHistoryVisualization::kPanelDefaultScale;

static FAutoConsoleVariableRef CVarInputHistoryPanelScale(
	TEXT("OGBrawler.InputHistoryPanelScale"),
	GInputHistoryPanelScale,
	TEXT("Size multiplier for the input-history panel. Default 1.0; CLAMPED to ")
	TEXT("[0.25, 4] when read, so a value outside it is pulled to the nearer end rather ")
	TEXT("than rejected. It scales the row geometry AND the text by the same factor, and ")
	TEXT("the panel re-centres itself on the scaled height, so nothing is reallocated."),
	ECVF_Default);

float GInputHistoryPanelAlpha =
	brawlerInputHistoryVisualization::kPanelDefaultBackgroundAlpha;

static FAutoConsoleVariableRef CVarInputHistoryPanelAlpha(
	TEXT("OGBrawler.InputHistoryPanelAlpha"),
	GInputHistoryPanelAlpha,
	TEXT("Opacity of the input-history panel's BACKGROUND only. Default 0; CLAMPED ")
	TEXT("to [0, 1] when read. At 0 -- the default -- the backdrop is not drawn at all ")
	TEXT("and the rows sit straight over the scene; at 1 the panel hides what is behind ")
	TEXT("it. The rows' own colours are unaffected."),
	ECVF_Default);

int32 GInputHistoryPanelRows = static_cast<int32>(
	brawlerInputHistoryVisualization::kPanelVisibleRows);

static FAutoConsoleVariableRef CVarInputHistoryPanelRows(
	TEXT("OGBrawler.InputHistoryPanelRows"),
	GInputHistoryPanelRows,
	TEXT("How many of the newest input-history rows the panel reserves height for and ")
	TEXT("draws. Default 24; CLAMPED to [1, 64] when read, 64 being the ring's own ")
	TEXT("capacity. The ring always stores 64 rows, so changing this reallocates ")
	TEXT("nothing and drops no row -- it only changes how many are shown."),
	ECVF_Default);

} // namespace

namespace inputHistoryVisualizationUImpl
{

bool masterEnabled()
{
	// ⛔G-08  docs/InputHistoryVisualizationUImpl-guards.md
	return GInputHistory;
}

bool displayEnabled()
{
	// ⛔G-03  docs/InputHistoryVisualizationUImpl-guards.md
	return GInputHistory && GInputHistoryDisplay;
}

bool provenanceEnabled()
{
	return GInputHistory && GInputHistoryProvenance;
}

bool inputDelayEnabled()
{
	return GInputHistory && GInputHistoryInputDelay;
}

bool characterStateEnabled()
{
	return GInputHistory && GInputHistoryCharacterState;
}

bool relayHealthEnabled()
{
	return GInputHistory && GInputHistoryRelayHealth;
}

brawlerInputHistoryVisualization::FrameMeterBarSelection barSelection(bool isNearestStack)
{
	brawlerInputHistoryVisualization::FrameMeterBarSelection selection;
	selection.provenance     = provenanceEnabled();
	selection.inputDelay     = inputDelayEnabled();
	selection.characterState = characterStateEnabled();
	// ⛔G-01  docs/InputHistoryVisualizationUImpl-guards.md
	selection.relayHealth    = isNearestStack && relayHealthEnabled();
	return selection;
}

bool anyBarEnabled()
{
	// ⛔G-02  docs/InputHistoryVisualizationUImpl-guards.md
	return brawlerInputHistoryVisualization::frameMeterEnabledBarCount(barSelection(true)) != 0u;
}

bool nearestStackEnabled()
{
	return GInputHistory && GInputHistoryNearest;
}

bool pauseLanesWhileIdle()
{
	return GInputHistoryPauseIdle;
}

uint32_t retainedLaneTicks()
{
	return brawlerInputHistoryVisualization::clampRetainedLaneTicks(
		static_cast<int64_t>(GInputHistoryLaneTicks));
}

float panelScale()
{
	return brawlerInputHistoryVisualization::clampPanelScale(GInputHistoryPanelScale);
}

float panelBackgroundAlpha()
{
	return brawlerInputHistoryVisualization::clampPanelBackgroundAlpha(
		GInputHistoryPanelAlpha);
}

std::size_t panelVisibleRows()
{
	return brawlerInputHistoryVisualization::clampPanelVisibleRows(
		static_cast<int64_t>(GInputHistoryPanelRows));
}

APlayerController* firstLocalPlayerController(const UWorld* world)
{
	if (world == nullptr)
		return nullptr;

	UGameInstance* gameInstance = world->GetGameInstance();
	if (gameInstance == nullptr || gameInstance->GetLocalPlayers().Num() == 0)
		return nullptr;

	// ⛔G-07  docs/InputHistoryVisualizationUImpl-guards.md
	ULocalPlayer* localPlayer = gameInstance->GetLocalPlayers()[0];
	if (localPlayer == nullptr)
		return nullptr;

	return localPlayer->GetPlayerController(world);
}

std::optional<unsigned int> firstLocalCharacterId(const UWorld* world)
{
	APlayerController* controller = firstLocalPlayerController(world);
	APawn*             pawn       = (controller != nullptr) ? controller->GetPawn() : nullptr;
	if (pawn == nullptr)
		return std::nullopt;

	const AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(pawn);
	if (character == nullptr)
		return std::nullopt;

	return toStorageKey(character->GetSimCharacterId());
}

std::optional<unsigned int> nearestCharacterIdTo(const ASimulationManagerUImpl* manager,
                                                 unsigned int                   localId,
                                                 std::optional<unsigned int>    previousChoice,
                                                 float&                         outDistanceCm)
{
	if (manager == nullptr)
		return std::nullopt;

	brawlerInputHistoryVisualization::NearestCharacterCandidateList candidates;
	manager->gatherNearestCandidates(candidates);

	const std::optional<unsigned int> chosen =
		brawlerInputHistoryVisualization::nearestCharacterIdTo(
			candidates, localId, previousChoice);

	if (chosen.has_value())
	{
		// ⛔G-05  docs/InputHistoryVisualizationUImpl-guards.md
		if (const std::optional<float> distance =
				brawlerInputHistoryVisualization::nearestCharacterDistanceCm(
					candidates, localId, *chosen))
		{
			outDistanceCm = *distance;
		}
	}

	return chosen;
}

} // namespace inputHistoryVisualizationUImpl
