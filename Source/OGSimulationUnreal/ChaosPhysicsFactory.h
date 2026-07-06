#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include <vector>
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsObjectFactory.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"

#include "Runtime/Engine/Classes/Components/PrimitiveComponent.h"

class ChaosSpatialQueryAdapter;
class AActor;
class USphereComponent;

class OGSIMULATIONUNREAL_API ChaosPhysicsFactory
{
public:
	struct PhysicalObjectResult
	{
		USphereComponent* component = nullptr;
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
	ChaosPhysicsBodyAdapter& m_bodyAdapter;
	ChaosSpatialQueryAdapter& m_queryAdapter;
	AActor* m_owner;
	UPrimitiveComponent* m_attachParent;
	BodyId m_parentBodyId;
};

static_assert(PhysicsObjectFactory<ChaosPhysicsFactory>);
