#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include <vector>
#include <array>
#include <cstdint>
#include <initializer_list>

#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/SpatialQueryResult.h"
#include "OGSimulation/SpatialQueryAdapter.h"

#include "Runtime/PhysicsCore/Public/CollisionShape.h"
#include "Runtime/Engine/Public/CollisionQueryParams.h"
#include "Runtime/Engine/Classes/Engine/EngineTypes.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h"
#include "Runtime/Engine/Classes/Components/SphereComponent.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"

#include "glm/mat4x4.hpp"
#include "OGSimulationUnreal/UGLMTypeConversion.h"

// Maps a DAttack-local collision category ID to a Chaos ECollisionChannel.
struct OGSIMULATIONUNREAL_API ChaosCategoryMapping
{
	uint32_t dattackCategory;
	ECollisionChannel chaosChannel;
};

class OGSIMULATIONUNREAL_API ChaosSpatialQueryAdapter
{
public:
	ChaosSpatialQueryAdapter(UWorld* world, std::initializer_list<ChaosCategoryMapping> categoryMappings);

	// Registration — setup-time only, not in concept

	// Register a query volume from a descriptor.
	// The engine channel is resolved from descriptor.traceCategory via the adapter's mapping table.
	QueryVolumeId registerVolume(
		const QueryVolumeDescriptor& descriptor,
		const FCollisionQueryParams& queryParams,
		const FActorInstanceHandle& owner);

	// Register a shape for enable/disable control.
	ShapeId registerShape(FBodyInstanceAsyncPhysicsTickHandle body, unsigned int shapeIndex);

	// Forward mapping: DAttack category -> Chaos channel (used by ChaosPhysicsFactory)
	ECollisionChannel toEngineChannel(uint32_t dattackCategory) const;

	// Concept operations

	SpatialQueryReport overlap(const std::vector<QueryVolumeId>& volumeIds);
	void setVolumeParentTransform(QueryVolumeId volumeId, const glm::mat4& transform);
	void enableShape(ShapeId shapeId);
	void disableShape(ShapeId shapeId);

private:
	// Reverse mapping: Chaos channel -> DAttack category
	uint32_t toDAttackCategory(ECollisionChannel channel) const;
	// Build FCollisionObjectQueryParams from a CollisionCategories bitmask
	FCollisionObjectQueryParams toObjectQueryParams(CollisionCategories categories) const;

	struct VolumeEntry
	{
		FCollisionShape uShape;
		ECollisionChannel collisionChannel;
		FCollisionQueryParams queryParams;
		FCollisionObjectQueryParams objectQueryParams;
		FCollisionResponseParams responseParams;
		glm::mat4 parentTransform{1.f};
		glm::mat4 offsetTransform{1.f};
	};

	struct ShapeEntry
	{
		FBodyInstanceAsyncPhysicsTickHandle body;
		unsigned int shapeIndex;
	};

	UWorld* m_world;
	std::vector<VolumeEntry> m_volumes;
	std::vector<ShapeEntry> m_shapes;
	uint32_t m_nextVolumeId = 0;
	uint32_t m_nextShapeId = 0;

	// Forward: indexed by DAttack category ID.
	std::vector<ECollisionChannel> m_toEngine;
	// Reverse: indexed by Chaos channel integer. Sparse (32 slots max for ECollisionChannel).
	static constexpr uint32_t kUnmapped = UINT32_MAX;
	std::array<uint32_t, 32> m_toDAttack;
};

static_assert(SpatialQueryAdapter<ChaosSpatialQueryAdapter>);
