// SPDX-License-Identifier: BUSL-1.1
// docs/ControlsHud-rationale.md

#pragma once

#include "CoreMinimal.h"

#include <optional>

#include "OGBrawler/BrawlerControlsHud.h"
#include "OGSimulation/InputMapping/InputActionDescriptor.h"

class UWorld;

namespace controlsHudUImpl
{

bool enabled();

float scale();

float backgroundAlpha();

bool legendOpen();

void toggleLegend();

const dInput::MappingContext& mappingContext();

std::optional<brawlerControlsHud::ControlsHudInput> gatherInput(const UWorld* world);

} // namespace controlsHudUImpl
