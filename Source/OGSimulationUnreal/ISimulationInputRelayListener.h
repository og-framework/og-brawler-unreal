// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal.h"

class ASimulationInputRelay;

// ---------------------------------------------------------------------------
// ISimulationInputRelayListener — the game-side boundary that lets a replicated
// ASimulationInputRelay find the object that wants its ring.
// (og-netcode-v2-input-relay / T39, Stage 1 of input-first replication;
//  design_task38_input_first_replication.md §5.1.)
//
// WHY THIS EXISTS AT ALL. The relay ring used to be a UPROPERTY on
// `USimmableUpdateComponent`, so its OnRep already stood on the object that
// consumes it. T39 moves the property onto its own Iris root object so the ring
// can be scheduled, prioritised and skipped independently of the (much larger)
// correction state — and that host lives in OGSimulationUnreal, which MUST NOT
// depend on OGBrawlerUnreal. The host therefore cannot name the character type
// or the component type, and needs a boundary to reach them through. This is
// the same shape, and the same registry idiom, as
// ISimulationConnectionRelayListener / ISimulationTimingRelayListener.
//
// ONE METHOD, AND IT IS "I EXIST", NOT "HERE IS A VALUE" — which is the
// deliberate difference from both sibling interfaces. Those two carry a wire
// TRANSITION (a tier, a clock buffer) because their payloads are notify-only
// scalars that must be latched or they are lost. The relay ring is a PERSISTENT
// property: re-reading it always yields the full current state, so the property
// is its own latch (see the DO-NOT-COPY-THE-LATCH-PATTERN note at
// SimulationNetSync::registerPredictionOwner). All this boundary has to do is
// get the two objects introduced; once linked, the host pushes ring arrivals
// straight into the callback the game side bound, with no further indirection
// and nothing per-arrival passing through the listener.
//
// THE LISTENER IS THE MANAGER, and it holds no map: it resolves
// host -> GetOwner() -> character -> component in three lines, because
// AActor::Owner replicates and the host is spawned with SetOwner(character) on
// the authority. A registry keyed by character would be a second structure to
// keep in step with spawn/destroy for no gain.
//
// CLIENT SLOT ONLY, IN PRACTICE. Both slots are registered so the lifetime rules
// match the siblings', but only the non-authority slot is ever consulted: the
// server WRITES the ring (relayRemoteInput) and links the host at spawn time, so
// it never needs to be told the host exists.
// ---------------------------------------------------------------------------
class OGSIMULATIONUNREAL_API ISimulationInputRelayListener
{
public:
    virtual ~ISimulationInputRelayListener() = default;

    // A relay host has become resolvable — it has replicated in, or its Owner
    // pointer has just landed. The implementation is expected to resolve the
    // host's owner to the object that consumes relayed input and link the two.
    //
    // IDEMPOTENT BY CONTRACT. This fires from BeginPlay, from OnRep_Owner, and
    // again from any ring OnRep that arrives while still unlinked — three
    // independent paths on purpose, because none of them alone covers every
    // arrival order. The implementation must tolerate being called when the link
    // already exists, and must tolerate an owner that does not resolve yet.
    virtual void onInputRelayHostReady(ASimulationInputRelay& host) = 0;

    // Static per-world-role registry — mirrors ISimulationConnectionRelayListener's
    // exactly. Slot 0 = authority, slot 1 = non-authority (client).
    static ISimulationInputRelayListener* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    static void registerInstance(bool isAuthority, ISimulationInputRelayListener* instance)
    {
        s_instances[isAuthority ? 0 : 1] = instance;
    }

    static void unregisterInstance(bool isAuthority)
    {
        s_instances[isAuthority ? 0 : 1] = nullptr;
    }

private:
    static ISimulationInputRelayListener* s_instances[2];
};
