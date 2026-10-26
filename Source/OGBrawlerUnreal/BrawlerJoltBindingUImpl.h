// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerJoltBindingUImpl-rationale.md · docs/BrawlerJoltBindingUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"

#include <vector>

class AOGBrawlerUECharacter;

struct BrawlerJoltBindingUImpl
{
	using Simulatable = SimulatableBrawler;

	static constexpr float kParkSnap = -0.5f * brawlerProjectileSimulation::kParkHeight;

	static const std::vector<UEStaticCategoryChannel>& categoryChannels();

	static uint32 mappedCategories();

	static bool contactCountsEnabled();

	static void checkCapsuleContract(const AOGBrawlerUECharacter& character, unsigned int id);
};
