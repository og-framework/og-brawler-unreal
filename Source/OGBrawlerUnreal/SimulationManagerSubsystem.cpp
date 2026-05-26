// SPDX-License-Identifier: BUSL-1.1

#include "SimulationManagerSubsystem.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"

bool USimulationManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld();
}

void USimulationManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    InWorld.SpawnActor<ASimulationManagerUImpl>();

    if (InWorld.GetNetMode() != NM_Client)
        InWorld.SpawnActor<ASimulationTimingRelay>();
}
