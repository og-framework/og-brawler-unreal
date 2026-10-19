// SPDX-License-Identifier: MPL-2.0
// docs/UEPresentControls-rationale.md · docs/UEPresentControls-guards.md

#include "UEPresentControls.h"

#include "HAL/IConsoleManager.h"
#include "UERenderTargetTable.h"

namespace
{
	TAutoConsoleVariable<float> CVarRenderInterpolationDelaySteps(
		TEXT("og.Render.InterpolationDelaySteps"),
		2.f,
		TEXT("Jolt configuration only. How many physics steps behind the host time the characters are rendered, ")
		TEXT("interpolated between the two published steps that bracket that time by their step deadlines. 2 matches ")
		TEXT("Chaos's p.AsyncInterpolationMultiplier. Clamped to [0, 2]: the render snapshot channel keeps four steps."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarRenderNewestSnapshot(
		TEXT("og.Render.NewestSnapshot"),
		0,
		TEXT("Jolt configuration only, for comparison. 1 = render the newest published step, with no interpolation and ")
		TEXT("no delay."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionSeconds(
		TEXT("og.Render.ErrorCorrectionSeconds"),
		0.3f,
		TEXT("Jolt configuration only. How long a resimulation's correction of a rendered body is blended out, in seconds, ")
		TEXT("converted to physics steps as Chaos converts p.RenderInterp.ErrorCorrectionDuration (0.3 there and here). ")
		TEXT("0 = no blending: every correction shows as a pop."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionMaxCm(
		TEXT("og.Render.ErrorCorrectionMaxCm"),
		250.f,
		TEXT("Jolt configuration only. A correction of a body by this many centimetres or more is not blended: the body ")
		TEXT("snaps to the corrected pose. The same threshold as Chaos's p.RenderInterp.MaximumErrorCorrectionBeforeSnapping ")
		TEXT("(250 there and here)."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarRenderErrorCorrectionDesyncSeconds(
		TEXT("og.Render.ErrorCorrectionDesyncSeconds"),
		0.6f,
		TEXT("Jolt configuration only. A correction shorter than the distance the body covers in this many seconds at its ")
		TEXT("corrected speed is blended even above og.Render.ErrorCorrectionMaxCm. Chaos's ")
		TEXT("p.RenderInterp.MaximumErrorCorrectionDesyncTimeBeforeSnapping (0.6 there and here). 0 or less = off."),
		ECVF_Default);

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarRenderTraceFrames(
		TEXT("og.Render.TraceFrames"),
		0,
		TEXT("Jolt configuration, non-shipping. 1 = log every frame's rendered capsule position per character, with the ")
		TEXT("render time and the bracketing steps ([SimHost.RenderTrace] on LogOGSimHost, Log)."),
		ECVF_Default);
#endif
}

PresentControls UEPresentControls::fromConsoleVariables(float parkSnap)
{
	PresentControls controls;
	controls.delaySteps        = CVarRenderInterpolationDelaySteps.GetValueOnGameThread();
	controls.newestOnly        = CVarRenderNewestSnapshot.GetValueOnGameThread() != 0;
	controls.correctionSeconds = CVarRenderErrorCorrectionSeconds.GetValueOnGameThread();
	controls.maxCorrection     = UERenderTargetTable::lengthFromCentimetres(CVarRenderErrorCorrectionMaxCm.GetValueOnGameThread());
	controls.desyncSeconds     = CVarRenderErrorCorrectionDesyncSeconds.GetValueOnGameThread();
#if !UE_BUILD_SHIPPING
	controls.traceFrames       = CVarRenderTraceFrames.GetValueOnGameThread() != 0;
#endif
	controls.parkSnap          = parkSnap;
	controls.lengthReportScale = 1.0;
	return controls;
}
