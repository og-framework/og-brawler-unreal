// SPDX-License-Identifier: BUSL-1.1

#pragma once

// =============================================================================
// AOGBrawlerUEHUD - THE PROJECT'S ONE SCREEN-SPACE DRAWING SURFACE.
// =============================================================================
// Before this class there was none: every on-screen diagnostic here is either a 3D
// DrawDebugHelpers call or an engine toast, and neither can lay out a vertical list
// of rows. AGameModeBase sets no HUDClass by default, so AOGBrawlerUEGameMode's
// constructor names this class -- one HUD class, one assignment, nothing else.
//
// Every layout number, colour and endpoint comes from pure og-brawler code, which is
// the part a Catch2 case can reach; this class holds only the canvas calls.
// ⛔ MINIMAL ON PURPOSE. It draws the two input-history displays and nothing else.
//
// READS ONLY. It borrows the row ring through a pointer to const and touches no
// simulation state; nothing it reads is written back anywhere.
// =============================================================================

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include <cstddef>
#include <optional>

#include "OGBrawler/BrawlerInputHistoryVisualizationBars.h"
#include "OGBrawler/BrawlerInputHistoryVisualizationPanel.h"

#include "OGBrawlerUEHUD.generated.h"

class ASimulationManagerUImpl;

UCLASS()
class AOGBrawlerUEHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	// The one selection both displays make: this HUD's own local player, that player's
	// character, and the role's manager. nullptr whenever any link is missing.
	// ⛔ HELD AS const, so "reads only" is a compile error to break rather than a promise.
	const ASimulationManagerUImpl* findHistorySource(unsigned int& outCharacterId) const;

	// The whole panel. Silently draws nothing when there is no local character, no
	// manager or no history yet -- every one of those is an ordinary frame.
	void drawInputHistoryPanel();

	// The meter, as a whole: which characters its stacks are about, and where each one
	// sits. ⛔ THE ONLY PLACE THE SECOND STACK'S EXISTENCE IS DECIDED.
	void drawInputHistoryFrameMeter();

	// ONE stack of bars, one cell per capture tick, newest at the right.
	//
	// `liftPixels` raises it off the meter's own bottom anchor so another stack can take
	// that anchor; 0 IS the anchor. `header` names the character the bars are about and
	// is drawn only when there are two stacks to tell apart -- one stack alone is the
	// display that shipped before this, down to the pixel.
	// The tier line becomes the relay line, and the clock -- which describes the axis
	// rather than a character -- is drawn on the primary alone.
	// ⛔ THE NEAREST STACK SWAPS TWO READOUTS, NEVER THE BARS.
	void drawInputHistoryFrameMeterStack(
		const ASimulationManagerUImpl&                                 manager,
		unsigned int                                                   characterId,
		float                                                          liftPixels,
		float                                                          labelHeight,
		const std::optional<brawlerInputHistoryVisualization::FrameMeterStackHeader>& header);

	// The one line that says which character a stack is about, in its label band.
	// ⛔ EVERY TOKEN COMES FROM THE HEADER MODEL. This formats a string; it reads nothing.
	void drawFrameMeterStackHeader(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&    geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&      layout,
		const brawlerInputHistoryVisualization::FrameMeterStackHeader& header);

	// The relay reading, where the primary stack shows its tier decomposition. Silently
	// draws nothing when the readout is not `present` -- this client has served no
	// relayed read for that character.
	// ⛔ EVERY TOKEN COMES FROM THE READOUT MODEL. This formats a string; it reads nothing.
	void drawFrameMeterRelayReadout(
		const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
		const brawlerInputHistoryVisualization::RelayReadReadout&   readout,
		const brawlerInputHistoryVisualization::RelayHealthReadout& health);

	// The small font's line height, measured once per drawn frame. Every band on both
	// stacks is placed from this ONE number.
	// ⛔ MEASURED ONCE: two measures can differ by a pixel and the stacks would overlap.
	float meterLabelHeight();

	// Last frame's nearest choice, so the pure selector's hysteresis has something to
	// hold. ⛔ A DRAWING-SIDE MEMORY OF A DRAWING-SIDE CHOICE -- nothing reads it back
	//   into the simulation, and it is dropped whenever the second stack is switched off.
	std::optional<unsigned int> m_nearestCharacterId;

	// One bar, from cells the pure header already read out of a lane. The palette arrives
	// as the lane's own style table, so this method names no colour of its own.
	// ⛔ `barSlot` PLACES IT; `kind` ONLY DECIDES RUN LABELS -- compaction lives in the
	//   pure header's `frameMeterBarSlotOf`, never recomputed here.
	void drawFrameMeterBar(const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
	                       const brawlerInputHistoryVisualization::FrameMeterBarCells& bar,
	                       uint32_t                                                    barSlot,
	                       brawlerInputHistoryVisualization::FrameMeterBarKind         kind,
	                       brawlerInputHistoryVisualization::LaneCellStyleOfOrdinal    styleOf);

	// Every cut in the lane axis inside `window` -- a collapsed idle span or a hard resync --
	// drawn across every enabled bar and labelled with what its own kind claims.
	// ⛔ ONE SWITCH ON `kind` DECIDES BOTH COLOUR AND LABEL, per mark, never per ledger.
	void drawFrameMeterAxisEvents(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&     geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&       layout,
		const brawlerInputHistoryVisualization::InputHistoryTickLanes&  lanes,
		const brawlerInputHistoryVisualization::PollWindow&             window);

	// Every rate correction the clock made inside `window` -- a short mark on a column
	// EDGE in the label band, with its signed glyph beside it. A skip and a stall own no
	// cell, so neither is drawn on one.
	// ⛔ THE GLYPH IS DERIVED FROM `kind`, never stored: one kind, one glyph, decided once.
	void drawFrameMeterRateMarks(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&     geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&       layout,
		const brawlerInputHistoryVisualization::InputHistoryTickLanes&  lanes,
		const brawlerInputHistoryVisualization::PollWindow&             window);

	// One vertical rule, from a style the pure header owns. Both markers that cross these
	// bars come through here, so neither can acquire a look of its own at the canvas.
	void drawFrameMeterRule(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&    geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&      layout,
		float                                                         x,
		const brawlerInputHistoryVisualization::FrameMeterMarkerStyle& style);

	// The authority marker: where the server is, plus the offset it sits behind by.
	// ⛔ AN OFF-BAR TARGET FLAGS AN EDGE and is never moved onto a column.
	void drawFrameMeterAuthorityMarker(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&        geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&          layout,
		const brawlerInputHistoryVisualization::FrameMeterAuthorityMarker& marker);

	// The delay decomposition, as one line of text below the offset label. Silently draws
	// nothing when the readout is not `present` -- the delay display is not being fed.
	// ⛔ EVERY TOKEN COMES FROM THE READOUT MODEL. This formats a string; it reads nothing.
	void drawFrameMeterDelayReadout(
		const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
		const brawlerInputHistoryVisualization::InputDelayReadout&  readout);

	// The residency reading, as one line of text below the bars -- drawn only from the
	// Provenance-slot branch, mirroring the delay readout's "drawn only when its bar is" rule.
	// ⛔ EVERY TOKEN COMES FROM THE READOUT MODEL. This formats a string; it reads nothing.
	void drawFrameMeterResidencyReadout(
		const brawlerInputHistoryVisualization::FrameMeterGeometry&           geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&             layout,
		const brawlerInputHistoryVisualization::ProvenanceResidencyReadout&   readout,
		uint32_t                                                              lineIndex);

	// The client clock, as one line of text below the bars. It describes the AXIS rather than
	// any one bar, so it is drawn from the frame meter itself and not from a bar's own branch.
	// ⛔ EVERY TOKEN COMES FROM THE READOUT MODEL. This formats a string; it reads nothing.
	void drawFrameMeterClockReadout(
		const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
		const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
		const brawlerInputHistoryVisualization::ClockDriftReadout&  readout,
		uint32_t                                                    lineIndex);

	void drawInputHistoryRow(const brawlerInputHistoryVisualization::PanelLayout& layout,
	                         const brawlerInputHistoryVisualization::InputHistoryRow& row,
	                         std::size_t slotFromTop);

	// ⛔ EVERY ENDPOINT COMES FROM THE PURE HEADER. This rotates nothing and decides
	//   no sign; it is three DrawLine calls, or one DrawRect for Neutral.
	void drawDirectionGlyph(const brawlerInputHistoryVisualization::PanelLayout& layout,
	                        float rowTopY,
	                        brawlerInputHistoryVisualization::DirectionBucket bucket,
	                        const FLinearColor& color);

	// The ring-out scoreboard [ringout task 6b]: every fighter's id, score and -- while
	// that fighter is out -- its respawn countdown, flush RIGHT and vertically centred.
	// Silently draws nothing when this is not the first local player's HUD, when there is
	// no manager, or when no character exists yet; each of those is an ordinary frame.
	// ⛔ EVERY LAYOUT NUMBER, THE ORDERING AND BOTH CLAMPS COME FROM PURE CODE. This holds
	//   the canvas calls and the gather, and decides no geometry of its own.
	void drawScoreboard();
};
