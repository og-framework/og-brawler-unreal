// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerPlayerController.h"
#include "SimulationManagerUImpl.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "OGBrawlerUECharacter.h"
#include "OGBrawlerPlayerCameraManager.h"
#include "SharedIsometricCameraActor.h"

AOGBrawlerPlayerController::AOGBrawlerPlayerController()
{
    // Layer 1 — disable engine's "auto-fall-back to suggested pawn as view
    // target" path. APlayerController::AutoManageActiveCameraTarget gates on
    // this flag; false makes it a no-op, which kills Clobber #1 identified in
    // impl_notes_phase-d_task-9.md (SetPawn / APawn::PossessedBy invoking
    // AutoManage → PCM->SetViewTarget(pawn, blend=0)). Our refreshViewTarget
    // covers the work AutoManage would have done.
    bAutoManageActiveCameraTarget = false;

    // Layer 2 enabler — install our PCM subclass so refreshViewTarget can
    // drive its setSuppressPawnFallback() chokepoint to drop Clobber #2
    // (AcknowledgePossession's PCM->SetViewTarget(P, blend=0)).
    PlayerCameraManagerClass = AOGBrawlerPlayerCameraManager::StaticClass();
}

void AOGBrawlerPlayerController::JoinLocalPlayer()
{
    UWorld* world = GetWorld();
    if (world == nullptr) return;

    UGameplayStatics::CreatePlayer(world, /*ControllerId=*/-1, /*bSpawnPlayerController=*/true);

    // Belt-and-suspenders splitscreen disable: re-flush at join time in case
    // any per-LP-add path reactivated splitscreen state.
    if (UGameViewportClient* viewport = world->GetGameViewport())
    {
        viewport->SetForceDisableSplitscreen(true);
        viewport->UpdateActiveSplitscreenType();
    }

    // Fan out refreshViewTarget to all local PCs on this client so each PC
    // re-evaluates its policy now that Num() has changed.
    if (UGameInstance* gi = GetGameInstance())
    {
        for (ULocalPlayer* lp : gi->GetLocalPlayers())
        {
            if (lp == nullptr) continue;
            if (AOGBrawlerPlayerController* siblingPc = Cast<AOGBrawlerPlayerController>(lp->GetPlayerController(world)))
            {
                siblingPc->refreshViewTarget();
            }
        }
    }
}

void AOGBrawlerPlayerController::LeaveLocalPlayer()
{
    UWorld* world = GetWorld();
    if (world == nullptr) return;

    ULocalPlayer* lp = GetLocalPlayer();
    if (lp == nullptr) return;

    // Don't allow LP0 (the primary) to remove itself — UE's primary LP is
    // load-bearing for the viewport. Bail if this exec was invoked on the primary.
    if (UGameplayStatics::GetPlayerControllerID(this) == 0)
    {
        UE_LOG(LogOG, Warning, TEXT("LeaveLocalPlayer: refusing to remove primary LP"));
        return;
    }

    UGameplayStatics::RemovePlayer(this, /*bDestroyPawn=*/true);

    // After the leaving LP's pawn is destroyed, fan out refreshViewTarget to
    // remaining local PCs so each one re-evaluates policy with the new Num().
    if (UGameInstance* gi = GetGameInstance())
    {
        for (ULocalPlayer* remainingLp : gi->GetLocalPlayers())
        {
            if (remainingLp == nullptr) continue;
            if (AOGBrawlerPlayerController* siblingPc = Cast<AOGBrawlerPlayerController>(remainingLp->GetPlayerController(world)))
            {
                siblingPc->refreshViewTarget();
            }
        }
    }
}

void AOGBrawlerPlayerController::BeginPlayingState()
{
    Super::BeginPlayingState();
    refreshViewTarget();
    fanOutRefreshToSiblings();
}

void AOGBrawlerPlayerController::OnPossess(APawn* pawn)
{
    Super::OnPossess(pawn);
    refreshViewTarget();
    fanOutRefreshToSiblings();
}

void AOGBrawlerPlayerController::OnUnPossess()
{
    Super::OnUnPossess();
    refreshViewTarget();
    fanOutRefreshToSiblings();
}

void AOGBrawlerPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    refreshViewTarget();
    fanOutRefreshToSiblings();
}

void AOGBrawlerPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Hard-key bindings for the local player join/leave so we don't depend on
    // the in-game console (which is unavailable in some standalone build
    // configurations).
    if (InputComponent)
    {
        InputComponent->BindKey(EKeys::Tab,    IE_Pressed, this, &AOGBrawlerPlayerController::JoinLocalPlayer);
        InputComponent->BindKey(EKeys::Insert, IE_Pressed, this, &AOGBrawlerPlayerController::LeaveLocalPlayer);
    }
}

void AOGBrawlerPlayerController::fanOutRefreshToSiblings()
{
    UWorld* world = GetWorld();
    if (world == nullptr) return;
    UGameInstance* gi = GetGameInstance();
    if (gi == nullptr) return;

    for (ULocalPlayer* lp : gi->GetLocalPlayers())
    {
        if (lp == nullptr) continue;
        AOGBrawlerPlayerController* siblingPc = Cast<AOGBrawlerPlayerController>(lp->GetPlayerController(world));
        if (siblingPc != nullptr && siblingPc != this)
        {
            siblingPc->refreshViewTarget();
        }
    }
}

AActor* AOGBrawlerPlayerController::computeDesiredViewTarget()
{
    if (!IsLocalController()) return nullptr;
    UWorld* world = GetWorld();
    if (world == nullptr) return nullptr;

    TArray<AOGBrawlerUECharacter*> localBrawlers;
    for (TActorIterator<AOGBrawlerUECharacter> it(world); it; ++it)
    {
        if (it->IsLocallyControlled())
            localBrawlers.Add(*it);
    }

    if (localBrawlers.Num() == 0)
        return nullptr;
    if (localBrawlers.Num() == 1)
        return localBrawlers[0];

    ASharedIsometricCameraActor* cam = findOrSpawnSharedCamera();
    if (cam != nullptr)
        cam->setOwningWorld(world);
    return cam;
}

void AOGBrawlerPlayerController::refreshViewTarget()
{
    AActor* desired = computeDesiredViewTarget();
    if (desired == nullptr)
        return; // No local brawler yet — lifecycle hook will refire.

    // Layer 2 control: sync the PCM's pawn-fallback filter with the policy.
    // - desired == GetPawn() (solo mode): suppress=false. Engine fallback paths
    //   align with our policy; let them through unmolested.
    // - desired != GetPawn() (iso-cam mode): suppress=true. Engine attempts to
    //   reassert VT=pawn at blend=0 (AcknowledgePossession's pattern) are
    //   dropped at the PCM API boundary.
    if (auto* pcm = Cast<AOGBrawlerPlayerCameraManager>(PlayerCameraManager))
    {
        pcm->setSuppressPawnFallback(desired != GetPawn());
    }

    SetViewTargetWithBlend(desired, m_viewTargetBlendSeconds);
}

ASharedIsometricCameraActor* AOGBrawlerPlayerController::findOrSpawnSharedCamera()
{
    if (m_sharedCamera.IsValid())
        return m_sharedCamera.Get();

    UWorld* world = GetWorld();

    for (TActorIterator<ASharedIsometricCameraActor> it(world); it; ++it)
    {
        m_sharedCamera = *it;
        return m_sharedCamera.Get();
    }

    m_sharedCamera = world->SpawnActor<ASharedIsometricCameraActor>();
    return m_sharedCamera.Get();
}
