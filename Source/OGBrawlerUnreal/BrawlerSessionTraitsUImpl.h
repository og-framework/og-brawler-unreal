// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionTraitsUImpl-rationale.md · docs/BrawlerSessionTraitsUImpl-guards.md

#pragma once

#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/PhysicsBackendHostUImpl.h"
#include "OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h"

class SimulatableBrawler;
class UOGBrawlerSimulatableComponent;
class AOGBrawlerUECharacter;
class AOGBrawlerSimulationManager;
class UOGBrawlerInputCollectionComponent;
class BrawlerSessionGameUImpl;
template <typename TraitsT> class UESimulationSession;

struct BrawlerSessionTraitsUImpl
{
	using Simulatable          = SimulatableBrawler;
	using PlayerInput          = simulatableBrawler::PlayerInput;
	using State                = simulatableBrawler::State;
	using StaticData           = simulatableBrawler::StaticData;
	using Component            = UOGBrawlerSimulatableComponent;
	using Character            = AOGBrawlerUECharacter;
	using SessionOwner         = AOGBrawlerSimulationManager;
	using Game                 = BrawlerSessionGameUImpl;
	using InputSampleSource    = const UOGBrawlerInputCollectionComponent*;
	using Composition          = BrawlerCompositionUImpl;
	using NetConfig            = UEBrawlerNetConfig;
	using EngineTraits         = ::EngineTraits;
	using PhysicsBackendWorlds = physicsBackendUImpl::BackendWorlds;

	template <typename SessionT> using PhysicsBackend = physicsBackendUImpl::BackendFor<SessionT>;
	template <typename SessionT> using TickDriverGame = BrawlerTickDriverGameUImpl<SessionT>;

	using Storage           = SimulationObjectStorage<SimulatableBrawler>;
	using Reconciliation    = SimulationReconciliation<SimulatableBrawler>;
	using InputResolution   = SimulationInputResolution<SimulatableBrawler>;
	using NetSync           = SimulationNetSync<SimulatableBrawler>;
	using SystemsExec       = BrawlerCompositionUImpl::SystemsExec;
	using SimulationCore    = BrawlerCompositionUImpl::Manager;
};

static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::Storage, BrawlerCompositionUImpl::Storage>,
	"BrawlerSessionTraitsUImpl::Storage must name the same type as BrawlerCompositionUImpl::Storage, which the session "
	"checks against its own Storage because it requires og-simulation's own core systems (user ruling 2026-10-24).");
static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::Reconciliation, BrawlerCompositionUImpl::Reconciliation>,
	"BrawlerSessionTraitsUImpl::Reconciliation must name the same type as BrawlerCompositionUImpl::Reconciliation, which "
	"the session checks against its own Reconciliation because it requires og-simulation's own core systems (user ruling "
	"2026-10-24).");
static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::InputResolution, BrawlerCompositionUImpl::InputResolution>,
	"BrawlerSessionTraitsUImpl::InputResolution must name the same type as BrawlerCompositionUImpl::InputResolution, which "
	"the session checks against its own InputResolution because it requires og-simulation's own core systems (user ruling "
	"2026-10-24).");
static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::NetSync, BrawlerCompositionUImpl::NetSync>,
	"BrawlerSessionTraitsUImpl::NetSync must name the same type as BrawlerCompositionUImpl::NetSync, which the session "
	"checks against its own NetSync because it requires og-simulation's own core systems (user ruling 2026-10-24).");
static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::SystemsExec, BrawlerCompositionUImpl::SystemsExec>,
	"BrawlerSessionTraitsUImpl::SystemsExec must name the same type as BrawlerCompositionUImpl::SystemsExec, which the "
	"session checks against the SystemsExec it spells from the composition's Systems list, because a game supplies its "
	"systems while the session requires og-simulation's own core systems and executors (user ruling 2026-10-24).");
static_assert(std::is_same_v<BrawlerSessionTraitsUImpl::SimulationCore, BrawlerCompositionUImpl::Manager>,
	"BrawlerSessionTraitsUImpl::SimulationCore must name the same type as BrawlerCompositionUImpl::Manager, which the "
	"session checks against its own ManagerType because it requires og-simulation's own core systems (user ruling "
	"2026-10-24).");

static_assert(physicsBackendUImpl::PhysicsBackendHost<BrawlerSessionTraitsUImpl::PhysicsBackend<UESimulationSession<BrawlerSessionTraitsUImpl>>>,
	"BrawlerSessionTraitsUImpl::PhysicsBackend<UESimulationSession<BrawlerSessionTraitsUImpl>> must provide every entry point of "
	"the seam (the PhysicsBackendHost concept, PhysicsBackendHostUImpl.h): the session's owner calls each one from exactly "
	"one site, unconditionally, in both configurations. PhysicsBackendHost is a brawler concept, so the MPL session cannot "
	"assert it; it is asserted here, on the type the session holds as m_physicsBackend. Was the PhysicsBackendHost row of "
	"AOGBrawlerSimulationManager::compositionContractsHold (og-simulationscheduler-withjolt task 83); moved beside the traits "
	"by og-ue-simulationcomponentandmanagersplit task 24a.");
