#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "GameFramework/Info.h"

#include "SimulationConnectionRelay.generated.h"

class UNetConnection;

OGSIMULATIONUNREAL_API DECLARE_LOG_CATEGORY_EXTERN(LogOGConnRelay, Log, All);

// ---------------------------------------------------------------------------
// ASimulationConnectionRelay — the PER-CONNECTION replicated channel.
// (og-netcode-v2-input-relay / T10; RelayDelaySpectrumDesign.md §12.)
//
// Sibling of ASimulationTimingRelay, and the other half of a deliberate split:
//
//   Quantity            Scope     Audience        Vehicle
//   ------------------  --------  --------------  -------------------------
//   Authority tick      session   everyone        ASimulationTimingRelay
//   Connection tier     ONE wire  that wire only  THIS actor
//
// The tier used to ride `USimmableUpdateComponent::m_replicatedConnectionTier`
// (COND_OwnerOnly on a per-CHARACTER vehicle). That worked, but it anchored a
// WIRE property to a character: it needed a per-ownerId publish dedup in the
// core to stop split-screen siblings starving each other, and the channel did
// not exist until a character had registered. One actor per ROOT connection
// makes "one wire = one tier" structural instead of dedup-guarded, and gives
// the future per-wire owner-only tenants (§12.3/§13.2: A2 arrival-margin
// feedback, and dynamic per-wire directives generally) a home that already
// exists. NOTE the deliberate absence of the word "rollback" here: in this
// codebase it means RESIM DEPTH (T14's fence), and nothing on this channel is
// about resim depth.
// The floor (T11) is NOT a tenant here — it is session-scoped and rides the
// timing relay.
//
// WHY NOT the shared timing relay: it is an unowned bAlwaysRelevant AInfo, so
// COND_OwnerOnly on it would pass for NO connection; and one actor carries ONE
// value per property for ALL connections — vanilla property replication has no
// per-connection value branching. See §12.1.
//
// LIFECYCLE — SPAWN ON DEMAND (review A3b). The server creates this actor from
// the tier SINK write path (`findOrSpawnForConnection`), not from
// `GameMode::PostLogin`. PostLogin is NOT called for players carried through
// seamless travel, so a PostLogin-only spawn would die with the old level and
// leave the tier channel permanently dead afterwards. Spawning where the value
// is produced self-heals BOTH late join and travel: whatever the lifecycle did,
// the next publish finds no relay for the wire and makes one.
//   Consequence, and it is the CORRECT one: a wire whose tier never leaves 0
//   never gets a relay at all, because the core never publishes tier 0 as a
//   first value (`m_lastPublishedTier` baseline 0). That preserves today's
//   standing tier-0 quirk exactly — see the note in the .cpp.
//
// DESTRUCTION. The owning PlayerController does not destroy its owned actors,
// so the relay reaps itself: a 1 Hz server-side timer destroys it once its root
// connection or owning PC is gone. That covers logout, timeout, and a hard
// mid-session client kill uniformly, without reaching into the game's GameMode
// (which lives in a module this one must not depend on).
//
// LATCH/REPLAY (review A3a). The tier UPROPERTY dirties only on CHANGE, so an
// OnRep that fires before the client manager binds its listener would otherwise
// never be re-notified — the value would sit here with nothing listening for
// the rest of the session. This actor therefore latches (observed flag + value)
// and offers `replayLatchedTier`, which the manager PULLS at listener bind.
// Do NOT reason from ISimulationTimingRelayListener here: that channel survives
// dropped notifications only because its buffer rewrites at 100 Hz.
// ---------------------------------------------------------------------------
UCLASS()
class OGSIMULATIONUNREAL_API ASimulationConnectionRelay : public AInfo
{
    GENERATED_BODY()

public:
    ASimulationConnectionRelay();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // SERVER: the relay owned by `rootConnection`, spawning one if this wire has
    // none yet. `rootConnection` must already be root-walked (see
    // GetRootNetConnection) — split-screen siblings share ONE relay by
    // construction. Returns nullptr when the world/connection cannot supply an
    // owner (no PlayerController yet), which the caller treats as "not sendable
    // this time"; the core's per-owner dedup means a lost publish is not retried,
    // so this path logs.
    static ASimulationConnectionRelay* findOrSpawnForConnection(UWorld* world, UNetConnection* rootConnection);

    // CLIENT: the single relay resident in this world. Owner-only relevancy means
    // a client ever holds exactly one — its own. Never call this on an authority
    // world, where N relays (one per remote wire) coexist and "the local one" is
    // meaningless; the accessor asserts that.
    static ASimulationConnectionRelay* findLocalClientRelay(UWorld* world);

    // SERVER: publish the authoritative tier for this wire. Called from the
    // ConnectionTierSink implementation; the core has already applied the
    // "no reading" skip and the publish-only-on-change dedup, so this is pure
    // transport.
    void setConnectionTier(uint8 tier);

    uint8 getConnectionTier() const { return m_connectionTier; }

    // True once an OnRep for the tier has actually landed. Keyed on ARRIVAL, not
    // on the VALUE, for the same reason ReplicatedTierConsumer is: 0 is both the
    // property default and a legal tier.
    bool hasObservedConnectionTier() const { return m_connectionTierOnRepObserved; }

    // CLIENT: push the latched tier at the currently-registered listener, if any
    // has landed. The manager calls this when it binds (A3a pull-at-bind). Uses
    // the REPLAY entry point — no stall; see ISimulationConnectionRelayListener.
    void replayLatchedTier() const;

private:
    UFUNCTION()
    void OnRep_ConnectionTier(uint8 OldTier);

    void reapIfConnectionGone();

    // The authoritative connection tier for this wire. uint8 because the tier
    // index is bounded by ConnectionTierTable::kTierCount (4 today).
    //
    // Replicated unconditionally — the RELEVANCY filter (bOnlyRelevantToOwner)
    // already restricts this actor to the owning wire, so a COND_OwnerOnly on
    // top would be redundant. That is precisely the property this migration
    // bought: on the retired component channel the vehicle was visible to
    // everyone and the CONDITION had to do the narrowing.
    UPROPERTY(ReplicatedUsing = OnRep_ConnectionTier)
    uint8 m_connectionTier = 0;

    // [A3a] True once OnRep_ConnectionTier has fired at least once, even if no
    // listener was bound at the time. Distinguishes "the server sent tier 0"
    // from "the property is still at its default 0".
    bool m_connectionTierOnRepObserved = false;

    // SERVER ONLY. The wire this relay belongs to — the identity
    // findOrSpawnForConnection matches on, and the liveness the reap timer polls.
    // Weak: a closed connection is GC'd, and that alone is a valid death signal.
    TWeakObjectPtr<UNetConnection> m_rootConnection;

    FTimerHandle m_reapTimerHandle;
};
