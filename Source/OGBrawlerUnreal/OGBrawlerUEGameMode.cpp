// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUEGameMode.h"
#include "OGBrawlerUECharacter.h"
#include "OGBrawlerUEHUD.h"
#include "OGBrawlerPlayerController.h"
#include "OGBuildIdentityUImpl.h"
#include "OGBrawlerUnreal/BrawlerPlayerStartUImpl.h"
#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGBrawler/SessionConstants.h"
#include "OGSimulation/BodySlotOccupancy.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"

static_assert(og::brawler::session::hardPlayerCeiling <= static_cast<int>(kMaxSimulatableSlots),
    "hardPlayerCeiling exceeds kMaxSimulatableSlots: an admitted character would find no physics slot (an OG_CHECK). "
    "Was the og-input-latency P7 design's prose 'the hard ceiling 8 (slots and spawn points)'.");
static_assert(og::brawler::session::hardPlayerCeiling <= static_cast<int>(brawlerRingout::kMaxSpawnPoints),
    "hardPlayerCeiling exceeds brawlerRingout::kMaxSpawnPoints: an admitted character would get no spawn point. "
    "Was the og-input-latency P7 design's prose 'the hard ceiling 8 (slots and spawn points)'.");
static_assert(og::brawler::session::maxLocalPlayersPerClient <= og::brawler::session::hardPlayerCeiling,
    "maxLocalPlayersPerClient exceeds hardPlayerCeiling: one connection could ask for more players than the server admits. "
    "Was unstated: og-input-latency P7-5's task text.");
static_assert(og::brawler::session::maxLocalPlayersPerClient >= 1 && og::brawler::session::maxLocalPlayersPerClient <= 255,
    "maxLocalPlayersPerClient must count the connection's own player and fit AGameSession::MaxSplitscreensPerConnection (uint8). "
    "Was a silent truncation in InitGame's static_cast<uint8>.");


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

void AOGBrawlerUEGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    checkf(GameSession != nullptr, TEXT("AGameModeBase::InitGame spawned no GameSession, so the admission caps cannot be set."));
    GameSession->MaxPlayers = og::brawler::session::hardPlayerCeiling;
    GameSession->MaxSplitscreensPerConnection = static_cast<uint8>(og::brawler::session::maxLocalPlayersPerClient);

    if (GetNetMode() != NM_Standalone)
    {
        UE_LOG(LogGameSession, Warning, TEXT("[Admission] maxPlayers=%d maxLocalPerConnection=%d (SessionConstants)"),
            GameSession->MaxPlayers, static_cast<int32>(GameSession->MaxSplitscreensPerConnection));
    }
}

void AOGBrawlerUEGameMode::BeginPlay()
{
    Super::BeginPlay();

    // The world already listens here (UEngine::LoadMap calls Listen before BeginPlay).
    m_sessionLog.noteListening(*this);

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

void AOGBrawlerUEGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
    FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (ErrorMessage.IsEmpty())
        buildIdentityUImpl::refuseMismatchedBackendLogin(Options, ErrorMessage);
}

void AOGBrawlerUEGameMode::PostLogin(APlayerController* newPlayer)
{
    Super::PostLogin(newPlayer);

    if (newPlayer != nullptr)
        m_sessionLog.noteJoined(*this, *newPlayer);
}

void AOGBrawlerUEGameMode::Logout(AController* exiting)
{
    m_sessionLog.noteLeft(*this, exiting);

    Super::Logout(exiting);
}

AActor* AOGBrawlerUEGameMode::ChoosePlayerStart_Implementation(AController* player)
{
    if (AActor* start = brawlerPlayerStartUImpl::choosePlayerStart(*this, player))
        return start;
    return Super::ChoosePlayerStart_Implementation(player);
}
