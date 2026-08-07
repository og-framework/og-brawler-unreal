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
// LogOGRtt — the RTT-source diagnostic channel, shared by the two read sites
// (readRoundTripMs below, feeding the TIER; ASimulationTimingRelay::OnRep_Buffer,
// feeding the client prediction OFFSET). Declared here because this header is
// where the RTT engine primitive lives, and because a category owned by
// OGSimulationUnreal keeps the log off the OGBrawlerUnreal LogOG* family — this
// module must not depend on the game module.
//
// Default verbosity Log, so the one-shot Warnings below are visible with NO
// DefaultEngine.ini entry. That is deliberate: the failure these report is
// exactly the kind that must not need a config opt-in to be seen.
// ---------------------------------------------------------------------------
OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGRtt, Log, All);

// The "no reading" sentinel, in each site's own unit. NEGATIVE by design and
// shared by both RTT read sites, because both consumers already reject negatives:
// ServerReceptionCoordinator::noteRttSample skips `rttMs < 0.0` (publishing no
// tier), and NetworkTimeEstimator::updateRTT rejects and counts it (latching
// nothing). Zero must never be used — it is indistinguishable from a genuine
// 0 ms LAN ping, which is what made the pre-T21 silent-zero defect invisible.
inline constexpr double kNoRttReadingMs      = -1.0;
inline constexpr double kNoRttReadingSeconds = -1.0;

// ---------------------------------------------------------------------------
// readRoundTripMs — a connection's RAW round-trip RTT in MILLISECONDS.
// (arch-latency T21; the RTT-source engine primitive of the transport adapter.)
//
// Returns kNoRttReadingMs when there is no reading — null connection, no
// FNetPing, the ping type not enabled, or its accumulator still empty. That
// sentinel is passed straight into ServerReceptionCoordinator::noteRttSample,
// which skips the sample so the adapter publishes no tier.
//
// SINGLE-SMOOTHING (Option A, load-bearing — UNCHANGED by the T21 edit): this
// returns the engine's RAW RoundTrip value with NO NetworkTimeEstimator EMA on
// top; the tier table's own rttSmoothingAlpha EMA is the only smoothing in this
// path. Double-smoothing would reintroduce exactly the client/server asymmetry
// Option A removes. The T21 change added only a validity gate in front of the
// value; nothing was inserted between the read and the return.
//
// FPingValues are in SECONDS (NetPing.h); the tier table and
// TimeConfig::rttTierBoundariesMs are in MILLISECONDS, hence the *1000.
//
// ===========================================================================
// READ THIS BEFORE "FIXING" THE PING TYPE. The enum names are INVERTED
// relative to what the engine feeds them. Verified against UE 5.6.1 source
// (unmodified in this tree) by og-netcode-v2-input-relay T21, whose entire
// stated purpose was to switch these reads to EPingType::RoundTripExclFrame
// and which was reversed by this finding.
//
// UNetConnection::ReadPacketInfo (Engine/Private/NetConnection.cpp) does:
//
//     const double RTT          = (CurrentTime - OutLagTime[Index]);
//     const double RTTExclFrame = RTT - (bExcludeFrameTime ? ServerFrameTime : 0.0);
//     const double NewLag       = FMath::Max(RTTExclFrame, 0.0);
//     NetPing->UpdatePing(EPingType::RoundTrip,          CurrentTime, NewLag);
//     NetPing->UpdatePing(EPingType::RoundTripExclFrame, CurrentTime, FMath::Max(RTT, 0.0));
//
// NewLag IS the frame-time-subtracted value, and it goes to `RoundTrip`. The
// raw, frame-time-INCLUSIVE RTT goes to `RoundTripExclFrame`. So the type whose
// name promises exclusion is the one that never excludes anything.
//
// AND TODAY THEY ARE THE SAME NUMBER ANYWAY. `bExcludeFrameTime` reads
// net.PingExcludeFrameTime, which defaults to 0 and is set nowhere in this
// project's config, so the subtraction is a no-op and both types receive
// max(RTT, 0.0) — identical samples. Switching the ping type here would change
// NOTHING about frame time; it would only swap the averaging configured in
// DefaultEngine.ini (RoundTrip=PlayerStateAvg vs RoundTripExclFrame=MovingAverage),
// i.e. a pure smoothing change with no evidence behind it.
//
// AND ON THIS SITE SPECIFICALLY IT COULD NOT MATTER EVEN IF ENABLED. This read
// runs SERVER-side. ServerFrameTime is parsed under `if (!Driver->IsServer())`
// and is therefore structurally 0.0 on the server, so the subtracted term is
// zero here by construction regardless of any cvar.
//
// WHAT ACTUALLY INFLATES RTT DURING A LOCAL FRAME HITCH — the effect T21 set
// out to kill — is neither ping type. CurrentTime is FApp::GetCurrentTime()
// (frame-start), so a local hitch delays ack processing and inflates the
// measurement, and the only frame time either type can ever subtract is the
// REMOTE's. The lever that excludes local frame time is
// net.PingUsePacketRecvTime; it is NOT enabled here, carries threading
// prerequisites, and is scoped as its own backlog task rather than folded in.
// ===========================================================================
//
// ENGINE PRIMITIVE: a second engine (Godot) must supply its own RTT source here.
inline double readRoundTripMs(UNetConnection* conn)
{
    // ONE-SHOT diagnostic. Shares the rationale of the client-side twin in
    // SimulationTimingRelay.cpp: the silent-zero class of failure is defined by
    // being unobservable, so it has to announce itself — but this runs on the
    // per-bundle input-RPC receive path, once per connection per datagram, so
    // an unthrottled warning would be a firehose. Function-local static in an
    // inline function is one object across the module (game-thread only, per
    // the ServerReceptionCoordinator threading contract).
    auto warnOnce = [](const TCHAR* cause)
    {
        static bool bWarned = false;
        if (bWarned)
        {
            return;
        }
        bWarned = true;
        UE_LOG(LogOGRtt, Warning,
            TEXT("[RttSample] SERVER tier RTT source unavailable (%s). No tier will be ")
            TEXT("derived or published for this connection, so it stays on the default ")
            TEXT("tier 0. One-shot warning."),
            cause);
    };

    if (conn == nullptr)
    {
        // Routine and self-resolving (an actor with no wire identity, or a
        // connection not yet established) — the documented sentinel path, not a
        // fault. Deliberately NOT warned, so the one-shot budget above is spent
        // on the failures that never resolve on their own.
        return kNoRttReadingMs;
    }

    UE::Net::FNetPing* netPing = conn->GetNetPing();
    if (netPing == nullptr)
    {
        warnOnce(TEXT("no FNetPing on the connection - check net.NetPingEnabled"));
        return kNoRttReadingMs;
    }

    if (!EnumHasAnyFlags(netPing->GetPingTypes(), EPingType::RoundTrip))
    {
        warnOnce(TEXT("EPingType::RoundTrip not enabled - check net.NetPingTypes"));
        return kNoRttReadingMs;
    }

    const UE::Net::FPingValues pingVals = netPing->GetPingValues(EPingType::RoundTrip);
    if (pingVals.Current < 0.0)
    {
        // GetPingValues documents Current = -1.0 for "not set". Before T21 this
        // fell through and returned -1000.0, which happened to be negative and
        // so happened to be skipped downstream — correct behaviour by accident.
        // Made explicit so it survives any future edit to the units.
        warnOnce(TEXT("EPingType::RoundTrip enabled but its accumulator is empty (no ack sampled yet)"));
        return kNoRttReadingMs;
    }

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
