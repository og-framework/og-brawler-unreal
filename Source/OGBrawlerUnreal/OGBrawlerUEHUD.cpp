// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/OGBrawlerUEHUD.h"

#include <optional>

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"

namespace
{

using brawlerInputHistoryVisualization::PanelLayout;

// Held buttons are the one field a player reads without looking, so they get a colour
// of their own rather than sharing the row's.
const FLinearColor kHeldButtons(1.00f, 0.85f, 0.15f, 1.f);
const FLinearColor kIdleButtons(0.38f, 0.38f, 0.38f, 1.f);
const FLinearColor kRowFields(0.92f, 0.92f, 0.92f, 1.f);

// Black, and its ALPHA arrives from the cvar at the draw rather than living here.
const FLinearColor kPanelBackgroundInk(0.f, 0.f, 0.f, 1.f);

// The bar's own ground. A hole draws nothing, so this is what a hole LOOKS like -- which
// is why it is near-black and shares no hue with either palette.
const FLinearColor kMeterBackground(0.02f, 0.02f, 0.03f, 0.72f);

// The count above a collapsed span. Bright, because a marker nobody notices lets the
// bar go on implying that the runs either side of it were neighbours.
const FLinearColor kMeterElisionInk(1.00f, 0.88f, 0.70f, 1.f);

const FLinearColor kMeterDarkInk(0.04f, 0.04f, 0.04f, 1.f);
const FLinearColor kMeterLightInk(0.97f, 0.97f, 0.97f, 1.f);

FLinearColor meterCellColor(brawlerInputHistoryVisualization::LaneCellColor color)
{
	return FLinearColor(color.r, color.g, color.b, 1.f);
}

// A marker's ink is its style's, alpha included. ⛔ NO VERTICAL MARKER IS TINTED HERE.
FLinearColor meterMarkerColor(const brawlerInputHistoryVisualization::FrameMeterMarkerStyle& style)
{
	return FLinearColor(style.color.r, style.color.g, style.color.b, style.alpha);
}

// ⛔ LITERAL, NOT PARAPHRASED: the floor-class word is the enumerator's own name, upper-cased.
FString floorClassWord(brawlerInputHistoryVisualization::RelayFloorClass floorClass)
{
	using brawlerInputHistoryVisualization::RelayFloorClass;

	switch (floorClass)
	{
	case RelayFloorClass::Inert:            return TEXT("INERT");
	case RelayFloorClass::ActiveNotBinding: return TEXT("ACTIVENOTBINDING");
	case RelayFloorClass::Tie:              return TEXT("TIE");
	case RelayFloorClass::Binding:          return TEXT("BINDING");
	}

	return FString();
}

// The advisory's own name, unmodified -- only the floor-class word is upper-cased.
FString advisoryName(RelayDelayFloorAdvisory advisory)
{
	switch (advisory)
	{
	case RelayDelayFloorAdvisory::None:                   return FString();
	case RelayDelayFloorAdvisory::BelowHiccupBaseline:    return TEXT("BelowHiccupBaseline");
	case RelayDelayFloorAdvisory::UniformDFairnessActive: return TEXT("UniformDFairnessActive");
	}

	return FString();
}

// ⛔ LITERAL, NOT PARAPHRASED: the enumerator's own name, upper-cased, like the floor class.
FString driftActionWord(ClientPredictionClock::DriftAction action)
{
	switch (action)
	{
	case ClientPredictionClock::DriftAction::None:       return TEXT("NONE");
	case ClientPredictionClock::DriftAction::Skip:       return TEXT("SKIP");
	case ClientPredictionClock::DriftAction::Stall:      return TEXT("STALL");
	case ClientPredictionClock::DriftAction::HardResync: return TEXT("HARDRESYNC");
	}

	return FString();
}

// The sign is the tick DISPLACEMENT -- a skip inserted one, a stall withheld one -- which is
// the resync marker's own convention, so one sign reads across every correction of this axis.
// ⛔ DERIVED FROM THE KIND, NEVER STORED: one kind, one glyph, decided in one place.
FString rateMarkGlyph(const brawlerInputHistoryVisualization::FrameMeterRateMark& mark)
{
	using brawlerInputHistoryVisualization::RateMarkKind;

	const FString sign = (mark.kind == RateMarkKind::Skip) ? TEXT("+") : TEXT("−");

	// A poll that saw more than one correction of a kind can place only the last, so the
	// mark says how many it stands for rather than claiming to have been the only one.
	return (mark.count > 1u) ? sign + FString::Printf(TEXT("%u"), mark.count) : sign;
}

// Built from the readout model's fields ONLY -- the pure header owns the facts, this owns
// the string. ⛔ NO LIVE READ OF ANY CLOCK: everything here is already in `readout`.
FString buildClockDriftReadoutText(
	const brawlerInputHistoryVisualization::ClockDriftReadout& readout)
{
	const brawlerInputHistoryVisualization::ClockDriftReading& r = readout.reading;

	// ⛔ SIGNED: the drift says ahead of the target or behind it; a count says neither.
	return FString::Printf(
		TEXT("clock drift %+d  target %u = auth %u %+d  next %s  debt %u  auth static %u ticks")
		TEXT("  skips %u  stalls %u  resyncs %u"),
		r.driftTicks, r.targetTick, r.authorityTick,
		static_cast<int32_t>(r.targetTick) - static_cast<int32_t>(r.authorityTick),
		*driftActionWord(r.pendingAction), r.stallDebtTicks, readout.authorityStaticTicks,
		readout.skips, readout.stalls, readout.resyncs);
}

// Built from the readout model's fields ONLY, the fixed vocabulary given verbatim.
// ⛔ NO LIVE READ OF ANY INPUT: everything here is already in `readout`.
FString buildProvenanceResidencyReadoutText(
	const brawlerInputHistoryVisualization::ProvenanceResidencyReadout& readout)
{
	if (!readout.hasCache)
		return TEXT("NO CORRECTION CACHE - this role does not predict");

	if (!readout.anyResident)
		return TEXT("cache sim -- (0)");

	return FString::Printf(TEXT("cache sim %u..%u (%u)"),
		readout.oldestResident, readout.newestResident, readout.residentCount);
}

// Built from the readout model's fields ONLY -- the pure header owns the facts, this
// owns the string. ⛔ NO LIVE READ OF ANY INPUT: everything here is already in `readout`.
FString buildInputDelayReadoutText(
	const brawlerInputHistoryVisualization::InputDelayReadout& readout)
{
	using namespace brawlerInputHistoryVisualization;

	const InputDelayDecomposition& d = readout.decomposition;

	FString tierPart;
	if (d.tierKnown)
	{
		// ⛔ A LABELLED PAIR, NEVER AN ARROW: an arrow here reads as a tier transition.
		tierPart = FString::Printf(TEXT("tier %d  base %d"), d.tierIndex, d.baseTicks);
		if (d.lanOverrideApplied)
			tierPart += TEXT(" (LAN override)");
	}
	else
	{
		tierPart = FString::Printf(TEXT("no tier  fallback base %d"), d.baseTicks);
	}

	FString pubPart = readout.publishMismatch
		? FString::Printf(TEXT("pub %d ✗ UNPUBLISHED"), d.publishedTicks)
		: FString::Printf(TEXT("pub %d ✓"), d.publishedTicks);

	if (readout.formulaMismatch)
		pubPart += TEXT(" ✗ FORMULA");

	FString line = FString::Printf(TEXT("delay %d  floor %d (req %d, cap %d) %s  %s  %s"),
		d.effectiveTicks, d.floorTicks, d.floorRequested, d.floorHardCap,
		*floorClassWord(d.floorClass), *tierPart, *pubPart);

	if (readout.divergedInWindow > 0u)
		line += FString::Printf(TEXT("  diverged %u in window"), readout.divergedInWindow);

	const FString advisory = advisoryName(d.advisory);
	if (!advisory.IsEmpty())
		line += TEXT("  ") + advisory;

	return line;
}

} // namespace

void AOGBrawlerUEHUD::DrawHUD()
{
	Super::DrawHUD();

	// Each display hangs off one branch here, so a toggle that is off costs a flag read
	// and the HUD does nothing else at all for it.
	if (inputHistoryVisualizationUImpl::displayEnabled())
		drawInputHistoryPanel();

	// ⛔ ANY ONE BAR ON OPENS THIS BRANCH -- `anyBarEnabled()` already folds the master
	//   and all three bar toggles, so this is one call rather than an OR of three.
	if (inputHistoryVisualizationUImpl::anyBarEnabled())
		drawInputHistoryFrameMeter();
}

const ASimulationManagerUImpl* AOGBrawlerUEHUD::findHistorySource(
	unsigned int& outCharacterId) const
{
	const UWorld* world = GetWorld();

	// ⛔ ONE LOCAL PLAYER'S HUD DRAWS, NOT EVERY SIBLING'S -- each local player owns a
	//   HUD, so without this every one of them would stack the same display on the screen.
	if (GetOwningPlayerController()
		!= inputHistoryVisualizationUImpl::firstLocalPlayerController(world))
	{
		return nullptr;
	}

	const std::optional<unsigned int> characterId =
		inputHistoryVisualizationUImpl::firstLocalCharacterId(world);
	if (!characterId.has_value())
		return nullptr;

	outCharacterId = *characterId;

	// The same role expression the poll's own manager lookup uses: the history lives on
	// one instance and reading the other role's would find none.
	return ASimulationManagerUImpl::instanceFor(GetNetMode() != NM_Client);
}

void AOGBrawlerUEHUD::drawInputHistoryPanel()
{
	using namespace brawlerInputHistoryVisualization;

	if (Canvas == nullptr || GEngine == nullptr)
		return;

	unsigned int                   characterId = 0u;
	const ASimulationManagerUImpl* manager     = findHistorySource(characterId);
	if (manager == nullptr)
		return;

	// ⛔ POINTER TO CONST, and it stays one: the panel may not write a row it draws.
	const InputHistoryRowRing* rows = manager->getInputHistoryRows(characterId);
	if (rows == nullptr || rows->empty())
		return;

	// One call, so the scale cannot be applied after the centring: the layout arrives
	// already scaled AND already placed against this frame's own viewport height.
	// ⛔ THE PANEL IS FLUSH LEFT AND VERTICALLY CENTRED, and neither is decided here.
	const PanelLayout layout = placedPanelLayout(PanelLayout{},
		inputHistoryVisualizationUImpl::panelScale(),
		inputHistoryVisualizationUImpl::panelVisibleRows(),
		static_cast<float>(Canvas->SizeY));

	const std::size_t drawnRows = panelDrawnRowCount(layout, rows->size());

	// ⛔ A FULLY TRANSPARENT BACKDROP IS NOT DRAWN. Zero is the shipped default, and an
	//   invisible rectangle is still a canvas call on every frame the panel is up.
	const float backgroundAlpha = inputHistoryVisualizationUImpl::panelBackgroundAlpha();
	if (backgroundAlpha > 0.f)
	{
		DrawRect(FLinearColor(kPanelBackgroundInk.R, kPanelBackgroundInk.G,
			         kPanelBackgroundInk.B, backgroundAlpha),
			layout.originX, layout.originY,
			layout.rowWidth, panelHeight(layout, drawnRows));
	}

	for (std::size_t slot = 0u; slot < drawnRows; ++slot)
	{
		drawInputHistoryRow(layout, rows->at(panelRingIndexForSlot(rows->size(), slot)), slot);
	}
}

void AOGBrawlerUEHUD::drawInputHistoryFrameMeter()
{
	using namespace brawlerInputHistoryVisualization;

	if (Canvas == nullptr || GEngine == nullptr)
		return;

	unsigned int                   characterId = 0u;
	const ASimulationManagerUImpl* manager     = findHistorySource(characterId);
	if (manager == nullptr)
		return;

	// ⛔ POINTER TO CONST, and it stays one: the bars may not write a cell they draw.
	const InputHistoryTickLanes* lanes = manager->getInputHistoryLanes(characterId);
	if (lanes == nullptr || !lanes->hasAxis())
		return;

	// A second window would drift by a tick, and a vertical slice through the two bars
	// would then quietly mean two different things.
	// ⛔ ONE WINDOW FEEDS BOTH BARS.
	const PollWindow window =
		retainedLaneWindow(*lanes, inputHistoryVisualizationUImpl::retainedLaneTicks());

	// The selection each bar's own CVar makes, and the compaction it buys -- both come
	// straight from the pure header; nothing here recomputes a slot.
	const FrameMeterBarSelection selection = inputHistoryVisualizationUImpl::barSelection();
	const uint32_t               barCount  = frameMeterEnabledBarCount(selection);

	// ⛔ ZERO BARS DRAWS NOTHING AND COMPUTES NO GEOMETRY -- `DrawHUD` already gates this
	//   whole method on `anyBarEnabled()`, so this is a second, cheap fence, not the only one.
	if (barCount == 0u)
		return;

	const FrameMeterLayout   layout;
	const FrameMeterGeometry geometry = frameMeterGeometryFor(layout,
		static_cast<float>(Canvas->SizeX), static_cast<float>(Canvas->SizeY),
		frameMeterCellCount(window), barCount);

	if (geometry.cellCount == 0u)
		return;

	DrawRect(kMeterBackground,
		geometry.originX - layout.backdropPadding,
		geometry.originY - layout.backdropPadding,
		frameMeterWidth(geometry) + layout.backdropPadding * 2.f,
		frameMeterHeight(geometry) + layout.backdropPadding * 2.f);

	FrameMeterBarCells bar;

	// ⛔ INDEXES THE SAME LINE-PLACEMENT FORMULA THE DELAY READOUT USES: line 0 when the
	// delay bar is absent, line 1 when it has already claimed line 0.
	const bool delaySlotPresent =
		frameMeterBarSlotOf(selection, FrameMeterBarKind::InputDelay).has_value();

	// Each bar draws at its OWN slot among the enabled bars -- `frameMeterBarSlotOf` is the
	// only place compaction happens, so an absent bar simply has no slot to draw at.
	if (const std::optional<uint32_t> slot =
			frameMeterBarSlotOf(selection, FrameMeterBarKind::Provenance))
	{
		readProvenanceBar(*lanes, window, bar);
		drawFrameMeterBar(geometry, bar, *slot, FrameMeterBarKind::Provenance,
			&provenanceCellStyleOfOrdinal);

		// The horizon and the residency readout describe THIS bar alone, so both are drawn
		// only from inside its own branch -- mirroring the delay readout's own rule.
		const FrameMeterHorizon horizon = frameMeterHorizonOf(*lanes, window);
		if (horizon.anchor == AuthorityMarkerAnchor::Column
			|| horizon.anchor == AuthorityMarkerAnchor::RightEdge)
		{
			drawFrameMeterRule(geometry, layout, authorityMarkerX(geometry, horizon),
				kFrameMeterHorizonStyle);
		}

		drawFrameMeterResidencyReadout(geometry, layout,
			buildProvenanceResidencyReadout(*lanes), delaySlotPresent ? 1u : 0u);
	}

	// ⛔ THE READOUT IS DRAWN ONLY WHEN THE DELAY BAR IS: it describes that bar alone.
	if (const std::optional<uint32_t> slot =
			frameMeterBarSlotOf(selection, FrameMeterBarKind::InputDelay))
	{
		readDelayBar(*lanes, window, bar);
		drawFrameMeterBar(geometry, bar, *slot, FrameMeterBarKind::InputDelay,
			&delayVerdictStyleOfOrdinal);

		drawFrameMeterDelayReadout(geometry, layout, buildInputDelayReadout(*lanes, window));
	}

	if (const std::optional<uint32_t> slot =
			frameMeterBarSlotOf(selection, FrameMeterBarKind::CharacterState))
	{
		readMachineStateBar(*lanes, window, bar);
		drawFrameMeterBar(geometry, bar, *slot, FrameMeterBarKind::CharacterState,
			&machineCellStyleOfOrdinal);
	}

	drawFrameMeterAxisEvents(geometry, layout, *lanes, window);

	// The clock's own corrections, on the boundaries BETWEEN columns rather than on one.
	drawFrameMeterRateMarks(geometry, layout, *lanes, window);

	// Where the server is. Absent on a role that does not predict, which is the only
	// case with no offset to show at all.
	drawFrameMeterAuthorityMarker(geometry, layout,
		frameMeterAuthorityMarkerOf(*lanes, window));

	// ⛔ THE NEXT LINE AFTER WHICHEVER READOUTS THIS SELECTION DREW -- never a claimed one.
	const uint32_t clockLine = (delaySlotPresent ? 1u : 0u)
		+ (frameMeterBarSlotOf(selection, FrameMeterBarKind::Provenance).has_value() ? 1u : 0u);

	drawFrameMeterClockReadout(geometry, layout, buildClockDriftReadout(*lanes), clockLine);
}

void AOGBrawlerUEHUD::drawFrameMeterAxisEvents(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&   geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&     layout,
	const brawlerInputHistoryVisualization::InputHistoryTickLanes& lanes,
	const brawlerInputHistoryVisualization::PollWindow&            window)
{
	using namespace brawlerInputHistoryVisualization;

	FrameMeterAxisEventList events;
	collectFrameMeterAxisEvents(lanes, window, events);

	UFont* const font = GEngine->GetSmallFont();

	for (uint32_t index = 0u; index < events.count; ++index)
	{
		const FrameMeterAxisEvent& mark = events.marks[index];
		const float                x    = frameMeterCellX(geometry, mark.offset);

		// ⛔ ASKED ONCE PER MARK, NEVER ONCE PER LEDGER: two kinds at adjacent columns
		//   draw two cells in two colours precisely because this is inside the loop.
		const bool resync = (mark.kind == LaneAxisEventKind::Resync);

		// ⛔ ACROSS EVERY ENABLED BAR, so it reads as a cut in the axis rather than as a
		// state in one of them -- a cell is one bar tall and this deliberately is not.
		DrawRect(meterCellColor(resync ? kLaneResyncColor : kLaneElisionColor),
			x, geometry.originY, geometry.cellWidth, frameMeterHeight(geometry));

		// An elision counts the time the display removed; a resync states how far the axis
		// jumped, SIGNED, because its direction is the whole of what it claims.
		const FString label = resync ? FString::Printf(TEXT("%+d"), mark.deltaTicks)
		                             : FString::Printf(TEXT("%u"), mark.skippedTicks);

		float labelWidth  = 0.f;
		float labelHeight = 0.f;
		GetTextSize(label, labelWidth, labelHeight, font);

		// Above the backdrop, over no cell at all: the number says what became of time,
		// and a number sitting on a cell would read as that cell's own run length.
		DrawText(label, kMeterElisionInk,
			x + geometry.cellWidth * 0.5f - labelWidth * 0.5f,
			frameMeterElisionLabelTopY(geometry, layout, labelHeight), font);
	}
}

void AOGBrawlerUEHUD::drawFrameMeterRateMarks(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&     geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&       layout,
	const brawlerInputHistoryVisualization::InputHistoryTickLanes&  lanes,
	const brawlerInputHistoryVisualization::PollWindow&             window)
{
	using namespace brawlerInputHistoryVisualization;

	FrameMeterRateMarkList marks;
	collectFrameMeterRateMarks(lanes, window, marks);

	UFont* const       font = GEngine->GetSmallFont();
	const FLinearColor ink  = meterMarkerColor(kFrameMeterRateMarkStyle);

	for (uint32_t index = 0u; index < marks.count; ++index)
	{
		const FrameMeterRateMark& mark  = marks.marks[index];
		const FString             glyph = rateMarkGlyph(mark);

		// The edge of the mark's own column, which is what a boundary between two ticks
		// is in x: the two kinds sit one stride apart on the same column.
		const float x = rateMarkX(geometry, mark);

		float glyphWidth  = 0.f;
		float glyphHeight = 0.f;
		GetTextSize(glyph, glyphWidth, glyphHeight, font);

		// ⛔ IN THE LABEL BAND, ACROSS NO CELL: this style needs no palette clearance.
		const float topY = frameMeterElisionLabelTopY(geometry, layout, glyphHeight);
		DrawLine(x, topY, x, geometry.originY - layout.backdropPadding,
			ink, kFrameMeterRateMarkStyle.thickness);

		// Beside the mark, never centred on it: a glyph astride the boundary would read
		// as belonging to whichever column it happened to sit further into.
		DrawText(glyph, ink, x + layout.backdropPadding, topY, font);
	}
}

void AOGBrawlerUEHUD::drawFrameMeterRule(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&    geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&      layout,
	float                                                         x,
	const brawlerInputHistoryVisualization::FrameMeterMarkerStyle& style)
{
	using namespace brawlerInputHistoryVisualization;

	DrawLine(x, geometry.originY - layout.backdropPadding,
		x, geometry.originY + frameMeterHeight(geometry) + layout.backdropPadding,
		meterMarkerColor(style), style.thickness);
}

void AOGBrawlerUEHUD::drawFrameMeterAuthorityMarker(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&        geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&          layout,
	const brawlerInputHistoryVisualization::FrameMeterAuthorityMarker& marker)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ NOTHING IS DRAWN WITHOUT AN ESTIMATE: there is no offset to be wrong about.
	if (marker.anchor == AuthorityMarkerAnchor::None)
		return;

	const FrameMeterMarkerStyle style = authorityMarkerStyleOf(marker.anchor);
	const float                 x     = authorityMarkerX(geometry, marker);

	drawFrameMeterRule(geometry, layout, x, style);

	// The offset itself, under the rule it belongs to -- an elision count sits above the
	// bars, so the display's two numbers never share a line.
	// ⛔ THE DIRECTION IS A WORD, NOT A SIGN: a minus on an unsigned count reads as negative.
	const FString label = FString::Printf(TEXT("%u back"), marker.offsetTicks);
	UFont* const  font  = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(label, labelWidth, labelHeight, font);

	DrawText(label, meterMarkerColor(style), x - labelWidth * 0.5f,
		frameMeterAuthorityLabelTopY(geometry, layout), font);
}

void AOGBrawlerUEHUD::drawFrameMeterDelayReadout(
	const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
	const brawlerInputHistoryVisualization::InputDelayReadout&  readout)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ NOTHING IS DRAWN WITHOUT A READING: the delay display is not being fed.
	if (!readout.present)
		return;

	const FString text = buildInputDelayReadoutText(readout);
	UFont* const  font = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(text, labelWidth, labelHeight, font);

	// The newest verdict's own cell colour, or light ink when the window names none yet.
	const FLinearColor ink = readout.newestVerdict.has_value()
		? meterCellColor(delayVerdictStyleOf(*readout.newestVerdict).color)
		: kMeterLightInk;

	DrawText(text, ink, geometry.originX,
		frameMeterDelayReadoutTopY(geometry, layout, labelHeight), font);
}

void AOGBrawlerUEHUD::drawFrameMeterResidencyReadout(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&         geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&           layout,
	const brawlerInputHistoryVisualization::ProvenanceResidencyReadout& readout,
	uint32_t                                                            lineIndex)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ NOTHING IS DRAWN WITHOUT A READING: the provenance bar has not been polled yet.
	if (!readout.present)
		return;

	const FString text = buildProvenanceResidencyReadoutText(readout);
	UFont* const  font = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(text, labelWidth, labelHeight, font);

	DrawText(text, kMeterLightInk, geometry.originX,
		frameMeterReadoutLineTopY(geometry, layout, labelHeight, lineIndex), font);
}

void AOGBrawlerUEHUD::drawFrameMeterClockReadout(
	const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
	const brawlerInputHistoryVisualization::ClockDriftReadout&  readout,
	uint32_t                                                    lineIndex)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ NOTHING IS DRAWN WITHOUT A READING: this role has no clock, and a zero claims one.
	if (!readout.present)
		return;

	const FString text = buildClockDriftReadoutText(readout);
	UFont* const  font = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(text, labelWidth, labelHeight, font);

	DrawText(text, kMeterLightInk, geometry.originX,
		frameMeterReadoutLineTopY(geometry, layout, labelHeight, lineIndex), font);
}

void AOGBrawlerUEHUD::drawFrameMeterBar(
	const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
	const brawlerInputHistoryVisualization::FrameMeterBarCells& bar,
	uint32_t                                                    barSlot,
	brawlerInputHistoryVisualization::FrameMeterBarKind         kind,
	brawlerInputHistoryVisualization::LaneCellStyleOfOrdinal    styleOf)
{
	using namespace brawlerInputHistoryVisualization;

	const float  top  = frameMeterBarTopY(geometry, barSlot);
	UFont* const font = GEngine->GetSmallFont();

	for (uint32_t offset = 0u; offset < bar.count; ++offset)
	{
		// ⛔ A HOLE DRAWS NOTHING, so the ground shows through and reads as absence.
		if (!bar.cells[offset].filled)
			continue;

		const LaneCellStyle style = styleOf(bar.cells[offset].value);

		DrawRect(meterCellColor(style.color), frameMeterCellX(geometry, offset), top,
			geometry.cellWidth, geometry.barHeight);
	}

	// ⛔ DECIDED IN THE PURE HEADER, NOT HERE -- delay-bar runs arrive one tick in N, noise.
	if (!frameMeterBarDrawsRunLabels(kind))
		return;

	LaneRunList runs;
	collectLaneRuns(bar, runs);

	for (uint32_t index = 0u; index < runs.count; ++index)
	{
		const LaneRun& run = runs.runs[index];

		// Overlapping neighbours is expected at narrow cells and the reference does it
		// too; a number narrower than its own run is the only one worth drawing at all.
		if (!runLabelFits(geometry, run))
			continue;

		const LaneCellStyle style = styleOf(run.value);
		const FString       label = FString::Printf(TEXT("%u"), run.length);

		float labelWidth  = 0.f;
		float labelHeight = 0.f;
		GetTextSize(label, labelWidth, labelHeight, font);

		DrawText(label,
			laneLabelPrefersDarkInk(style.color) ? kMeterDarkInk : kMeterLightInk,
			runLabelCenterX(geometry, run) - labelWidth * 0.5f,
			top + (geometry.barHeight - labelHeight) * 0.5f, font);
	}
}

void AOGBrawlerUEHUD::drawInputHistoryRow(
	const brawlerInputHistoryVisualization::PanelLayout&     layout,
	const brawlerInputHistoryVisualization::InputHistoryRow& row,
	std::size_t                                              slotFromTop)
{
	using namespace brawlerInputHistoryVisualization;

	const float  top   = panelRowTopY(layout, slotFromTop);
	const float  textY = top + layout.textOffsetY;
	UFont* const font  = GEngine->GetSmallFont();

	// GetSmallFont is FIXED-SIZE, so the layout's own factor is what makes the glyphs
	// grow with the rows. The measure takes it too, or right-alignment drifts.
	// ⛔ EVERY DrawText AND GetTextSize HERE PASSES layout.textScale.
	const float textScale = layout.textScale;

	// Right-aligned, because a column of left-aligned counts stops reading as a column.
	const FString countText   = FString::Printf(TEXT("%u"), row.tickCount);
	float         countWidth  = 0.f;
	float         countHeight = 0.f;
	GetTextSize(countText, countWidth, countHeight, font, textScale);
	DrawText(countText, kRowFields,
		layout.originX + layout.tickCountRightX - countWidth, textY, font, textScale);

	drawDirectionGlyph(layout, top, row.direction, kRowFields);

	const FLinearColor buttonColor = (row.buttonMask != 0u) ? kHeldButtons : kIdleButtons;
	DrawText(FString(ANSI_TO_TCHAR(buttonMaskGlyph(row.buttonMask))), buttonColor,
		layout.originX + layout.buttonsX, textY, font, textScale);
}

void AOGBrawlerUEHUD::drawDirectionGlyph(
	const brawlerInputHistoryVisualization::PanelLayout& layout,
	float                                               rowTopY,
	brawlerInputHistoryVisualization::DirectionBucket   bucket,
	const FLinearColor&                                 color)
{
	using namespace brawlerInputHistoryVisualization;

	const DirectionGlyph glyph = directionGlyphOf(layout, rowTopY, bucket);

	// ⛔ NEUTRAL DRAWS A DOT, so it can never read as an arrow pointing somewhere.
	if (glyph.isNeutralDot)
	{
		DrawRect(color, glyph.dotOrigin.x, glyph.dotOrigin.y, glyph.dotSize, glyph.dotSize);
		return;
	}

	const ArrowSegment segments[] = { glyph.shaft, glyph.leftBarb, glyph.rightBarb };

	for (const ArrowSegment& segment : segments)
	{
		DrawLine(segment.from.x, segment.from.y, segment.to.x, segment.to.y,
			color, layout.arrowThickness);
	}
}
