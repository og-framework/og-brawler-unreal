// SPDX-License-Identifier: MPL-2.0

#pragma once

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulation/SimulationQueues.h"

// ---------------------------------------------------------------------------
// buildRedundancyBundle — producer-side helper for the Stage 1 unreliable +
// redundancy local-input channel (Task 9; proposal §3.1 step 8).
//
// Walks the most-recent `redundancyDepth` ticks still retained in the client's
// PendingInputQueue (capped at FInputRedundancyBundle::kMaxSlots for wire safety)
// and appends one (capture_tick, input) slot per tick into `outBundle`, oldest
// first. The version byte is written lazily by appendSlot via
// FInputRedundancyBundle::kWireFormatVersion.
//
// `outBundle` is reset first so a reused/pooled bundle does not accumulate stale
// slots across frames. The R-T5 per-capture-tick immutability invariant is
// enforced inside FInputRedundancyBundle::appendSlot (a duplicate capture_tick
// OG_CHECK-fails); the PendingInputQueue holds distinct ticks, so the window is
// already dedup-clean.
//
// This is the UE-side bridge that lets the UE-free SimulationNetSync layer drive
// a UE wire type: SimulationNetSync::sendLocalInputToAuthorityAll hands the owner
// its core PendingInputQueue + scalar params, and the owner
// (USimmableUpdateComponent::sendLocalInputToAuthority) calls this helper to build
// the bundle before firing the unreliable RPC.
//
// Unit-tested in isolation by the Task 12 regression suite.
// ---------------------------------------------------------------------------
template <typename InputType>
void buildRedundancyBundle(
    const PendingInputQueue<InputType>& queue,
    uint32 currentTick,
    uint8 redundancyDepth,
    FInputRedundancyBundle& outBundle)
{
    outBundle.wireBytes.Reset();

    const uint8 depth = redundancyDepth < FInputRedundancyBundle::kMaxSlots
        ? redundancyDepth
        : FInputRedundancyBundle::kMaxSlots;

    queue.forEachRecent(currentTick, static_cast<size_t>(depth),
        [&outBundle](uint32 captureTick, const InputType& input)
        {
            outBundle.appendSlot<InputType>(captureTick, input);
        });
}
