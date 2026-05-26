#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"
#include "PMSSlopeGenerator.h"

// ---------------------------------------------------------------------------
// FPMSCanyonDef — defines a canyon carved into the slope
// ---------------------------------------------------------------------------
struct FPMSCanyonDef
{
	float StartDistance    = 0.0f;   // start along descent axis
	float EndDistance      = 0.0f;   // end along descent axis
	float LateralOffset   = 0.0f;   // offset from corridor center
	float Width           = 20.0f;  // canyon width at top
	float Depth           = 30.0f;  // canyon depth below surrounding terrain
	float EdgeSmoothness  = 2.0f;   // 1.0 = V-shape, 3.0+ = U-shape
	float TransitionDist  = 20.0f;  // fade-in/fade-out distance at start/end
};

// ---------------------------------------------------------------------------
// FPMSCliffLayerDef — additional cliff height on one side of the corridor
// ---------------------------------------------------------------------------
enum class EPMSCliffSide : uint8
{
	Left,
	Right
};

struct FPMSCliffLayerDef
{
	float StartDistance      = 0.0f;
	float EndDistance        = 0.0f;
	EPMSCliffSide Side      = EPMSCliffSide::Left;
	float AdditionalHeight  = 20.0f;  // extra height above profile wall
	float Roughness         = 1.0f;   // noise amplitude on cliff face
	float TransitionDist    = 20.0f;  // fade-in/fade-out distance
};

// ---------------------------------------------------------------------------
// FPMSFeatureCarver — applies canyon and cliff features to a heightfield
// ---------------------------------------------------------------------------
struct FPMSFeatureCarver
{
	/**
	 * Carve a canyon channel into the heightfield.
	 * Subtracts depth using a power-curve falloff from the canyon center.
	 */
	static void CarveCanyon(FPMSHeightfield& Heightfield,
	                         const FPMSCanyonDef& Canyon,
	                         uint32 Seed);

	/**
	 * Layer additional cliff height onto one side of the corridor.
	 * Adds height above existing values with optional roughness noise.
	 */
	static void LayerCliff(FPMSHeightfield& Heightfield,
	                        const FPMSCliffLayerDef& Cliff,
	                        uint32 Seed);
};
