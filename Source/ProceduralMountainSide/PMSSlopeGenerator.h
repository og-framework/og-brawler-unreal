#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"
#include "PMSTypes.h"

/**
 * FPMSHeightfield — a 2D grid of height values for a region of the slope.
 *
 * Grid layout:
 *   X axis = lateral (across the slope), centered at 0.
 *   Y axis = descent direction (increasing Y = further down the slope).
 *   heights is row-major: heights[y * ResolutionX + x].
 */
struct FPMSHeightfield
{
	TArray<float> Heights;
	int32 ResolutionX = 0;  // samples across the slope (lateral)
	int32 ResolutionY = 0;  // samples along the slope (descent)
	float CellSize    = 2.0f;
	float OriginX     = 0.0f;
	float OriginY     = 0.0f;

	/** Bounds-checked accessor. Returns 0 if out of bounds. */
	float SampleHeight(int32 X, int32 Y) const
	{
		if (X < 0 || X >= ResolutionX || Y < 0 || Y >= ResolutionY)
		{
			return 0.0f;
		}
		return Heights[Y * ResolutionX + X];
	}

	/** Set height at grid coordinates. No-op if out of bounds. */
	void SetHeight(int32 X, int32 Y, float Value)
	{
		if (X >= 0 && X < ResolutionX && Y >= 0 && Y < ResolutionY)
		{
			Heights[Y * ResolutionX + X] = Value;
		}
	}

	/** Bilinear interpolation at world coordinates. */
	float SampleHeightWorld(float WorldX, float WorldY) const
	{
		const float LocalX = (WorldX - OriginX) / CellSize;
		const float LocalY = (WorldY - OriginY) / CellSize;

		const int32 X0 = FMath::FloorToInt32(LocalX);
		const int32 Y0 = FMath::FloorToInt32(LocalY);
		const float FracX = LocalX - static_cast<float>(X0);
		const float FracY = LocalY - static_cast<float>(Y0);

		const float H00 = SampleHeight(X0,     Y0);
		const float H10 = SampleHeight(X0 + 1, Y0);
		const float H01 = SampleHeight(X0,     Y0 + 1);
		const float H11 = SampleHeight(X0 + 1, Y0 + 1);

		const float Top    = FMath::Lerp(H00, H10, FracX);
		const float Bottom = FMath::Lerp(H01, H11, FracX);
		return FMath::Lerp(Top, Bottom, FracY);
	}

	/** Central-difference normal at world coordinates. */
	FVector SampleNormalWorld(float WorldX, float WorldY) const
	{
		const float D = CellSize;
		const float Hx0 = SampleHeightWorld(WorldX - D, WorldY);
		const float Hx1 = SampleHeightWorld(WorldX + D, WorldY);
		const float Hy0 = SampleHeightWorld(WorldX, WorldY - D);
		const float Hy1 = SampleHeightWorld(WorldX, WorldY + D);

		// dH/dx and dH/dy
		const float Dx = (Hx1 - Hx0) / (2.0f * D);
		const float Dy = (Hy1 - Hy0) / (2.0f * D);

		FVector Normal(-Dx, -Dy, 1.0f);
		Normal.Normalize();
		return Normal;
	}

	/** Convert grid coords to world coords. */
	float GridToWorldX(int32 X) const { return OriginX + static_cast<float>(X) * CellSize; }
	float GridToWorldY(int32 Y) const { return OriginY + static_cast<float>(Y) * CellSize; }
};

/**
 * FPMSSlopeGenerator — generates a heightfield for a region of the slope.
 * Stateless: all state comes from the FPMSSlopeDefinition.
 */
struct FPMSSlopeGenerator
{
	/**
	 * Generate the heightfield for the slope region [StartDistance, EndDistance)
	 * along the descent axis.
	 *
	 * The generated grid covers:
	 *   X: [-MaxLateralExtent, +MaxLateralExtent]
	 *   Y: [StartDistance, EndDistance]
	 */
	static FPMSHeightfield Generate(const FPMSSlopeDefinition& Definition,
	                                 float StartDistance,
	                                 float EndDistance);
};
