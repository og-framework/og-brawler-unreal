// SPDX-License-Identifier: MPL-2.0
// docs/UEPresentControls-rationale.md · docs/UEPresentControls-guards.md

#pragma once

#include "OGSimulationUnreal.h"

#include "OGSimulation/TickDriverPresenter.h"

class OGSIMULATIONUNREAL_API UEPresentControls
{
public:
	UEPresentControls() = delete;

	static PresentControls fromConsoleVariables(float parkSnap);
};
