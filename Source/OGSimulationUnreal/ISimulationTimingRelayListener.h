// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include <cstdint>

// Interface implemented by the simulation manager so that SimulationTimingRelay
// can call back into it without a compile-time dependency on the brawler-specific
// concrete class.
//
// [T11] The relay now carries a SECOND, independent tenant — the session relay
// delay floor — and it gets TWO entry points for the same load-bearing reason
// ISimulationConnectionRelayListener's tier does:
//   * onRelayDelayFloorReceived is a genuine floor CHANGE (an OnRep). The client
//     has been predicting against the previous effective delay, so an INCREASE
//     has to be paid for with a prediction stall.
//   * onRelayDelayFloorReplayed is the LATCH REPLAY of a value that landed before
//     this listener bound. The manager pulls it inside its own BeginPlay, in the
//     same breath as publishing the pre-arrival baseline and before the first
//     prediction tick — so no tick has been predicted at the old delay and there
//     is nothing to give back. It must NOT stall.
// Note the asymmetry with the tier's pair: the floor's transition arm carries no
// "old" value, because the quantity that must be paid for is the change in the
// EFFECTIVE delay (floor vs. tier, whichever dominates), which only the manager's
// shared recompute can know. The relay does not compute it.
class OGSIMULATIONUNREAL_API ISimulationTimingRelayListener
{
public:
    virtual ~ISimulationTimingRelayListener() = default;

    virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) = 0;

    // A floor CHANGE arrived on the session channel. Re-derives the effective
    // input delay and pays for an increase with a prediction stall.
    virtual void onRelayDelayFloorReceived(uint8_t floorTicks) = 0;

    // Replay of a latched floor that arrived before this listener existed. Applies
    // + republishes but does NOT stall — see the note above.
    virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) = 0;

    // Static per-world-role registry — mirrors ASimulationManagerUImpl::s_instances.
    // Slot 0 = authority, slot 1 = non-authority (client).
    static ISimulationTimingRelayListener* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    static void registerInstance(bool isAuthority, ISimulationTimingRelayListener* instance)
    {
        s_instances[isAuthority ? 0 : 1] = instance;
    }

    static void unregisterInstance(bool isAuthority)
    {
        s_instances[isAuthority ? 0 : 1] = nullptr;
    }

private:
    static ISimulationTimingRelayListener* s_instances[2];
};
