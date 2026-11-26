// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionTraitsUImpl-rationale.md · docs/BrawlerSessionTraitsUImpl-guards.md

#pragma once

#include "OGSimulation/PhysicsEngineTraits.h"

#include <concepts>
#include <cstddef>
#include <optional>

class AOGBrawlerUECharacter;

using CapsuleContractCheck = void (*)(const AOGBrawlerUECharacter&, unsigned int);

template <typename BindingT, typename EngineTraitsT>
concept BrawlerEngineBinding = PhysicsEngineBinding<BindingT, EngineTraitsT> && requires
{
	{ &BindingT::checkCapsuleContract } -> std::convertible_to<CapsuleContractCheck>;
};

namespace physicsEngineSeamSelfCheck
{
struct BrawlerBindingCore
{
	using Simulatable = physicsEngineSeamSelfCheck::Simulatable;
	static int categoryChannels();
};

struct BindingWithoutParkSnap : BrawlerBindingCore
{
	static void checkCapsuleContract(const AOGBrawlerUECharacter&, unsigned int);
};

struct Binding : BindingWithoutParkSnap
{
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithoutCapsuleContract : BrawlerBindingCore
{
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithNullCapsuleContract : BrawlerBindingCore
{
	static constexpr std::nullptr_t checkCapsuleContract = nullptr;
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithNoneParkSnap : BindingWithoutParkSnap
{
	static constexpr std::nullopt_t kParkSnap = std::nullopt;
};

static_assert(BrawlerEngineBinding<Binding, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: the reference binding must model the concept over the driver-scheduled reference "
	"traits. Added by og-tickdriver task 15a; its two self-scheduled conjuncts were removed by og-session-runtime "
	"task 5.");
static_assert(!BrawlerEngineBinding<BindingWithoutParkSnap, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a binding without kParkSnap must NOT model the concept. Added by og-tickdriver "
	"task 15a.");
static_assert(!BrawlerEngineBinding<BindingWithoutCapsuleContract, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a binding of a driver-scheduled engine without checkCapsuleContract must NOT "
	"model the concept. Added by og-tickdriver task 15b, when the capsule check and the park snap became required "
	"only of driver-scheduled engines.");
static_assert(!BrawlerEngineBinding<BindingWithNullCapsuleContract, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a driver-scheduled engine's capsule check must be a function, never a typed "
	"none: a std::nullptr_t member converts to a function pointer, so a host that stored it would compile and make a "
	"null call. Was the Chaos binding's typed-none capsule check (og-tickdriver task 15a); og-tickdriver task 15b.");
static_assert(!BrawlerEngineBinding<BindingWithNoneParkSnap, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a driver-scheduled engine's park snap must be a float, never a typed none "
	"(std::nullopt_t converts to std::optional<float>). Was the Chaos binding's typed-none park snap (og-tickdriver "
	"task 15a); og-tickdriver task 15b.");
} // namespace physicsEngineSeamSelfCheck

#include "OGSimulationJoltUnreal/JoltEngineTraitsUImpl.h"
#include "OGBrawlerUnreal/BrawlerJoltBindingUImpl.h"
using EngineTraits  = JoltEngineTraitsUImpl;
using EngineBinding = BrawlerJoltBindingUImpl;

static_assert(PhysicsEngineTraits<EngineTraits>,
	"EngineTraits must model PhysicsEngineTraits: the session, the hosts and the build identity read the engine's "
	"adapter types, name, fingerprint and log sink through it. Was the per-configuration alias block of this header "
	"(og-simulationscheduler-withjolt task 83); og-tickdriver task 15a.");
static_assert(BrawlerEngineBinding<EngineBinding, EngineTraits>,
	"EngineBinding must model BrawlerEngineBinding over EngineTraits: the hosts read the brawler's category table, "
	"capsule contract check, park snap and simulatable type through it. Was brawler glue inside the two host .cpp files "
	"(the Jolt host's and the former Chaos host's, before og-tickdriver task 15b renamed them); og-tickdriver task 15a.");

#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/PhysicsBackendHostUImpl.h"
#include "OGBrawlerUnreal/BrawlerSessionHooksUImpl.h"

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
