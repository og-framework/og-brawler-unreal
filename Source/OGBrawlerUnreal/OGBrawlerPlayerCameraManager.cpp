// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerPlayerCameraManager.h"
#include "SimulationManagerUImpl.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformStackWalk.h"

namespace
{
	// `og.pcm.debug 1` enables a Warning-level log on every SetViewTarget call,
	// including a C++ stack trace. Default 0 — the diagnostic path is one int
	// compare per call.
	static int32 g_pcmDebug = 0;
	static FAutoConsoleVariableRef CVarPcmDebug(
		TEXT("og.pcm.debug"),
		g_pcmDebug,
		TEXT("PCM debug: 0=off, 1=log every SetViewTarget call with C++ stack trace."));
}

void AOGBrawlerPlayerCameraManager::SetViewTarget(AActor* NewTarget, FViewTargetTransitionParams TransitionParams)
{
	// Pawn-fallback filter — load-bearing for the iso-cam policy.
	// Drops engine SetViewTarget(currentPawn, blend=0) calls while the owning
	// PC's policy diverges from VT=GetPawn(). Those calls would otherwise hit
	// the same-target branch in Super::SetViewTarget and silently clear our
	// PendingViewTarget. See impl_notes_phase-d_task-9.md / task-10.md.
	if (bSuppressPawnFallback && TransitionParams.BlendTime == 0.f && PCOwner != nullptr)
	{
		const APawn* ownerPawn = PCOwner->GetPawn();
		if (NewTarget != nullptr && NewTarget == ownerPawn)
		{
			if (g_pcmDebug != 0)
			{
				UE_LOG(LogOG, Warning,
					TEXT("[PCMDebug] %s::SetViewTarget(new=%s, blend=0) DROPPED by pawn-fallback filter | curVT=%s curPVT=%s"),
					*GetName(),
					*NewTarget->GetName(),
					ViewTarget.Target ? *ViewTarget.Target->GetName() : TEXT("null"),
					PendingViewTarget.Target ? *PendingViewTarget.Target->GetName() : TEXT("null"));
			}
			return;
		}
	}

	if (g_pcmDebug != 0)
	{
		const SIZE_T StackTraceSize = 8192;
		ANSICHAR StackTrace[StackTraceSize] = { 0 };
		FPlatformStackWalk::StackWalkAndDump(StackTrace, StackTraceSize, /*IgnoreCount=*/2);

		UE_LOG(LogOG, Warning,
			TEXT("[PCMDebug] %s::SetViewTarget(new=%s, blend=%.3f) | curVT=%s curPVT=%s curBlendLeft=%.3f\nStack:\n%s"),
			*GetName(),
			NewTarget ? *NewTarget->GetName() : TEXT("null"),
			TransitionParams.BlendTime,
			ViewTarget.Target ? *ViewTarget.Target->GetName() : TEXT("null"),
			PendingViewTarget.Target ? *PendingViewTarget.Target->GetName() : TEXT("null"),
			BlendTimeToGo,
			ANSI_TO_TCHAR(StackTrace));
	}

	Super::SetViewTarget(NewTarget, TransitionParams);
}
