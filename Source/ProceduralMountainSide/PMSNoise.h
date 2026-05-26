#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"

/**
 * Deterministic 2D Simplex noise and fractal noise utilities.
 * All functions are pure — same inputs always produce the same output.
 * No external dependencies; self-contained integer-based gradient selection.
 */
struct FPMSNoise
{
	/** 2D Simplex noise. Returns a value in [-1, 1]. */
	static float Simplex2D(float X, float Y, uint32 Seed);

	/**
	 * Fractal (fBm) noise built from multiple octaves of Simplex2D.
	 * Returns a value approximately in [-1, 1] (can slightly exceed with many octaves).
	 */
	static float FractalNoise2D(float X, float Y, uint32 Seed,
	                             int32 Octaves = 4,
	                             float Lacunarity = 2.0f,
	                             float Persistence = 0.5f);
};
