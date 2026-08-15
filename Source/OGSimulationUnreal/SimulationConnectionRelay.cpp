// SPDX-License-Identifier: MPL-2.0

#include "SimulationConnectionRelay.h"
#include "ISimulationConnectionRelayListener.h"

#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"

DEFINE_LOG_CATEGORY(LogOGConnRelay);

namespace
{
    // How often the server re-checks that this relay's wire is still alive. The
    // relay carries a value that changes at most once per tier dwell period, so
    // there is nothing to gain from a tighter cadence — this exists only so a
    // relay whose client vanished (logout, timeout, hard kill) cannot outlive it.
    constexpr float kConnectionReapPeriodSeconds = 1.0f;
}

ASimulationConnectionRelay::ASimulationConnectionRelay()
{
    bReplicates = true;

    // NOT bAlwaysRelevant (the timing relay's setting) — this is the whole point
    // of the actor. Owner-only relevancy is also what makes couch co-op correct
    // for free: the engine builds a connection's viewer set as root + all child
    // connections, so an actor owned by the ROOT PlayerController is relevant to
    // that connection and its bunches reach the one machine both siblings live on.
    bOnlyRelevantToOwner = true;

    // Tier changes are dwell-gated and rare; each write calls ForceNetUpdate() so
    // it goes out on the next replication pass regardless of this rate.
    SetNetUpdateFrequency(10.0f);
    SetMinNetUpdateFrequency(2.0f);
}

void ASimulationConnectionRelay::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASimulationConnectionRelay, m_connectionTier);
}

void ASimulationConnectionRelay::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        // Owner liveness reap — see the LIFECYCLE note in the header. First fire is
        // one full period out, which is also why findOrSpawnForConnection can bind
        // m_rootConnection through a deferred spawn without racing this.
        GetWorldTimerManager().SetTimer(
            m_reapTimerHandle, this, &ASimulationConnectionRelay::reapIfConnectionGone,
            kConnectionReapPeriodSeconds, /*inbLoop=*/true);
        return;
    }

    // CLIENT. The initial bunch's RepNotifies run before PostNetInit/BeginPlay, so
    // a tier can already have landed by now. If a listener bound in that window,
    // hand it the latched value; if none has, the manager's pull-at-bind does it.
    replayLatchedTier();
}

void ASimulationConnectionRelay::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* world = GetWorld())
    {
        world->GetTimerManager().ClearTimer(m_reapTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

ASimulationConnectionRelay* ASimulationConnectionRelay::findOrSpawnForConnection(
    UWorld* world, UNetConnection* rootConnection)
{
    if (world == nullptr || rootConnection == nullptr)
    {
        return nullptr;
    }

    // Linear scan rather than a cached map, deliberately: the lookup runs only on
    // an actual tier CHANGE for one wire (the core dedups everything else), which
    // the tier table's dwell gate bounds to roughly one per second per connection
    // in the worst case. A cache keyed on a connection pointer, by contrast, would
    // have to be invalidated on every disconnect/travel to avoid misrouting a
    // publish to a dead wire's relay — cost we would be paying for nothing.
    for (TActorIterator<ASimulationConnectionRelay> it(world); it; ++it)
    {
        ASimulationConnectionRelay* relay = *it;
        if (IsValid(relay) && relay->m_rootConnection.Get() == rootConnection)
        {
            return relay;
        }
    }

    // Owner = the ROOT connection's PlayerController. That is what
    // bOnlyRelevantToOwner resolves against, and the root PC is a viewer of its
    // own connection, so the actor replicates over that wire exactly once no
    // matter how many split-screen siblings share it.
    AActor* ownerActor = rootConnection->PlayerController != nullptr
        ? static_cast<AActor*>(rootConnection->PlayerController)
        : ToRawPtr(rootConnection->OwningActor);

    if (ownerActor == nullptr)
    {
        // No PC on the wire yet. The core's per-owner dedup has ALREADY recorded
        // this tier as published, so it will not retry — log loudly rather than
        // dropping silently. Reaching this needs an input RPC to arrive before the
        // connection has a PlayerController, which the join sequence does not do.
        UE_LOG(LogOGConnRelay, Warning,
            TEXT("[ConnectionTier] cannot spawn connection relay: root connection has no owning actor"));
        return nullptr;
    }

    // Deferred so m_rootConnection is bound BEFORE BeginPlay arms the reap timer.
    ASimulationConnectionRelay* relay = world->SpawnActorDeferred<ASimulationConnectionRelay>(
        ASimulationConnectionRelay::StaticClass(), FTransform::Identity, ownerActor);
    if (relay == nullptr)
    {
        UE_LOG(LogOGConnRelay, Warning, TEXT("[ConnectionTier] connection relay spawn failed"));
        return nullptr;
    }

    relay->m_rootConnection = rootConnection;
    relay->FinishSpawning(FTransform::Identity);

    UE_LOG(LogOGConnRelay, Log,
        TEXT("[ConnectionTier] spawned connection relay for wire %s (owner %s)"),
        *rootConnection->GetName(), *ownerActor->GetName());

    return relay;
}

ASimulationConnectionRelay* ASimulationConnectionRelay::findLocalClientRelay(UWorld* world)
{
    if (world == nullptr)
    {
        return nullptr;
    }

    // An authority world holds one relay per REMOTE wire, so "the local one" has
    // no meaning there; asking for it is a wiring bug, not a degraded state.
    checkf(world->GetNetMode() == NM_Client,
        TEXT("findLocalClientRelay is client-only (netmode=%d)"), (int)world->GetNetMode());

    for (TActorIterator<ASimulationConnectionRelay> it(world); it; ++it)
    {
        if (IsValid(*it))
        {
            return *it;
        }
    }

    return nullptr;
}

void ASimulationConnectionRelay::setConnectionTier(uint8 tier)
{
    if (!HasAuthority())
    {
        return;
    }

    m_connectionTier = tier;

    // Tier transitions are rare and latency-sensitive on both ends (the client
    // rolls back prediction on receipt), so do not wait for this actor's ordinary
    // update slot.
    ForceNetUpdate();
}

void ASimulationConnectionRelay::OnRep_ConnectionTier(uint8 OldTier)
{
    // [A3a] LATCH FIRST, unconditionally. The property dirties only on change, so
    // this OnRep is the only notification this value will ever produce; if no
    // listener is bound yet the value must survive here until one pulls it.
    m_connectionTierOnRepObserved = true;

    UE_LOG(LogOGConnRelay, Log,
        TEXT("[ConnectionTier] client received tier %u (was %u)"),
        (unsigned int)m_connectionTier, (unsigned int)OldTier);

    // OnRep only runs on non-authority replicated actors; the guard mirrors
    // ASimulationTimingRelay::OnRep_Buffer so the non-authority manager slot can
    // be looked up directly.
    if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_Standalone)
    {
        return;
    }

    ISimulationConnectionRelayListener* manager =
        ISimulationConnectionRelayListener::instanceFor(/*isAuthority=*/false);
    if (manager == nullptr)
    {
        return;     // latched above — the manager replays it at listener bind
    }

    manager->onConnectionTierReceived(OldTier, m_connectionTier);
}

void ASimulationConnectionRelay::replayLatchedTier() const
{
    if (!m_connectionTierOnRepObserved)
    {
        return;     // nothing has landed; there is no value to replay
    }

    ISimulationConnectionRelayListener* manager =
        ISimulationConnectionRelayListener::instanceFor(/*isAuthority=*/false);
    if (manager == nullptr)
    {
        return;
    }

    UE_LOG(LogOGConnRelay, Log,
        TEXT("[ConnectionTier] replaying latched tier %u to a late-bound listener"),
        (unsigned int)m_connectionTier);

    manager->onConnectionTierReplayed(m_connectionTier);
}

void ASimulationConnectionRelay::reapIfConnectionGone()
{
    // Destroying the owning PlayerController does NOT destroy the actors it owns —
    // the engine only clears their Owner pointer — and an owner-less
    // bOnlyRelevantToOwner actor is relevant to nobody, i.e. an invisible leak.
    // Both halves are checked because either can go first: the connection object
    // is GC'd on cleanup (weak pointer nulls), and the PC is destroyed on logout.
    const UNetConnection* connection = m_rootConnection.Get();
    const bool connectionGone =
        (connection == nullptr) ||
        (connection->GetConnectionState() != USOCK_Open && connection->GetConnectionState() != USOCK_Pending);
    const bool ownerGone = !IsValid(GetOwner());

    if (!connectionGone && !ownerGone)
    {
        return;
    }

    UE_LOG(LogOGConnRelay, Log,
        TEXT("[ConnectionTier] reaping connection relay (connectionGone=%d ownerGone=%d)"),
        connectionGone ? 1 : 0, ownerGone ? 1 : 0);

    Destroy();
}
