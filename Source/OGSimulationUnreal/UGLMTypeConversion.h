#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include <glm/gtc/quaternion.hpp>
#include "Math/MathFwd.h"

namespace uglm
{
	OGSIMULATIONUNREAL_API void toFtransform(const glm::mat4x4& fromTransform, FTransform& toTransform);
	OGSIMULATIONUNREAL_API FTransform toFtransform(const glm::mat4x4& fromTransform);

	OGSIMULATIONUNREAL_API void toGLMMat4(const FTransform& fromTransform, glm::mat4x4& toTransform);
	OGSIMULATIONUNREAL_API glm::mat4x4 toGLMMat4(const FTransform& fromTransform);

	OGSIMULATIONUNREAL_API void toGLMVec3(const FVector& fromVector, glm::vec3& toVector);
	OGSIMULATIONUNREAL_API glm::vec3 toGLMVec3(const FVector& fromVector);

	OGSIMULATIONUNREAL_API void toFVector(const glm::vec3& fromVector, FVector& toVector);
	OGSIMULATIONUNREAL_API FVector toFVector(const glm::vec3& fromVector);

	OGSIMULATIONUNREAL_API void toGLMQuat(const FQuat& fromQuat, glm::quat& toQuat);
	OGSIMULATIONUNREAL_API glm::quat toGLMQuat(const FQuat& fromQuat);

	OGSIMULATIONUNREAL_API void toFQuat(const glm::quat& fromQuat, FQuat& toQuat);
	OGSIMULATIONUNREAL_API FQuat toFQuat(const glm::quat& fromQuat);

	OGSIMULATIONUNREAL_API void toFRotator(const glm::quat& fromQuat, FRotator& toRotator);
	OGSIMULATIONUNREAL_API FRotator toFRotator(const glm::quat& fromQuat);
	OGSIMULATIONUNREAL_API FRotator toFRotator(const glm::mat4x4& fromTransform);

	OGSIMULATIONUNREAL_API void toGLMEuler(const FRotator& fromRotator, glm::vec3& toEuler);
}

