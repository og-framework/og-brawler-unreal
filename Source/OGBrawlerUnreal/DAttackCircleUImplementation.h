#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "glm/vec3.hpp"
#include <vector>

class DAttackCircle;
class DAttackRadialSequence;
struct DAttackSegment;
enum class DAttackRadialSequenceState;
class UWorld;

namespace dDAttackCircleUImplementation
{
	void drawAllSegmentLines(const DAttackCircle& attackCircle, const FTransform& transform, UWorld* world);
	void drawSegment(const std::vector<glm::vec3>& upperVerts, const std::vector<glm::vec3>& lowerVerts, const std::vector<unsigned int>& indicies, const DAttackSegment& segment, UWorld* world);
	void drawAllSegments(const DAttackCircle& attackCircle, const DAttackRadialSequence& attackSequence, const FTransform& transform, UWorld* world);
	void drawTrace(const DAttackCircle& attackCircle, const DAttackRadialSequence& attackSequence, float traveledAngle, const FVector& initialAimAxis, float initialAimAngle, const FTransform& transform, UWorld* world);
	FColor getDAttackRadialSequenceStateColor(const DAttackRadialSequenceState& attackSequence);

}

