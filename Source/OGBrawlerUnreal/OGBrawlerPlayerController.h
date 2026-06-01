// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "OGBrawlerPlayerController.generated.h"

class ASharedIsometricCameraActor;

UCLASS()
class AOGBrawlerPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    AOGBrawlerPlayerController();

    UFUNCTION(Exec)
    void JoinLocalPlayer();

    UFUNCTION(Exec)
    void LeaveLocalPlayer();

    // Computes the view target this PC SHOULD have right now based on locally-
    // controlled brawler count. Pure — no side effects. Returns nullptr when no
    // local brawler exists yet (caller should wait for possession to wire up).
    AActor* computeDesiredViewTarget();

    // Applies the view-target policy. Resolves the desired target, syncs the
    // AOGBrawlerPlayerCameraManager's pawn-fallback filter, then calls
    // SetViewTargetWithBlend.
    //
    // Two-layer policy enforcement against engine clobbers (full diagnosis in
    // impl_notes_phase-d_task-9.md and task-10.md):
    //   Layer 1 — bAutoManageActiveCameraTarget=false (PC ctor) makes the
    //             SetPawn / APawn::PossessedBy → AutoManageActiveCameraTarget
    //             path a no-op.
    //   Layer 2 — AOGBrawlerPlayerCameraManager::setSuppressPawnFallback drops
    //             AcknowledgePossession's direct PCM->SetViewTarget(P) call
    //             (and any other future call matching the same pattern).
    //             Toggled here based on whether desired != GetPawn().
    void refreshViewTarget();

protected:
    virtual void BeginPlayingState() override;
    virtual void OnPossess(APawn* pawn) override;
    virtual void OnUnPossess() override;
    virtual void OnRep_Pawn() override;
    virtual void SetupInputComponent() override;

private:
    ASharedIsometricCameraActor* findOrSpawnSharedCamera();

    // Fans out refreshViewTarget() to every sibling local PC. Called on possession
    // changes so that when a late-spawned brawler (mid-game CreatePlayer) finally
    // gets possessed, all already-existing local PCs re-evaluate their view target.
    // Skips self — the caller refreshes itself first.
    void fanOutRefreshToSiblings();

    UPROPERTY(EditAnywhere, Category=Camera) float m_viewTargetBlendSeconds = 0.25f;

    TWeakObjectPtr<ASharedIsometricCameraActor> m_sharedCamera;
};
