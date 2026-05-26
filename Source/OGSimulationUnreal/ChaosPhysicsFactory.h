#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include <vector>
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsObjectFactory.h"

class ChaosPhysicsBodyAdapter;
class ChaosSpatialQueryAdapter;
class AActor;
class USceneComponent;
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

	ChaosPhysicsFactory(ChaosPhysicsBodyAdapter& bodyAdapter,
						ChaosSpatialQueryAdapter& queryAdapter,
						AActor* owner,
						USceneComponent* attachParent)
		: m_bodyAdapter(bodyAdapter), m_queryAdapter(queryAdapter),
		  m_owner(owner), m_attachParent(attachParent) {}

	PhysicalObjectResult createPhysicalObject(
		const PhysicalObjectDescriptor& descriptor,
		const char* name);

private:
	ChaosPhysicsBodyAdapter& m_bodyAdapter;
	ChaosSpatialQueryAdapter& m_queryAdapter;
	AActor* m_owner;
	USceneComponent* m_attachParent;
};

static_assert(PhysicsObjectFactory<ChaosPhysicsFactory>);
