// SPDX-License-Identifier: MPL-2.0

#include "ChaosSpatialQueryAdapter.h"

#include "Runtime/Experimental/Chaos/Public/Chaos/ShapeInstanceFwd.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/ShapeInstance.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h"
#include "Runtime/Engine/Public/Physics/PhysicsFiltering.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosScene.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/Framework/PhysicsSolverBase.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"
#include "Physics/Experimental/PhysInterface_Chaos.h"
#pragma optimize( "", off )

ChaosSpatialQueryAdapter::ChaosSpatialQueryAdapter(UWorld* world, std::initializer_list<ChaosCategoryMapping> categoryMappings)
	: m_world(world)
{
	m_toDAttack.fill(kUnmapped);

	for (const auto& mapping : categoryMappings)
	{
		// Forward: DAttack category -> Chaos channel
		if (mapping.dattackCategory >= m_toEngine.size())
			m_toEngine.resize(mapping.dattackCategory + 1, static_cast<ECollisionChannel>(0));
		m_toEngine[mapping.dattackCategory] = mapping.chaosChannel;

		// Reverse: Chaos channel -> DAttack category
		const uint32_t channelIdx = static_cast<uint32_t>(mapping.chaosChannel);
		if (channelIdx < m_toDAttack.size())
			m_toDAttack[channelIdx] = mapping.dattackCategory;
	}
}

QueryVolumeId ChaosSpatialQueryAdapter::registerVolume(
	const QueryVolumeDescriptor& descriptor,
	const FCollisionQueryParams& queryParams,
	const FActorInstanceHandle& owner)
{
	FCollisionShape uShape = std::visit([](const auto& geo) -> FCollisionShape {
		if constexpr (std::is_same_v<std::decay_t<decltype(geo)>, SphereGeometry>)
			return FCollisionShape::MakeSphere(geo.radius);
		else if constexpr (std::is_same_v<std::decay_t<decltype(geo)>, BoxGeometry>)
			return FCollisionShape::MakeBox(FVector(geo.halfExtents.x, geo.halfExtents.y, geo.halfExtents.z));
		else // CapsuleGeometry
			return FCollisionShape::MakeCapsule(geo.radius, geo.halfHeight);
	}, descriptor.geometry);

	FCollisionObjectQueryParams objectQueryParams = toObjectQueryParams(descriptor.searchCategories);
	ECollisionChannel collisionChannel = toEngineChannel(descriptor.traceCategory);

	m_volumes.push_back(VolumeEntry{
		uShape,
		collisionChannel,
		queryParams,
		objectQueryParams,
		FCollisionResponseParams(ECR_Block),
		glm::mat4(1.f),
		descriptor.offsetTransform
	});

	return QueryVolumeId{m_nextVolumeId++};
}

ShapeId ChaosSpatialQueryAdapter::registerShape(FBodyInstanceAsyncPhysicsTickHandle body, unsigned int shapeIndex)
{
	m_shapes.push_back(ShapeEntry{body, shapeIndex});
	return ShapeId{m_nextShapeId++};
}

SpatialQueryReport ChaosSpatialQueryAdapter::overlap(const std::vector<QueryVolumeId>& volumeIds)
{
	TArray<FHitResult> results;

	for (const auto& volumeId : volumeIds)
	{
		const auto& vol = m_volumes[volumeId.value];
		FTransform worldTransform;
		glm::mat4 worldMat = vol.parentTransform * vol.offsetTransform;
		uglm::toFtransform(worldMat, worldTransform);

		FPhysicsInterface::GeomSweepMulti(
			m_world,
			vol.uShape,
			FQuat::Identity,
			results,
			worldTransform.GetTranslation(),
			worldTransform.GetTranslation(),
			vol.collisionChannel,
			vol.queryParams,
			vol.responseParams,
			vol.objectQueryParams);
	}

	// Post-filter disabled shapes — the Chaos acceleration structure can only be
	// updated on the game thread, so we post-filter on the physics thread.
	if (Chaos::IsInPhysicsThreadContext())
	{
		for (int32 i = results.Num() - 1; i >= 0; --i)
		{
			FHitResult& hit = results[i];
			auto* proxy = hit.GetComponent()->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle().Proxy;
			if (!proxy->GetPhysicsThreadAPI()->ShapesArray()[0]->GetQueryEnabled())
				results.RemoveAt(i);
		}
	}
	else
	{
		for (int32 i = results.Num() - 1; i >= 0; --i)
		{
			FHitResult& hit = results[i];
			auto* proxy = hit.GetComponent()->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle().Proxy;
			if (!proxy->GetGameThreadAPI().ShapesArray()[0]->GetQueryEnabled())
				results.RemoveAt(i);
		}
	}

	// Convert to engine-independent results — reverse-map Chaos channels to DAttack categories.
	SpatialQueryReport report;
	for (auto& hit : results)
	{
		SpatialQueryHit sqHit;
		sqHit.objectPosition = uglm::toGLMVec3(hit.HitObjectHandle.GetLocation());
		sqHit.objectIndex = hit.HitObjectHandle.GetInstanceIndex();

		// Reverse-map: extract Chaos channel from hit, convert to DAttack category.
		auto* bodyInstance = hit.GetComponent()->GetBodyInstance();
		auto handle = bodyInstance->GetBodyInstanceAsyncPhysicsTickHandle();
		ECollisionChannel chaosChannel;
		if (Chaos::IsInPhysicsThreadContext())
		{
			chaosChannel = static_cast<ECollisionChannel>(
				GetCollisionChannel(handle.Proxy->GetPhysicsThreadAPI()
					->ShapesArray()[0]->GetQueryData().Word3));
		}
		else
		{
			chaosChannel = static_cast<ECollisionChannel>(
				GetCollisionChannel(handle.Proxy->GetGameThreadAPI()
					.ShapesArray()[0]->GetQueryData().Word3));
		}

		const uint32_t dattackCat = toDAttackCategory(chaosChannel);
		if (dattackCat != kUnmapped)
			sqHit.objectCategories = CollisionCategories::single(dattackCat);

		// Body ID from engine's native particle index
		sqHit.bodyId = BodyId{static_cast<uint32_t>(handle.Proxy->GetParticle_LowLevel()->UniqueIdx().Idx)};

		report.hits.push_back(sqHit);
	}
	return report;
}

void ChaosSpatialQueryAdapter::setVolumeParentTransform(QueryVolumeId volumeId, const glm::mat4& transform)
{
	m_volumes[volumeId.value].parentTransform = transform;
}

void ChaosSpatialQueryAdapter::enableShape(ShapeId shapeId)
{
	auto& entry = m_shapes[shapeId.value];
	entry.body.Proxy->GetPhysicsThreadAPI()->ShapesArray()[entry.shapeIndex]->SetQueryEnabled(true);
}

void ChaosSpatialQueryAdapter::disableShape(ShapeId shapeId)
{
	auto& entry = m_shapes[shapeId.value];
	entry.body.Proxy->GetPhysicsThreadAPI()->ShapesArray()[entry.shapeIndex]->SetQueryEnabled(false);
}

ECollisionChannel ChaosSpatialQueryAdapter::toEngineChannel(uint32_t dattackCategory) const
{
	if (dattackCategory < m_toEngine.size())
		return m_toEngine[dattackCategory];
	return static_cast<ECollisionChannel>(0);
}

uint32_t ChaosSpatialQueryAdapter::toDAttackCategory(ECollisionChannel channel) const
{
	const uint32_t idx = static_cast<uint32_t>(channel);
	if (idx < m_toDAttack.size())
		return m_toDAttack[idx];
	return kUnmapped;
}

FCollisionObjectQueryParams ChaosSpatialQueryAdapter::toObjectQueryParams(CollisionCategories categories) const
{
	FCollisionObjectQueryParams params;
	for (uint32_t cat = 0; cat < static_cast<uint32_t>(m_toEngine.size()); ++cat)
	{
		if (categories.contains(cat))
			params.AddObjectTypesToQuery(m_toEngine[cat]);
	}
	return params;
}
