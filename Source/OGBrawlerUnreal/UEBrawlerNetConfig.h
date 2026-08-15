// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "OGSimulation/SimulationManagerConcept.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"

// ---------------------------------------------------------------------------
// UEBrawlerNetConfig — OGBrawler's concrete NetConfig binding for Unreal.
// (Stage 3 / D3.2; proposal_ogbrawler_netcode.md §2.1.)
//
// MODULE BOUNDARY (decided 2026-07-20): this file lives in OGBrawlerUnreal, NOT
// OGSimulationUnreal, even though proposal §2.1 sketches it in the latter. The
// entire point of the NetConfig concept is per-GAME parameterization, so the
// concrete binding is game-specific by construction. OGSimulationUnreal is
// currently brawler-free (only doc comments mention the brawler) and must stay
// that way so a second game — or a Godot adapter — can reuse it; a
// brawler-named config type there would be its first hard brawler dependency.
// The reusable halves live one level down and stay shared: the NetConfig
// concept in og-simulation, FUEConnectionHandle in OGSimulationUnreal.
//
// No Input / State members here — see the NetConfig doc comment in
// SimulationManagerConcept.h for why those belong to the variadic simulatable
// pack rather than to the session config.
// ---------------------------------------------------------------------------
struct UEBrawlerNetConfig
{
    using Address = FUEConnectionHandle;

    // Mirrors TimeConfig::tickFrequency (60 Hz, set in Stage 2). These are two
    // separate declarations of one physical rate: this one parameterizes the
    // per-connection templates at compile time, TimeConfig's drives the runtime
    // clock. They must move together — TimeConfigDefaultsTest pins the runtime
    // side, and NetConfigConceptTest pins this one against the same value.
    static constexpr int tickFrequencyHz = 60;
};

// Build fails here if the concept or the handle regresses.
static_assert(NetConfig<UEBrawlerNetConfig>,
    "UEBrawlerNetConfig must satisfy the NetConfig concept");
