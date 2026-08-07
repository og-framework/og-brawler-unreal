// SPDX-License-Identifier: MPL-2.0

#include "SimulationTimingRelay.h"
#include "ISimulationTimingRelayListener.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "UEConnectionHandle.h"

namespace
{
// ONE-SHOT diagnostic for a client RTT source that yields no reading.
//
// WHY IT IS LOUD (og-netcode-v2-input-relay T21). Every failure path in the
// read below used to return 0.0 — indistinguishable from a genuine 0 ms ping —
// and the estimator's predOffsetFloorTicks then produced a plausible offset out
// of nothing. The whole failure mode was that it was INVISIBLE, so the fix is
// only half a fix without a line an operator actually notices.
//
// WHY ONE-SHOT. OnRep_Buffer runs at the relay's 100 Hz replication rate; an
// unconditional warning here is a several-thousand-lines-per-minute firehose of
// the kind that produced the 6.4 MB log T17 had to clean up. Game-thread only
// (OnRep), so a plain static needs no synchronisation.
void warnRttSourceUnavailableOnce(const TCHAR* cause)
{
    static bool bWarned = false;
    if (bWarned)
    {
        return;
    }
    bWarned = true;

    UE_LOG(LogOGRtt, Warning,
        TEXT("[RttSample] CLIENT prediction-offset RTT source unavailable (%s). ")
        TEXT("NetworkTimeEstimator will REJECT the sample, so the prediction offset ")
        TEXT("is running on predOffsetFloorTicks alone and is not tracking the network. ")
        TEXT("One-shot warning."),
        cause);
}
}   // namespace

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

    // [T11] Unconditional, exactly like the buffer: the floor is SESSION state
    // and every client needs the same value. No COND_ — this actor is
    // bAlwaysRelevant and owned by nobody, so an owner-scoped condition would
    // pass for no connection at all.
    DOREPLIFETIME(ASimulationTimingRelay, m_relayDelayFloorTicks);
}

void ASimulationTimingRelay::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        return;     // the composition root writes the floor; nothing to receive
    }

    // CLIENT, belt-and-braces (mirrors ASimulationConnectionRelay::BeginPlay).
    // The initial bunch's RepNotifies run before PostNetInit/BeginPlay, so a floor
    // can already have landed by now. If a listener bound in that window, hand it
    // the latched value; if none has, the manager's pull-at-bind does it.
    replayLatchedRelayDelayFloor();
}

void ASimulationTimingRelay::setRelayDelayFloorTicks(uint8 floorTicks)
{
    if (!HasAuthority())
    {
        return;
    }

    m_relayDelayFloorTicks = floorTicks;

    // The floor changes a player's felt input lag on both ends the moment it
    // lands, so do not wait for this actor's ordinary update slot. (At session
    // start this is redundant with the initial bunch; it is the dynamic-floor
    // path of §11 Q6 that needs it.)
    ForceNetUpdate();
}

void ASimulationTimingRelay::OnRep_RelayDelayFloor()
{
    // [A3a] LATCH FIRST, unconditionally. This property dirties only on change,
    // so this OnRep is the only notification the value will ever produce; if no
    // listener is bound yet it must survive here until one pulls it.
    m_relayDelayFloorOnRepObserved = true;

    if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_Standalone)
        return;

    ISimulationTimingRelayListener* manager =
        ISimulationTimingRelayListener::instanceFor(/*isAuthority=*/false);
    if (manager == nullptr)
        return;     // latched above — the manager replays it at listener bind

    manager->onRelayDelayFloorReceived(m_relayDelayFloorTicks);
}

void ASimulationTimingRelay::replayLatchedRelayDelayFloor() const
{
    if (!m_relayDelayFloorOnRepObserved)
        return;     // nothing has landed; there is no value to replay

    ISimulationTimingRelayListener* manager =
        ISimulationTimingRelayListener::instanceFor(/*isAuthority=*/false);
    if (manager == nullptr)
        return;

    manager->onRelayDelayFloorReplayed(m_relayDelayFloorTicks);
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

    // -----------------------------------------------------------------------
    // THE CLIENT-SIDE RTT READ THAT FEEDS THE PREDICTION OFFSET.
    // (og-netcode-v2-input-relay T21.)
    //
    // STILL EPingType::RoundTrip — DELIBERATELY, and this is a REVERSAL of what
    // T21 was dispatched to do. The task was written to switch this read to
    // EPingType::RoundTripExclFrame on the premise that the frame-time-EXCLUDED
    // ping type was "plumbed and simply unused". Reading the engine refuted the
    // premise; see readRoundTripMs in UEConnectionHandle.h for the full
    // derivation from UE 5.6.1 source, and the T21 impl notes for the decision.
    // In one line: UE feeds the two ping types SWAPPED relative to their names
    // (NetConnection.cpp — RoundTrip receives the subtracted value, and
    // RoundTripExclFrame receives the raw inclusive RTT), and because this
    // project leaves net.PingExcludeFrameTime unset the subtraction is a no-op,
    // so today both types carry NUMERICALLY IDENTICAL samples. Switching would
    // therefore have bought no frame-time exclusion at all — only an
    // unevidenced PlayerStateAvg -> MovingAverage smoothing change, landing
    // immediately before a measurement campaign.
    //
    // WHAT T21 DID CHANGE IS BELOW: the failure paths no longer return 0.0.
    //
    // 0.0 WAS A LIE. A missing world / net driver / server connection /
    // FNetPing is "no reading", but 0.0 says "a perfect 0 ms ping" — and the
    // estimator has no way to tell those apart. Worse, GetPingValues itself
    // returns Current = -1.0 when the ping type is disabled or its accumulator
    // is empty, and that -1.0 was passed through unguarded to be latched as the
    // first EMA sample. Both roads ended at predOffsetFloorTicks silently
    // standing in for a real measurement. Every path now yields the SAME
    // negative sentinel the engine uses, which NetworkTimeEstimator::updateRTT
    // rejects (and counts) rather than latches.
    //
    // THE SAMPLE IS STILL PASSED DOWN, NOT DROPPED HERE. onTimingInfoReceived
    // also carries the authority tick, which is valid regardless of the ping
    // source and must keep flowing; the rejection therefore belongs at the
    // estimator, not at this call site.
    // -----------------------------------------------------------------------
    const double roundTripTime = [this]() -> double {
        UWorld* world = GetWorld();
        if (world == nullptr)
        {
            warnRttSourceUnavailableOnce(TEXT("no UWorld"));
            return kNoRttReadingSeconds;
        }
        UNetDriver* netDriver = world->GetNetDriver();
        if (netDriver == nullptr)
        {
            warnRttSourceUnavailableOnce(TEXT("no UNetDriver"));
            return kNoRttReadingSeconds;
        }
        UNetConnection* serverConnection = netDriver->ServerConnection;
        if (serverConnection == nullptr)
        {
            warnRttSourceUnavailableOnce(TEXT("no ServerConnection"));
            return kNoRttReadingSeconds;
        }
        UE::Net::FNetPing* netPing = serverConnection->GetNetPing();
        if (netPing == nullptr)
        {
            // net.NetPingEnabled is off for this driver — a CONFIG failure, not
            // a warm-up state. It never resolves on its own.
            warnRttSourceUnavailableOnce(
                TEXT("no FNetPing on the server connection - check net.NetPingEnabled"));
            return kNoRttReadingSeconds;
        }
        if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
        {
            // The type is not in net.NetPingTypes at all. Also permanent, and
            // distinguished from "warming up" because the remedies differ.
            warnRttSourceUnavailableOnce(
                TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes"));
            return kNoRttReadingSeconds;
        }
        const UE::Net::FPingValues PingVals = netPing->GetPingValues(EPingType::RoundTrip);
        if (PingVals.Current < 0.0)
        {
            // Enabled but no samples yet. Expected briefly at session start;
            // the warning is one-shot so a normal warm-up costs one line.
            warnRttSourceUnavailableOnce(
                TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)"));
            return kNoRttReadingSeconds;
        }
        return PingVals.Current;
    }();

    const SimulationTimeStep remoteAuthorityStep = ServerTickClock::readFromSyncedBuffer(m_buffer, 0);
    manager->onTimingInfoReceived(remoteAuthorityStep.getTick(), roundTripTime);
}
