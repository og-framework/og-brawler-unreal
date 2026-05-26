// SPDX-License-Identifier: MPL-2.0

#include "DShapeUImplementation.h"
#include "DVolume/DVolumeAsset.h"

DShapeUImplementation::DShapeUImplementation(const DVolumeAsset* DVolumeAsset, const glm::mat4x4& transform)
{
	auto& assetGeometry = DVolumeAsset->getGeometry();
	if (std::holds_alternative<DSphereAsset>(assetGeometry))
	{
		shape = FCollisionShape::MakeSphere(std::get<DSphereAsset>(assetGeometry).radius);
	}
	else if (std::holds_alternative<DBoxAsset>(assetGeometry))
	{
		auto& boxGeometry = std::get<DBoxAsset>(assetGeometry);
		shape = FCollisionShape::MakeBox(FVector(boxGeometry.halfExtentX, boxGeometry.halfExtentY, boxGeometry.halfExtentZ));
	}
	else if (std::holds_alternative<DCapsuleAsset>(assetGeometry))
	{
		auto& capsuleGeometry = std::get<DCapsuleAsset>(assetGeometry);
		shape = FCollisionShape::MakeCapsule(capsuleGeometry.radius, capsuleGeometry.halfHeight);
	}
	else if (std::holds_alternative<DLineAsset>(assetGeometry))
	{
		shape = FCollisionShape();
	}
	else
	{
		void* crash = nullptr;
		*(int*)crash = 0;
	}
}