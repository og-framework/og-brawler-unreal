#pragma once

// SPDX-License-Identifier: BUSL-1.1

// Thin bridge header — specializes SimulatableOwnerTraits<SimulatableBrawler>,
// binding it to og-sim-net's per-character channel for both prediction and
// authority roles: UESimNetChannel<SimulatableBrawler> (UESimNetLanes.h), the one
// SessionChannel type over the og-net sim lanes' transport that the session's
// SimNetServer and SimNetClient both register. Since og-input-latency task P5-7;
// before it both owners were the game component, UOGBrawlerSimulatableComponent,
// whose net-sync sibling carried the traffic. UESimulationSession asserts the
// identity against both sessions' Channel (compositionContractsHold).
//
// Include this header at any site that instantiates
// SimulationNetSync<SimulatableBrawler> or the registerSimulatable free
// functions for SimulatableBrawler. Do NOT include inside a UHT-parsed header
// (*.h with UCLASS/USTRUCT) since DAttack includes are not UHT-visible.
//
// Placed in OGBrawlerUnreal (not DAttack) because the channel names the Unreal
// sim lanes' transport, which must not appear in the DAttack adapter-agnostic layer.

#include "OGBrawler/SimulatableBrawler.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulationUnreal/UESimNetLanes.h"

template <>
struct SimulatableOwnerTraits<SimulatableBrawler>
{
    using PredictionOwnerType = UESimNetChannel<SimulatableBrawler>;
    using AuthorityOwnerType  = UESimNetChannel<SimulatableBrawler>;
};
