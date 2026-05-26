#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "SimulationManagerSubsystem.generated.h"

UCLASS()
class USimulationManagerSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
};
