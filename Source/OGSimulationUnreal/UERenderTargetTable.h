#pragma once
// SPDX-License-Identifier: MPL-2.0
// docs/UERenderTargetTable-rationale.md · docs/UERenderTargetTable-guards.md

#include "OGSimulationUnreal.h"

#include <cstdint>
#include <optional>
#include <string>

#include "Containers/Map.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "glm/vec3.hpp"
#include "glm/gtc/quaternion.hpp"

class AActor;
class USceneComponent;

class OGSIMULATIONUNREAL_API UERenderTargetTable
{
public:
	static constexpr double kCentimetresPerLength = 1.0;

	UERenderTargetTable() = default;

	UERenderTargetTable(const UERenderTargetTable&) = delete;
	UERenderTargetTable& operator=(const UERenderTargetTable&) = delete;

	static float lengthFromCentimetres(float centimetres);

	void registerTarget(uint32_t id, AActor* actor, const USceneComponent* expectedRoot, bool runsPrediction);
	void unregisterTarget(uint32_t id);

	std::optional<glm::dvec3> applyRootPose(uint32_t id, const glm::vec3& position, const std::optional<glm::quat>& rotation);
	std::optional<glm::dvec3> readAnchor(uint32_t id) const;
	const char* name(uint32_t id) const;

private:
	struct Target
	{
		TWeakObjectPtr<AActor> actor;
		std::string            name;
	};

	AActor* actorOf(uint32_t id) const;

	TMap<uint32_t, Target> m_targets;
};
