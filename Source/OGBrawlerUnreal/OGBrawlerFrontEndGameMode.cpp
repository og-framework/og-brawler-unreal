// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#include "OGBrawlerUnreal/OGBrawlerFrontEndGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#include "OGBrawlerUnreal/JoinScreenUImpl.h"
#include "OGBrawlerUnreal/OGBrawlerJoinSessionSubsystem.h"

void AOGBrawlerFrontEndGameMode::InitGame(const FString& mapName, const FString& options, FString& errorMessage)
{
	Super::InitGame(mapName, options, errorMessage);

	const UWorld* world = GetWorld();
	// ⛔G-01  docs/JoinScreen-guards.md
	m_frontEndActive = world != nullptr && frontEndApplies(GetNetMode(), world->IsPlayInPreview());

	if (!m_frontEndActive)
		return;

	DefaultPawnClass      = nullptr;
	PlayerControllerClass = APlayerController::StaticClass();
}

void AOGBrawlerFrontEndGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!m_frontEndActive)
		return;

	UGameInstance* gameInstance = GetGameInstance();
	UOGBrawlerJoinSessionSubsystem* session =
		(gameInstance != nullptr) ? gameInstance->GetSubsystem<UOGBrawlerJoinSessionSubsystem>() : nullptr;
	if (session == nullptr)
	{
		UE_LOG(LogOGJoinScreen, Warning, TEXT("OGJoinScreen: front-end world has no join session"));
		return;
	}

	const bool reachedViaFailureReturn = UGameplayStatics::HasOption(OptionsString, TEXT("closed"))
	                                  || UGameplayStatics::HasOption(OptionsString, TEXT("failed"));

	session->noteFrontEndShown(*GetWorld(), reachedViaFailureReturn);
}
