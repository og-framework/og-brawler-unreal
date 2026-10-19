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
#include <memory>
#include <set>
#include <iterator>
#include <array>
#include <cstddef>
#include "Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulationLogRoutes.h"
#include "OGSimulation/RenderSnapshot.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGBrawlerUnreal/PhysicsBackendHostUImpl.h"
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
#include "OGBrawlerUnreal/LatencyBudgetDiagnosticsUImpl.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitDetectionSystem.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"
#include "OGBrawler/SimCharacterId.h"
#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGBrawler/BrawlerRingoutScoreSystem.h"

#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"
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

enum class OGLogRoute : uint8
{
	SimTick, ResimProbe, Sim, Net, RelayProbe, DivergenceProbe, Mgmt
};

struct OGLogRouteEntry
{
	const TCHAR*        prefix;
	OGLogRoute          route;
	ELogVerbosity::Type plainLevel;
};

inline constexpr OGLogRouteEntry kOGGameLogRoutes[] = {
	{ TEXT("[ResimProbe"),                       OGLogRoute::ResimProbe,      ELogVerbosity::Log },
	{ TEXT("[ResimCheck.TriggerRewind]"),        OGLogRoute::SimTick,         ELogVerbosity::Verbose },
	{ TEXT("[ClientPrediction]"),                OGLogRoute::SimTick,         ELogVerbosity::Verbose },
	{ TEXT("[ServerReceive]"),                   OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[SendRemoteInputToClients]"),        OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ReceiveCorrectionState]"),          OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[ReceiveCorrectionInput]"),          OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[InjectCorrectionInput]"),           OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[DrainOutOfOrder]"),                 OGLogRoute::Net,             ELogVerbosity::Log },
	{ TEXT("[ReleaseBatch]"),                    OGLogRoute::Net,             ELogVerbosity::Verbose },
	{ TEXT("[RelayProbe"),                       OGLogRoute::RelayProbe,      ELogVerbosity::Log },
	{ TEXT("SimulationManager:"),                OGLogRoute::Mgmt,            ELogVerbosity::Log },
	{ TEXT("tryRegister:"),                      OGLogRoute::Mgmt,            ELogVerbosity::Log },
	{ TEXT("NewFramework:"),                     OGLogRoute::Mgmt,            ELogVerbosity::Log }
};

inline constexpr OGLogRoute kOGLogRouteUnmapped = static_cast<OGLogRoute>(0xFF);

constexpr OGLogRoute ogLogRouteOf(SimulationLogRoute route)
{
	switch (route)
	{
	case SimulationLogRoute::SimTick:         return OGLogRoute::SimTick;
	case SimulationLogRoute::Sim:             return OGLogRoute::Sim;
	case SimulationLogRoute::Net:             return OGLogRoute::Net;
	case SimulationLogRoute::DivergenceProbe: return OGLogRoute::DivergenceProbe;
	}
	return kOGLogRouteUnmapped;
}

constexpr ELogVerbosity::Type ogLogVerbosityOf(HostLogLevel level)
{
	switch (level)
	{
	case HostLogLevel::Verbose: return ELogVerbosity::Verbose;
	case HostLogLevel::Log:     return ELogVerbosity::Log;
	case HostLogLevel::Warning: return ELogVerbosity::Warning;
	}
	return ELogVerbosity::NoLogging;
}

constexpr std::size_t ogSimulationLogRoutePrefixLength(const char* prefix)
{
	std::size_t length = 0;
	while (prefix[length] != 0)
		++length;
	return length;
}

constexpr std::size_t ogLongestSimulationLogRoutePrefix()
{
	std::size_t longest = 0;
	for (const SimulationLogRouteEntry& entry : kSimulationLogRoutes)
	{
		if (ogSimulationLogRoutePrefixLength(entry.prefix) > longest)
			longest = ogSimulationLogRoutePrefixLength(entry.prefix);
	}
	return longest;
}

constexpr bool ogSimulationLogRoutePrefixesArePrintableAscii()
{
	for (const SimulationLogRouteEntry& entry : kSimulationLogRoutes)
	{
		for (const char* c = entry.prefix; *c != 0; ++c)
		{
			if (*c < 0x20 || *c > 0x7E)
				return false;
		}
	}
	return true;
}

static_assert(ogSimulationLogRoutePrefixesArePrintableAscii(),
	"kSimulationLogRoutes: a prefix holds a character outside printable ASCII. Each prefix is widened "
	"to TCHAR one char at a time, which is exact for ASCII only.");

struct OGSimulationLogRouteWidePrefix
{
	TCHAR text[ogLongestSimulationLogRoutePrefix() + 1];
};

constexpr std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> ogWidenSimulationLogRoutePrefixes()
{
	std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> wide{};
	for (std::size_t row = 0; row < std::size(kSimulationLogRoutes); ++row)
	{
		const char* prefix = kSimulationLogRoutes[row].prefix;
		for (std::size_t i = 0; prefix[i] != 0; ++i)
			wide[row].text[i] = static_cast<TCHAR>(prefix[i]);
	}
	return wide;
}

inline constexpr std::array<OGSimulationLogRouteWidePrefix, std::size(kSimulationLogRoutes)> kOGSimulationLogRoutePrefixes =
	ogWidenSimulationLogRoutePrefixes();

inline constexpr std::size_t kOGSimulationLogRouteCount = std::size(kSimulationLogRoutes);

constexpr std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGGameLogRoutes)> ogConcatenateLogRoutes()
{
	std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGGameLogRoutes)> routes{};
	for (std::size_t row = 0; row < kOGSimulationLogRouteCount; ++row)
	{
		routes[row] = { kOGSimulationLogRoutePrefixes[row].text,
			ogLogRouteOf(kSimulationLogRoutes[row].route),
			ogLogVerbosityOf(kSimulationLogRoutes[row].plainLevel) };
	}
	for (std::size_t row = 0; row < std::size(kOGGameLogRoutes); ++row)
		routes[kOGSimulationLogRouteCount + row] = kOGGameLogRoutes[row];
	return routes;
}

inline constexpr std::array<OGLogRouteEntry, kOGSimulationLogRouteCount + std::size(kOGGameLogRoutes)> kOGLogRoutes =
	ogConcatenateLogRoutes();

constexpr bool ogLogRoutesAreAllMapped()
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (entry.route == kOGLogRouteUnmapped)
			return false;
	}
	return true;
}

static_assert(ogLogRoutesAreAllMapped(),
	"kOGLogRoutes: an og-simulation row's SimulationLogRoute has no OGLogRoute in ogLogRouteOf, so "
	"RouteOGMessage would drop its lines. Map the new route to a LogOG* category.");

constexpr TCHAR ogLogRouteFoldCase(TCHAR c)
{
	return (c >= TEXT('A') && c <= TEXT('Z')) ? static_cast<TCHAR>(c - TEXT('A') + TEXT('a')) : c;
}

constexpr bool ogLogRouteStartsWith(const TCHAR* text, const TCHAR* prefix)
{
	for (; *prefix != 0; ++text, ++prefix)
	{
		if (*text == 0 || ogLogRouteFoldCase(*text) != ogLogRouteFoldCase(*prefix))
			return false;
	}
	return true;
}

constexpr bool ogLogRouteBlocksAreDisjoint()
{
	for (std::size_t simRow = 0; simRow < kOGSimulationLogRouteCount; ++simRow)
	{
		for (std::size_t gameRow = kOGSimulationLogRouteCount; gameRow < std::size(kOGLogRoutes); ++gameRow)
		{
			if (ogLogRouteStartsWith(kOGLogRoutes[simRow].prefix, kOGLogRoutes[gameRow].prefix)
				|| ogLogRouteStartsWith(kOGLogRoutes[gameRow].prefix, kOGLogRoutes[simRow].prefix))
				return false;
		}
	}
	return true;
}

static_assert(ogLogRouteBlocksAreDisjoint(),
	"kOGLogRoutes: a prefix in the og-simulation block and a prefix in this module's block BEGIN WITH "
	"one another (compared ignoring case, as FString::StartsWith compares), so one line can match a "
	"row in each block and the block order decides its route. The og-simulation block comes first only "
	"because no line can match both; give the new row a tag no prefix in the other block begins with.");

constexpr bool ogLogRoutesAreAllReachable()
{
	for (std::size_t later = 0; later < std::size(kOGLogRoutes); ++later)
	{
		for (std::size_t earlier = 0; earlier < later; ++earlier)
		{
			if (ogLogRouteStartsWith(kOGLogRoutes[later].prefix, kOGLogRoutes[earlier].prefix))
				return false;
		}
	}
	return true;
}
static_assert(ogLogRoutesAreAllReachable(),
	"RouteOGMessage: a route's prefix BEGINS WITH an earlier route's prefix, so the earlier "
	"route always wins and the later one can never be taken. FString::StartsWith ignores "
	"case and so does this check. Was the ORDER IS LOAD-BEARING fence: [Resim.Input] must be "
	"matched ahead of the [Resim. catch-all, and widening that catch-all to [Resim would "
	"swallow every [ResimCheck. line.");

constexpr bool ogLogRoutePlainLevelsAreLogOrVerbose()
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (entry.plainLevel != ELogVerbosity::Log && entry.plainLevel != ELogVerbosity::Verbose)
			return false;
	}
	return true;
}

static_assert(ogLogRoutePlainLevelsAreLogOrVerbose(),
	"kOGLogRoutes: a row's plainLevel is the level of a line that carries no [Verbose] or "
	"[Warning] token, and it may only be Log (rare events) or Verbose (per tick, per packet, "
	"per relay). A line that must reach Warning says so with its own [Warning] token.");

constexpr const OGLogRouteEntry* ogLogRouteFor(const TCHAR* line)
{
	for (const OGLogRouteEntry& entry : kOGLogRoutes)
	{
		if (ogLogRouteStartsWith(line, entry.prefix))
			return &entry;
	}
	return nullptr;
}

#define OG_LOG_ROUTED(Category, Format, ...) \
	do \
	{ \
		constexpr const OGLogRouteEntry* ogLogRouteEntryForSite = ogLogRouteFor(Format); \
		static_assert(ogLogRouteEntryForSite != nullptr, \
			"OG_LOG_ROUTED: the format's leading tag has no kOGLogRoutes row, so it has no level. " \
			"Add a row (prefix, route, plainLevel) or log this line with UE_LOG at a fixed level."); \
		if constexpr (ogLogRouteEntryForSite->plainLevel == ELogVerbosity::Verbose) \
		{ \
			UE_LOG(Category, Verbose, Format, ##__VA_ARGS__); \
		} \
		else \
		{ \
			UE_LOG(Category, Log, Format, ##__VA_ARGS__); \
		} \
	} while (0)

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
class AOGBrawlerUECharacter;

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
        editLatencyBudgetDiagnostics().noteWireSample(roundTripTime);
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

    const LatencyBudgetDiagnosticsUImpl& getLatencyBudgetDiagnostics() const { return m_latencyBudgetDiagnostics; }
    LatencyBudgetDiagnosticsUImpl&       editLatencyBudgetDiagnostics()       { return m_latencyBudgetDiagnostics; }
    void OnPostPhysicsStep();

    SimCharacterId allocateSimCharacterId();

    TryRegisterStatus tryRegister(
        SimCharacterId simId,
        SimulatableBrawler simulatable,
        USimmableUpdateComponent& owner,
        BrawlerInputProviderFn inputProvider,
        bool isAuthority);

    void unregisterFromNewFramework(SimCharacterId simId, USimmableUpdateComponent& owner, bool isAuthority);

    EngineTraits::VizQuery&        editVizQuery();
    const EngineTraits::VizReader& getVizReader() const;
    std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

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

    void noteDelayedInputComponent(SimCharacterId simId, USimmableUpdateComponent& component);

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
    friend physicsBackendUImpl::Backend;
    template <typename SessionT> friend class BrawlerTickDriverGameUImpl;
    friend class LatencyBudgetDiagnosticsUImpl;

    std::set<unsigned int> m_authorityRegisteredIds;

    brawlerRingout::SpawnSlotAllocator m_spawnSlots;

    SimCharacterIdAllocator m_simCharacterIds;

    // ⛔G-14  docs/SimulationManagerUImpl-guards.md
    void seedRingoutSpawnPointsFromLevel(UWorld& world);

    bool m_ringoutSpawnPointsSeeded = false;

    void onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps);

    void releaseDelayedInputsForStep(int32 firstTick, int32 numSteps);

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

    LatencyBudgetDiagnosticsUImpl m_latencyBudgetDiagnostics{ std::as_const(*this) };

    physicsBackendUImpl::BackendWorlds m_backendWorlds;

    std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

    ASimulationTimingRelay* m_timingRelay = nullptr;
    ASimulationTimingRelay* findTimingRelay();

    std::optional<EngineTraits::BodyAdapter>   m_physAdapter;
    std::optional<EngineTraits::ReaderAdapter> m_physReaderAdapter;
    std::optional<EngineTraits::QueryAdapter>  m_queryAdapter;

    static FMovementStaticDataCVars readMovementStaticDataCVars();

    using BrawlerSimulatables    = BrawlerCompositionUImpl::Simulatables;
    using BrawlerStorage         = BrawlerCompositionUImpl::Storage;
    using BrawlerNetSync         = BrawlerCompositionUImpl::NetSync;
    using BrawlerInputResolution = BrawlerCompositionUImpl::InputResolution;
    using BrawlerReconciliation  = BrawlerCompositionUImpl::Reconciliation;
    using RenderSnapshot         = BrawlerCompositionUImpl::RenderSnapshot;

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

    template <typename... SimulatableTs> using BrawlerIntegrationExecFor_UE = BrawlerCompositionUImpl::IntegrationExecFor<SimulatableTs...>;
    using BrawlerIntegrationExec = BrawlerCompositionUImpl::IntegrationExec;

    using BrawlerHitDetectionSystem = BrawlerCompositionUImpl::HitDetectionSystem;

    using BrawlerSystemsExec = BrawlerCompositionUImpl::SystemsExec;

    // ⛔G-75  docs/SimulationManagerUImpl-guards.md
    std::optional<BrawlerSystemsExec> m_systemsExec;

    using IntegrationLayerType = BrawlerIntegrationExec;
    std::optional<IntegrationLayerType> m_integrationLayer;

    using ManagerType = BrawlerCompositionUImpl::Manager;
    std::optional<ManagerType> m_manager;
    physicsBackendUImpl::Backend m_backend{ *this, m_backendWorlds };

    static ASimulationManagerUImpl* s_instances[2];

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
        static_assert(std::is_same_v<ManagerType, BrawlerCompositionUImpl::Manager>,
            "ASimulationManagerUImpl::ManagerType must BE BrawlerCompositionUImpl::Manager. ManagerType is private, so "
            "a backend host declared outside this class (TickDriverBackendUImpl, task 83's Jolt host, whose tick driver's step driver is "
            "SimulationStepDriver<Manager, JoltWorld, TickDriverStepHooks<...>>) names the manager "
            "through BrawlerCompositionUImpl; a ManagerType that drifts from it hands that host a different "
            "SimulationManager instantiation than m_manager. Added by task 81.");
        static_assert(std::is_same_v<BrawlerInputProviderFn, LatencyBudgetDiagnosticsUImpl::InputProvider>,
            "tryRegister hands a client's BrawlerInputProviderFn to LatencyBudgetDiagnosticsUImpl::wrapInputProvider and "
            "registers what it returns. The two spellings must name ONE std::function type: a different one would "
            "convert, wrapping the provider in a second heap-allocated callable on every registration. Added by "
            "og-tickdriver task 18.");
        static_assert(__builtin_offsetof(ASimulationManagerUImpl, m_backendWorlds)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_physAdapter)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_physAdapter)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_physReaderAdapter)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_physAdapter)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_queryAdapter)
                   && __builtin_offsetof(ASimulationManagerUImpl, m_queryAdapter)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_integrationLayer),
            "ASimulationManagerUImpl members destruct in REVERSE declaration order. The backend's worlds must outlive "
            "every adapter bound to them (the Jolt step world; the reader holds the body adapter's bind table, the "
            "integration layer holds the adapters), so m_backendWorlds sits above the adapters in both configurations "
            "(it is empty on Chaos). Keep m_backendWorlds < m_physAdapter < {m_physReaderAdapter, m_queryAdapter} < "
            "m_integrationLayer. Was design D's compile-time row 'runtime < world < adapters' (task 18); its "
            "runtime < world half is og-simulation-jolt's JoltTickWorlds' own row (runtime < contactCounter < stepWorld) "
            "since og-tickdriver task 10, and was the Jolt host's worlds struct's from task 83.");
        static_assert(__builtin_offsetof(ASimulationManagerUImpl, m_manager)
                          < __builtin_offsetof(ASimulationManagerUImpl, m_backend),
            "ASimulationManagerUImpl members destruct in REVERSE declaration order. The backend holds the Jolt step "
            "driver, which unregisters its resync callback from the manager's client clock in its destructor, so the "
            "backend must go before the manager. Keep m_manager < m_backend. Was design D's compile-time row "
            "'manager < hooks < driver' (task 18); its hooks < driver half is og-simulation's SimulationTickDriver's "
            "own row, and the tick driver last is TickDriverBackendUImpl's, since og-tickdriver task 8.");
        static_assert(physicsBackendUImpl::PhysicsBackendHost<physicsBackendUImpl::Backend>,
            "physicsBackendUImpl::Backend must provide every entry point of the seam (the PhysicsBackendHost concept, "
            "PhysicsBackendHostUImpl.h): this class calls each one from exactly one site, unconditionally, in both "
            "configurations. Added by task 83.");
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