// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "OGBrawlerPlayerCameraManager.generated.h"

// Project PlayerCameraManager. Two responsibilities:
//
// 1. Pawn-fallback filter (load-bearing for iso-cam policy).
//    While bSuppressPawnFallback is true, drop any incoming SetViewTarget call
//    matching `SetViewTarget(PCOwner->GetPawn(), blend=0)` — the signature used
//    by engine pawn-fallback paths (AcknowledgePossession's direct
//    PCM->SetViewTarget(P), and AutoManageActiveCameraTarget's call site if it
//    were ever re-enabled). Those calls would otherwise silently cancel our
//    PendingViewTarget via APlayerCameraManager::SetViewTarget's same-target
//    branch — see impl_notes_phase-d_task-9.md for the full diagnosis.
//    AOGBrawlerPlayerController toggles the flag inside refreshViewTarget so
//    it tracks the current policy: true while iso cam is desired (multi-LP),
//    false while solo brawler is desired (Num==1).
//
// 2. Diagnostic SetViewTarget logging. When `og.pcm.debug 1` is set, every
//    SetViewTarget call (including dropped pawn-fallback ones) is logged at
//    Warning level with NewTarget name, blend time, current VT/PVT/blendLeft,
//    and a C++ stack trace. Zero cost when the CVar is 0.
UCLASS()
class AOGBrawlerPlayerCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()
public:
	virtual void SetViewTarget(AActor* NewTarget, FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams()) override;

	// Set true while the owning PC's policy diverges from `VT = GetPawn()`.
	// While true, engine SetViewTarget(currentPawn, blend=0) calls are dropped
	// to prevent silent PendingViewTarget cancellation.
	void setSuppressPawnFallback(bool bSuppress) { bSuppressPawnFallback = bSuppress; }

private:
	bool bSuppressPawnFallback = false;
};
