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
#include "glm/vec3.hpp"
#include "OGSimulationUnreal/UGLMTypeConversion.h"

#include "Logging/LogMacros.h"

// [movement-sim T38] Diagnostics for the hit post-filter, and specifically for
// task 38 question 1: does FPhysicsInterface::GeomSweepMulti return the hit AT
// ALL in physics-thread context, or does filterDisabledAndUnreadyHits eat it?
// Every [SpatialQuery.Filter] line carries the result count immediately BEFORE
// the filter (raw=) and immediately AFTER it (kept=), plus one counter per drop
// reason, so those two questions can never be confused again. Self-quieting: the
// first 300 filter calls IN EACH THREAD CONTEXT are logged unconditionally; after
// that, only calls that actually dropped something, one in 64.
//
// ⭐ [movement-sim T40] "IN EACH THREAD CONTEXT" is the whole of task 40 item 2.
// Until T40 that budget was a SINGLE process-wide counter, and in the user's
// 2026-09-05 PIE run all 300 lines per peer were spent by GAME-THREAD calls three
// seconds after level load — before the physics-thread subject was armed at all.
// The log therefore held 300 `ctx=GT` rows and ZERO `ctx=PT` rows, and that was
// read as evidence that Chaos::IsInPhysicsThreadContext() is false inside the PT
// callback. It is evidence of nothing: it is a LOGGING ARTIFACT. One counter per
// context makes the PT budget unspendable by GT traffic, so the next run can
// answer a question the previous one could not even ask.
//
// ⭐ [movement-sim T40] This category ALSO carries the unmapped-collision-category
// diagnostics — [SpatialQuery.UnmappedCategory], [SpatialQuery.EmptyObjectQuery],
// [SpatialQuery.PartialObjectQuery], [SpatialQuery.UnmappedTraceCategory] and
// [SpatialQuery.MapGap] — all at Error, all either setup-time or hard-bounded to
// at most 33 lines per process. See the pure-logic block at the top of
// ChaosSpatialQueryAdapter.cpp for why none of them can spam a tick.
//
// ⚠ SHIPPED VERBOSITY, stated deliberately: this category is declared with a
// compile-time default of Log and there is no Config/DefaultEngine.ini entry for
// it, so it runs at that default and Warning AND Error lines are visible in a
// plain PIE run with no user action and no cvar to remember. That is sized for the
// next physics-thread run; it should be demoted once the physics-thread sweep
// question is closed, and NOT before.
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGSpatialQuery, Log, All);

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

	// Sweep the volume registered as `volumeId` from world pose `transform` (the
	// volume's own offsetTransform applied on top, exactly as overlap does) through
	// the world-space displacement `delta`. Returns the NEAREST BLOCKING hit only
	// (v1); a miss returns a default-constructed SweepHit (blocked == false,
	// fraction == 1).
	// READ-ONLY in the volume table: the pose is an ARGUMENT, so this never reads or
	// writes the volume's stored parentTransform and never mutates m_volumes. That is
	// what lets a mover issue several sub-sweeps per tick (wall-slide iterations)
	// without setVolumeParentTransform churn.
	SweepHit sweep(QueryVolumeId volumeId, const glm::mat4& transform, const glm::vec3& delta);

	void setVolumeParentTransform(QueryVolumeId volumeId, const glm::mat4& transform);
	void enableShape(ShapeId shapeId);
	void disableShape(ShapeId shapeId);

private:
	// Reverse mapping: Chaos channel -> DAttack category
	uint32_t toDAttackCategory(ECollisionChannel channel) const;
	// Build FCollisionObjectQueryParams from a CollisionCategories bitmask
	FCollisionObjectQueryParams toObjectQueryParams(CollisionCategories categories) const;

	// ⭐ [movement-sim T40] Registration-time validation of a descriptor's categories,
	// and the reason the "do not log on the hot query path" worry does not bind here:
	// THERE IS NO HOT PATH THROUGH EITHER MAPPING FUNCTION. toObjectQueryParams has
	// exactly ONE caller in the whole tree — registerVolume, above — and toEngineChannel
	// has three, all setup-time (registerVolume, and ChaosPhysicsFactory::applyDescriptor's
	// two body-creation loops). overlap() and sweep() read the STORED
	// VolumeEntry::objectQueryParams and ::collisionChannel and never re-derive them, so a
	// category diagnostic costs a per-tick query exactly nothing. That is what makes
	// registration the right site rather than merely the preferred one: it is both free
	// and ATTRIBUTABLE — it names the volume and the caller's own descriptor, where a
	// query-time complaint would be anonymous.
	// Emits at Error, at most once per registered volume, and ONLY when something is
	// actually wrong. A fully-mapped descriptor is silent, and so is an EMPTY
	// searchCategories mask (a real, valid case — see isSilentlyEmptyQuery in the .cpp).
	void diagnoseVolumeCategories(const QueryVolumeDescriptor& descriptor, QueryVolumeId volumeId) const;

	// Shared by overlap() and sweep(). Drops hits whose shape is query-disabled and,
	// on the physics thread, hits whose body is MID-SPAWN (simulating, but with no
	// async-physics-tick view yet). See the three-case note at the definition.
	// [movement-sim T38] It does NOT drop a hit merely because the proxy has no
	// physics-thread view: static and kinematic world geometry never has one, by
	// design, so dropping those would silently eat every hit against the world.
	// ⛔ [movement-sim T41] That was NOT, however, the reason sweeps against the world
	// came back empty. They came back empty because sweep() set bIgnoreTouches on a
	// MULTI OBJECT query and the ENGINE's pre-filter therefore rejected every shape
	// before narrow phase — raw=0 on all 586 physics-thread rows, i.e. this filter was
	// handed an empty array. Do not cite this function as the cause; see the
	// [task41-objectquery] block and the note in sweep() in the .cpp.
	// POSTCONDITION, which resolveHitIdentity depends on: every SURVIVING hit has a
	// non-null GetComponent(), a non-null GetBodyInstance() and a non-null
	// async-physics-tick Proxy.
	void filterDisabledAndUnreadyHits(TArray<FHitResult>& results) const;

	// PRECONDITION — `hit` must ALREADY have passed filterDisabledAndUnreadyHits().
	// This dereferences hit.GetComponent(), its FBodyInstance and that body's
	// async-physics-tick Proxy with NO null guards; the filter's postcondition is
	// what makes all three safe, and the filter is the code that drops the hits
	// that would not be. Both current callers keep that order: overlap() and
	// sweep() each filter before they resolve. A third caller must too. The
	// ordering was self-evident while this code was inline in overlap(); extracting
	// it so sweep() could share it turned an intra-function sequence into an
	// inter-function precondition, which is why it is written down.
	// [movement-sim T38] What the filter's postcondition does NOT promise any more
	// is a PHYSICS-THREAD VIEW: a static-geometry hit now survives the filter and
	// has none, so the physics-thread branch below null-checks
	// GetPhysicsThreadAPI() and falls back to the external view for exactly that
	// case. Before task 38 that read was unguarded and was safe only because the
	// filter dropped every such hit — a real defect, and one that would have bitten
	// the moment a static hit arrived. [movement-sim T41] It is NOT the defect that
	// made sweeps come back empty; that one lived in sweep()'s own query params.
	void resolveHitIdentity(FHitResult& hit,
	                        CollisionCategories& outCategories,
	                        BodyId& outBodyId,
	                        BodyId& outRootBodyId) const;

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
	// ⭐ [movement-sim T40] Which categories were EXPLICITLY mapped, one bit per
	// category ID. This is NOT derivable from m_toEngine and never was: the ctor
	// resizes that vector to max(mapped category)+1 and pads the gaps with
	// ECollisionChannel(0) — and ECC_WorldStatic IS ECollisionChannel(0). So "index in
	// range" and "category mapped" are different questions, and neither the stored
	// value nor the vector's size can separate them. Every T40 diagnostic keys on THIS
	// mask; none of them keys on m_toEngine.size().
	uint32_t m_mappedCategoryMask = 0;
	// Reverse: indexed by Chaos channel integer. Sparse (32 slots max for ECollisionChannel).
	static constexpr uint32_t kUnmapped = UINT32_MAX;
	std::array<uint32_t, 32> m_toDAttack;
};

static_assert(SpatialQueryAdapter<ChaosSpatialQueryAdapter>);
