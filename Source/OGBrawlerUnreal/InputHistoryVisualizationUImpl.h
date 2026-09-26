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
class ASimulationManagerUImpl;
class UWorld;

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

std::optional<unsigned int> nearestCharacterIdTo(const ASimulationManagerUImpl* manager,
                                                 unsigned int                   localId,
                                                 std::optional<unsigned int>    previousChoice,
                                                 float&                         outDistanceCm);

template <typename SimulatableT, typename ReconciliationT>
class ReconciliationSlotReader
{
public:
	ReconciliationSlotReader(const ReconciliationT& reconciliation, unsigned int id)
		: m_reconciliation(reconciliation)
		, m_id(id)
	{
	}

	AppliedCaptureRef appliedCaptureRef(uint32_t simTick) const
	{
		return m_reconciliation.template getAppliedCaptureTickRef<SimulatableT>(m_id, simTick);
	}

	std::optional<SlotStateProvenance> slotProvenance(uint32_t simTick) const
	{
		return m_reconciliation.getDiagnostics()
			.template slotStateProvenance<SimulatableT>(m_id, simTick);
	}

	bool hasCorrectionCache() const
	{
		// ⛔G-06  docs/InputHistoryVisualizationUImpl-guards.md
		return m_reconciliation.template findCorrectionCache<SimulatableT>(m_id) != nullptr;
	}

private:
	const ReconciliationT& m_reconciliation;
	unsigned int           m_id;
};

template <typename SimulatableT, typename ReconciliationT>
ReconciliationSlotReader<SimulatableT, ReconciliationT> makeReconciliationSlotReader(
	const ReconciliationT& reconciliation, unsigned int id)
{
	return ReconciliationSlotReader<SimulatableT, ReconciliationT>(reconciliation, id);
}

class InputHistoryStore
{
public:
	using Ring  = brawlerInputHistoryVisualization::InputHistoryRowRing;
	using Lanes = brawlerInputHistoryVisualization::InputHistoryTickLanes;

	brawlerInputHistoryVisualization::InputHistoryPollCounts poll(
		unsigned int                                           id,
		const LocalInputCache<simulatableBrawler::PlayerInput>& captures,
		uint32_t                                               newestTick,
		float                                                  deadzone)
	{
		static_assert(std::is_trivially_destructible_v<simulatableBrawler::PlayerInput>,
			"A capture slot is read from the game thread while the physics thread may be "
			"writing it. A capture that owned memory would make that tear a crash rather "
			"than a wrong glyph, so the read must be re-argued before such a member lands.");
		static_assert(std::is_trivially_copyable_v<dAttackMachineSimulation::PlayerInput>,
			"The sub-input the display actually reads must stay plain values, for the same "
			"reason. (The COMPOSITE fails this trait through std::tuple alone, not through "
			"any member of its own -- std::tuple<int, float> fails it identically.)");

		const simulatableBrawler::DelayLineMotionHistory history(captures);

		return brawlerInputHistoryVisualization::pollInputHistory(
			history, newestTick, deadzone, m_byId[id].rows);
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
