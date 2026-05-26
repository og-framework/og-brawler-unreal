// SPDX-License-Identifier: BUSL-1.1

#include "PMSSlopeGenerator.h"
#include "PMSNoise.h"

FPMSHeightfield FPMSSlopeGenerator::Generate(const FPMSSlopeDefinition& Definition,
                                              float StartDistance,
                                              float EndDistance)
{
	const FPMSSlopeConfig& Cfg = Definition.Config;

	FPMSHeightfield HF;
	HF.CellSize = 2.0f;
	HF.OriginX  = -Cfg.MaxLateralExtent;
	HF.OriginY  = StartDistance;

	const float LateralRange = Cfg.MaxLateralExtent * 2.0f;
	HF.ResolutionX = FMath::Max(1, FMath::CeilToInt32(LateralRange / HF.CellSize) + 1);
	HF.ResolutionY = FMath::Max(1, FMath::CeilToInt32((EndDistance - StartDistance) / HF.CellSize) + 1);

	HF.Heights.SetNumZeroed(HF.ResolutionX * HF.ResolutionY);

	for (int32 Gy = 0; Gy < HF.ResolutionY; ++Gy)
	{
		const float WorldY = HF.GridToWorldY(Gy);

		// Sample the interpolated cross-section profile at this descent distance
		const FPMSCrossSectionProfile Prof = Definition.SampleProfileAt(WorldY);

		const float HalfPlayable = Prof.PlayableWidth * 0.5f;

		// Base slope height: descending with Y
		const float BaseH = -WorldY * Cfg.BaseGrade;

		for (int32 Gx = 0; Gx < HF.ResolutionX; ++Gx)
		{
			const float WorldX = HF.GridToWorldX(Gx);

			// Distance from corridor center (X=0)
			const float AbsX = FMath::Abs(WorldX);
			const float DistFromEdge = AbsX - HalfPlayable; // <0 inside corridor, >0 outside

			float H = BaseH;

			if (DistFromEdge <= 0.0f)
			{
				// --- Inside the playable corridor ---

				// Apply banking: tilt the surface based on BankingAngle
				if (FMath::Abs(Prof.BankingAngle) > KINDA_SMALL_NUMBER)
				{
					const float BankRad = FMath::DegreesToRadians(Prof.BankingAngle);
					H += WorldX * FMath::Tan(BankRad);
				}

				// Add noise only within the playable corridor
				const float Noise = FPMSNoise::FractalNoise2D(
					WorldX * Cfg.NoiseFrequency,
					WorldY * Cfg.NoiseFrequency,
					Cfg.Seed, 4, 2.0f, 0.5f);
				H += Noise * Cfg.NoiseAmplitude;
			}
			else
			{
				// --- Outside the playable corridor ---
				// Determine wall height or drop depth based on which side

				const float TransitionWidth = 10.0f; // meters for smooth transition
				// Smooth step factor from corridor edge to full feature
				const float T = FMath::Clamp(DistFromEdge / TransitionWidth, 0.0f, 1.0f);
				const float SmoothT = T * T * (3.0f - 2.0f * T);

				if (WorldX > 0.0f)
				{
					// Right side
					if (Prof.RightWallHeight > 0.0f)
					{
						H += Prof.RightWallHeight * SmoothT;
					}
					else if (Prof.RightDropDepth > 0.0f)
					{
						H -= Prof.RightDropDepth * SmoothT;
					}
				}
				else
				{
					// Left side
					if (Prof.LeftWallHeight > 0.0f)
					{
						H += Prof.LeftWallHeight * SmoothT;
					}
					else if (Prof.LeftDropDepth > 0.0f)
					{
						H -= Prof.LeftDropDepth * SmoothT;
					}
				}
			}

			HF.SetHeight(Gx, Gy, H);
		}
	}

	return HF;
}
