// SPDX-License-Identifier: BUSL-1.1
// docs/PhysicsBackendHostUImpl-rationale.md

#pragma once

#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"

#if OG_PHYSICS_BACKEND_CHAOS
#include "OGBrawlerUnreal/ChaosBackendUImpl.h"
#else
#include "OGBrawlerUnreal/JoltBackendUImpl.h"
#endif

#include <concepts>
#include <optional>

class UWorld;
class AActor;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;

namespace physicsBackendUImpl
{

template <typename BackendT>
concept PhysicsBackendHost = requires(BackendT& backend, const BackendT& constBackend, UWorld& world, float dt,
	bool flag, USimmableUpdateComponent& component, AOGBrawlerUECharacter& character, SimulatableBrawler& simulatable,
	unsigned int id, const QueryVolumeDescriptor& descriptor, AActor& actor)
{
	{ backend.buildWorld(world, dt, flag) } -> std::same_as<void>;
	{ backend.startStepping(world, dt, flag) } -> std::same_as<void>;
	{ backend.onEndPlayBegin() } -> std::same_as<void>;
	{ backend.onEndPlayWorld(world) } -> std::same_as<void>;
	{ backend.beforePresent_GameThread() } -> std::same_as<void>;
	{ backend.updateVisualization_GameThread() } -> std::same_as<void>;
	{ backend.editVizQuery() } -> std::same_as<VizQuery&>;
	{ constBackend.getVizReader() } -> std::same_as<const VizReader&>;
	{ backend.registerVizVolume(descriptor, actor) } -> std::same_as<std::optional<QueryVolumeId>>;
	{ backend.createBodies_GameThread(component, character, simulatable, id) } -> std::same_as<bool>;
	{ backend.onRegistered_GameThread(component, id) } -> std::same_as<void>;
	{ backend.onUnregistered_GameThread(component, id) } -> std::same_as<void>;
	{ backend.sampleLatencyWindow_GameThread(flag) } -> std::same_as<void>;
	{ backend.logLatencyWindow_GameThread(flag) } -> std::same_as<void>;
};

#if OG_PHYSICS_BACKEND_CHAOS
using BackendWorlds = ChaosBackendWorldsUImpl;
using Backend       = ChaosBackendUImpl;
#else
using BackendWorlds = JoltBackendWorldsUImpl;
using Backend       = JoltBackendUImpl;
#endif

} // namespace physicsBackendUImpl
