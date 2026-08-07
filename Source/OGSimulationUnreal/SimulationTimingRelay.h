#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"

#include "SimulationTimingRelay.generated.h"

// ---------------------------------------------------------------------------
// ASimulationTimingRelay — the SESSION-scoped replicated channel.
//
// Two tenants, and they are deliberately separate properties:
//
//   Quantity              Cadence        Property
//   --------------------  -------------  ---------------------------------
//   Authority tick + RTT  100 Hz rewrite m_buffer (FSmallSimulationStateSyncBuffer)
//   Relay delay floor     on change      m_relayDelayFloorTicks (T11)
//
// WHY THE FLOOR IS NOT IN THE BUFFER (RelayDelaySpectrumDesign.md §6, §11 Q1):
// the buffer is a serialized clock payload rewritten every frame; folding a
// session constant into it would change its wire format, spend a byte 100 times
// a second on a value that changes approximately never, and mix a config lever
// into the clock's serialization. A separate uint8 UPROPERTY dirties only on
// change and rides the initial bunch on join.
//
// LATCH/REPLAY (review finding A3a) — the SAME requirement T10's connection
// relay has, for the same reason. A dirty-on-change property notifies exactly
// once; if that OnRep fires before the client manager binds its listener, the
// value sits here with nothing listening and is NEVER re-notified. So this actor
// latches (observed flag + value) and offers `replayLatchedRelayDelayFloor`,
// which the manager PULLS at listener bind.
// Do NOT reason from `m_buffer` here: that tenant survives dropped notifications
// only because it is rewritten at 100 Hz. The floor gets no second chance.
// ---------------------------------------------------------------------------
UCLASS()
class OGSIMULATIONUNREAL_API ASimulationTimingRelay : public AInfo
{
    GENERATED_BODY()

public:
    ASimulationTimingRelay();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;

    FSmallSimulationStateSyncBuffer& editBuffer() { return m_buffer; }
    const FSmallSimulationStateSyncBuffer& readBuffer() const { return m_buffer; }

    // ---- [T11 / og-netcode-v2-input-relay] the relay delay floor ------------

    // SERVER: publish the session's relay delay floor. Called once by the
    // composition root at BeginPlay, from the authoritative TimeConfig; the
    // deferred dynamic-floor policy (§11 Q6) simply calls it again. uint8 because
    // the value is clamped well below 255 core-side (see clampRelayDelayFloorTicks).
    void setRelayDelayFloorTicks(uint8 floorTicks);

    uint8 getRelayDelayFloorTicks() const { return m_relayDelayFloorTicks; }

    // True once an OnRep for the floor has actually landed. Keyed on ARRIVAL, not
    // on the VALUE, for the same reason ReplicatedTierConsumer is: 0 is both the
    // property default and a perfectly legal floor (it is the shipped default).
    bool hasObservedRelayDelayFloor() const { return m_relayDelayFloorOnRepObserved; }

    // CLIENT: push the latched floor at the currently-registered listener, if any
    // value has landed. The manager calls this when it binds (A3a pull-at-bind).
    // Uses the REPLAY entry point — no prediction stall; see
    // ISimulationTimingRelayListener.
    void replayLatchedRelayDelayFloor() const;

private:
    UFUNCTION()
    void OnRep_Buffer();

    UFUNCTION()
    void OnRep_RelayDelayFloor();

    UPROPERTY(ReplicatedUsing = OnRep_Buffer)
    FSmallSimulationStateSyncBuffer m_buffer;

    // The session relay delay floor, in ticks. Its OWN property, not a field of
    // m_buffer — see the class note above.
    UPROPERTY(ReplicatedUsing = OnRep_RelayDelayFloor)
    uint8 m_relayDelayFloorTicks = 0;

    // [A3a] True once OnRep_RelayDelayFloor has fired at least once, even if no
    // listener was bound at the time. Distinguishes "the server sent floor 0"
    // from "the property is still at its default 0".
    bool m_relayDelayFloorOnRepObserved = false;
};
