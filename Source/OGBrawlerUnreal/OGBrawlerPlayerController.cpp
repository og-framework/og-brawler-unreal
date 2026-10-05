// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerPlayerController.h"
#include "SimulationManagerUImpl.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LocalPlayer.h"
#include "Engine/ChildConnection.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "OGBrawlerUECharacter.h"
#include "OGBrawlerPlayerCameraManager.h"
#include "SharedIsometricCameraActor.h"
#include "JoinScreenUImpl.h"
#include "OGBrawlerJoinSessionSubsystem.h"

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

    UGameInstance* const gameInstance        = GetGameInstance();
    const int32          localPlayersBefore  = (gameInstance != nullptr) ? gameInstance->GetNumLocalPlayers() : 0;

    UGameplayStatics::CreatePlayer(world, /*ControllerId=*/-1, /*bSpawnPlayerController=*/true);

    // The engine allows at most MaxSplitscreenPlayers local players per client (4 by
    // default) and refuses the next one client-side, before anything reaches the server.
    // Tell the player instead of failing silently [og-brawler-uploadtosteam task 13].
    // ⚠ The local-player COUNT decides, not CreatePlayer's return value: on a network
    // client it returns null even on success, because the new player's controller only
    // arrives later from the server (measured). Rationale:
    // Source/OGBrawlerUnreal/docs/JoinScreen-rationale.md section 14.
    UGameViewportClient* const viewportClient = world->GetGameViewport();
    if (viewportClient != nullptr && gameInstance != nullptr
        && gameInstance->GetNumLocalPlayers() == localPlayersBefore
        && localPlayersBefore >= viewportClient->MaxSplitscreenPlayers)
    {
        if (UOGBrawlerJoinSessionSubsystem* session = gameInstance->GetSubsystem<UOGBrawlerJoinSessionSubsystem>())
            session->noteLocalPlayerLimitReached(viewportClient->MaxSplitscreenPlayers);
    }

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

    UGameInstance* gameInstance = GetGameInstance();
    if (gameInstance == nullptr || GetLocalPlayer() == nullptr) return;

    // The leave key (End; Insert before og-attackstatetransition-cleanup task 12, then BackSpace
    // until task 13) removes the LAST local player (the highest-numbered), whichever local PC
    // received it, and never LP0 -- UE's primary LP is load-bearing for the viewport (user ruling
    // 2026-09-29, og-brawler-uploadtosteam task 13). The keyboard leave key always arrives on LP0,
    // which owns the keyboard, and only there: one press, one removal.
    // ⛔ ONLY THE LAST ONE. Client and server pair a split player's controller with its
    //   local player by ARRAY INDEX (NetPlayerIndex = index in UNetConnection::Children
    //   on both sides, and in the client's local-player list), so removing a middle one
    //   would mis-pair the next joiner and the engine closes the whole connection
    //   (BadChildConnectionIndex). Rationale: Source/OGBrawlerUnreal/docs/JoinScreen-rationale.md
    //   section 14.
    const TArray<ULocalPlayer*>& localPlayers = gameInstance->GetLocalPlayers();
    if (localPlayers.Num() <= 1)
    {
        UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: leave local player: no other local player to remove; local player 0 stays"));
        return;
    }

    ULocalPlayer* const leavingPlayer = localPlayers.Last();
    AOGBrawlerPlayerController* const leaving =
        (leavingPlayer != nullptr) ? Cast<AOGBrawlerPlayerController>(leavingPlayer->GetPlayerController(world)) : nullptr;
    if (leaving == nullptr)
    {
        UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: leave local player: the last local player has no controller yet; nothing removed"));
        return;
    }

    UE_LOG(LogOGJoinScreen, Log, TEXT("OGJoinScreen: leave local player: removing local player %d of %d"),
        localPlayers.Num() - 1, localPlayers.Num());

    // UGameplayStatics::RemovePlayer alone never tells a server: on a network client it
    // only drops the local player, and the server keeps the character. So the leaving
    // player's own controller asks the server to remove its split connection first.
    if (leaving->GetLocalRole() != ROLE_Authority)
        leaving->ServerLeaveLocalPlayer();

    UGameplayStatics::RemovePlayer(leaving, /*bDestroyPawn=*/true);

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

void AOGBrawlerPlayerController::ServerLeaveLocalPlayer_Implementation()
{
    // Server side of LeaveLocalPlayer. Only a split player (a child connection) can
    // leave this way -- the primary leaves by disconnecting -- and only the LAST child
    // of its connection, for the index pairing explained in LeaveLocalPlayer.
    // Removal is what the engine's own disconnect does for each child
    // (UChildConnection::CleanUp -> OnNetCleanup -> Destroy -> GameMode Logout), minus
    // the rest of the connection. Rationale: Source/OGBrawlerUnreal/docs/JoinScreen-rationale.md
    // section 14.
    UChildConnection* const child  = Cast<UChildConnection>(NetConnection);
    UNetConnection* const   parent = (child != nullptr) ? child->Parent.Get() : nullptr;
    if (parent == nullptr || parent->Children.Num() == 0 || parent->Children.Last() != child)
    {
        UE_LOG(LogOGJoinScreen, Warning,
            TEXT("OGJoinScreen: refused a local-player leave from %s: not the last split player of its connection"),
            *GetName());
        return;
    }

    parent->Children.Remove(child);
    child->CleanUp();
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
    // The keys are built from brawlerJoinScreen::kLocalCoopKeyNames -- the same
    // constant the join screen's local co-op hint is written from -- so the hint
    // and the binding cannot disagree; a name that is not an engine key fails a
    // checkf at the first game start (og-brawler BrawlerJoinScreen-guards.md G-01,
    // Source/OGBrawlerUnreal/docs/JoinScreen-rationale.md section 7).
    if (InputComponent)
    {
        InputComponent->BindKey(joinScreenUImpl::localCoopAddPlayerKey(),    IE_Pressed, this, &AOGBrawlerPlayerController::JoinLocalPlayer);
        InputComponent->BindKey(joinScreenUImpl::localCoopRemovePlayerKey(), IE_Pressed, this, &AOGBrawlerPlayerController::LeaveLocalPlayer);
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

    // Iso -> solo: re-seed the solo camera at the iso angle (user decision, og-attackstatetransition-cleanup
    // task 9). Only on the transition itself: the shared camera is the current view target and no blend
    // to the pawn is already pending, so a refresh of an already-solo view keeps the player's angle.
    // Rationale: Source/OGBrawlerUnreal/docs/OGBrawlerUECharacter-rationale.md section 14.
    if (desired == GetPawn()
        && Cast<ASharedIsometricCameraActor>(GetViewTarget()) != nullptr
        && (PlayerCameraManager == nullptr || PlayerCameraManager->PendingViewTarget.Target != desired))
    {
        if (AOGBrawlerUECharacter* brawler = Cast<AOGBrawlerUECharacter>(desired))
            brawler->seedCameraBoomAtIsoRotation();
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
