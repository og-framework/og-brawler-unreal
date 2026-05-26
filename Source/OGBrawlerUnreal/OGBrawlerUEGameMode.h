// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OGBrawlerUEGameMode.generated.h"

UCLASS(minimalapi)
class AOGBrawlerUEGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AOGBrawlerUEGameMode();

	virtual void BeginPlay() override;
};



