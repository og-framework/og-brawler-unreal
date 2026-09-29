// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "InputCoreTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include <string>

#include "OGBrawler/BrawlerJoinScreen.h"

#include "OGBrawlerJoinSessionSubsystem.generated.h"

class UNetDriver;
class UWorld;

UCLASS()
class UOGBrawlerJoinSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UOGBrawlerJoinSessionSubsystem* forShownFrontEnd(const UWorld* world);

	virtual bool ShouldCreateSubsystem(UObject* outer) const override;

	virtual void Initialize(FSubsystemCollectionBase& collection) override;

	virtual void Deinitialize() override;

	const brawlerJoinScreen::JoinScreenModel& model() const { return m_model; }

	void noteFrontEndShown(UWorld& world, bool reachedViaFailureReturn);

	bool handleCharacter(TCHAR character);

	bool handleKey(const FKey& key, bool controlDown, UWorld& world);

	void noteLocalPlayerLimitReached(int32 maxLocalPlayers);

	TOptional<FString> localPlayerLimitNotice() const;

private:
	void join(UWorld& world);

	bool cancelJoin(UWorld& world);

	void travelTo(UWorld& world, const std::string& address);

	bool ownsWorld(const UWorld* world) const;

	void handleNetworkFailure(UWorld* world, UNetDriver* netDriver, ENetworkFailure::Type failure,
	                          const FString& errorText);

	void handleTravelFailure(UWorld* world, ETravelFailure::Type failure, const FString& errorText);

	void noteFailure(brawlerJoinScreen::JoinFailureReason reason, const TCHAR* source, const FString& errorText);

	void handlePostLoadMap(UWorld* world);

	brawlerJoinScreen::JoinScreenModel m_model;

	bool m_autoJoinPending = false;

	double m_localPlayerLimitReachedAt = 0.0;

	int32 m_localPlayerLimit = 0;

	FDelegateHandle m_networkFailureHandle;
	FDelegateHandle m_travelFailureHandle;
	FDelegateHandle m_postLoadMapHandle;
};
