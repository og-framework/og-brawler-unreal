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
    // ORDER IS LOAD-BEARING since T11 (og-netcode-v2-input-relay): the timing
    // relay is spawned FIRST so that the manager's BeginPlay — which runs
    // synchronously inside SpawnActor, the world having already begun play — can
    // find it and publish the session relay delay floor onto it. The relay does
    // not touch the manager at spawn time (its client-side OnRep path does, and
    // that cannot run on an authority world), so nothing depends on the previous
    // order; the manager's own findTimingRelay() call sites were all post-BeginPlay
    // before this change.
    if (InWorld.GetNetMode() != NM_Client)
        InWorld.SpawnActor<ASimulationTimingRelay>();

    InWorld.SpawnActor<ASimulationManagerUImpl>();
}
