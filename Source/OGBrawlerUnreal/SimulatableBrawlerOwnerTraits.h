#pragma once

// SPDX-License-Identifier: BUSL-1.1

// Thin bridge header — specializes SimulatableOwnerTraits<SimulatableBrawler>
// binding it to UOGBrawlerSimulatableComponent for both prediction and authority roles.
//
// Include this header at any site that instantiates
// SimulationNetSync<SimulatableBrawler> or the registerSimulatable free
// functions for SimulatableBrawler. Do NOT include inside a UHT-parsed header
// (*.h with UCLASS/USTRUCT) since DAttack includes are not UHT-visible.
//
// Placed in OGBrawlerUnreal (not DAttack) because UOGBrawlerSimulatableComponent is a UE type
// that must not appear in the DAttack adapter-agnostic layer.

#include "OGBrawler/SimulatableBrawler.h"
#include "OGSimulation/SimulationNetSync.h"

// Forward-declare UOGBrawlerSimulatableComponent to avoid pulling in the full
// OGBrawlerSimulatableComponent.h here (which transitively drags in Chaos headers).
// Full definition is only needed at call sites that access owner methods.
class UOGBrawlerSimulatableComponent;

template <>
struct SimulatableOwnerTraits<SimulatableBrawler>
{
    using PredictionOwnerType = UOGBrawlerSimulatableComponent;
    using AuthorityOwnerType  = UOGBrawlerSimulatableComponent;
};
