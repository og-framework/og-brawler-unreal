#include "glm/mat4x4.hpp"

#pragma once

// SPDX-License-Identifier: MPL-2.0

struct FCollisionShape;
class DVolumeAsset;

class DShapeUImplementation
{
public:

 	DShapeUImplementation(const DVolumeAsset* DVolumeAsset, const glm::mat4x4& transform);
private:
	DShapeUImplementation() = default;

	FCollisionShape shape;
};

