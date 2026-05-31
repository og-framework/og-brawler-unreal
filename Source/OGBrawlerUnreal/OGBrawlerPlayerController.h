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
    AOGBrawlerPlayerController() = default;

    UFUNCTION(Exec)
    void JoinLocalPlayer();

    UFUNCTION(Exec)
    void LeaveLocalPlayer();

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
