// SPDX-License-Identifier: BUSL-1.1

#include "DAttackCircleUImplementation.h"
#include "OGBrawler/DAttackCircle.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"
//#include "Runtime/Engine/Public/DrawDebugHelpers.h"
//C:\dev\UnrealEngine\Engine\Source\Runtime\Core\Public\Math\Color.h
//C:\dev\UnrealEngine\Engine\Source\Runtime\Core\Public\Math\MathFwd.h

#pragma optimize( "", off )

namespace dDAttackCircleUImplementation
{

////////////

void drawAllSegmentLines(const DAttackCircle& attackCircle, const FTransform& transform, UWorld* world)
{
	const FVector rootPosition = transform.GetTranslation();
	const FQuat rootRotation = transform.GetRotation();

	std::vector<glm::vec3> lines;
	attackCircle.getLines(lines, true);
	for (auto& line : lines)
	{
		FVector start = rootPosition;
		FVector end = rootPosition + rootRotation * FVector(line.x, line.y, line.z);
		DrawDebugLine(world, start, end, FColor::Green, false, -1.f, 0, 1.f);
	}
}

////////////

void drawSegment(const std::vector<glm::vec3>& upperVerts, const std::vector<glm::vec3>& lowerVerts, const std::vector<unsigned int>& indicies, const DAttackSegment& segment, UWorld* world)
{
	if(segment.state == DAttackRadialSequenceState::Idle)
		return;

	TArray<FVector> fUpperVerts;
	TArray<FVector> fLowerVerts;
	TArray<int32> fIndices;

	for(const glm::vec3& vert : upperVerts)
		fUpperVerts.Add(FVector(vert.x, vert.y, vert.z));

	for(const glm::vec3& vert : lowerVerts)
		fLowerVerts.Add(FVector(vert.x, vert.y, vert.z));

	for (unsigned int index : indicies)
		fIndices.Add(index);

	DrawDebugMesh(world, fUpperVerts, fIndices, getDAttackRadialSequenceStateColor(segment.state), false, -1.f, 5.f);
	DrawDebugMesh(world, fLowerVerts, fIndices, getDAttackRadialSequenceStateColor(segment.state), false, -1.f, 4.f);
}

////////////

void drawAllSegments(const DAttackCircle& attackCircle, const DAttackRadialSequence& attackSequence, const FTransform& transform, UWorld* world)
{
	const FVector rootPosition = transform.GetTranslation();
	const FQuat rootRotation = transform.GetRotation();

	for (unsigned int i = 0; i < attackSequence.getAttackSegmentCount(); ++i)
	{
		auto segment = attackSequence.getAttackSegment(i);

		float segmentHeight = 20.f;
		float segmentHeightHalved = segmentHeight * 0.5f;
		TArray<FVector> upperVerts;
		TArray<FVector> middleVerts;
		TArray<FVector> lowerVerts;
		TArray<int32> indices;
		indices.AddUninitialized(6);
		indices[0] = 0; indices[1] = 2; indices[2] = 1;
		indices[3] = 1; indices[4] = 2; indices[5] = 3;


		auto buildActiveSegment = [&rootPosition, &rootRotation, &upperVerts, &middleVerts, &lowerVerts, &attackCircle, &segmentHeightHalved](float angle) {
			FVector innerEnd = rootPosition + /*rootRotation * */FVector(FMath::Cos(angle), FMath::Sin(angle), 0.f) * attackCircle.getInnerRadius();
			FVector outerEnd = rootPosition + /*rootRotation * */ FVector(FMath::Cos(angle), FMath::Sin(angle), 0.f) * attackCircle.getOuterRadius();

			upperVerts.Add(innerEnd + FVector(0.f, 0.f, segmentHeightHalved));
			upperVerts.Add(outerEnd + FVector(0.f, 0.f, segmentHeightHalved));
			lowerVerts.Add(innerEnd - FVector(0.f, 0.f, segmentHeightHalved));
			lowerVerts.Add(outerEnd - FVector(0.f, 0.f, segmentHeightHalved));
			};
		buildActiveSegment(segment.startAngle);
		buildActiveSegment(segment.endAngle);

		DrawDebugMesh(world, upperVerts, indices, getDAttackRadialSequenceStateColor(segment.state), false, -1.f, 5.f);
		DrawDebugMesh(world, lowerVerts, indices, getDAttackRadialSequenceStateColor(segment.state), false, -1.f, 4.f);
	}
}


////////////

void drawTrace(const DAttackCircle& attackCircle, const DAttackRadialSequence& attackSequence, float traveledAngle, const FVector& initialAimAxis, float initialAimAngle, const FTransform& transform, UWorld* world)
{
	const float currentAttackSegmentAngle = attackSequence.getInitialAngle() - traveledAngle;

	for (unsigned int i = 0; i < attackSequence.getAttackSegmentCount(); ++i)
	{
		auto segment = attackSequence.getAttackSegment(i);
		if (currentAttackSegmentAngle > segment.endAngle)
		{

			FQuat initialRotationQuat(initialAimAxis, initialAimAngle);
			const FVector localStartDirection = FVector(std::cos(segment.startAngle), std::sin(segment.startAngle), 0.f);
			const FVector localEndDirection = FVector(std::cos(segment.endAngle), std::sin(segment.endAngle), 0.f);
			FVector localMidDirection = (localStartDirection + localEndDirection) * 0.5f;
			localMidDirection.Normalize();
			FVector worldMidDirection = initialRotationQuat * localMidDirection;


			const FVector rootPosition = transform.GetTranslation();
			const FQuat rootRotation = transform.GetRotation();
			const float directionAngle = segment.startAngle + (segment.endAngle - segment.startAngle) * 0.5f;
			const FVector direction = /*rootRotation * */FVector(FMath::Cos(directionAngle), FMath::Sin(directionAngle), 0.f);
			DrawDebugCircleArc(world, rootPosition, attackCircle.getOuterRadius(), worldMidDirection, (segment.endAngle - segment.startAngle)*0.5f, 10, getDAttackRadialSequenceStateColor(segment.state), false, -1.f, 5, 2.f);
		}
	}
}


FColor getDAttackRadialSequenceStateColor(const DAttackRadialSequenceState& attackSequence)
{
	switch (attackSequence)
	{
	case DAttackRadialSequenceState::WindUp:
		return FColor::Orange;
	case DAttackRadialSequenceState::Damaging:
		return FColor::Red;
	case DAttackRadialSequenceState::WindDown:
		//return FColor::Blue;
		return FColor::Orange;
	case DAttackRadialSequenceState::Idle:
		return FColor::Yellow;
	}
	return FColor::White;
}


////////////

}

#pragma optimize( "", on )
