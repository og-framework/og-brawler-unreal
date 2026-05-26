// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include <cstdint>

// Interface implemented by the simulation manager so that SimulationTimingRelay
// can call back into it without a compile-time dependency on the brawler-specific
// concrete class.
class OGSIMULATIONUNREAL_API ISimulationTimingRelayListener
{
public:
    virtual ~ISimulationTimingRelayListener() = default;

    virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) = 0;

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
