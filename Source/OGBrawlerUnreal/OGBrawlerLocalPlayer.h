// SPDX-License-Identifier: BUSL-1.1
// docs/OGBuildIdentity-rationale.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/LocalPlayer.h"
#include "OGBrawlerLocalPlayer.generated.h"

UCLASS()
class UOGBrawlerLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	virtual FString GetGameLoginOptions() const override;
};
