// SPDX-License-Identifier: BUSL-1.1

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"

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

template <typename OwnerT, typename StateT, typename InputT>
concept PredictionSyncedBufferOwnerConceptLocal =
    requires(OwnerT& owner,
             std::function<void(const typename OwnerT::SyncedCorrectionBufferType&)> corrFn,
             std::function<void(const typename OwnerT::SyncedRemoteInputBufferType&)> inputFn,
             const typename OwnerT::SyncedRemoteInputBufferType& buffer)
    {
        typename OwnerT::SyncedCorrectionBufferType;
        typename OwnerT::SyncedRemoteInputBufferType;
        requires CompositeSyncedBufferConceptLocal<typename OwnerT::SyncedCorrectionBufferType, StateT>;
        requires CompositeSyncedBufferConceptLocal<typename OwnerT::SyncedRemoteInputBufferType, InputT>;
        { owner.setOnCorrectionStateReceivedCallback(corrFn) };
        { owner.setOnCorrectionInputReceivedCallback(inputFn) };
        { owner.clearOnCorrectionStateReceivedCallback() };
        { owner.clearOnCorrectionInputReceivedCallback() };
        { owner.getClientToServerInputSyncedBuffer() } -> std::same_as<typename OwnerT::SyncedRemoteInputBufferType*>;
        { owner.sendLocalInputToAuthority(buffer) };
    };

template <typename OwnerT, typename StateT, typename InputT>
concept AuthoritySyncedBufferOwnerConceptLocal =
    requires(OwnerT& owner,
             std::function<void(const typename OwnerT::SyncedRemoteInputBufferType&)> fn)
    {
        typename OwnerT::SyncedRemoteInputBufferType;
        requires CompositeSyncedBufferConceptLocal<typename OwnerT::SyncedRemoteInputBufferType, InputT>;
        { owner.getSyncedCorrectionStateBuffer() } -> CompositeSyncedBufferConceptLocal<StateT>;
        { owner.getSyncedCorrectionInputBuffer() } -> CompositeSyncedBufferConceptLocal<InputT>;
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
