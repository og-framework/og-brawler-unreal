// SPDX-License-Identifier: BUSL-1.1

#include "PMSFeatureCarver.h"
#include "PMSNoise.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{

/**
 * Compute a longitudinal fade factor for the feature.
 * Returns 0 at start/end boundaries, 1 in the middle.
 */
float ComputeLongitudinalFade(float WorldY, float Start, float End, float TransitionDist)
{
	if (WorldY < Start || WorldY > End)
	{
		return 0.0f;
	}
	float Fade = 1.0f;
	if (TransitionDist > 0.0f)
	{
		if (WorldY < Start + TransitionDist)
		{
			Fade = FMath::Clamp((WorldY - Start) / TransitionDist, 0.0f, 1.0f);
		}
		else if (WorldY > End - TransitionDist)
		{
			Fade = FMath::Clamp((End - WorldY) / TransitionDist, 0.0f, 1.0f);
		}
		// Smoothstep the fade
		Fade = Fade * Fade * (3.0f - 2.0f * Fade);
	}
	return Fade;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// CarveCanyon
// ---------------------------------------------------------------------------
void FPMSFeatureCarver::CarveCanyon(FPMSHeightfield& Heightfield,
                                     const FPMSCanyonDef& Canyon,
                                     uint32 Seed)
{
	const float HalfWidth = Canyon.Width * 0.5f;
	if (HalfWidth <= 0.0f || Canyon.Depth <= 0.0f)
	{
		return;
	}

	for (int32 Gy = 0; Gy < Heightfield.ResolutionY; ++Gy)
	{
		const float WorldY = Heightfield.GridToWorldY(Gy);

		const float LongFade = ComputeLongitudinalFade(
			WorldY, Canyon.StartDistance, Canyon.EndDistance, Canyon.TransitionDist);
		if (LongFade <= 0.0f)
		{
			continue;
		}

		for (int32 Gx = 0; Gx < Heightfield.ResolutionX; ++Gx)
		{
			const float WorldX = Heightfield.GridToWorldX(Gx);

			// Lateral distance from canyon center
			const float DistFromCenter = FMath::Abs(WorldX - Canyon.LateralOffset);
			if (DistFromCenter >= HalfWidth)
			{
				continue;
			}

			// Power-curve falloff: deepest at center, zero at edges
			const float NormDist = DistFromCenter / HalfWidth;
			const float DepthFactor = FMath::Pow(FMath::Max(0.0f, 1.0f - NormDist),
			                                      Canyon.EdgeSmoothness);

			const float Subtraction = Canyon.Depth * DepthFactor * LongFade;

			const float Current = Heightfield.SampleHeight(Gx, Gy);
			Heightfield.SetHeight(Gx, Gy, Current - Subtraction);
		}
	}
}

// ---------------------------------------------------------------------------
// LayerCliff
// ---------------------------------------------------------------------------
void FPMSFeatureCarver::LayerCliff(FPMSHeightfield& Heightfield,
                                    const FPMSCliffLayerDef& Cliff,
                                    uint32 Seed)
{
	if (Cliff.AdditionalHeight <= 0.0f)
	{
		return;
	}

	for (int32 Gy = 0; Gy < Heightfield.ResolutionY; ++Gy)
	{
		const float WorldY = Heightfield.GridToWorldY(Gy);

		const float LongFade = ComputeLongitudinalFade(
			WorldY, Cliff.StartDistance, Cliff.EndDistance, Cliff.TransitionDist);
		if (LongFade <= 0.0f)
		{
			continue;
		}

		for (int32 Gx = 0; Gx < Heightfield.ResolutionX; ++Gx)
		{
			const float WorldX = Heightfield.GridToWorldX(Gx);

			// Only affect the correct side
			const bool bIsLeft = (WorldX < 0.0f);
			if ((Cliff.Side == EPMSCliffSide::Left && !bIsLeft) ||
			    (Cliff.Side == EPMSCliffSide::Right && bIsLeft))
			{
				continue;
			}

			// Cliff effect increases with distance from center
			const float AbsX = FMath::Abs(WorldX);
			// The cliff starts affecting terrain at some distance; use a gradient
			const float CliffGradient = FMath::Clamp(AbsX / 50.0f, 0.0f, 1.0f);

			float Addition = Cliff.AdditionalHeight * CliffGradient * LongFade;

			// Add roughness noise to the cliff face
			if (Cliff.Roughness > 0.0f)
			{
				const float RoughNoise = FPMSNoise::Simplex2D(
					WorldX * 0.05f, WorldY * 0.05f, Seed);
				Addition += Cliff.Roughness * RoughNoise * LongFade;
			}

			const float Current = Heightfield.SampleHeight(Gx, Gy);
			Heightfield.SetHeight(Gx, Gy, Current + Addition);
		}
	}
}
