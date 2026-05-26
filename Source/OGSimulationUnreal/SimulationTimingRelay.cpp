// SPDX-License-Identifier: MPL-2.0

#include "SimulationTimingRelay.h"
#include "ISimulationTimingRelayListener.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"

ASimulationTimingRelay::ASimulationTimingRelay()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    SetNetUpdateFrequency(100.0f);
    SetMinNetUpdateFrequency(100.0f);
}

void ASimulationTimingRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASimulationTimingRelay, m_buffer);
}

void ASimulationTimingRelay::OnRep_Buffer()
{
    if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_Standalone)
        return;

    // OnRep_Buffer only runs on non-authority (client) replicated actors —
    // the early-out above excludes server and standalone — so look up the
    // non-authority manager slot directly.
    ISimulationTimingRelayListener* manager = ISimulationTimingRelayListener::instanceFor(/*isAuthority=*/false);
    if (manager == nullptr)
        return;

    const double roundTripTime = [this]() {
        UWorld* world = GetWorld();
        if (world == nullptr) return 0.0;
        UNetDriver* netDriver = world->GetNetDriver();
        if (netDriver == nullptr) return 0.0;
        UNetConnection* serverConnection = netDriver->ServerConnection;
        if (serverConnection == nullptr) return 0.0;
        UE::Net::FNetPing* netPing = serverConnection->GetNetPing();
        if (netPing == nullptr) return 0.0;
        const UE::Net::FPingValues PingVals = netPing->GetPingValues(EPingType::RoundTrip);
        return PingVals.Current;
    }();

    const SimulationTimeStep remoteAuthorityStep = ServerTickClock::readFromSyncedBuffer(m_buffer, 0);
    manager->onTimingInfoReceived(remoteAuthorityStep.getTick(), roundTripTime);
}
