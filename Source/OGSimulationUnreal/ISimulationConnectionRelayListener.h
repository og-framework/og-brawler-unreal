// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"
#include <cstdint>

// ---------------------------------------------------------------------------
// ISimulationConnectionRelayListener — the client-side receive boundary for the
// PER-CONNECTION channel carried by ASimulationConnectionRelay.
// (og-netcode-v2-input-relay / T10; RelayDelaySpectrumDesign.md §12.)
//
// Sibling of ISimulationTimingRelayListener, and deliberately a SEPARATE
// interface rather than two more methods on it: the timing relay is one shared
// always-relevant actor carrying SESSION-scoped state to EVERYONE, while the
// connection relay is one actor per wire carrying WIRE-scoped state to that
// wire's owner only. Same registry idiom, different channel — keeping the
// interfaces apart keeps that scope/audience split legible at every call site.
//
// The relay actor calls this so it never needs a compile-time dependency on the
// brawler-specific concrete manager (OGSimulationUnreal must not depend on
// OGBrawlerUnreal).
//
// TWO ENTRY POINTS, NOT ONE — this is load-bearing, not ceremony:
//   * onConnectionTierReceived is a wire TRANSITION (a relay OnRep). It carries
//     the pre-OnRep value so the listener can turn (old -> new) into a
//     prediction stall.
//   * onConnectionTierReplayed is the LATCH REPLAY of a value that landed
//     before this listener bound. It is the FIRST tier the listener ever sees,
//     so there are no ticks predicted against a previous tier's delay to give
//     back — a tier-to-tier delta would be the wrong quantity to stall by.
//     It therefore carries no "old" value and must NOT stall.
// This mirrors exactly the two call sites the retired component channel had
// (OnRep_ConnectionTier vs. the replay in tryInitializeWithManager).
// ---------------------------------------------------------------------------
class OGSIMULATIONUNREAL_API ISimulationConnectionRelayListener
{
public:
    virtual ~ISimulationConnectionRelayListener() = default;

    // A tier transition observed on the wire. `oldTier` is the value the
    // property held before the OnRep that produced this call.
    virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier) = 0;

    // Replay of a latched tier that arrived before this listener existed. No
    // transition semantics — see the header note above.
    virtual void onConnectionTierReplayed(uint8_t tier) = 0;

    // Static per-world-role registry — mirrors ISimulationTimingRelayListener's.
    // Slot 0 = authority, slot 1 = non-authority (client). Only the client slot
    // is ever consulted by the relay (a server writes the property, it never
    // receives an OnRep for it), but both are registered so the lifetime rules
    // stay identical to the timing listener's.
    static ISimulationConnectionRelayListener* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    static void registerInstance(bool isAuthority, ISimulationConnectionRelayListener* instance)
    {
        s_instances[isAuthority ? 0 : 1] = instance;
    }

    static void unregisterInstance(bool isAuthority)
    {
        s_instances[isAuthority ? 0 : 1] = nullptr;
    }

private:
    static ISimulationConnectionRelayListener* s_instances[2];
};
