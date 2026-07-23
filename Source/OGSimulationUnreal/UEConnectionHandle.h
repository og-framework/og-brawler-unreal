// SPDX-License-Identifier: MPL-2.0

#pragma once

#include <cstddef>
#include <functional>

#include "CoreMinimal.h"
#include "Engine/ChildConnection.h"
#include "Engine/NetConnection.h"
#include "GameFramework/Actor.h"
#include "Runtime/Engine/Public/Net/NetPing.h"

// ---------------------------------------------------------------------------
// FUEConnectionHandle — the UE engine adapter's `NetConfig::Address` type.
// (Stage 3 / D3.2; proposal_ogbrawler_netcode.md §2.1, §7.1, §8.2.)
//
// The engine-agnostic per-connection structures in the sim core
// (ConnectionTierTable, ServerInputDelayQueue, and — Stage 4 —
// ServerSubstitutionTracker) are templated on an opaque `Address` handle so the
// core never names a UE type. This header supplies the UE binding of that
// handle: a small regular, hashable value type wrapping a weak reference to the
// ROOT UNetConnection.
//
// WHY ITS OWN HEADER (rather than a sibling declaration in SimulationTimingRelay.h):
// the templates that consume `Address` are verified by Catch2 tests that do NOT
// link OGSimulationUnreal — they instantiate the same templates over
// FStandaloneTestHandle instead. Isolating the UE handle in a dedicated header
// keeps that coupling narrow and makes the engine-free path obvious.
//
// WHY THE *ROOT* CONNECTION: under split-screen / multi-ULocalPlayer clients
// (proposal §8.2 "Path A"), each ULocalPlayer gets its own APlayerController
// backed by a UChildConnection of the one real client connection. Tier, input
// delay, and substitution state are properties of the WIRE, not of a viewport,
// so all siblings must resolve to a single table entry. GetRootNetConnection
// walks UChildConnection::Parent to guarantee that.
//
// SENTINEL / "NO WIRE" STATE: a default-constructed handle, and equally the
// handle produced from an actor with no net connection (standalone play, or a
// locally-controlled pawn on a listen-server host), holds a null weak pointer.
// This is a legitimate value, not an error — it is the concept's required
// default-initializable sentinel, and it compares equal to every other
// no-wire handle so local play collapses to exactly one table entry.
// ---------------------------------------------------------------------------

// Walks from `actor` to the root UNetConnection driving it, resolving any
// UChildConnection to its parent. Returns nullptr when the actor has no wire
// identity (standalone, local PIE host, or a null actor) — see the sentinel
// note above. The loop is bounded by the connection chain, which UE builds at
// most one level deep; the `parent == nullptr` guard additionally makes a
// malformed chain terminate rather than spin.
inline UNetConnection* GetRootNetConnection(AActor* actor)
{
    if (actor == nullptr)
    {
        return nullptr;
    }

    UNetConnection* conn = actor->GetNetConnection();
    while (conn != nullptr)
    {
        UChildConnection* childConn = conn->GetUChildConnection();
        if (childConn == nullptr)
        {
            break; // already the root
        }

        UNetConnection* parent = childConn->GetParentConnection();
        if (parent == nullptr)
        {
            break; // malformed chain — treat this link as the root
        }

        conn = parent;
    }

    return conn;
}

// ---------------------------------------------------------------------------
// GetPlayerSlotForActor — which LOCAL PLAYER on the wire owns `actor`.
// (T17; proposal_ogbrawler_netcode.md §8.2 "player_slot".)
//
// Returns 0 for the primary player (the parent UNetConnection's), and 1..N for
// each additional local player on that same machine. Returns 0 for an actor with
// no wire identity, matching GetRootNetConnection's sentinel — a standalone or
// listen-server-local pawn is the only player on its (absent) wire.
//
// DERIVATION: `UNetConnection::GetConnectionHandle().GetChildConnectionId()`.
// This is the ENGINE'S OWN child-connection identity, and it already has exactly
// the numbering this function needs: FConnectionHandle documents "a
// ChildConnectionId of zero indicates it's the parent connection itself", parent
// connections are constructed as `FConnectionHandle(AllocateConnectionId())`
// (child id 0), and UChildConnection::AssignConnectionHandle assigns children the
// lowest unused id starting at 1 (0 is reserved as InvalidChildId).
//
// WHY NOT `Parent->Children.IndexOfByKey(child) + 1`, the obvious candidate:
// Epic explicitly warns against it in AssignConnectionHandle's own comment —
// "Don't assume the children remain on a particular index or that the array
// isn't ever shrinking." The child id is assigned ONCE at InitChildConnection and
// never reassigned, so it is stable for the connection's lifetime and therefore
// for the lifetime of any character on it; an array index is not contractually
// either. The id is also what the engine itself round-trips over the wire for
// child connections, so binding to it keeps this notion aligned with the engine's
// rather than parallel to it.
//
// STABILITY CONTRACT (why this matters here): the input delay queue keys parked
// input on (root connection, slot). If a slot changed under a live character its
// parked input would be orphaned in the old bucket and silently dropped. The
// assign-once child id is what makes that impossible.
inline uint8 GetPlayerSlotForActor(AActor* actor)
{
    if (actor == nullptr)
    {
        return 0;
    }

    const UNetConnection* conn = actor->GetNetConnection();
    if (conn == nullptr)
    {
        return 0;       // no wire identity — sole player, slot 0
    }

    // NOTE: deliberately the actor's OWN connection, NOT the root. The root walk
    // is what erases the distinction between local players; this function exists
    // to recover it, so it must read the un-walked connection.
    const uint32 childId = conn->GetConnectionHandle().GetChildConnectionId();

    // Saturate rather than wrap. The caller range-checks against the uint8
    // substitution-mask bound (ConnectionSlotKey::kMaxPlayerSlot) and takes a
    // documented fallback; a truncating cast could alias a large id onto a valid
    // slot and reintroduce exactly the conflation T17 removes.
    return static_cast<uint8>(childId > 255u ? 255u : childId);
}

// ---------------------------------------------------------------------------
// readRoundTripMs — a connection's RAW round-trip RTT in MILLISECONDS.
// (T21; the RTT-source engine primitive of the transport adapter.)
//
// Returns -1.0 when there is no reading yet (null connection, or no FNetPing on
// it). That sentinel is passed straight into
// ServerReceptionCoordinator::noteRttSample, which skips the sample and returns
// tier -1 so the adapter publishes nothing — matching the pre-T21 behaviour.
//
// SINGLE-SMOOTHING (Option A, load-bearing): this returns the engine's RAW
// RoundTrip value with NO NetworkTimeEstimator EMA on top; the tier table's own
// rttSmoothingAlpha EMA is the only smoothing in this path. Double-smoothing
// would reintroduce exactly the client/server asymmetry Option A removes.
//
// Uses EPingType::RoundTrip (not RoundTripExclFrame) to match the source the
// client already reads in SimulationTimingRelay.cpp. FPingValues are in SECONDS
// (NetPing.h); the tier table and TimeConfig::rttTierBoundariesMs are in
// MILLISECONDS, hence the *1000.
//
// ENGINE PRIMITIVE: a second engine (Godot) must supply its own RTT source here.
inline double readRoundTripMs(UNetConnection* conn)
{
    if (conn == nullptr)
    {
        return -1.0;
    }

    UE::Net::FNetPing* netPing = conn->GetNetPing();
    if (netPing == nullptr)
    {
        return -1.0;
    }

    const UE::Net::FPingValues pingVals = netPing->GetPingValues(EPingType::RoundTrip);
    return pingVals.Current * 1000.0;
}

struct FUEConnectionHandle
{
    // Weak by design: the tier / delay-queue tables outlive individual
    // connections, and a handle must stay hashable and comparable AFTER the
    // UNetConnection dies so the owning table can still find and reap its entry.
    TWeakObjectPtr<UNetConnection> m_conn;

    FUEConnectionHandle() = default;

    explicit FUEConnectionHandle(UNetConnection* conn)
        : m_conn(conn)
    {
    }

    // Named ctor: resolve an actor to its root connection and wrap it.
    // Composes with GetRootNetConnection, so a split-screen sibling actor and
    // its parent-connection peer produce EQUAL handles.
    static FUEConnectionHandle FromRootOf(AActor* actor)
    {
        return FUEConnectionHandle(GetRootNetConnection(actor));
    }

    // "Is this connection still on the wire?" — drives reaping of dropped
    // connections. A sentinel (no-wire) handle reports false.
    bool isAlive() const
    {
        return m_conn.IsValid();
    }

    // Hidden friend: gives std::equality_comparable (and, with the implicit
    // C++20 rewrite, operator!=) without polluting the enclosing scope.
    friend bool operator==(const FUEConnectionHandle& lhs, const FUEConnectionHandle& rhs)
    {
        return lhs.m_conn == rhs.m_conn;
    }
};

namespace std
{
    template <>
    struct hash<FUEConnectionHandle>
    {
        std::size_t operator()(const FUEConnectionHandle& handle) const noexcept
        {
            // UE's GetTypeHash for a weak pointer hashes the object's remote id
            // (index + serial number), NOT the raw address. That identity stays
            // stable after the pointee dies, which is exactly what a table
            // keyed on possibly-dead connections needs: a handle keeps hashing
            // to its original bucket right up until it is reaped.
            return static_cast<std::size_t>(GetTypeHash(handle.m_conn));
        }
    };
}
