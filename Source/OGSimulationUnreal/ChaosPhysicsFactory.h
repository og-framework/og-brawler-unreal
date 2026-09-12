#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include <vector>
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsObjectFactory.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"

#include "Logging/LogMacros.h"
#include "Runtime/Engine/Classes/Components/PrimitiveComponent.h"

class ChaosSpatialQueryAdapter;
class AActor;

// [movement-sim T8] Setup-time diagnostics for this factory. Verbose by design:
// at most one line per body, emitted while the actor registers, silent forever
// after. Enable with `LogOGPhysicsFactory Verbose`.
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGPhysicsFactory, Log, All);

class OGSIMULATIONUNREAL_API ChaosPhysicsFactory
{
public:
	struct PhysicalObjectResult
	{
		// [movement-sim T8] Widened from USphereComponent*: the factory now also
		// creates UCapsuleComponents, and on the adopt-root path it hands back the
		// caller's own root component unchanged. Nothing outside this file reads
		// this member — the sole external caller
		// (OGBrawlerUnreal/SimulationManagerUImpl.cpp:1161) takes only `bodyId`
		// and `shapeIds`, and the PhysicsObjectFactory concept names only those
		// two — so the widening is source-compatible everywhere.
		UPrimitiveComponent* component = nullptr;
		BodyId bodyId;
		std::vector<ShapeId> shapeIds;
	};

	// attachParent: the primitive component that acts as both (a) the UE scene-graph
	// attach target for every shape component this factory creates and (b) the body
	// whose BodyId becomes the parent for each shape's registerShape() call. Under
	// the one-deep hierarchy today this parent IS the root, so overlap() emits
	// SpatialQueryHit::rootBodyId equal to the attach parent's body for hits on any
	// shape this factory produced. See current_state.md §D10. The parent BodyId is
	// derived from attachParent's body-instance handle in the constructor — callers
	// pass only ONE handle for the parent, eliminating the previous risk of
	// attachParent and parentBodyId disagreeing.
	// Precondition: attachParent's body instance must already be initialized
	// (i.e., the parent component must have registered its physics body) before
	// the factory is constructed.
	//
	// ── [movement-sim T8] ADOPT-ROOT ────────────────────────────────────────────
	// A descriptor whose BodyDescriptor::isRoot is set does NOT get a new
	// component. createPhysicalObject configures `attachParent` ITSELF from the
	// descriptor and returns bodyId == m_parentBodyId. Consequences, all deliberate:
	//   * nothing is created and NOTHING IS REGISTERED — the adopted component was
	//     already registered before this factory was constructed (the precondition
	//     above), which is exactly what makes its BodyId available in the ctor;
	//   * the capsule is NEVER RESIZED. A CapsuleGeometry in the descriptor is
	//     checked AGAINST the component's unscaled radius/half-height and a
	//     mismatch is a checkf, because the authored capsule is the single source
	//     of truth shared by the render offset and the sim's static data — silently
	//     resizing it here would desynchronise them;
	//   * each shape registers with parent = std::nullopt, i.e. the adopted body is
	//     its OWN root, so no m_shapeBodyToRootBody entry is created for it and
	//     overlap() falls back to rootBodyId == bodyId for a direct hit.
	//
	// WHY ADOPTION AND NOT CREATION. Sibling shapes registered through this same
	// factory already carry m_shapeBodyToRootBody entries pointing at the attach
	// parent's Chaos particle. Creating a *second* root particle would strand every
	// one of those entries on the old particle and silently break hit routing
	// (architecture §4.2). Adoption changes only WHERE the routing key is read
	// from, never WHAT it is — the value stays bit-identical across the cutover.
	//
	// SINGLE-ROOT INVARIANT. At most ONE descriptor per simulatable may set isRoot.
	// Two would adopt the same component, apply two conflicting descriptors to one
	// body instance, and return the same BodyId for two logically distinct bodies —
	// after which `ownBodyId` no longer identifies a body. The factory is called
	// once per declaration and cannot see across calls, so the invariant belongs to
	// the physics composite that owns the declarations; the parent = std::nullopt
	// registration above is only correct while it holds.
	// ───────────────────────────────────────────────────────────────────────────
	ChaosPhysicsFactory(ChaosPhysicsBodyAdapter& bodyAdapter,
						ChaosSpatialQueryAdapter& queryAdapter,
						AActor* owner,
						UPrimitiveComponent* attachParent)
		: m_bodyAdapter(bodyAdapter), m_queryAdapter(queryAdapter),
		  m_owner(owner), m_attachParent(attachParent),
		  m_parentBodyId(bodyAdapter.getBodyId(
			  attachParent->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle())) {}

	PhysicalObjectResult createPhysicalObject(
		const PhysicalObjectDescriptor& descriptor,
		const char* name);

private:
	// [movement-sim T8] Every descriptor-driven setting, applied identically to a
	// component this factory just created and to an adopted root. Shared by BOTH
	// branches of createPhysicalObject so the two can never drift.
	void applyDescriptor(UPrimitiveComponent* component,
						 const PhysicalObjectDescriptor& descriptor);

	ChaosPhysicsBodyAdapter& m_bodyAdapter;
	ChaosSpatialQueryAdapter& m_queryAdapter;
	AActor* m_owner;
	UPrimitiveComponent* m_attachParent;
	BodyId m_parentBodyId;
};

static_assert(PhysicsObjectFactory<ChaosPhysicsFactory>);
