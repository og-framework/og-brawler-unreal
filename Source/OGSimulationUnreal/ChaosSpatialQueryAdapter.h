#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include <vector>
#include <array>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <unordered_map>

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
	// parentBodyId: the IMMEDIATE parent of this shape in the body hierarchy — typically
	// the character capsule for one-deep hierarchies (hurtbox/guard shapes register with
	// the capsule as parent). When supplied, registerShape walks the parent chain at
	// store-time to resolve the topmost ancestor (root), so overlap() emits
	// SpatialQueryHit::rootBodyId equal to the root regardless of hierarchy depth. When
	// nullopt (standalone bodies), the shape is treated as its own root -> rootBodyId
	// falls back to the shape's own bodyId (see current_state.md §D10). No parent-
	// required invariant is enforced here — a consumer that omits the parent simply
	// opts into rootBodyId == bodyId.
	// Invariant: the body referenced by parentBodyId must correspond to either
	// (a) a body whose BodyId was obtained via ChaosPhysicsBodyAdapter::getBodyId
	// — no shape-map entry, walk terminates immediately at "no entry, this IS
	// root", or (b) an intermediate shape registered via a prior registerShape()
	// call — its map entry already points to the flattened root, walk terminates
	// in one hop. Under this invariant the parent-chain walk terminates in at
	// most one lookup.
	ShapeId registerShape(FBodyInstanceAsyncPhysicsTickHandle body,
	                      unsigned int shapeIndex,
	                      std::optional<BodyId> parentBodyId);

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

	// Maps a shape body's Chaos particle UniqueIdx -> the ROOT body's Chaos particle
	// UniqueIdx. Populated at registerShape when a parent is supplied; each stored
	// entry already points to the topmost ancestor because registerShape walks the
	// parent chain at store-time to flatten. Consulted in overlap() to emit
	// SpatialQueryHit::rootBodyId with a single map lookup regardless of hierarchy
	// depth. A missing entry means "standalone" -> rootBodyId falls back to the hit's
	// own bodyId.
	std::unordered_map<uint32_t, uint32_t> m_shapeBodyToRootBody;

	// Forward: indexed by DAttack category ID.
	std::vector<ECollisionChannel> m_toEngine;
	// Reverse: indexed by Chaos channel integer. Sparse (32 slots max for ECollisionChannel).
	static constexpr uint32_t kUnmapped = UINT32_MAX;
	std::array<uint32_t, 32> m_toDAttack;
};

static_assert(SpatialQueryAdapter<ChaosSpatialQueryAdapter>);
