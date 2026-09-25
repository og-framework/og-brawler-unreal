// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationManagerUImpl-rationale.md · docs/SimulationManagerUImpl-guards.md

#pragma once

#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"
#include "OGSimulationUnreal/ISimulationConnectionRelayListener.h"
#include "OGSimulationUnreal/ISimulationInputRelayListener.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include <set>
#include "Engine/World.h"
#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackInput.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGSimulationUnreal/PCTimeManagement/ChaosTickMapper.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/SimulationManagerConcept.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/Network/ConnectionSlotKey.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ServerInputDelayQueue.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitDetectionSystem.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"
#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGBrawler/BrawlerRingoutScoreSystem.h"

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h"
#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"

#include "SimulationManagerUImpl.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOGSim, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGSimTick, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGMgmt, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGNet, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOG, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGRelayProbe, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGDivergenceProbe, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGResimProbe, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

struct FMovementStaticDataCVars
{
	brawlerMovementSimulation::MovementModel model;
	float    maxWalkSpeed;
	uint32_t stepPeriodTicks;
	float    stepSpeed;
	float    gravity;
};

enum class TryRegisterStatus { Pending, Ready };

// ⛔G-01  docs/SimulationManagerUImpl-guards.md
using BrawlerInputProviderFn = std::function<simulatableBrawler::PlayerInput(
	const SimulationTimeStep&,
	const LocalInputCache<simulatableBrawler::PlayerInput>&)>;

class USimmableUpdateComponent;
class ASimulationManagerUImpl;
class ASimulationTimingRelay;
class ASimulationConnectionRelay;

struct FSimulationState2 : public Chaos::FSimCallbackOutput
{
	FSimulationState2()
		: FSimCallbackOutput()
		, bIsValid(false)
	{
	}

	virtual ~FSimulationState2() {}

public:
	void Reset() { bIsValid = false; }
	void Copy(const FSimulationState2& Value);
	bool IsValid() const { return bIsValid; }
	bool bIsValid;
	float DeltaTime;
};

struct FSimulationInput2 : public Chaos::FSimCallbackInput
{
	virtual ~FSimulationInput2() {}

	void Reset()
	{
		bInitialized = false;
		m_world = nullptr;
		m_manager = nullptr;
	}

	bool bInitialized = false;
	UWorld* m_world;
	ASimulationManagerUImpl* m_manager = nullptr;

private:
};

class ASimulationManagerUImpl;

struct FRewindPushProbeStashedBody
{
	PhysicsBodyState pushed;
	PhysicsBodyState beforePush;
	BodyId           bodyId{};
	unsigned int     simulatableId = 0;
	// ⛔G-02  docs/SimulationManagerUImpl-guards.md
	const char*      bodyName = nullptr;
	bool             wireCarriesRotationAndSpin = false;
	bool             movedAtPush = false;
};

class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FSimulationInput2,
	FSimulationState2,
	Chaos::ESimCallbackOptions::Presimulate |
	Chaos::ESimCallbackOptions::Rewind |
	Chaos::ESimCallbackOptions::PostSolve>
{
public:
	virtual FName GetFNameForStatId() const override;

	void setManager(ASimulationManagerUImpl* manager)
	{
		m_manager = manager;
	}

private:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPostSolve_Internal() override;
	virtual void ProcessInputs_Internal(int32 PhysicsStep);
	virtual void ProcessInputs_External(int32 PhysicsStep);

	virtual int32 TriggerRewindIfNeeded_Internal(int32 LastCompletedStep);

	virtual void ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input) override;

	virtual void FirstPreResimStep_Internal(int32 PhysicsStep);

	ASimulationManagerUImpl* m_manager = nullptr;

	TArray<FRewindPushProbeStashedBody> m_pushProbeStash;
	int32    m_pushProbeStashFrame      = INDEX_NONE;
	uint32_t m_pushProbeStashSimTick    = 0;
	int32    m_pushProbeStashBodies     = 0;
	int32    m_pushProbeStashUnresolved = 0;

	void readPushProbeVerdict_Internal();
	void discardPushProbeStash(const TCHAR* reason);

	template <typename InputT, typename OutputT, Chaos::ESimCallbackOptions OptionsT>
	static constexpr Chaos::ESimCallbackOptions registeredOptionsOf(
		const Chaos::TSimCallbackObject<InputT, OutputT, OptionsT>*) { return OptionsT; }

	template <typename Self>
	static constexpr bool everyRegisteredHookIsOverridden()
	{
		using Opt = Chaos::ESimCallbackOptions;
		constexpr Opt options = registeredOptionsOf(static_cast<const Self*>(nullptr));
		static_assert(!EnumHasAnyFlags(options, Opt::Presimulate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreSimulate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::Presimulate but does not "
			"override OnPreSimulate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PreIntegrate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreIntegrate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PreIntegrate but does not "
			"override OnPreIntegrate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PostIntegrate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPostIntegrate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PostIntegrate but does not "
			"override OnPostIntegrate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::MidPhaseModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnMidPhaseModification_Internal), void (Self::*)(Chaos::FMidPhaseModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::MidPhaseModification but does not "
			"override OnMidPhaseModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::CCDModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnCCDModification_Internal), void (Self::*)(Chaos::FCCDModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::CCDModification but does not "
			"override OnCCDModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::StrainModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnStrainModification_Internal), void (Self::*)(Chaos::FStrainModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::StrainModification but does not "
			"override OnStrainModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::ContactModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnContactModification_Internal), void (Self::*)(Chaos::FCollisionContactModifier&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::ContactModification but does not "
			"override OnContactModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PreSolve)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreSolve_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PreSolve but does not "
			"override OnPreSolve_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PostSolve)
			|| requires { requires std::is_same_v<decltype(&Self::OnPostSolve_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PostSolve but does not "
			"override OnPostSolve_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		return true;
	}

	void assertEveryRegisteredHookIsOverridden()
	{
		static_assert(everyRegisteredHookIsOverridden<FSimulationManagerAsyncCallback>());
	}
};

UCLASS()
class ASimulationManagerUImpl : public AActor,
                                public ISimulationTimingRelayListener,
                                public ISimulationConnectionRelayListener,
                                public ISimulationInputRelayListener
{
    GENERATED_BODY()

public:
    ASimulationManagerUImpl();
    ~ASimulationManagerUImpl();

    static ASimulationManagerUImpl* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
    ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
    const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
    bool runsPrediction() const { return m_manager->runsPrediction(); }

    void requestInputDelayIncreaseStall(int32 deltaDelayTicks)
    {
        // ⛔G-03  docs/SimulationManagerUImpl-guards.md
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;

        m_manager->editClientClock().requestInputDelayIncreaseStall(deltaDelayTicks);
    }
    void onGameSimulation(const SimulationUpdateInfo& info)
    {
        m_manager->onGameSimulation(info);
    }
    void onPostGameSimulation(const SimulationUpdateInfo& info) { m_manager->onPostGameSimulation(info); }
    unsigned int onCheckIsSimilar() { return m_manager->onCheckIsSimilar(); }
    void prepareResimulation(int32_t chaosStep, uint32_t simTick) { m_manager->prepareResimulation(chaosStep, simTick); }

    void noteResimRequest(unsigned int anchorTick, int32 lastCompletedStep, int32 requestedChaosFrame)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;
        m_manager->editResimGateProbe().noteRequest(
            static_cast<std::uint32_t>(anchorTick), lastCompletedStep, requestedChaosFrame);
    }

    void noteResimGrant(int32 grantedChaosFrame)
    {
        if (!m_manager.has_value() || !m_manager->runsPrediction())
            return;
        m_manager->editResimGateProbe().noteGrant(grantedChaosFrame);
    }
    void onPostSimulationGameThread();

    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    FSmallSimulationStateSyncBuffer& getSyncedTimingBuffer();
    void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
    {
        m_onTimingInfoReceivedCallback = std::move(fn);
    }

    virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) override
    {
        if (m_onTimingInfoReceivedCallback)
        {
            m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
            return;
        }
        if (!m_manager.has_value())
            return;
        m_manager->editNetworkEstimator().updateRTT(roundTripTime);
        m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
    }

    void OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime);
    void OnPhysicsStep(FPhysScene* Scene, float DeltaTime);
    void OnPostPhysicsStep(FChaosScene* Scene);

    TryRegisterStatus tryRegister(
        unsigned int id,
        SimulatableBrawler simulatable,
        USimmableUpdateComponent& owner,
        BrawlerInputProviderFn inputProvider,
        bool isAuthority);

    void unregisterFromNewFramework(unsigned int id, USimmableUpdateComponent& owner, bool isAuthority);

    void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);

    ChaosTickMapper& editChaosTickMapper() { return m_chaosTickMapper; }
    const ChaosTickMapper& getChaosTickMapper() const { return m_chaosTickMapper; }

    ChaosPhysicsBodyAdapter&  editPhysicsBodyAdapter() { return m_physAdapter.value(); }
    ChaosSpatialQueryAdapter& editQueryAdapter()       { return m_queryAdapter.value(); }

    const ChaosPhysicsBodyReaderAdapter& getPhysicsBodyReaderAdapter() const { return m_physReaderAdapter.value(); }
    SimulationObjectStorage<SimulatableBrawler>& editStorage() { return m_storage; }

    SimulationReconciliation<SimulatableBrawler>&       editReconciliation()       { return m_reconciliation; }
    const SimulationReconciliation<SimulatableBrawler>& editReconciliation() const { return m_reconciliation; }

    std::optional<simulatableBrawler::PlayerInput> getLastRelayedInput(unsigned int id) const
    {
        return m_inputResolution.getLastRelayedInput<SimulatableBrawler>(id);
    }

    const LocalInputCache<simulatableBrawler::PlayerInput>* getLocalInputCache(unsigned int id) const
    {
        return m_inputResolution.getDiagnostics().localInputCache<SimulatableBrawler>(id);
    }

    bool isLocallyControlledOnThisPeer(unsigned int id) const
    {
        return getLocalInputCache(id) != nullptr;
    }

    void pollInputHistory(unsigned int id, uint32 newestTick, float deadzone)
    {
        const LocalInputCache<simulatableBrawler::PlayerInput>* captures = getLocalInputCache(id);
        if (captures == nullptr)
            return;

        m_inputHistory.poll(id, *captures, newestTick, deadzone);
    }

    const brawlerInputHistoryVisualization::InputHistoryRowRing* getInputHistoryRows(
        unsigned int id) const
    {
        return m_inputHistory.findRows(id);
    }

    std::optional<brawlerRingout::State> getRingoutVizState(unsigned int id) const
    {
        if (!m_storage.has<SimulatableBrawler>(id))
            return std::nullopt;

        // ⛔G-04  docs/SimulationManagerUImpl-guards.md
        return m_storage.get<SimulatableBrawler>(id)
            .getVizState().getState().get<brawlerRingout::State>();
    }

    void pollInputHistoryLanes(unsigned int id, uint32 liveTick, DAttackState machineState,
        std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput,
        bool pauseWhileIdle, bool includeDelay)
    {
        const std::optional<uint32> predictionOffsetTicks =
            // ⛔G-05  docs/SimulationManagerUImpl-guards.md
            (m_manager.has_value() && m_manager->runsPrediction())
                ? std::optional<uint32>(
                      m_manager->getNetworkEstimator().getPredictionOffsetTicks())
                : std::nullopt;

        // ⛔G-06  docs/SimulationManagerUImpl-guards.md
        const bool isLocallyControlled = isLocallyControlledOnThisPeer(id);

        const std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition> delay =
            // ⛔G-07  docs/SimulationManagerUImpl-guards.md
            (includeDelay && isLocallyControlled
                && m_replicatedTierConsumer.has_value() && m_manager.has_value())
                ? std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition>(
                      brawlerInputHistoryVisualization::decomposeInputDelay(
                          m_replicatedTierConsumer->hasReceivedTier(),
                          m_replicatedTierConsumer->currentTierIndex(),
                          m_manager->getTimeConfig(),
                          m_replicatedTierConsumer->effectiveInputDelayTicks(),
                          m_inputResolution.getClientEffectiveInputDelayTicks()))
                : std::nullopt;

        std::optional<brawlerInputHistoryVisualization::ClockDriftReading> clock;
        // ⛔G-08  docs/SimulationManagerUImpl-guards.md
        if (m_manager.has_value() && m_manager->runsPrediction())
        {
            const ClientPredictionClock& predictionClock = m_manager->getClientClock();
            const NetworkTimeEstimator&  estimator       = m_manager->getNetworkEstimator();

            brawlerInputHistoryVisualization::ClockDriftReading reading;
            reading.predictionTick = predictionClock.getPredictionTick();
            reading.targetTick     = estimator.getTargetPredictionTick();
            reading.authorityTick  = estimator.getLastAuthorityTick();
            reading.driftTicks     = static_cast<int32_t>(reading.targetTick)
                                   - static_cast<int32_t>(reading.predictionTick);
            reading.pendingAction  = predictionClock.evaluateDrift();
            reading.stallDebtTicks = predictionClock.getRequiredInputDelayIncreaseStallTicks();
            // ⛔G-09  docs/SimulationManagerUImpl-guards.md
            reading.skipCount              = predictionClock.getDiagnostics().skipCount();
            reading.lastSkipTick           = predictionClock.getDiagnostics().lastSkipTick();
            reading.stallCount             = predictionClock.getDiagnostics().stallCount();
            reading.lastStallTick          = predictionClock.getDiagnostics().lastStallTick();
            reading.hardResyncCount        = predictionClock.getDiagnostics().hardResyncCount();
            reading.lastHardResyncFromTick =
                predictionClock.getDiagnostics().lastHardResyncFromTick();
            reading.lastHardResyncToTick   = predictionClock.getDiagnostics().lastHardResyncToTick();

            clock = reading;
        }

        const RelayedReadObservationRing* const relayedReads =
            isLocallyControlled ? nullptr : getRelayedReadObservations(id);

        const RelayedInputArrivalRing* const relayedArrivals =
            isLocallyControlled ? nullptr : getRelayedInputArrivals(id);

        const uint32 rollbackWindowTicks =
            // ⛔G-10  docs/SimulationManagerUImpl-guards.md
            (m_manager.has_value() && m_manager->getTimeConfig().rollbackWindowTicks > 0)
                ? static_cast<uint32>(m_manager->getTimeConfig().rollbackWindowTicks)
                : 0u;

        const auto reader =
            inputHistoryVisualizationUImpl::makeReconciliationSlotReader<SimulatableBrawler>(
                m_reconciliation, id);

        // ⛔G-11  docs/SimulationManagerUImpl-guards.md
        if (relayedReads != nullptr && relayedArrivals != nullptr)
        {
            m_inputHistory.pollLanes(id, reader, liveTick, machineState, liveInput,
                pauseWhileIdle, predictionOffsetTicks, delay, clock, *relayedReads,
                *relayedArrivals, rollbackWindowTicks);
        }
        else
        {
            m_inputHistory.pollLanes(id, reader, liveTick, machineState, liveInput,
                pauseWhileIdle, predictionOffsetTicks, delay, clock);
        }
    }

    const RelayedReadObservationRing* getRelayedReadObservations(unsigned int id) const
    {
        return m_inputResolution.getDiagnostics()
            .relayedReadObservations<SimulatableBrawler>(id);
    }

    const RelayedInputArrivalRing* getRelayedInputArrivals(unsigned int id) const
    {
        return m_inputResolution.getDiagnostics()
            .relayedInputArrivals<SimulatableBrawler>(id);
    }

    brawlerInputHistoryVisualization::RelayReadReadout getRelayReadReadout(unsigned int id) const
    {
        brawlerInputHistoryVisualization::RelayReadReadout readout;

        const RelayedReadObservationRing* const ring = getRelayedReadObservations(id);
        if (ring == nullptr)
            return readout;

        bool   anyObservation = false;
        uint32 newestSimTick  = 0u;

        for (std::size_t index = 0u; index < ring->size(); ++index)
        {
            const RelayedReadObservation* const observation = ring->at(index);
            if (observation == nullptr)
                continue;

            switch (observation->outcome)
            {
            case ScheduledRelayedReadOutcome::Hit:        ++readout.hits;        break;
            case ScheduledRelayedReadOutcome::Miss:       ++readout.misses;      break;
            case ScheduledRelayedReadOutcome::VerifyFail: ++readout.verifyFails; break;
            case ScheduledRelayedReadOutcome::NoProbe:    ++readout.noProbes;    break;
            }

            // ⛔G-12  docs/SimulationManagerUImpl-guards.md
            if (!anyObservation || observation->simTick > newestSimTick)
            {
                anyObservation       = true;
                newestSimTick        = observation->simTick;
                readout.dLatestKnown = observation->outcome != ScheduledRelayedReadOutcome::NoProbe;
                readout.dLatest      = static_cast<uint32_t>(observation->dLatest);
            }
        }

        readout.present = anyObservation;
        return readout;
    }

    void gatherNearestCandidates(
        brawlerInputHistoryVisualization::NearestCharacterCandidateList& out) const
    {
        m_storage.forEachSimulatable<SimulatableBrawler>(
            [&out](unsigned int id, const auto& simulatable)
            {
                // ⛔G-13  docs/SimulationManagerUImpl-guards.md
                const auto& movement = simulatable.getVizState().getState()
                    .template get<brawlerMovementSimulation::State>();

                out.add(brawlerInputHistoryVisualization::NearestCharacterCandidate{
                    id, movement.bodyState.position.x, movement.bodyState.position.y });
            });
    }

    const brawlerInputHistoryVisualization::InputHistoryTickLanes* getInputHistoryLanes(
        unsigned int id) const
    {
        return m_inputHistory.findLanes(id);
    }

    const TimeConfig* getTimeConfigPtr() const
    {
        return m_manager.has_value() ? &m_manager->getTimeConfig() : nullptr;
    }

    void publishClientEffectiveInputDelayTicks(int32 delayTicks)
    {
        m_inputResolution.setClientEffectiveInputDelayTicks(delayTicks);
    }

    int32 getClientEffectiveInputDelayTicks() const
    {
        return m_inputResolution.getClientEffectiveInputDelayTicks();
    }

    virtual void onConnectionTierReceived(uint8_t oldTier, uint8_t newTier) override;

    virtual void onConnectionTierReplayed(uint8_t tier) override;

    virtual void onInputRelayHostReady(ASimulationInputRelay& host) override;

    const ReplicatedTierConsumer* getReplicatedTierConsumer() const
    {
        return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
    }

    virtual void onRelayDelayFloorReceived(uint8_t floorTicks) override;

    virtual void onRelayDelayFloorReplayed(uint8_t floorTicks) override;

    using BrawlerReceptionCoordinator =
        ServerReceptionCoordinator<FUEConnectionHandle, SimulatableBrawler>;
    bool hasServerTierWiring() const { return m_receptionCoordinator.has_value(); }

    BrawlerReceptionCoordinator* getReceptionCoordinator()
    {
        return m_receptionCoordinator.has_value() ? &*m_receptionCoordinator : nullptr;
    }

    int32 getServerReceptionTick() const
    {
        return static_cast<int32>(getServerClock().getSimulationStep().getTick());
    }

    void noteDelayedInputComponent(unsigned int id, USimmableUpdateComponent& component);

    void deliverRemoteInput(unsigned int id, uint32 captureTick,
                            const simulatableBrawler::PlayerInput& input);

    void relayRemoteInput(unsigned int id, uint32 captureTick, uint8 dA,
                          const simulatableBrawler::PlayerInput& input);

    // ∴D-01  docs/SimulationManagerUImpl-rationale.md
    static constexpr int32 kPreDietCharacterCap = 4;
    static_assert(static_cast<uint32>(kPreDietCharacterCap) <= brawlerRingout::kMaxSpawnPoints,
        "kPreDietCharacterCap must not exceed brawlerRingout::kMaxSpawnPoints. The cap only WARNS - "
        "it allocates nothing - while the ring-out spawn table has exactly kMaxSpawnPoints entries, so "
        "a character registered past the table gets kNoFreeSlot and respawns with NO teleport seed (one "
        "[Warning][Ringout.spawnSlot] per respawn). A cap above the table stops warning about exactly the "
        "characters that cannot be placed: raise both together. Was the 'COUPLED TO "
        "brawlerRingout::kMaxSpawnPoints' prose fence of SimulationManagerUImpl.h (task 11).");

private:
    std::set<unsigned int> m_authorityRegisteredIds;

    brawlerRingout::SpawnSlotAllocator m_spawnSlots;

    // ⛔G-14  docs/SimulationManagerUImpl-guards.md
    void seedRingoutSpawnPointsFromLevel(UWorld& world);

    bool m_ringoutSpawnPointsSeeded = false;

    void releaseDelayedInputsForStep(int32 physicsStep, int32 numSteps);

    void applyReplicatedConnectionTier(uint8 tier);

    void applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease);

    // ⛔G-15  docs/SimulationManagerUImpl-guards.md
    void logRelayDelayFloorAdvisory(int32 floorTicks);

    // ⛔G-16  docs/SimulationManagerUImpl-guards.md
    int32 recomputeAndPublishEffectiveInputDelay();

    void applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier);

    std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

    int32 m_lastPublishedEffectiveInputDelayTicks = 0;

    std::optional<BrawlerReceptionCoordinator> m_receptionCoordinator;

    FrameHealthProbe m_frameHealthProbe;

    // ⛔G-17  docs/SimulationManagerUImpl-guards.md
    RelayWriteProbe       m_relayWriteProbe{
        RelayStageCapacity{ static_cast<uint32>(relayedInputRing::kMaxDepth) } };
    ConnectionBudgetProbe m_connectionBudgetProbe;

    std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>
        m_delayedInputComponentsById;

    inputHistoryVisualizationUImpl::InputHistoryStore m_inputHistory;

    FSimulationManagerAsyncCallback* m_asyncCallback;

    FDelegateHandle m_injectInputsExternalCallbackHandle;
    FDelegateHandle m_hysScenePostTickCallbackHandle;

    std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

    ASimulationTimingRelay* m_timingRelay = nullptr;
    ASimulationTimingRelay* findTimingRelay();

    std::optional<ChaosPhysicsBodyAdapter>   m_physAdapter;
    std::optional<ChaosPhysicsBodyReaderAdapter> m_physReaderAdapter;
    std::optional<ChaosSpatialQueryAdapter>  m_queryAdapter;

    static FMovementStaticDataCVars readMovementStaticDataCVars();

    // ⛔G-18  docs/SimulationManagerUImpl-guards.md
    using BrawlerSimulatables    = SimulatableList<SimulatableBrawler>;
    using BrawlerStorage         = apply_t<SimulationObjectStorage,     BrawlerSimulatables>;
    using BrawlerNetSync         = apply_t<SimulationNetSync,           BrawlerSimulatables>;
    using BrawlerInputResolution = apply_t<SimulationInputResolution,   BrawlerSimulatables>;
    using BrawlerReconciliation  = apply_t<SimulationReconciliation,    BrawlerSimulatables>;

    BrawlerStorage m_storage;

    const FMovementStaticDataCVars m_movementStaticDataCVars = readMovementStaticDataCVars();

    simulatableBrawler::StaticData m_staticData{
        m_movementStaticDataCVars.model,
        m_movementStaticDataCVars.maxWalkSpeed,
        m_movementStaticDataCVars.stepPeriodTicks,
        m_movementStaticDataCVars.stepSpeed,
        m_movementStaticDataCVars.gravity };

    BrawlerReconciliation  m_reconciliation{ m_storage };
    BrawlerInputResolution m_inputResolution{ m_storage, m_reconciliation };
    BrawlerNetSync         m_netSync{ m_storage, m_reconciliation, m_inputResolution };

    template <typename... SimulatableTs>
    using BrawlerIntegrationExecFor_UE = SimulationIntegrationExecutor<
        simulatableBrawler::StaticData, ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter, SimulatableTs...>;
    using BrawlerIntegrationExec = apply_t<BrawlerIntegrationExecFor_UE, BrawlerSimulatables>;

    using BrawlerHitDetectionSystem =
        brawlerHitDetection::System<ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter>;

    using BrawlerSystemsExec = SimulationSystemsExecutor<
        BrawlerSimulatables,
        simulatableBrawler::StaticData,
        BrawlerHitDetectionSystem,
        brawlerHitRouting::System,
        brawlerRingout::ScoreSystem>;
    static_assert(brawlerHitDetection::firesBefore<BrawlerSystemsExec,
                      BrawlerHitDetectionSystem, brawlerHitRouting::System>,
        "BrawlerSystemsExec must fire brawlerHitDetection::System BEFORE brawlerHitRouting::System "
        "(firing order is template order): both run in preIntegrate, and routing branch 2 reads "
        "the hitsThisTick the detector writes earlier in the SAME pass, branch 5 the guard block. "
        "Reversed, routing reads them after the radial's integrate cleared them and before the "
        "detector refilled them, so NO melee hit and NO guard block is ever routed - "
        "og-netcode-v2-field-defects tasks 9 and 20.");

    // ⛔G-75  docs/SimulationManagerUImpl-guards.md
    std::optional<BrawlerSystemsExec> m_systemsExec;

    using IntegrationLayerType = BrawlerIntegrationExec;
    std::optional<IntegrationLayerType> m_integrationLayer;

    using ManagerType = SimulationManager<
        BrawlerIntegrationExec, BrawlerNetSync, BrawlerInputResolution, BrawlerReconciliation,
        BrawlerSystemsExec, BrawlerStorage, simulatableBrawler::StaticData>;
    std::optional<ManagerType> m_manager;

    static ASimulationManagerUImpl* s_instances[2];

    ChaosTickMapper m_chaosTickMapper;

    struct PendingRegistration
    {
        std::optional<SimulatableBrawler> simulatable;
        bool bodiesCreated = false;
        BrawlerInputProviderFn inputProvider;
        bool isAuthority = false;
    };
    std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;

    static constexpr bool compositionContractsHold()
    {
        static_assert(__builtin_offsetof(ASimulationManagerUImpl, m_storage)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_movementStaticDataCVars)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_movementStaticDataCVars)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_staticData)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_staticData)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_reconciliation)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_reconciliation)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_inputResolution)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_inputResolution)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_netSync),
            "ASimulationManagerUImpl members construct in DECLARATION order, and four of these read a "
            "sibling declared above them in their own initializer: m_staticData reads "
            "m_movementStaticDataCVars; m_reconciliation binds m_storage; m_inputResolution binds "
            "m_storage and m_reconciliation; m_netSync binds all three peers above it. A reorder "
            "compiles (no -Wreorder-as-error in this tree) and constructs in the new, wrong order. "
            "Keep m_storage < m_movementStaticDataCVars < m_staticData < m_reconciliation < "
            "m_inputResolution < m_netSync. Was the CONSTRUCTION ORDER prose fence of "
            "SimulationManagerUImpl.h (task 11).");
        static_assert(std::is_same_v<decltype(m_authorityRegisteredIds), std::set<unsigned int>>,
            "m_authorityRegisteredIds is the pre-diet cap's denominator and must be a SET, not a counter: "
            "tryRegister inserts only on the Ready path, while unregisterFromNewFramework erases for any "
            "component ending play - one abandoned mid-Pending included - so a counter drifts downward and "
            "disarms the cap. Was the 'A SET, not a counter' prose fence of SimulationManagerUImpl.h "
            "(task 11).");
        return true;
    }

    void assertCompositionContracts()
    {
        static_assert(compositionContractsHold());
    }
};

static_assert(std::is_same_v<decltype(&ASimulationManagerUImpl::getRingoutVizState),
        std::optional<brawlerRingout::State> (ASimulationManagerUImpl::*)(unsigned int) const>,
    "ASimulationManagerUImpl::getRingoutVizState must stay a CONST member returning the slice BY VALUE. "
    "The scoreboard holds a const ASimulationManagerUImpl*, so 'the scoreboard writes no simulation "
    "state' is a compile error to break; a reference would hand a drawing surface a pointer into live "
    "storage for as long as it cared to keep it. Was the 'const, AND BY VALUE' prose fence of "
    "SimulationManagerUImpl.h (task 11).");

static_assert(std::is_same_v<decltype(&ASimulationManagerUImpl::getTimeConfigPtr),
        const TimeConfig* (ASimulationManagerUImpl::*)() const>,
    "ASimulationManagerUImpl::getTimeConfigPtr returns a POINTER so that the pre-BeginPlay state - no "
    "manager, therefore no TimeConfig - is a representable nullptr rather than a reference into an "
    "empty optional. Callers bind lazily and retry. Was the 'Pointer, so pre-construction is not UB' "
    "prose fence of SimulationManagerUImpl.h (task 11).");