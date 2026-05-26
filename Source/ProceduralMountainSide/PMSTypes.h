#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// FPMSSlopeConfig — master generation parameters
// ---------------------------------------------------------------------------
struct FPMSSlopeConfig
{
	uint32 Seed              = 0;
	float  SlopeLength       = 10000.0f;   // total descent distance (m)
	float  BaseGrade         = 0.3f;       // rise/run (~17 degrees)
	float  NoiseAmplitude    = 2.0f;       // max vertical noise displacement (m)
	float  NoiseFrequency    = 0.005f;     // spatial frequency (~200m wavelength)
	float  ChunkLength       = 128.0f;     // chunk size along descent axis (m)
	float  MaxLateralExtent  = 256.0f;     // max half-width including cliffs (m)
};

// ---------------------------------------------------------------------------
// FPMSCrossSectionProfile — terrain shape at a point along the descent
// ---------------------------------------------------------------------------
struct FPMSCrossSectionProfile
{
	float PlayableWidth   = 100.0f;  // traversable corridor width (m)
	float LeftWallHeight  = 0.0f;    // blocking cliff height on left (0 = none)
	float RightWallHeight = 0.0f;    // blocking cliff height on right (0 = none)
	float LeftDropDepth   = 0.0f;    // drop-off depth on left (0 = none)
	float RightDropDepth  = 0.0f;    // drop-off depth on right (0 = none)
	float BankingAngle    = 0.0f;    // surface tilt in degrees (0 = flat)

	static FPMSCrossSectionProfile Lerp(const FPMSCrossSectionProfile& A,
	                                     const FPMSCrossSectionProfile& B,
	                                     float T)
	{
		FPMSCrossSectionProfile Result;
		Result.PlayableWidth   = FMath::Lerp(A.PlayableWidth,   B.PlayableWidth,   T);
		Result.LeftWallHeight  = FMath::Lerp(A.LeftWallHeight,  B.LeftWallHeight,  T);
		Result.RightWallHeight = FMath::Lerp(A.RightWallHeight, B.RightWallHeight, T);
		Result.LeftDropDepth   = FMath::Lerp(A.LeftDropDepth,   B.LeftDropDepth,   T);
		Result.RightDropDepth  = FMath::Lerp(A.RightDropDepth,  B.RightDropDepth,  T);
		Result.BankingAngle    = FMath::Lerp(A.BankingAngle,    B.BankingAngle,    T);
		return Result;
	}
};

// ---------------------------------------------------------------------------
// FPMSProfileKeyframe — a profile at a specific descent distance
// ---------------------------------------------------------------------------
struct FPMSProfileKeyframe
{
	float DistanceAlongSlope = 0.0f;
	FPMSCrossSectionProfile Profile;
};

// ---------------------------------------------------------------------------
// FPMSSlopeDefinition — full slope specification (config + keyframes)
// ---------------------------------------------------------------------------
struct FPMSSlopeDefinition
{
	FPMSSlopeConfig Config;
	TArray<FPMSProfileKeyframe> ProfileKeyframes;

	/**
	 * Hermite-interpolates the cross-section profile at the given descent distance.
	 * Clamps to first/last keyframe outside the keyframe range.
	 */
	FPMSCrossSectionProfile SampleProfileAt(float Distance) const
	{
		const int32 Num = ProfileKeyframes.Num();
		if (Num == 0)
		{
			return FPMSCrossSectionProfile();
		}
		if (Distance <= ProfileKeyframes[0].DistanceAlongSlope || Num == 1)
		{
			return ProfileKeyframes[0].Profile;
		}
		if (Distance >= ProfileKeyframes[Num - 1].DistanceAlongSlope)
		{
			return ProfileKeyframes[Num - 1].Profile;
		}

		// Find the surrounding keyframes
		int32 Idx = 0;
		for (int32 I = 0; I < Num - 1; ++I)
		{
			if (Distance >= ProfileKeyframes[I].DistanceAlongSlope &&
			    Distance <= ProfileKeyframes[I + 1].DistanceAlongSlope)
			{
				Idx = I;
				break;
			}
		}

		const float D0 = ProfileKeyframes[Idx].DistanceAlongSlope;
		const float D1 = ProfileKeyframes[Idx + 1].DistanceAlongSlope;
		const float Span = D1 - D0;
		if (Span <= 0.0f)
		{
			return ProfileKeyframes[Idx].Profile;
		}

		const float LinearT = (Distance - D0) / Span;

		// Hermite smoothstep: 3t^2 - 2t^3
		const float T = LinearT * LinearT * (3.0f - 2.0f * LinearT);

		return FPMSCrossSectionProfile::Lerp(
			ProfileKeyframes[Idx].Profile,
			ProfileKeyframes[Idx + 1].Profile,
			T);
	}
};
