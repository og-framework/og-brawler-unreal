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
// slots across frames. The PendingInputQueue holds distinct ticks, so the window
// this helper walks is already dedup-clean.
//
// INVARIANT (R-T5 / D3.11) — append-only, immutable per capture-tick:
//
//   Inputs written into FInputRedundancyBundle are append-only and immutable per
//   capture-tick. The client MUST NOT revise the input value for a previously-
//   emitted capture-tick; any producer that does so silently loses the corrected
//   value at the dedup-by-capture-tick step on the server. If a future feature
//   requires input revision post-capture, the wire format needs an explicit
//   revision-number field and the dedup contract must be re-designed.
//
// Why the server cannot save a violating producer: the server's receive path
// (forEachSlot -> RemoteMoveQueue::queueMove) is first-writer-wins per capture
// tick. It has no way to distinguish "redundant re-send of the same value" —
// which is the entire point of this channel — from "revision of a value already
// sent". So it keeps the first and discards the second, and a revised input is
// lost with no diagnostic. That is why the invariant is enforced at the
// PRODUCER, where the violation is observable.
//
// Enforcement lives in the engine-agnostic codec
// (OGSimulation/InputRedundancyBundleCodec.h): appendSlot() OG_CHECK-fails on a
// duplicate capture_tick in checked builds, and in shipping builds — where the
// OG_CHECK compiles out — the duplicate is silently DROPPED so the first-arrival
// input is preserved. A shipping client can therefore never revise an already-
// emitted input, only fail to add a new one. See that header's ENFORCEMENT block
// and inputRedundancyBundle::tryAppendSlot.
//
// The kMaxSlots wire budget is enforced by the same split (T16): the always-
// compiled kernel drops an append that would exceed kMaxSlots in every build
// config. That is defence in depth for callers OTHER than this one — the `depth`
// clamp below already keeps this helper strictly within the budget, so the drop
// is unreachable from here. Any future producer that drives the bundle without
// an equivalent clamp is nonetheless prevented from emitting an over-budget
// payload in a shipping build.
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
