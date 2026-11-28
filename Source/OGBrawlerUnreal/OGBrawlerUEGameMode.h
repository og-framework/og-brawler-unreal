// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SessionLogUImpl.h"
#include "OGBrawlerUEGameMode.generated.h"

UCLASS(minimalapi)
class AOGBrawlerUEGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOGBrawlerUEGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

	virtual void BeginPlay() override;

	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	// The server's join/leave log for the host launcher [og-brawler-uploadtosteam task 13].
	// Lines and rules: Source/OGBrawlerUnreal/docs/SessionLog-guards.md and -rationale.md.
	virtual void PostLogin(APlayerController* newPlayer) override;

	virtual void Logout(AController* exiting) override;

	virtual AActor* ChoosePlayerStart_Implementation(AController* player) override;

private:
	sessionLogUImpl::SessionLog m_sessionLog;
};



