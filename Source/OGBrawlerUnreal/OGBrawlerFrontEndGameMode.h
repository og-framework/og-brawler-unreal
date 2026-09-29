// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#pragma once

#include "CoreMinimal.h"

#include "OGBrawlerUnreal/OGBrawlerUEGameMode.h"

#include "OGBrawlerFrontEndGameMode.generated.h"

UCLASS()
class AOGBrawlerFrontEndGameMode : public AOGBrawlerUEGameMode
{
	GENERATED_BODY()

public:
	static constexpr bool frontEndApplies(ENetMode netMode, bool playInPreview)
	{
		return netMode == NM_Standalone && !playInPreview;
	}

	virtual void InitGame(const FString& mapName, const FString& options, FString& errorMessage) override;

	virtual void BeginPlay() override;

	bool isFrontEndActive() const { return m_frontEndActive; }

private:
	bool m_frontEndActive = false;
};

static_assert(AOGBrawlerFrontEndGameMode::frontEndApplies(NM_Standalone, false)
                  && !AOGBrawlerFrontEndGameMode::frontEndApplies(NM_Standalone, true)
                  && !AOGBrawlerFrontEndGameMode::frontEndApplies(NM_DedicatedServer, false)
                  && !AOGBrawlerFrontEndGameMode::frontEndApplies(NM_ListenServer, false)
                  && !AOGBrawlerFrontEndGameMode::frontEndApplies(NM_Client, false),
              "Was the T10 spike's Q1/Q8 ruling: the join screen shows only in a standalone world "
              "that was not launched by the editor (-PIEVIACONSOLE). Every other world keeps "
              "the gameplay GameMode's behaviour unchanged. See docs/JoinScreen-rationale.md section 2.");
