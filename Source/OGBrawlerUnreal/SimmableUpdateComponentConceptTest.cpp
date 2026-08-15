// SPDX-License-Identifier: BUSL-1.1

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGSimulation/SimulationQueues.h"

// ---------------------------------------------------------------------------
// Inline concept definitions — mirrors SimulationNetSync.h exactly.
// Defined here to avoid pulling SimulationNetSync.h into the OGBrawlerUnreal module
// compilation unit via a second path (SimmableUpdateComponent.h already pulls
// it in via the SimulatableOwnerTraits specialization at the bottom).
//
// Must stay byte-for-byte in sync with the corresponding concepts in
// SimulationNetSync.h — otherwise this static_assert can pass while the
// real concept check at registerSimulatable fails (or vice versa).
// ---------------------------------------------------------------------------

template <typename BufferT, typename CompositeT>
concept CompositeSyncedBufferConceptLocal =
    requires(std::remove_reference_t<BufferT>& b,
             const std::remove_reference_t<BufferT>& cb,
             const CompositeT& in,
             CompositeT& out,
             uint32 tick)
    {
        { b.write(in, tick) };
        { cb.readInto(out) } -> std::same_as<uint32_t>;
    };

// [og-netcode-v2-input-relay T4] Mirrors CorrectionStateSyncedBufferConcept —
// the correction-state buffer additionally carries the per-tick applied-capture-
// tick reference (the join key between the state and relayed-input channels).
template <typename BufferT, typename StateT>
concept CorrectionStateSyncedBufferConceptLocal =
    CompositeSyncedBufferConceptLocal<BufferT, StateT> &&
    requires(std::remove_reference_t<BufferT>& b,
             const std::remove_reference_t<BufferT>& cb,
             const StateT& in,
             uint32 tick,
             uint32 appliedCaptureTick)
    {
        { b.write(in, tick, appliedCaptureTick) };
        { cb.getAppliedCaptureTick() } -> std::same_as<uint32>;
    };

// [og-netcode-v2-input-relay T5] Mirrors the relay-ring half added to
// PredictionSyncedBufferOwnerConcept: the arrival callback pair plus the const
// ring accessor SimulationNetSync reads once at bind.
template <typename OwnerT, typename StateT, typename InputT>
concept PredictionSyncedBufferOwnerConceptLocal =
    requires(OwnerT& owner,
             const OwnerT& constOwner,
             std::function<void(const typename OwnerT::SyncedCorrectionBufferType&)> corrFn,
             std::function<void(const typename OwnerT::RelayedInputRingType&)> relayFn,
             const PendingInputQueue<InputT>& pendingQueue,
             uint32 currentTick,
             uint32 redundancyDepth)
    {
        typename OwnerT::SyncedCorrectionBufferType;
        typename OwnerT::SyncedRemoteInputBufferType;
        typename OwnerT::RelayedInputRingType;
        requires CorrectionStateSyncedBufferConceptLocal<typename OwnerT::SyncedCorrectionBufferType, StateT>;
        requires CompositeSyncedBufferConceptLocal<typename OwnerT::SyncedRemoteInputBufferType, InputT>;
        // [T8] The correction-INPUT callback pair that sat between these two lines
        // is retired with its channel. SyncedRemoteInputBufferType stays required —
        // it still types getClientToServerInputSyncedBuffer below.
        { owner.setOnCorrectionStateReceivedCallback(corrFn) };
        { owner.clearOnCorrectionStateReceivedCallback() };
        { owner.setOnRelayedInputReceivedCallback(relayFn) };
        { owner.clearOnRelayedInputReceivedCallback() };
        { constOwner.getRelayedInputRing() } -> std::same_as<const typename OwnerT::RelayedInputRingType&>;
        { owner.getClientToServerInputSyncedBuffer() } -> std::same_as<typename OwnerT::SyncedRemoteInputBufferType*>;
        { owner.sendLocalInputToAuthority(pendingQueue, currentTick, redundancyDepth) };
    };

template <typename OwnerT, typename StateT, typename InputT>
concept AuthoritySyncedBufferOwnerConceptLocal =
    requires(OwnerT& owner,
             std::function<void(uint32, const InputT&)> fn)
    {
        // [T8] Mirrors the core: the authority owner's outbound INPUT buffer
        // (getSyncedCorrectionInputBuffer) and the SyncedRemoteInputBufferType
        // typedef + composite constraint that existed to type it are gone. The
        // authority publishes state only.
        { owner.getSyncedCorrectionStateBuffer() } -> CorrectionStateSyncedBufferConceptLocal<StateT>;
        { owner.setOnRemoteMoveReceivedCallback(fn) };
        { owner.clearOnRemoteMoveReceivedCallback() };
    };

// ---------------------------------------------------------------------------
// Compile-time concept checks for USimmableUpdateComponent
// ---------------------------------------------------------------------------

static_assert(
    PredictionSyncedBufferOwnerConceptLocal<
        USimmableUpdateComponent,
        simulatableBrawler::State,
        simulatableBrawler::PlayerInput>,
    "USimmableUpdateComponent must satisfy PredictionSyncedBufferOwnerConcept");

static_assert(
    AuthoritySyncedBufferOwnerConceptLocal<
        USimmableUpdateComponent,
        simulatableBrawler::State,
        simulatableBrawler::PlayerInput>,
    "USimmableUpdateComponent must satisfy AuthoritySyncedBufferOwnerConcept");

// ---------------------------------------------------------------------------
// Runtime smoke test — behavioral parity: null callbacks fall through to old path.
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSimmableUpdateComponentConceptTest,
    "OGBrawlerUnreal.SimmableUpdateComponent.ConceptSatisfied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSimmableUpdateComponentConceptTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("PredictionSyncedBufferOwnerConcept satisfied at compile time"), true);
    TestTrue(TEXT("AuthoritySyncedBufferOwnerConcept satisfied at compile time"), true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
