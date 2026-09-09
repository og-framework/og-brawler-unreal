// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUEGameMode.h"
#include "OGBrawlerUECharacter.h"
#include "OGBrawlerUEHUD.h"
#include "OGBrawlerPlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"


AOGBrawlerUEGameMode::AOGBrawlerUEGameMode()
{
	// ⭐ [movement-sim task 19] THE C++ CLASS DIRECTLY — checked, and unchanged by the migration.
	// `DefaultPawnClass` is a `TSubclassOf<APawn>`, so rebasing `AOGBrawlerUECharacter` from the
	// engine's walking-pawn base onto `APawn` needs nothing here. ⚠ AND THIS IS WHAT THE GAME
	// SPAWNS: no Blueprint sits between this line and the pawn, so no binary asset participates
	// in the class hierarchy that task 19 changed.
	DefaultPawnClass = AOGBrawlerUECharacter::StaticClass();
	PlayerControllerClass = AOGBrawlerPlayerController::StaticClass();
	// ⛔ AGameModeBase LEAVES THIS UNSET, so without this line the engine's no-op AHUD
	//   is in play and nothing screen-space can be drawn at all.
	HUDClass = AOGBrawlerUEHUD::StaticClass();
}

void AOGBrawlerUEGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (UGameViewportClient* viewport = GetWorld()->GetGameViewport())
    {
        viewport->SetForceDisableSplitscreen(true);
        viewport->UpdateActiveSplitscreenType();
    }

    // Get all PlayerStarts in the level
    TArray<AActor*> PlayerStarts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), PlayerStarts);

    // Ensure there are at least two PlayerStarts
    //if (PlayerStarts.Num() >= 1)
    if (false)
    {
        // Spawn the first player at the first PlayerStart
        //APawn* Player1 = GetWorld()->SpawnActor<APawn>(DefaultPawnClass, PlayerStarts.Last()->GetActorLocation(), PlayerStarts.Last()->GetActorRotation());
        APlayerController* Player1Controller = UGameplayStatics::CreatePlayer(GetWorld(), -1, true);
        //Player1Controller->Possess(Player1);

        // Spawn the second player at the second PlayerStart
        //GetWorld()->SpawnActor<APawn>(DefaultPawnClass, PlayerStarts[1]->GetActorLocation(), PlayerStarts[0]->GetActorRotation());
    }
}