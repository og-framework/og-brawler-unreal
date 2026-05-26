// SPDX-License-Identifier: MPL-2.0

#include "JoltWorldContext.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSettings.h>

JoltWorldContext::JoltWorldContext(const JoltWorldConfig& config)
	: m_config(config)
	, m_tempAllocator(config.tempAllocatorSizeMB * 1024 * 1024)
	, m_jobSystem(JPH::cMaxPhysicsJobs)
{
	// Build broadphase layer interface
	m_broadPhaseLayerInterface = MakeUnique<JPH::BroadPhaseLayerInterfaceTable>(JoltLayers::NUM_LAYERS, JoltBroadPhaseLayers::NUM_LAYERS);
	m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(JoltLayers::STATIC,   JoltBroadPhaseLayers::BP_STATIC);
	m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(JoltLayers::DYNAMIC,  JoltBroadPhaseLayers::BP_DYNAMIC);
	m_broadPhaseLayerInterface->MapObjectToBroadPhaseLayer(JoltLayers::DISABLED, JoltBroadPhaseLayers::BP_DISABLED);

	// Build object layer pair filter
	m_objectLayerPairFilter = MakeUnique<JPH::ObjectLayerPairFilterTable>(JoltLayers::NUM_LAYERS);
	m_objectLayerPairFilter->EnableCollision(JoltLayers::DYNAMIC, JoltLayers::STATIC);
	m_objectLayerPairFilter->EnableCollision(JoltLayers::DYNAMIC, JoltLayers::DYNAMIC);
	// DISABLED collides with nothing (default)

	// Derive cross-filter from the two above
	m_objectVsBroadPhaseFilter = MakeUnique<JPH::ObjectVsBroadPhaseLayerFilterTable>(
		*m_broadPhaseLayerInterface, JoltBroadPhaseLayers::NUM_LAYERS,
		*m_objectLayerPairFilter, JoltLayers::NUM_LAYERS
	);

	// Initialize physics system
	m_physicsSystem.Init(
		config.maxBodies,
		0, // numBodyMutexes — 0 = auto (irrelevant for single-threaded)
		config.maxBodyPairs,
		config.maxContactConstraints,
		*m_broadPhaseLayerInterface,
		*m_objectVsBroadPhaseFilter,
		*m_objectLayerPairFilter
	);

	m_physicsSystem.SetGravity(JPH::Vec3(config.gravityX, config.gravityY, config.gravityZ));

	// Ensure deterministic simulation
	JPH::PhysicsSettings settings = m_physicsSystem.GetPhysicsSettings();
	settings.mDeterministicSimulation = true;
	m_physicsSystem.SetPhysicsSettings(settings);

	UE_LOG(LogJoltPhysics, Log, TEXT("JoltWorldContext created (maxBodies=%u, gravity=(%.2f,%.2f,%.2f))"),
		config.maxBodies, config.gravityX, config.gravityY, config.gravityZ);
}

JoltWorldContext::~JoltWorldContext()
{
	UE_LOG(LogJoltPhysics, Log, TEXT("JoltWorldContext destroyed"));
}

JPH::BodyInterface& JoltWorldContext::getBodyInterface()
{
	return m_physicsSystem.GetBodyInterfaceNoLock();
}

const JPH::BodyInterface& JoltWorldContext::getBodyInterface() const
{
	return m_physicsSystem.GetBodyInterfaceNoLock();
}

const JPH::NarrowPhaseQuery& JoltWorldContext::getNarrowPhaseQuery() const
{
	return m_physicsSystem.GetNarrowPhaseQueryNoLock();
}

const JPH::BroadPhaseQuery& JoltWorldContext::getBroadPhaseQuery() const
{
	return m_physicsSystem.GetBroadPhaseQuery();
}

void JoltWorldContext::optimizeBroadPhase()
{
	m_physicsSystem.OptimizeBroadPhase();
}

JPH::EPhysicsUpdateError JoltWorldContext::step(float dt)
{
	return m_physicsSystem.Update(dt, 1, &m_tempAllocator, &m_jobSystem);
}
