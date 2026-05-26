// SPDX-License-Identifier: MPL-2.0

#include <vector>
#include <variant>
#include "glm/mat4x4.hpp"

#pragma once

class DVolumeAsset;

struct DVolumeShapeCreationParams
{
	DVolumeShapeCreationParams(const DVolumeAsset* asset, const glm::mat4x4& transform)
		: asset(asset), transform(transform)
	{
	}
	const DVolumeAsset* asset;
	glm::mat4x4 transform;
};

struct DVolumeCreationParams
{
	std::vector<DVolumeShapeCreationParams> assets;
	float test;
};

template <typename dShape>
class DVolume
{
public:
	DVolume(const DVolumeCreationParams& creationParams)
	{
		for (const DVolumeShapeCreationParams& shapeCreationParams : creationParams.assets)
		{
			m_shapes.push_back(dShape(shapeCreationParams.asset, shapeCreationParams.transform));
			m_shapeCreationInfo.push_back(shapeCreationParams);
		}
	}

	~DVolume()
	{
	}

	const std::vector<dShape>& getShapes() const
	{
		return m_shapes;
	}

private:
	std::vector<dShape> m_shapes;
	std::vector<DVolumeShapeCreationParams> m_shapeCreationInfo;
};
