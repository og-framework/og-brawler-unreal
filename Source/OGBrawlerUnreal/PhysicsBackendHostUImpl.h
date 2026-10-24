// SPDX-License-Identifier: BUSL-1.1
// docs/PhysicsBackendHostUImpl-rationale.md

#pragma once

#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"

#if OG_PHYSICS_BACKEND_CHAOS
#include "OGBrawlerUnreal/ChaosTickBackendUImpl.h"
#else
#include "OGSimulationUnreal/TickDriverBackendUImpl.h"
#include "OGBrawlerUnreal/BrawlerTickDriverGameUImpl.h"

extern template class TickDriverBackendUImpl<BrawlerTickDriverGame, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>;
#endif

#include <concepts>
#include <optional>
#include <type_traits>

class UWorld;
class AActor;
class UOGBrawlerSimulatableComponent;
class AOGBrawlerUECharacter;
struct BrawlerSessionTraitsUImpl;
template <typename TraitsT> class UESimulationSession;

namespace physicsBackendUImpl
{

template <typename BackendT>
concept PhysicsBackendHost = requires(BackendT& backend, const BackendT& constBackend, UWorld& world, float dt,
	bool flag, UOGBrawlerSimulatableComponent& component, AOGBrawlerUECharacter& character, SimulatableBrawler& simulatable,
	unsigned int id, const QueryVolumeDescriptor& descriptor, AActor& actor)
{
	{ backend.buildWorld(world, dt, flag) } -> std::same_as<void>;
	{ backend.startStepping(world, dt, flag) } -> std::same_as<void>;
	{ backend.onEndPlayBegin() } -> std::same_as<void>;
	{ backend.onEndPlayWorld(world) } -> std::same_as<void>;
	{ backend.beforePresent_GameThread() } -> std::same_as<void>;
	{ backend.updateVisualization_GameThread() } -> std::same_as<void>;
	{ backend.editVizQuery() } -> std::same_as<EngineTraits::VizQuery&>;
	{ constBackend.getVizReader() } -> std::same_as<const EngineTraits::VizReader&>;
	{ backend.registerVizVolume(descriptor, actor) } -> std::same_as<std::optional<QueryVolumeId>>;
	{ backend.createBodies_GameThread(component, character, simulatable, id) } -> std::same_as<bool>;
	{ backend.onRegistered_GameThread(component, id) } -> std::same_as<void>;
	{ backend.onUnregistered_GameThread(component, id) } -> std::same_as<void>;
	{ backend.editDiagnostics().sampleLatencyWindow_GameThread(flag) } -> std::same_as<void>;
	{ backend.editDiagnostics().logLatencyWindow_GameThread(flag) } -> std::same_as<void>;
};

#if OG_PHYSICS_BACKEND_CHAOS
static_assert(std::is_same_v<EngineTraits, ChaosEngineTraitsUImpl> && std::is_same_v<EngineBinding, BrawlerChaosBindingUImpl>,
	"The Chaos composition is ChaosTickBackendUImpl over ChaosEngineTraitsUImpl and BrawlerChaosBindingUImpl, so "
	"PhysicsBackendUImpl.h's selector must pick that pair in the same configuration. Added by og-tickdriver task 15b.");
template <typename SessionT>
using BackendFor    = ChaosTickBackendUImpl;
using Backend       = BackendFor<UESimulationSession<BrawlerSessionTraitsUImpl>>;
using BackendWorlds = ChaosTickBackendWorldsUImpl;
#else
static_assert(std::is_same_v<EngineTraits, JoltEngineTraitsUImpl> && std::is_same_v<EngineBinding, BrawlerJoltBindingUImpl>,
	"The Jolt composition is TickDriverBackendUImpl over JoltEngineTraitsUImpl and BrawlerJoltBindingUImpl, so "
	"PhysicsBackendUImpl.h's selector must pick that pair in the same configuration. Added by og-tickdriver task 15b.");
template <typename SessionT>
using BackendFor    = TickDriverBackendUImpl<BrawlerTickDriverGameUImpl<SessionT>, JoltEngineTraitsUImpl, BrawlerJoltBindingUImpl>;
using Backend       = BackendFor<UESimulationSession<BrawlerSessionTraitsUImpl>>;
using BackendWorlds = Backend::Worlds;
#endif

static_assert(PhysicsBackendHost<Backend>,
	"physicsBackendUImpl::Backend must provide every entry point of the seam (the PhysicsBackendHost concept) in the "
	"configuration being built: ChaosTickBackendUImpl, or TickDriverBackendUImpl over the Jolt engine traits and "
	"binding. BrawlerSessionTraitsUImpl.h asserts it again on the traits' PhysicsBackend<UESimulationSession<...>>, the type the "
	"session holds (AOGBrawlerSimulationManager asserted it until og-ue-simulationcomponentandmanagersplit task 24a). Added "
	"by og-tickdriver task 15b.");

} // namespace physicsBackendUImpl
