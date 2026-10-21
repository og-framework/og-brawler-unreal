// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionTraitsUImpl-rationale.md · docs/BrawlerSessionTraitsUImpl-guards.md

#pragma once

#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/PhysicsBackendHostUImpl.h"
#include "OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h"

class SimulatableBrawler;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;
class ASimulationManagerUImpl;
class UOGBrawlerInputCollectionComponent;
class BrawlerSessionGameUImpl;
template <typename TraitsT> class UESimulationSession;

struct BrawlerSessionTraitsUImpl
{
	using Simulatable       = SimulatableBrawler;
	using PlayerInput       = simulatableBrawler::PlayerInput;
	using State             = simulatableBrawler::State;
	using StaticData        = simulatableBrawler::StaticData;
	using Component         = USimmableUpdateComponent;
	using Character         = AOGBrawlerUECharacter;
	using SessionOwner      = ASimulationManagerUImpl;
	using Game              = BrawlerSessionGameUImpl;
	using InputSampleSource = const UOGBrawlerInputCollectionComponent*;
	using Composition       = BrawlerCompositionUImpl;
	using NetConfig         = UEBrawlerNetConfig;
	using EngineTraits      = ::EngineTraits;
	using BackendWorlds     = physicsBackendUImpl::BackendWorlds;

	template <typename SessionT> using Backend        = physicsBackendUImpl::BackendFor<SessionT>;
	template <typename SessionT> using TickDriverGame = BrawlerTickDriverGameUImpl<SessionT>;
};

static_assert(physicsBackendUImpl::PhysicsBackendHost<BrawlerSessionTraitsUImpl::Backend<UESimulationSession<BrawlerSessionTraitsUImpl>>>,
	"BrawlerSessionTraitsUImpl::Backend<UESimulationSession<BrawlerSessionTraitsUImpl>> must provide every entry point of "
	"the seam (the PhysicsBackendHost concept, PhysicsBackendHostUImpl.h): the session's owner calls each one from exactly "
	"one site, unconditionally, in both configurations. PhysicsBackendHost is a brawler concept, so the MPL session cannot "
	"assert it; it is asserted here, on the type the session holds as m_backend. Was the PhysicsBackendHost row of "
	"ASimulationManagerUImpl::compositionContractsHold (og-simulationscheduler-withjolt task 83); moved beside the traits "
	"by og-ue-simulationcomponentandmanagersplit task 24a.");
