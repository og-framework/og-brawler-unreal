// SPDX-License-Identifier: BUSL-1.1

#include "SimulationManagerSubsystem.h"
#include "OGBrawlerUnreal/OGBrawlerSimulationManager.h"

bool USimulationManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld();
}

void USimulationManagerSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    // The manager's BeginPlay runs synchronously inside SpawnActor (the world has
    // already begun play). It publishes the session relay delay floor through
    // og-sim-net's server (og-input-latency task P5-7); until then a timing relay
    // actor carried it and was spawned here first.
    InWorld.SpawnActor<AOGBrawlerSimulationManager>();
}
