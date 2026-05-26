// SPDX-License-Identifier: MPL-2.0

#include "UGLMTypeConversion.h"
#include <algorithm>
#include <stdexcept>

#pragma optimize( "", off )

namespace uglm
{
	void toFtransform(const glm::mat4x4& fromTransform, FTransform& toTransform)
	{
  		// Extract the translation, rotation (as a quaternion), and scale from the glm::mat4
		glm::vec3 fromTranslation = glm::vec3(fromTransform[3]);
		glm::quat fromRotation = glm::quat_cast(fromTransform);
		glm::vec3 fromScale = glm::vec3(glm::length(fromTransform[0]), glm::length(fromTransform[1]), glm::length(fromTransform[2]));

		// Convert to Unreal types
		FVector toTranslation(fromTranslation.x, fromTranslation.y, fromTranslation.z);
		FQuat toRotation(fromRotation.x, fromRotation.y, fromRotation.z, fromRotation.w);
		FVector toScale(fromScale.x, fromScale.y, fromScale.z);

		// Create an FTransform from the translation, rotation, and scale
		toTransform = FTransform(toRotation, toTranslation, toScale);
	}
	
	FTransform toFtransform(const glm::mat4x4& fromTransform)
	{
		  // Extract the translation, rotation (as a quaternion), and scale from the glm::mat4
		glm::vec3 fromTranslation = glm::vec3(fromTransform[3]);
		glm::quat fromRotation = glm::quat_cast(fromTransform);
		glm::vec3 fromScale = glm::vec3(glm::length(fromTransform[0]), glm::length(fromTransform[1]), glm::length(fromTransform[2]));

		// Convert to Unreal types
		FVector toTranslation(fromTranslation.x, fromTranslation.y, fromTranslation.z);
		FQuat toRotation(fromRotation.x, fromRotation.y, fromRotation.z, fromRotation.w);
		FVector toScale(fromScale.x, fromScale.y, fromScale.z);

		// Create an FTransform from the translation, rotation, and scale
		FTransform toTransform(toRotation, toTranslation, toScale);

		return toTransform;
	}

	void toGLMMat4(const FTransform& fromTransform, glm::mat4x4& toTransform)
	{
		FVector fromTranslation = fromTransform.GetTranslation();
		FQuat fromRotation = fromTransform.GetRotation();
		FVector fromScale = fromTransform.GetScale3D();

		// Convert to GLM types
		glm::vec3 toTranslation(fromTranslation.X, fromTranslation.Y, fromTranslation.Z);
		//glm::quat toRotation(fromRotation.X, fromRotation.Y, fromRotation.Z, fromRotation.W);
		glm::quat toRotation(fromRotation.W, fromRotation.X, fromRotation.Y, fromRotation.Z);
		glm::vec3 toScale(fromScale.X, fromScale.Y, fromScale.Z);

		// Create a transformation matrix from the translation, rotation, and scale
		toTransform = glm::translate(glm::mat4(1.0f), toTranslation);
		toTransform *= glm::mat4_cast(toRotation);
		toTransform = glm::scale(toTransform, toScale);
	}

	glm::mat4x4 toGLMMat4(const FTransform& fromTransform)
	{
		FVector fromTranslation = fromTransform.GetTranslation();
		FQuat fromRotation = fromTransform.GetRotation();
		FVector fromScale = fromTransform.GetScale3D();

		// Convert to GLM types
		glm::vec3 toTranslation(fromTranslation.X, fromTranslation.Y, fromTranslation.Z);
		//glm::quat toRotation(fromRotation.X, fromRotation.Y, fromRotation.Z, fromRotation.W);
		glm::quat toRotation(fromRotation.W, fromRotation.X, fromRotation.Y, fromRotation.Z);
		glm::vec3 toScale(fromScale.X, fromScale.Y, fromScale.Z);

		// Create a transformation matrix from the translation, rotation, and scale
		glm::mat4x4 toTransform = glm::translate(glm::mat4(1.0f), toTranslation);
		toTransform *= glm::mat4_cast(toRotation);
		toTransform = glm::scale(toTransform, toScale);
		return toTransform;
	}

	void toGLMVec3(const FVector& fromVector, glm::vec3& toVector)
	{
		toVector.x = fromVector.X;
		toVector.y = fromVector.Y;
		toVector.z = fromVector.Z;
	}

	glm::vec3 toGLMVec3(const FVector& fromVector)
	{
		glm::vec3 toVector;
		toVector.x = fromVector.X;
		toVector.y = fromVector.Y;
		toVector.z = fromVector.Z;
		return toVector;
	}

	void toFVector(const glm::vec3& fromVector, FVector& toVector)
	{
		toVector.X = fromVector.x;
		toVector.Y = fromVector.y;
		toVector.Z = fromVector.z;
	}

	FVector toFVector(const glm::vec3& fromVector)
	{
		FVector toVector;
		toVector.X = fromVector.x;
		toVector.Y = fromVector.y;
		toVector.Z = fromVector.z;
		return toVector;
	}

	void toGLMQuat(const FQuat& fromQuat, glm::quat& toQuat)
	{
		toQuat.x = fromQuat.X;
		toQuat.y = fromQuat.Y;
		toQuat.z = fromQuat.Z;
		toQuat.w = fromQuat.W;
	}

	glm::quat toGLMQuat(const FQuat& fromQuat)
	{
		glm::quat toQuat;
		toQuat.x = fromQuat.X;
		toQuat.y = fromQuat.Y;
		toQuat.z = fromQuat.Z;
		toQuat.w = fromQuat.W;
		return toQuat;
	}

	void toFQuat(const glm::quat& fromQuat, FQuat& toQuat)
	{
		toQuat.X = fromQuat.x;
		toQuat.Y = fromQuat.y;
		toQuat.Z = fromQuat.z;
		toQuat.W = fromQuat.w;
	}

	FQuat toFQuat(const glm::quat& fromQuat)
	{
		FQuat toQuat;
		toQuat.X = fromQuat.x;
		toQuat.Y = fromQuat.y;
		toQuat.Z = fromQuat.z;
		toQuat.W = fromQuat.w;
		return toQuat;
	}

	void toFRotator(const glm::quat& fromQuat, FRotator& toRotator)
	{
		FQuat toQuat;
		toFQuat(fromQuat, toQuat);
		toRotator = toQuat.Rotator();
	}

	FRotator toFRotator(const glm::quat& fromQuat)
	{
		FRotator toRotator;
		FQuat toQuat;
		toFQuat(fromQuat, toQuat);
		toRotator = toQuat.Rotator();
		return toRotator;
	}

	FRotator toFRotator(const glm::mat4x4& fromTransform)
	{
		FTransform toTransform;
		toFtransform(fromTransform, toTransform);
		return toTransform.Rotator();
	}

	void toGLMEuler(const FRotator& fromRotator, glm::vec3& toEuler)
	{
		toEuler.x = fromRotator.Pitch;
		toEuler.y = fromRotator.Yaw;
		toEuler.z = fromRotator.Roll;
	}
}

#pragma optimize( "", on )
