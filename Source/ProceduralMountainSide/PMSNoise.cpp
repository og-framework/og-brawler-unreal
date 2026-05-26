// SPDX-License-Identifier: BUSL-1.1

#include "PMSNoise.h"

// ---------------------------------------------------------------------------
// Internal helpers — integer hash-based gradient selection for determinism
// ---------------------------------------------------------------------------
namespace
{

// Deterministic integer hash (no float operations)
FORCEINLINE uint32 HashCoords(int32 X, int32 Y, uint32 Seed)
{
	// Based on a variant of xxHash/MurmurHash mixing
	uint32 H = Seed;
	H ^= static_cast<uint32>(X) * 0x85EBCA6Bu;
	H = (H << 13) | (H >> 19);
	H *= 0xC2B2AE35u;
	H ^= static_cast<uint32>(Y) * 0xCC9E2D51u;
	H = (H << 13) | (H >> 19);
	H *= 0x1B873593u;
	// Final avalanche
	H ^= H >> 16;
	H *= 0x85EBCA6Bu;
	H ^= H >> 13;
	H *= 0xC2B2AE35u;
	H ^= H >> 16;
	return H;
}

// 2D gradient vectors (12 directions on the unit circle, integer-rational approximations)
// Using the 12-gradient set from standard simplex noise
struct FGrad2D
{
	float X, Y;
};

static const FGrad2D Gradients2D[12] = {
	{ 1.0f,  0.0f}, { -1.0f,  0.0f}, { 0.0f,  1.0f}, { 0.0f, -1.0f},
	{ 1.0f,  1.0f}, { -1.0f,  1.0f}, { 1.0f, -1.0f}, {-1.0f, -1.0f},
	{ 1.0f,  0.5f}, { -1.0f,  0.5f}, { 0.5f,  1.0f}, {-0.5f,  1.0f}
};

FORCEINLINE float GradDot(uint32 Hash, float X, float Y)
{
	const FGrad2D& G = Gradients2D[Hash % 12];
	return G.X * X + G.Y * Y;
}

FORCEINLINE int32 FastFloor(float V)
{
	const int32 I = static_cast<int32>(V);
	return (V < static_cast<float>(I)) ? I - 1 : I;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// 2D Simplex noise
// ---------------------------------------------------------------------------
float FPMSNoise::Simplex2D(float X, float Y, uint32 Seed)
{
	// Skew factors for 2D simplex
	constexpr float F2 = 0.3660254037844386f;  // (sqrt(3) - 1) / 2
	constexpr float G2 = 0.21132486540518713f; // (3 - sqrt(3)) / 6

	// Skew input space to determine simplex cell
	const float S = (X + Y) * F2;
	const int32 I = FastFloor(X + S);
	const int32 J = FastFloor(Y + S);

	// Unskew back to (x,y) space
	const float T = static_cast<float>(I + J) * G2;
	const float X0 = static_cast<float>(I) - T;
	const float Y0 = static_cast<float>(J) - T;

	// Distances from cell origin
	const float Dx0 = X - X0;
	const float Dy0 = Y - Y0;

	// Determine which simplex we're in
	int32 I1, J1;
	if (Dx0 > Dy0)
	{
		I1 = 1; J1 = 0; // lower triangle
	}
	else
	{
		I1 = 0; J1 = 1; // upper triangle
	}

	// Offsets for middle and last corners
	const float Dx1 = Dx0 - static_cast<float>(I1) + G2;
	const float Dy1 = Dy0 - static_cast<float>(J1) + G2;
	const float Dx2 = Dx0 - 1.0f + 2.0f * G2;
	const float Dy2 = Dy0 - 1.0f + 2.0f * G2;

	// Hashed gradient indices
	const uint32 H0 = HashCoords(I,      J,      Seed);
	const uint32 H1 = HashCoords(I + I1, J + J1, Seed);
	const uint32 H2 = HashCoords(I + 1,  J + 1,  Seed);

	// Contribution from the three corners
	float N = 0.0f;

	float T0 = 0.5f - Dx0 * Dx0 - Dy0 * Dy0;
	if (T0 > 0.0f)
	{
		T0 *= T0;
		N += T0 * T0 * GradDot(H0, Dx0, Dy0);
	}

	float T1 = 0.5f - Dx1 * Dx1 - Dy1 * Dy1;
	if (T1 > 0.0f)
	{
		T1 *= T1;
		N += T1 * T1 * GradDot(H1, Dx1, Dy1);
	}

	float T2 = 0.5f - Dx2 * Dx2 - Dy2 * Dy2;
	if (T2 > 0.0f)
	{
		T2 *= T2;
		N += T2 * T2 * GradDot(H2, Dx2, Dy2);
	}

	// Scale to [-1, 1] range (70 is the standard 2D simplex scaling factor)
	return FMath::Clamp(70.0f * N, -1.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// Fractal (fBm) noise
// ---------------------------------------------------------------------------
float FPMSNoise::FractalNoise2D(float X, float Y, uint32 Seed,
                                  int32 Octaves, float Lacunarity, float Persistence)
{
	float Total = 0.0f;
	float Frequency = 1.0f;
	float Amplitude = 1.0f;
	float MaxAmplitude = 0.0f;

	for (int32 Oct = 0; Oct < Octaves; ++Oct)
	{
		// Per-octave seed variation via simple offset
		const uint32 OctaveSeed = Seed + static_cast<uint32>(Oct) * 0x9E3779B9u;
		Total += Amplitude * Simplex2D(X * Frequency, Y * Frequency, OctaveSeed);
		MaxAmplitude += Amplitude;
		Amplitude *= Persistence;
		Frequency *= Lacunarity;
	}

	// Normalize to [-1, 1]
	if (MaxAmplitude > 0.0f)
	{
		Total /= MaxAmplitude;
	}
	return Total;
}
