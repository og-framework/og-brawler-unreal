// SPDX-License-Identifier: BUSL-1.1

#pragma once

// =============================================================================
// THE UE SIDE OF THE INPUT-HISTORY DISPLAY -- ownership, selection and the poll.
// =============================================================================
// It owns one InputHistoryRowRing AND one pair of per-tick lanes PER CHARACTER ID and
// feeds them; the folding, the join and the classification are all pure og-brawler code
// reached through BrawlerInputHistoryVisualizationPoll.h. This layer supplies only what UE
// knows: which character, which tick, and the adapters onto the peers' diagnostic seams.
//
// KEYED ON CHARACTER ID, NEVER ON THE CONNECTION. Couch co-op shares one
// UNetConnection across local players, and a listen-server host's local player has
// none at all, so a connection-keyed store gives every sibling the same history or
// fails outright on the host. The key is the same
// `(unsigned int)USimmableUpdateComponent::GetUniqueID()` the registration uses.
//
// SINGLE-CHARACTER IS A SELECTION, NOT A STRUCTURE. `firstLocalCharacterId` picks
// one; the store behind it is a map, so a second local character gets its own
// independent ring by being polled, with nothing here reshaped.
//
// READS ONLY. Nothing in this file writes simulation state, and nothing it folds
// is replicated, enters a correction payload, or reaches compute_checksum.
// =============================================================================

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

// THE MASTER GATE -- `OGBrawler.InputHistory`, DEFAULT OFF. Read ALONE, once, by the
// poll's early-out; every accessor below also folds it in, so no call site can forget it.
bool masterEnabled();

// The one switch for the ROW PANEL -- `OGBrawler.InputHistoryDisplay`, DEFAULT ON.
// ⛔ FOLDS THE MASTER IN: false whenever the master is off, whatever this reads.
bool displayEnabled();

// The one switch for the PROVENANCE BAR -- `OGBrawler.InputHistoryProvenance`, DEFAULT ON.
// ⛔ FOLDS THE MASTER IN, same as every accessor on this page.
bool provenanceEnabled();

// The one switch for the INPUT-DELAY BAR -- `OGBrawler.InputHistoryInputDelay`, DEFAULT ON.
// Adds the per-tick server-lag verdict bar and its decomposition readout under the meter.
bool inputDelayEnabled();

// The one switch for the CHARACTER-STATE BAR -- `OGBrawler.InputHistoryCharacterState`,
// DEFAULT ON.
bool characterStateEnabled();

// The one switch for the RELAY-HEALTH BAR -- `OGBrawler.InputHistoryRelayHealth`,
// DEFAULT ON. ⛔ FOLDS THE MASTER IN, same as every accessor on this page.
bool relayHealthEnabled();

// The bar toggles folded into the pure header's own selection type -- the only shape
// `frameMeterEnabledBarCount` / `frameMeterBarSlotOf` accept.
//
// `isNearestStack` is the one thing the toggles alone cannot say: the relay-health bar
// belongs to the stack following someone else's character and to no other, so the answer
// differs per stack. ⛔ THE ONLY PLACE THAT TIE IS MADE.
brawlerInputHistoryVisualization::FrameMeterBarSelection barSelection(bool isNearestStack);

// True iff any bar is on. ⛔ THE LANE POLL'S OWN GATE, asked instead of ORing the three
// bar accessors at the call site: zero bars on needs no lane data at all.
bool anyBarEnabled();

// How many of the lanes' 240 stored ticks are retained for reading -- the console
// variable `OGBrawler.InputHistoryLaneTicks`, DEFAULT 120, clamped to [1, 240] here.
//
// ⛔ NOTHING IS REALLOCATED WHEN THIS CHANGES: the lanes are always 240 ticks wide.
uint32_t retainedLaneTicks();

// The one switch for the LANE PAUSE -- `OGBrawler.InputHistoryPauseIdle`, DEFAULT ON.
// While on, the lanes stop recording once the player has been idle for a short run, so
// the retained window holds activity rather than a wall of Idle.
//
// ⚠ ON, IT ELIDES: a correction or resimulation landing while idle is never recorded.
// Turning it off restores full-fidelity recording, which is how that is investigated.
bool pauseLanesWhileIdle();

// ---------------------------------------------------------------------------
// THE PANEL'S THREE LOOK KNOBS. All three are read once per drawn frame, all three
// are CLAMPED HERE rather than at the console, and none of them resizes anything --
// the ring is 64 rows whatever the window says, and the layout is a per-frame value.
// ⭐ THEY EXIST SO THAT TUNING THE DISPLAY IS A CONSOLE LINE AND NOT A REBUILD.
// ---------------------------------------------------------------------------

// `OGBrawler.InputHistoryPanelScale`, DEFAULT 1.0, clamped to [0.25, 4]. The ONE factor
// behind both the panel's geometry and its text; a value outside the range is pulled to
// the nearer end, and a non-number lands on the minimum.
float panelScale();

// `OGBrawler.InputHistoryPanelAlpha`, DEFAULT 0, clamped to [0, 1]. The BACKGROUND's
// opacity only: 0 leaves the rows drawn over a bare scene, 1 hides the scene entirely.
// ⛔ AT 0 NO BACKDROP IS DRAWN AT ALL -- an invisible rect is still a draw call.
float panelBackgroundAlpha();

// `OGBrawler.InputHistoryPanelRows`, DEFAULT 24, clamped to [1, 64]. How many of the
// newest rows the panel reserves height for and draws.
//
// ⛔ A READ BOUND, NOT AN ALLOCATION: the ring always holds 64 rows.
std::size_t panelVisibleRows();

// The first-joined local player's controller, or nullptr when there is none. The
// panel asks so that ONE local player's HUD draws, not every sibling's.
APlayerController* firstLocalPlayerController(const UWorld* world);

// The first-joined local player's character id, or nullopt when there is none to
// have -- a dedicated server, or a client before its pawn has been possessed.
std::optional<unsigned int> firstLocalCharacterId(const UWorld* world);

// The one switch for the SECOND STACK -- `OGBrawler.InputHistoryNearest`, DEFAULT ON.
// Off, the meter draws exactly the one stack it drew before this existed.
// ⛔ FOLDS THE MASTER IN, same as every accessor on this page.
bool nearestStackEnabled();

// The brawler nearest `localId`, or nullopt when the world holds no other -- the
// character the second stack draws.
//
// `previousChoice` is last frame's answer, held through the pure selector's own
// hysteresis so two candidates at nearly equal range do not swap the stack every frame.
//
// `outDistanceCm` is the range the answer was chosen at, and is left untouched when
// there is no answer. The distance comes off the SAME gather the choice was made from,
// so a header cannot report a range the choice was not made at.
// ⛔ AN OUT-PARAM RATHER THAN A SECOND CALL.
// ⛔ THE GATHER IS THE MANAGER'S AND THE CHOICE IS THE PURE HEADER'S. This function
//   owns neither; it is the one place they are put together.
std::optional<unsigned int> nearestCharacterIdTo(const ASimulationManagerUImpl* manager,
                                                 unsigned int                   localId,
                                                 std::optional<unsigned int>    previousChoice,
                                                 float&                         outDistanceCm);

// ---------------------------------------------------------------------------
// THE TWO DIAGNOSTIC SEAMS BEHIND ONE READER, both asked at the SAME simulation tick.
//
// Asking both at one tick is what licenses the join's rule that an observation naming
// no capture speaks for the tick it was asked about.
// ⭐ SAME-TICK IS THE LOAD-BEARING PART.
//
// Templated on the peer types so this header names no simulation peer of its own; the
// composition root binds `SimulatableT` where it already knows it.
// ---------------------------------------------------------------------------
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

	// ⛔ SAME-THREAD, NOT A CROSSING -- the cache map is mutated on the GAME THREAD alone
	// (createCacheFor/removeCacheFor); a presence test, not the input read the fence bars.
	bool hasCorrectionCache() const
	{
		return m_reconciliation.template findCorrectionCache<SimulatableT>(m_id) != nullptr;
	}

private:
	// A BORROWED reference, alive for one poll: the reader is built at the call site.
	const ReconciliationT& m_reconciliation;
	unsigned int           m_id;
};

template <typename SimulatableT, typename ReconciliationT>
ReconciliationSlotReader<SimulatableT, ReconciliationT> makeReconciliationSlotReader(
	const ReconciliationT& reconciliation, unsigned int id)
{
	return ReconciliationSlotReader<SimulatableT, ReconciliationT>(reconciliation, id);
}

// ---------------------------------------------------------------------------
// One row ring AND one pair of per-tick lanes per character id, created on that
// character's first poll and dropped by `forgetCharacter`, which the manager calls from
// its unregister contract. State left behind would be a leak keyed on an unreachable id.
// ---------------------------------------------------------------------------
class InputHistoryStore
{
public:
	using Ring  = brawlerInputHistoryVisualization::InputHistoryRowRing;
	using Lanes = brawlerInputHistoryVisualization::InputHistoryTickLanes;

	// One render-rate sweep of `id`'s resident capture window into its own ring.
	brawlerInputHistoryVisualization::InputHistoryPollCounts poll(
		unsigned int                                           id,
		const LocalInputCache<simulatableBrawler::PlayerInput>& captures,
		uint32_t                                               newestTick,
		float                                                  deadzone)
	{
		// ⛔ THE ACCEPTED TEAR MUST STAY A VALUE TEAR. A capture owning a pointer or an
		// allocation would make a torn game-thread read a crash, not a wrong glyph.
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

	// One sweep of `id`'s resident correction window into its provenance lane, plus the
	// single live machine-state sample the lane poll can take this tick.
	//
	// `liveInput` is the panel's own classification of this tick's capture, and nullopt
	// when the caller had none to read. ⛔ THE GATE, NOT THIS LAYER, DECIDES ON IT.
	//
	// `predictionOffsetTicks` is the estimator's offset, or nullopt on a role that does
	// not predict. ⛔ THE POLL PAIRS IT WITH `liveSimTick`, so the authority marker and
	//   the lane axis it is measured against are one snapshot rather than two reads.
	//
	// `delay` is this poll's input-delay decomposition, or nullopt when the delay display
	// is not being fed. Paired with `liveSimTick` the same way, for the same reason.
	//
	// `clock` is this poll's ONE read of the client clock, or nullopt on a role that does
	// not predict. Paired with `liveSimTick` the same way, for the same reason.
	//
	// `remoteObservations` is the relayed reads this client served for a REMOTE
	// character, and supplies the delay lane's client half for it. `remoteArrivals` says
	// when each relayed capture finally got here and `rollbackWindowTicks` how far back a
	// resim may still reach -- together, the relay-health lane. A locally controlled
	// character passes none of the three and keeps the delay line it always had.
	// ⛔ THE SOURCES ARE SELECTED BY TYPE -- the pure poll's own fence, so this
	//   layer forwards a choice rather than making a second one.
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
		// ⛔ SCRATCH, REBUILT PER POLL: a kept inversion would outlive the slots it describes.
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

	// The folded rows for `id`, or nullptr when that character has never been polled.
	const Ring* findRows(unsigned int id) const
	{
		const auto it = m_byId.find(id);
		return (it == m_byId.end()) ? nullptr : &it->second.rows;
	}

	// The per-tick lanes for `id`, or nullptr when that character has never been polled.
	const Lanes* findLanes(unsigned int id) const
	{
		const auto it = m_byId.find(id);
		return (it == m_byId.end()) ? nullptr : &it->second.lanes;
	}

	// Mirrors the peer's own unregisterCharacter erase of that id's capture line.
	void forgetCharacter(unsigned int id) { m_byId.erase(id); }

	std::size_t characterCount() const { return m_byId.size(); }

private:
	// ONE key for both stores, so nothing can survive an unregister the other honoured.
	struct CharacterHistory
	{
		Ring  rows;
		Lanes lanes;
	};

	std::unordered_map<unsigned int, CharacterHistory> m_byId;
};

} // namespace inputHistoryVisualizationUImpl
