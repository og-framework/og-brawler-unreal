// SPDX-License-Identifier: BUSL-1.1
// docs/InputHistoryDisplay-rationale.md · docs/InputHistoryVisualizationUImpl-guards.md

#pragma once

#include "CoreMinimal.h"

#include <cstddef>
#include <optional>
#include <type_traits>
#include <unordered_map>

#include "OGBrawler/BrawlerInputHistoryVisualizationBars.h"
#include "OGBrawler/BrawlerInputHistoryVisualizationPoll.h"

class APlayerController;
class BrawlerInputHistoryFeedUImpl;
class UWorld;
struct BrawlerSessionTraitsUImpl;
template <typename TraitsT> class UESimulationSession;

namespace inputHistoryVisualizationUImpl
{

bool masterEnabled();

bool displayEnabled();

bool provenanceEnabled();

bool inputDelayEnabled();

bool characterStateEnabled();

bool relayHealthEnabled();

brawlerInputHistoryVisualization::FrameMeterBarSelection barSelection(bool isNearestStack);

bool anyBarEnabled();

uint32_t retainedLaneTicks();

bool pauseLanesWhileIdle();

float panelScale();

float panelBackgroundAlpha();

std::size_t panelVisibleRows();

APlayerController* firstLocalPlayerController(const UWorld* world);

std::optional<unsigned int> firstLocalCharacterId(const UWorld* world);

bool nearestStackEnabled();

std::optional<unsigned int> nearestCharacterIdTo(const BrawlerInputHistoryFeedUImpl* feed,
                                                 unsigned int                        localId,
                                                 std::optional<unsigned int>         previousChoice,
                                                 float&                              outDistance);

class InputHistoryStore
{
public:
	using Ring  = brawlerInputHistoryVisualization::InputHistoryRowRing;
	using Lanes = brawlerInputHistoryVisualization::InputHistoryTickLanes;

	template <typename CaptureHistoryT>
	brawlerInputHistoryVisualization::InputHistoryPollCounts poll(
		unsigned int           id,
		const CaptureHistoryT& captures,
		uint32_t               newestTick,
		float                  deadzone)
	{
		return brawlerInputHistoryVisualization::pollInputHistory(
			captures, newestTick, deadzone, m_byId[id].rows);
	}

	template <typename SlotReader, typename RemoteObservationsT, typename RemoteArrivalsT>
	brawlerInputHistoryVisualization::TickLanePollCounts pollLanes(
		unsigned int                                                      id,
		const SlotReader&                                                 reader,
		uint32_t                                                          liveSimTick,
		DAttackState                                                      machineState,
		std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput,
		bool                                                              pauseWhileIdle,
		std::optional<uint32_t>                                           predictionOffsetTicks,
		std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition> delay,
		std::optional<brawlerInputHistoryVisualization::ClockDriftReading>        clock,
		const RemoteObservationsT&                                        remoteObservations,
		const RemoteArrivalsT&                                            remoteArrivals,
		uint32_t                                                          rollbackWindowTicks)
	{
		brawlerInputHistoryVisualization::AppliedCaptureInversion inversion;

		return brawlerInputHistoryVisualization::pollInputHistoryLanes(
			reader, liveSimTick, machineState, liveInput, pauseWhileIdle,
			predictionOffsetTicks, delay, clock, remoteObservations, remoteArrivals,
			rollbackWindowTicks, inversion, m_byId[id].lanes);
	}

	template <typename SlotReader>
	brawlerInputHistoryVisualization::TickLanePollCounts pollLanes(
		unsigned int                                                      id,
		const SlotReader&                                                 reader,
		uint32_t                                                          liveSimTick,
		DAttackState                                                      machineState,
		std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput,
		bool                                                              pauseWhileIdle,
		std::optional<uint32_t>                                           predictionOffsetTicks,
		std::optional<brawlerInputHistoryVisualization::InputDelayDecomposition> delay,
		std::optional<brawlerInputHistoryVisualization::ClockDriftReading>        clock)
	{
		return pollLanes(id, reader, liveSimTick, machineState, liveInput, pauseWhileIdle,
			predictionOffsetTicks, delay, clock,
			brawlerInputHistoryVisualization::NoRemoteDelayObservations{},
			brawlerInputHistoryVisualization::NoRemoteInputArrivals{}, 0u);
	}

	const Ring* findRows(unsigned int id) const
	{
		const auto it = m_byId.find(id);
		return (it == m_byId.end()) ? nullptr : &it->second.rows;
	}

	const Lanes* findLanes(unsigned int id) const
	{
		const auto it = m_byId.find(id);
		return (it == m_byId.end()) ? nullptr : &it->second.lanes;
	}

	void forgetCharacter(unsigned int id) { m_byId.erase(id); }

	std::size_t characterCount() const { return m_byId.size(); }

private:
	struct CharacterHistory
	{
		Ring  rows;
		Lanes lanes;
	};

	// ⛔G-04  docs/InputHistoryVisualizationUImpl-guards.md
	std::unordered_map<unsigned int, CharacterHistory> m_byId;
};

} // namespace inputHistoryVisualizationUImpl

class BrawlerInputHistoryFeedUImpl
{
public:
	using Session = UESimulationSession<BrawlerSessionTraitsUImpl>;

	explicit BrawlerInputHistoryFeedUImpl(const Session& session) : m_session(session) {}

	void pollInputHistory(unsigned int id, uint32 newestTick, float deadzone);

	const brawlerInputHistoryVisualization::InputHistoryRowRing* getInputHistoryRows(
		unsigned int id) const
	{
		return m_inputHistory.findRows(id);
	}

	void pollInputHistoryLanes(unsigned int id, uint32 liveTick, DAttackState machineState,
		std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput,
		bool pauseWhileIdle, bool includeDelay);

	brawlerInputHistoryVisualization::RelayReadReadout getRelayReadReadout(unsigned int id) const;

	void gatherNearestCandidates(
		brawlerInputHistoryVisualization::NearestCharacterCandidateList& out) const;

	const brawlerInputHistoryVisualization::InputHistoryTickLanes* getInputHistoryLanes(
		unsigned int id) const
	{
		return m_inputHistory.findLanes(id);
	}

	void forgetCharacter(unsigned int id) { m_inputHistory.forgetCharacter(id); }

private:
	const Session& m_session;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_session)>>,
		"BrawlerInputHistoryFeedUImpl reads production state through a CONST reference to the session only - the "
		"display snapshot (getDisplaySnapshot, findDisplayEntry, forEachDisplayVizState), the manager's role and time "
		"config (getManager), isLocallyControlledOnThisPeer and the tier consumer (getReplicatedTierConsumer) - and "
		"mutates nothing but its own store. A mutable reference here would be a write door from a diagnostic display "
		"into the session's mutable doors (editServerClock, editVizQuery; publishClientEffectiveInputDelayTicks until "
		"og-session-runtime task 24 inlined it into og-sim-session's SessionClock) and, "
		"through owner(), into the owner's. The pattern is UELatencyBudgetDiagnostics's (og-tickdriver task 18). Added by "
		"og-ue-simulationcomponentandmanagersplit task 22, when the input-history feeds left AOGBrawlerSimulationManager, "
		"on a const reference to the owner (its friend then); re-targeted to the session by task 24b; its reads are the "
		"display snapshot's since og-input-latency task P5-16, which also deleted the session's reconciliation door "
		"(this list since og-session-runtime task 16).");

	inputHistoryVisualizationUImpl::InputHistoryStore m_inputHistory;
};
