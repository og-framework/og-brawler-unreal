// SPDX-License-Identifier: BUSL-1.1
// docs/JoinScreen-rationale.md · docs/JoinScreen-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"

#include "OGBrawlerGameViewportClient.generated.h"

class UOGBrawlerJoinSessionSubsystem;

UCLASS()
class UOGBrawlerGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	virtual bool InputKey(const FInputKeyEventArgs& eventArgs) override;

	virtual bool InputChar(FViewport* viewport, int32 controllerId, TCHAR character) override;

private:
	UOGBrawlerJoinSessionSubsystem* joinSessionTakingInput() const;
};
