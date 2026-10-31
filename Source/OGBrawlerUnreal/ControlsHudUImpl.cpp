// SPDX-License-Identifier: BUSL-1.1
// docs/ControlsHud-rationale.md

#include "OGBrawlerUnreal/ControlsHudUImpl.h"

#include "HAL/IConsoleManager.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"

#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/SharedIsometricCameraActor.h"

namespace
{

bool GControlsHud = true;

static FAutoConsoleVariableRef CVarControlsHud(
	TEXT("OGBrawler.ControlsHud"),
	GControlsHud,
	TEXT("1 = on (DEFAULT -- this is game UI, not a debug overlay), 0 = off. Draws the ")
	TEXT("controls indicator in the TOP-LEFT corner during a match: one row per camera and ")
	TEXT("control-mode hotkey, with every key bound to it, the active movement scheme lit ")
	TEXT("and the camera-look state (solo camera only). The legend below it opens with the ")
	TEXT("legend key or OGBrawler.ControlsLegend. 0 hides the indicator AND the legend; ")
	TEXT("the keys keep working."),
	ECVF_Default);

float GControlsHudScale = brawlerControlsHud::kControlsHudDefaultScale;

static_assert(brawlerControlsHud::kControlsHudDefaultScale == 1.0f,
	"Was prose: the OGBrawler.ControlsHudScale help string below says `Default 1.0`, and so does "
	"docs/ControlsHud-rationale.md section 2. Retune the sentences too, or retune this line -- never only the constant.");
static_assert(brawlerControlsHud::kControlsHudMinScale == 0.25f && brawlerControlsHud::kControlsHudMaxScale == 4.f,
	"Was prose: the OGBrawler.ControlsHudScale help string below says `CLAMPED to [0.25, 4]`, and "
	"docs/ControlsHud-rationale.md section 2 repeats the same range.");

static FAutoConsoleVariableRef CVarControlsHudScale(
	TEXT("OGBrawler.ControlsHudScale"),
	GControlsHudScale,
	TEXT("Size multiplier for the controls indicator and legend. Default 1.0; CLAMPED to ")
	TEXT("[0.25, 4] when read, so a value outside it is pulled to the nearer end, and a ")
	TEXT("non-number lands on the minimum. It scales the geometry AND the text; the panel ")
	TEXT("stays anchored top-left. Requires OGBrawler.ControlsHud 1."),
	ECVF_Default);

float GControlsHudAlpha = brawlerControlsHud::kControlsHudDefaultBackgroundAlpha;

static_assert(brawlerControlsHud::kControlsHudDefaultBackgroundAlpha == 0.4f,
	"Was prose: the OGBrawler.ControlsHudAlpha help string below says `Default 0.4`, and so does "
	"docs/ControlsHud-rationale.md section 2.");
static_assert(brawlerControlsHud::kControlsHudMinBackgroundAlpha == 0.f
		&& brawlerControlsHud::kControlsHudMaxBackgroundAlpha == 1.f,
	"Was prose: the OGBrawler.ControlsHudAlpha help string below says `CLAMPED to [0, 1]` and `At 0 the "
	"backdrop is not drawn`, and docs/ControlsHud-rationale.md section 2 repeats it.");

static FAutoConsoleVariableRef CVarControlsHudAlpha(
	TEXT("OGBrawler.ControlsHudAlpha"),
	GControlsHudAlpha,
	TEXT("Opacity of the controls indicator's and legend's BACKGROUND only. Default 0.4; ")
	TEXT("CLAMPED to [0, 1] when read. At 0 the backdrop is not drawn at all; the text ")
	TEXT("colours are unaffected. Requires OGBrawler.ControlsHud 1."),
	ECVF_Default);

bool GControlsLegend = false;

static FAutoConsoleVariableRef CVarControlsLegend(
	TEXT("OGBrawler.ControlsLegend"),
	GControlsLegend,
	TEXT("The controls help state. 0 = collapsed (DEFAULT): the indicator is only its keys ")
	TEXT("column, the active scheme's key lit (and 5 while camera look is on, solo camera ")
	TEXT("only). 1 = expanded: ")
	TEXT("each row also shows its mode name and state, and the legend with the explanation ")
	TEXT("opens below. The help key (H / gamepad View) flips this same variable, so the console ")
	TEXT("and the key never disagree. Display only -- nothing in the simulation reads it. ")
	TEXT("Requires OGBrawler.ControlsHud 1 to be seen."),
	ECVF_Default);

} // namespace

namespace controlsHudUImpl
{

bool enabled()
{
	return GControlsHud;
}

float scale()
{
	return brawlerControlsHud::clampControlsHudScale(GControlsHudScale);
}

float backgroundAlpha()
{
	return brawlerControlsHud::clampControlsHudBackgroundAlpha(GControlsHudAlpha);
}

bool legendOpen()
{
	return GControlsLegend;
}

void toggleLegend()
{
	GControlsLegend = !GControlsLegend;
}

const dInput::MappingContext& mappingContext()
{
	static const dInput::MappingContext context = dInput::gameMapping::buildDefaultContext();
	return context;
}

std::optional<brawlerControlsHud::ControlsHudInput> gatherInput(const UWorld* world)
{
	const APlayerController* controller = inputHistoryVisualizationUImpl::firstLocalPlayerController(world);
	if (controller == nullptr)
		return std::nullopt;

	const AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(controller->GetPawn());
	if (character == nullptr)
		return std::nullopt;

	const UOGBrawlerInputCollectionComponent* inputCollection = character->getInputCollection();
	if (inputCollection == nullptr)
		return std::nullopt;

	brawlerControlsHud::ControlsHudInput input;
	input.scheme = dAttackMachineSimulation::g_movementScheme.load();
	input.view = (Cast<ASharedIsometricCameraActor>(controller->GetViewTarget()) != nullptr)
		? brawlerControlsHud::CameraView::Shared
		: brawlerControlsHud::CameraView::Solo;
	input.cameraLookOn = inputCollection->getBlockLook();
	input.legendOpen = legendOpen();
	input.legacyStickSwap = dAttackMachineSimulation::g_swapMoveAndAimSticks.load();
	return input;
}

} // namespace controlsHudUImpl
