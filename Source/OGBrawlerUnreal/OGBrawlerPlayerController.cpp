// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerPlayerController.h"
#include "SimulationManagerUImpl.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "OGBrawlerUECharacter.h"
#include "SharedIsometricCameraActor.h"

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

    // Fan out refreshViewTarget to all local PCs on this client — LP0's PC
    // does not natively fire OnPossess when LP1 joins, so it needs an external
    // trigger to re-evaluate its view-target policy.
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
    // remaining local PCs so LP0 switches back to per-character cam when
    // Num() drops to 1.
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

void AOGBrawlerPlayerController::refreshViewTarget()
{
    if (!IsLocalController()) return;

    UWorld* world = GetWorld();
    if (world == nullptr) return;

    TArray<AOGBrawlerUECharacter*> localBrawlers;
    for (TActorIterator<AOGBrawlerUECharacter> it(world); it; ++it)
    {
        if (it->IsLocallyControlled())
            localBrawlers.Add(*it);
    }

    if (localBrawlers.Num() == 0)
    {
        // No local brawler yet — OnPossess / OnRep_Pawn will refire this. Never SetViewTarget(nullptr).
        return;
    }

    if (localBrawlers.Num() == 1)
    {
        SetViewTargetWithBlend(localBrawlers[0], m_viewTargetBlendSeconds);
        return;
    }

    ASharedIsometricCameraActor* cam = findOrSpawnSharedCamera();
    cam->setOwningWorld(world);
    SetViewTargetWithBlend(cam, m_viewTargetBlendSeconds);
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
