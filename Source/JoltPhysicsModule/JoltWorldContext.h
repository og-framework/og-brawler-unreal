#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "JoltPhysicsModule.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/ObjectLayerPairFilterTable.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceTable.h>
#include <Jolt/Physics/Collision/BroadPhase/ObjectVsBroadPhaseLayerFilterTable.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>

// ---------------------------------------------------------------------------
// Collision layers
// ---------------------------------------------------------------------------
namespace JoltLayers
{
	static constexpr JPH::ObjectLayer STATIC   = 0;
	static constexpr JPH::ObjectLayer DYNAMIC  = 1;
	static constexpr JPH::ObjectLayer DISABLED = 2;
	static constexpr uint32 NUM_LAYERS         = 3;
}

namespace JoltBroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer BP_STATIC  (0);
	static constexpr JPH::BroadPhaseLayer BP_DYNAMIC (1);
	static constexpr JPH::BroadPhaseLayer BP_DISABLED(2);
	static constexpr uint32 NUM_LAYERS = 3;
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
struct JOLTPHYSICSMODULE_API JoltWorldConfig
{
	uint32 maxBodies              = 4096;
	uint32 maxBodyPairs           = 4096;
	uint32 maxContactConstraints  = 1024;
	uint32 tempAllocatorSizeMB    = 16;
	float  gravityX               = 0.0f;
	float  gravityY               = 0.0f;
	float  gravityZ               = -9.81f; // meters, Z-up
};

// ---------------------------------------------------------------------------
// JoltWorldContext — owns one JPH::PhysicsSystem + supporting objects
// ---------------------------------------------------------------------------
class JOLTPHYSICSMODULE_API JoltWorldContext
{
public:
	explicit JoltWorldContext(const JoltWorldConfig& config = JoltWorldConfig{});
	~JoltWorldContext();

	JoltWorldContext(const JoltWorldContext&) = delete;
	JoltWorldContext& operator=(const JoltWorldContext&) = delete;

	JPH::PhysicsSystem&           getPhysicsSystem()       { return m_physicsSystem; }
	const JPH::PhysicsSystem&     getPhysicsSystem() const { return m_physicsSystem; }

	// Non-locking body interface (single-threaded access model)
	JPH::BodyInterface&           getBodyInterface();
	const JPH::BodyInterface&     getBodyInterface() const;

	const JPH::NarrowPhaseQuery&  getNarrowPhaseQuery() const;
	const JPH::BroadPhaseQuery&   getBroadPhaseQuery()  const;

	void                          optimizeBroadPhase();

	// Step the simulation by dt. Returns any physics update error.
	JPH::EPhysicsUpdateError      step(float dt);

private:
	JoltWorldConfig                           m_config;
	JPH::TempAllocatorImpl                    m_tempAllocator;
	JPH::JobSystemSingleThreaded              m_jobSystem;

	// Heap-allocated because Jolt filter/interface types are NonCopyable & non-movable.
	// Order matters: broadphase & pair filters are constructed first, then the
	// cross-filter is derived from them, and finally the PhysicsSystem is init'd.
	TUniquePtr<JPH::BroadPhaseLayerInterfaceTable>      m_broadPhaseLayerInterface;
	TUniquePtr<JPH::ObjectLayerPairFilterTable>          m_objectLayerPairFilter;
	TUniquePtr<JPH::ObjectVsBroadPhaseLayerFilterTable>  m_objectVsBroadPhaseFilter;

	JPH::PhysicsSystem                        m_physicsSystem;
};
