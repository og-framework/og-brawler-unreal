// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "CoreMinimal.h"
#include "glm/mat4x4.hpp"
#include "Logging/LogMacros.h"
#include "glm/vec3.hpp"
#include <glm/gtc/quaternion.hpp>

class LoggingFunctorUImpl
{
public:
	LoggingFunctorUImpl(bool enabled)
		: m_enabled(enabled)
	{}

	void logFloat(const char* name, float value)
	{
		if (!m_enabled)
			return;
		
		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %f"), *uName, value);
	}

	void logVec3(const char* name, const glm::vec3& value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %f %f %f"), *uName, value.x, value.y, value.z);
	}

	void logVec2(const char* name, const glm::vec2& value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %f %f"), *uName, value.x, value.y);
	}

	void logMat4x4(const char* name, const glm::mat4x4& value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %f %f %f %f\n%f %f %f %f\n%f %f %f %f\n%f %f %f %f"),
			*uName,
			value[0][0], value[0][1], value[0][2], value[0][3],
			value[1][0], value[1][1], value[1][2], value[1][3],
			value[2][0], value[2][1], value[2][2], value[2][3],
			value[3][0], value[3][1], value[3][2], value[3][3]);
	}

	void logQuat(const char* name, const glm::quat& value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %f %f %f %f"), *uName, value.x, value.y, value.z, value.w);
	}

	void logBool(const char* name, bool value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *uName, value ? TEXT("true") : TEXT("false"));
	}

	void logInt(const char* name, int value)
	{
		if (!m_enabled)
			return;

		FString uName(name);
		UE_LOG(LogTemp, Warning, TEXT("%s: %d"), *uName, value);
	}

private:
	LoggingFunctorUImpl() = delete;

	bool m_enabled;
};