// SPDX-License-Identifier: MPL-2.0

#include "ChaosPhysicsFactory.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"

#include "Runtime/Engine/Classes/Components/SphereComponent.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

ChaosPhysicsFactory::PhysicalObjectResult ChaosPhysicsFactory::createPhysicalObject(
	const PhysicalObjectDescriptor& descriptor,
	const char* name)
{
	const auto& primaryShape = descriptor.shapes[0];

	// Create UE sphere component from SphereGeometry.
	// Only SphereGeometry is supported for now; extend when needed.
	USphereComponent* component = std::visit([&](const auto& geo) -> USphereComponent* {
		if constexpr (std::is_same_v<std::decay_t<decltype(geo)>, SphereGeometry>)
		{
			auto* comp = NewObject<USphereComponent>(m_owner, FName(name));
			comp->SetSphereRadius(geo.radius);
			return comp;
		}
		else
		{
			checkf(false, TEXT("ChaosPhysicsFactory::createPhysicalObject — only SphereGeometry supported"));
			return nullptr;
		}
	}, primaryShape.geometry);

	if (!component)
	{
		return PhysicalObjectResult{};
	}

	// Attach and configure body flags from descriptor — no hardcoded values
	component->SetupAttachment(m_attachParent);
	component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	component->SetSimulatePhysics(descriptor.body.simulatePhysics);
	component->SetEnableGravity(descriptor.body.enableGravity);
	component->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);

	// Translate first set bit in shape's CollisionCategories -> ECollisionChannel
	// using the query adapter's explicit DAttack-to-Chaos mapping.
	for (uint32_t cat = 0; cat < 32; ++cat)
	{
		if (primaryShape.categories.contains(cat))
		{
			component->SetCollisionObjectType(m_queryAdapter.toEngineChannel(cat));
			break;
		}
	}

	component->RegisterComponent();

	// Every body created through this factory replays Chaos's recorded history during
	// resim instead of re-simulating live. Centralized here so callers can't forget.
	component->GetBodyInstance()->GetPhysicsActorHandle()->GetGameThreadAPI()
		.SetResimType(Chaos::EResimType::ResimAsFollower);

	// Extract the BodyId from the newly-created body's Chaos handle. Note the
	// adapter is stateless — Chaos assigns the particle UniqueIdx at creation
	// (line 65's GetBodyInstanceAsyncPhysicsTickHandle materializes it); getBodyId
	// just wraps it in the engine-agnostic BodyId type.
	auto bodyHandle = component->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
	BodyId bodyId = m_bodyAdapter.getBodyId(bodyHandle);

	// Register each shape with the SpatialQueryAdapter for enable/disable.
	// Thread the factory's parent body id so overlap() emits an actor-level rootBodyId
	// for hits on these shapes (every shape here belongs to m_parentBodyId's character).
	std::vector<ShapeId> shapeIds;
	for (unsigned int i = 0; i < static_cast<unsigned int>(descriptor.shapes.size()); ++i)
	{
		shapeIds.push_back(m_queryAdapter.registerShape(bodyHandle, i, m_parentBodyId));
	}

	return PhysicalObjectResult{component, bodyId, std::move(shapeIds)};
}
