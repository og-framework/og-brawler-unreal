// SPDX-License-Identifier: BUSL-1.1
// docs/ScoreboardDisplay-rationale.md

#pragma once

#include "CoreMinimal.h"

#include <cstdint>
#include <optional>
#include <vector>

#include "OGBrawler/BrawlerScoreboardVisualization.h"

class ASimulationManagerUImpl;
class UWorld;

namespace scoreboardVisualizationUImpl
{

bool enabled();

float scale();

float backgroundAlpha();

std::optional<uint32_t> displayTick(const ASimulationManagerUImpl* manager);

std::vector<brawlerScoreboardVisualization::ScoreboardRow> gatherScoreboardRows(
	const UWorld* world, const ASimulationManagerUImpl* manager);

} // namespace scoreboardVisualizationUImpl
