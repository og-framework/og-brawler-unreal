// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerCompositionUImpl-rationale.md · docs/BrawlerCompositionUImpl-guards.md

#pragma once

#include "OGSimulation/BodySlotOccupancy.h"
#include "OGSimulation/RenderSnapshot.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGSimulation/SimulationManager.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitDetectionSystem.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"
#include "OGBrawler/BrawlerRingoutScoreSystem.h"
#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"

struct BrawlerCompositionUImpl
{
	// ⛔G-01  docs/BrawlerCompositionUImpl-guards.md
	using Simulatables    = SimulatableList<SimulatableBrawler>;
	using Storage         = apply_t<SimulationObjectStorage,     Simulatables>;
	using NetSync         = apply_t<SimulationNetSync,           Simulatables>;
	using InputResolution = apply_t<SimulationInputResolution,   Simulatables>;
	using Reconciliation  = apply_t<SimulationReconciliation,    Simulatables>;
	using RenderSnapshot  = RenderSnapshotT<kMaxSimulatableSlots * 6>;

	template <typename... SimulatableTs>
	using IntegrationExecFor = SimulationIntegrationExecutor<
		simulatableBrawler::StaticData, EngineTraits::BodyAdapter, EngineTraits::QueryAdapter, SimulatableTs...>;
	using IntegrationExec = apply_t<IntegrationExecFor, Simulatables>;

	using HitDetectionSystem =
		brawlerHitDetection::System<EngineTraits::BodyAdapter, EngineTraits::QueryAdapter>;

	using Systems = SystemList<
		HitDetectionSystem,
		brawlerHitRouting::System,
		brawlerRingout::ScoreSystem>;

	template <typename... SystemTs>
	using SystemsExecFor = SimulationSystemsExecutor<Simulatables, simulatableBrawler::StaticData, SystemTs...>;
	using SystemsExec = apply_t<SystemsExecFor, Systems>;
	static_assert(brawlerHitDetection::firesBefore<SystemsExec,
					  HitDetectionSystem, brawlerHitRouting::System>,
		"BrawlerSystemsExec must fire brawlerHitDetection::System BEFORE brawlerHitRouting::System "
		"(firing order is template order): both run in preIntegrate, and routing branch 2 reads "
		"the hitsThisTick the detector writes earlier in the SAME pass, branch 5 the guard block. "
		"Reversed, routing reads them after the radial's integrate cleared them and before the "
		"detector refilled them, so NO melee hit and NO guard block is ever routed - "
		"og-netcode-v2-field-defects tasks 9 and 20.");

	using Manager = SimulationManager<
		IntegrationExec, NetSync, InputResolution, Reconciliation,
		SystemsExec, Storage, simulatableBrawler::StaticData>;
};
