// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerChaosBindingUImpl-rationale.md · docs/BrawlerChaosBindingUImpl-guards.md

#pragma once

#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGBrawler/SimulatableBrawler.h"

#include <initializer_list>

struct BrawlerChaosBindingUImpl
{
	using Simulatable = SimulatableBrawler;

	static std::initializer_list<ChaosCategoryMapping> categoryChannels();
};
