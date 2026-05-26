// SPDX-License-Identifier: MPL-2.0

#include <vector>
#include <variant>

#pragma once

struct FCollisionShape;

struct DLineAsset
{
	float length;
};

struct DSphereAsset
{
	float radius;
};

struct DBoxAsset
{
	float halfExtentX;
	float halfExtentY;
	float halfExtentZ;
};

struct DCapsuleAsset
{
	float radius;
	float halfHeight;
};

struct DConvexAsset
{
	//std::vector<FVector> vertices;
};

class DVolumeAsset
{
public:
	DVolumeAsset(const DLineAsset& lineAsset) : geometry(lineAsset) {}
	DVolumeAsset(const DSphereAsset& sphereAsset) : geometry(sphereAsset) {}
	DVolumeAsset(const DBoxAsset& boxAsset) : geometry(boxAsset) {}
	DVolumeAsset(const DCapsuleAsset& capsuleAsset) : geometry(capsuleAsset) {}

	using Geometry = std::variant<DLineAsset, DSphereAsset, DBoxAsset, DCapsuleAsset, DConvexAsset>;

	const Geometry& getGeometry() const { return geometry; }
private:
	DVolumeAsset() = default;
	std::variant<DLineAsset, DSphereAsset, DBoxAsset, DCapsuleAsset, DConvexAsset> geometry;
};