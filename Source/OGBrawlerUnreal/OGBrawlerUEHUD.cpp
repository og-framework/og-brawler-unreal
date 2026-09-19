// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/OGBrawlerUEHUD.h"

#include <optional>

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGBrawlerUnreal/ScoreboardVisualizationUImpl.h"
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

// Built from the header model's fields ONLY. The word LOCAL / NEAREST says which stack
// this is; on the nearest, the range it was chosen at and whether this client controls
// it -- which is what says where its delay bar's client half came from.
// ⛔ NO LIVE READ OF ANY POSITION: everything here is already in `header`.
FString buildFrameMeterStackHeaderText(
	const brawlerInputHistoryVisualization::FrameMeterStackHeader& header)
{
	// ⛔ THE WORD IS NOT DECORATION: both stacks carry an id, and an id alone cannot say
	//   which of the two a reading belongs to.
	const FString control = header.isLocallyControlled ? TEXT("LOCAL") : TEXT("REMOTE");

	if (!header.isNearestStack)
		return FString::Printf(TEXT("LOCAL id=%u"), header.characterId);

	return FString::Printf(TEXT("NEAREST id=%u  d=%.1f m  %s"),
		header.characterId, header.distanceMeters, *control);
}

// Built from the readout model's fields ONLY -- the pure header owns the facts, this
// owns the string. ⛔ NO LIVE READ OF ANY RELAY: everything here is already in `readout`.
FString buildRelayReadReadoutText(
	const brawlerInputHistoryVisualization::RelayReadReadout& readout)
{
	// ⛔ A WORD, NOT A ZERO, when nothing has arrived: a `dA` of 0 is a legal LAN
	//   schedule stamp, so printing one for "no stamp at all" states a fact that is false.
	const FString stamp = readout.dLatestKnown
		? FString::Printf(TEXT("relay dA=%u"), readout.dLatest)
		: FString(TEXT("relay dA=-- (nothing arrived)"));

	return stamp + FString::Printf(TEXT("  hit/miss/verify %u/%u/%u  noprobe %u"),
		readout.hits, readout.misses, readout.verifyFails, readout.noProbes);
}

// The relay-health tally, on the SAME line the stamp above takes. Built from the readout
// model's fields ONLY -- the pure header owns the facts, this owns the string.
// ⛔ A WORD, NOT A ROW OF ZEROES, for a character this client controls: it resolves no
//   relayed input at all, and printing 0/0/0 would state a measurement nobody made.
FString buildRelayHealthReadoutText(
	const brawlerInputHistoryVisualization::RelayHealthReadout& readout)
{
	if (readout.local)
		return TEXT("relay: LOCAL - no relayed input");

	// ⛔ THE CAUSES ARE THE BAR'S OWN RUN LETTERS, so the line and the cells spell one
	//   vocabulary rather than two.
	const FString fallback = FString::Printf(
		TEXT("fb %u (L %u/S %u/O %u/V %u)"),
		readout.fallbackPending + readout.arrivedReplayable + readout.arrivedTooLate
			+ readout.neverArrived,
		readout.loss, readout.starved, readout.evicted, readout.verify);

	const FString lateness = readout.medianLatenessKnown
		? FString::Printf(TEXT("late %u (med %u t)"),
			readout.arrivedReplayable + readout.arrivedTooLate, readout.medianLatenessTicks)
		: FString(TEXT("late 0"));

	return FString::Printf(TEXT("%s  %s  toolate %u  never %u  pending %u"),
		*fallback, *lateness, readout.arrivedTooLate, readout.neverArrived,
		readout.fallbackPending);
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

	// ⛔ OFF COSTS THIS ONE BOOL READ, AND ON IS THE DEFAULT. Nothing below the branch runs:
	//   no actor is iterated, no simulation slice is fetched, no geometry is computed and
	//   no backdrop is drawn until the CVar reads true. [ringout task 6b]
	// ⚠ [ringout task 10, user ruling 13, 2026-09-13] THIS SENTENCE USED TO OPEN "DEFAULT
	//   OFF" AND THAT IS NO LONGER TRUE -- `GScoreboard` now initialises to `true`, making
	//   this the one viz MASTER in the project that does not default off, because the
	//   scoreboard is GAME-MODE UI rather than a debug draw. The one-bool-read property is
	//   unchanged; it is now what turning the board OFF costs. Full argument at
	//   `GScoreboard`'s initialiser in `ScoreboardVisualizationUImpl.cpp`.
	//   ⛔ THE BRANCH ITSELF IS UNTOUCHED -- this is a comment-only correction.
	if (scoreboardVisualizationUImpl::enabled())
		drawScoreboard();
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

	// The meter draws a second stack for the nearest brawler and this panel cannot: its
	// rows come from a capture line only a locally controlled character has. A reader
	// seeing two stacks and one panel would otherwise be guessing whose rows these are.
	// ⛔ THE ABSENCE IS DRAWN, NOT LEFT SILENT.
	if (!m_nearestCharacterId.has_value())
		return;

	const FString placeholder =
		FString::Printf(TEXT("NEAREST id=%u - no capture line"), *m_nearestCharacterId);

	DrawText(placeholder, kIdleButtons, panelPlaceholderX(layout), panelPlaceholderTopY(layout),
		GEngine->GetSmallFont(), layout.textScale);
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

	// The second stack's subject, chosen from positions that move every frame and held
	// across frames by the pure selector's own hysteresis.
	// ⛔ THE CHOICE IS FORGOTTEN WHILE THE STACK IS OFF, so switching it back on picks
	//   from where the fight is NOW rather than resuming one made from stale positions.
	std::optional<unsigned int> nearestId;
	float                       nearestDistanceCm = 0.f;

	if (inputHistoryVisualizationUImpl::nearestStackEnabled())
	{
		nearestId = inputHistoryVisualizationUImpl::nearestCharacterIdTo(
			manager, characterId, m_nearestCharacterId, nearestDistanceCm);
		m_nearestCharacterId = nearestId;
	}
	else
	{
		m_nearestCharacterId.reset();
	}

	// ⛔ ONE MEASURE FEEDS BOTH STACKS' BANDS. Two would differ by a pixel and the
	//   lift would leave the stacks overlapping or gapped by that pixel.
	const float labelHeight = meterLabelHeight();

	// How far the primary is raised so the nearest can have the anchor. With no second
	// stack this is ZERO, and the primary is then drawn from exactly the geometry it
	// always was -- which is what makes the one-stack display pixel-for-pixel today's.
	float liftPixels = 0.f;

	if (nearestId.has_value())
	{
		// ⛔ TWO SELECTIONS, BECAUSE THE TWO STACKS DO NOT DRAW THE SAME BARS: the
		//   relay-health bar is on the stack following someone else's character alone.
		const FrameMeterBarSelection selection =
			inputHistoryVisualizationUImpl::barSelection(false);
		const FrameMeterBarSelection nearestSelection =
			inputHistoryVisualizationUImpl::barSelection(true);

		// The PRIMARY stack's own readouts, because it is the primary that moves: its
		// tier line, its residency line, and the clock line below them.
		// A lift that shrank on a role with no clock reading would move both stacks for a
		// reason that has nothing to do with the display.
		// ⛔ THE CLOCK LINE IS RESERVED WHETHER OR NOT THIS ROLE HAS A CLOCK.
		const uint32_t primaryReadoutLines =
			(frameMeterBarSlotOf(selection, FrameMeterBarKind::InputDelay).has_value()
				? 1u : 0u)
			+ (frameMeterBarSlotOf(selection, FrameMeterBarKind::Provenance).has_value()
				? 1u : 0u)
			+ 1u;

		// ⛔ THE ANCHORED STACK'S BAR COUNT IS AN ARGUMENT: its bars are drawn ABOVE an
		//   origin pinned to the bottom margin, so a taller bar band raises its top edge.
		liftPixels = frameMeterPrimaryLift(FrameMeterLayout{},
			frameMeterEnabledBarCount(selection), primaryReadoutLines, labelHeight,
			frameMeterEnabledBarCount(nearestSelection));

		FrameMeterStackHeader primaryHeader;
		primaryHeader.characterId         = characterId;
		primaryHeader.isLocallyControlled = true;

		drawInputHistoryFrameMeterStack(*manager, characterId, liftPixels, labelHeight,
			primaryHeader);

		FrameMeterStackHeader nearestHeader;
		nearestHeader.isNearestStack = true;
		nearestHeader.characterId    = *nearestId;
		// Centimetres are the simulation's unit and metres are the one a reader has an
		// intuition for at fighting range.
		nearestHeader.distanceMeters = nearestDistanceCm * 0.01f;
		// ⛔ THE MANAGER'S ONE LOCALITY TEST, asked once and carried in the header from
		//   here on: a ring that has not been created yet is not a local character.
		nearestHeader.isLocallyControlled =
			manager->isLocallyControlledOnThisPeer(*nearestId);

		drawInputHistoryFrameMeterStack(*manager, *nearestId, 0.f, labelHeight, nearestHeader);
		return;
	}

	drawInputHistoryFrameMeterStack(*manager, characterId, liftPixels, labelHeight,
		std::nullopt);
}

void AOGBrawlerUEHUD::drawInputHistoryFrameMeterStack(
	const ASimulationManagerUImpl& manager,
	unsigned int                   characterId,
	float                          liftPixels,
	float                          labelHeight,
	const std::optional<brawlerInputHistoryVisualization::FrameMeterStackHeader>& header)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ POINTER TO CONST, and it stays one: the bars may not write a cell they draw.
	const InputHistoryTickLanes* lanes = manager.getInputHistoryLanes(characterId);
	if (lanes == nullptr || !lanes->hasAxis())
		return;

	// ⛔ ASKED ONCE: whether this is the second stack decides two readouts and nothing
	//   else, and two tests of one fact could disagree.
	const bool isNearestStack = header.has_value() && header->isNearestStack;

	// A second window would drift by a tick, and a vertical slice through the two bars
	// would then quietly mean two different things.
	// ⛔ ONE WINDOW FEEDS BOTH BARS.
	const PollWindow window =
		retainedLaneWindow(*lanes, inputHistoryVisualizationUImpl::retainedLaneTicks());

	// The selection each bar's own CVar makes, and the compaction it buys -- both come
	// straight from the pure header; nothing here recomputes a slot.
	// The stack that follows someone else's character is the one with a relay to report on.
	const FrameMeterBarSelection selection =
		inputHistoryVisualizationUImpl::barSelection(isNearestStack);
	const uint32_t               barCount  = frameMeterEnabledBarCount(selection);

	// ⛔ THE HEADER'S ANSWER, NEVER A SECOND TEST: the one locality read was made where the
	//   nearest was chosen. A stack drawn without a header is the primary, which is local.
	const bool isLocallyControlled = !header.has_value() || header->isLocallyControlled;

	// ⛔ ZERO BARS DRAWS NOTHING AND COMPUTES NO GEOMETRY -- `DrawHUD` already gates this
	//   whole method on `anyBarEnabled()`, so this is a second, cheap fence, not the only one.
	if (barCount == 0u)
		return;

	const FrameMeterLayout layout;

	// `frameMeterGeometryFor` computes the anchored geometry it always computed, and a
	// lift of 0 returns it unchanged -- which is why a single stack cannot have moved.
	// ⛔ THE LIFT IS A STEP ON THE ANSWER, NEVER AN ARGUMENT TO IT.
	const FrameMeterGeometry geometry = frameMeterLiftedBy(
		frameMeterGeometryFor(layout,
			static_cast<float>(Canvas->SizeX), static_cast<float>(Canvas->SizeY),
			frameMeterCellCount(window), barCount),
		liftPixels);

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

	const bool relaySlotPresent =
		frameMeterBarSlotOf(selection, FrameMeterBarKind::RelayHealth).has_value();

	// The primary spends it on the delay decomposition and the nearest on the relay
	// reading; the two replace each other and never stack, which is what keeps the two
	// stacks the same number of readout rows tall.
	// ⛔ LINE 0 HAS ONE OWNER PER STACK.
	const bool firstReadoutLineClaimed = isNearestStack ? relaySlotPresent : delaySlotPresent;

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
			buildProvenanceResidencyReadout(*lanes), firstReadoutLineClaimed ? 1u : 0u);
	}

	// ⛔ THE READOUT IS DRAWN ONLY WHEN THE DELAY BAR IS: it describes that bar alone.
	if (const std::optional<uint32_t> slot =
			frameMeterBarSlotOf(selection, FrameMeterBarKind::InputDelay))
	{
		readDelayBar(*lanes, window, bar);
		drawFrameMeterBar(geometry, bar, *slot, FrameMeterBarKind::InputDelay,
			&delayVerdictStyleOfOrdinal);

		// A tier is a property of the LOCAL connection and says nothing about a remote
		// proxy, so the nearest stack spends that line on the relay bar's own reading below.
		if (!isNearestStack)
			drawFrameMeterDelayReadout(geometry, layout, buildInputDelayReadout(*lanes, window));
	}

	// ⛔ THE READOUT IS DRAWN ONLY WHEN THE RELAY BAR IS: it describes that bar alone,
	//   exactly as the delay and residency readings describe theirs.
	if (const std::optional<uint32_t> slot =
			frameMeterBarSlotOf(selection, FrameMeterBarKind::RelayHealth))
	{
		readRelayHealthBar(*lanes, window, isLocallyControlled, bar);
		drawFrameMeterBar(geometry, bar, *slot, FrameMeterBarKind::RelayHealth,
			&relayReadVerdictStyleOfOrdinal);

		drawFrameMeterRelayReadout(geometry, layout,
			manager.getRelayReadReadout(characterId),
			buildRelayHealthReadout(*lanes, window, isLocallyControlled));
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

	// It describes THIS CLIENT'S AXIS, which both stacks share, so a second copy would be
	// the same reading printed twice -- and under a remote's id it would read as that
	// remote's clock, which nothing here knows anything about.
	// ⛔ THE CLOCK IS ON THE PRIMARY ONLY.
	if (!isNearestStack)
	{
		// ⛔ THE NEXT LINE AFTER WHICHEVER READOUTS THIS SELECTION DREW -- never a claimed one.
		const uint32_t clockLine = (delaySlotPresent ? 1u : 0u)
			+ (frameMeterBarSlotOf(selection, FrameMeterBarKind::Provenance).has_value() ? 1u : 0u);

		drawFrameMeterClockReadout(geometry, layout, buildClockDriftReadout(*lanes), clockLine);
	}

	// ⛔ LAST, AND ONLY WHEN THERE ARE TWO STACKS TO TELL APART. One stack alone is
	//   unambiguous, and a label it never had is a pixel it never had.
	if (header.has_value())
		drawFrameMeterStackHeader(geometry, layout, *header);
}

void AOGBrawlerUEHUD::drawFrameMeterStackHeader(
	const brawlerInputHistoryVisualization::FrameMeterGeometry&    geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&      layout,
	const brawlerInputHistoryVisualization::FrameMeterStackHeader& header)
{
	using namespace brawlerInputHistoryVisualization;

	const FString text = buildFrameMeterStackHeaderText(header);
	UFont* const  font = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(text, labelWidth, labelHeight, font);

	// The elision counts in this band are centred on the columns they mark, so the two
	// share a line without sharing a place; a header on a line of its own would cost every
	// stack another row of the margin the second stack was only just fitted into.
	// ⛔ FLUSH WITH THE BARS' OWN LEFT EDGE, in the elision label band.
	DrawText(text, kMeterLightInk, geometry.originX,
		frameMeterElisionLabelTopY(geometry, layout, labelHeight), font);
}

void AOGBrawlerUEHUD::drawFrameMeterRelayReadout(
	const brawlerInputHistoryVisualization::FrameMeterGeometry& geometry,
	const brawlerInputHistoryVisualization::FrameMeterLayout&   layout,
	const brawlerInputHistoryVisualization::RelayReadReadout&   readout,
	const brawlerInputHistoryVisualization::RelayHealthReadout& health)
{
	using namespace brawlerInputHistoryVisualization;

	// ⛔ NOTHING IS DRAWN WITHOUT A READING: this client has served no relayed read for
	//   that character and holds no cell about one, so there is no relay to report on.
	if (!readout.present && !health.present)
		return;

	// A locally controlled character has no ring to have stamped a schedule, so the health
	// line stands alone rather than being appended to a stamp that does not exist.
	const FString text = health.local
		? buildRelayHealthReadoutText(health)
		: buildRelayReadReadoutText(readout) + TEXT("  ")
			+ buildRelayHealthReadoutText(health);
	UFont* const  font = GEngine->GetSmallFont();

	float labelWidth  = 0.f;
	float labelHeight = 0.f;
	GetTextSize(text, labelWidth, labelHeight, font);

	// The same line the tier decomposition takes on the primary, through the same
	// placement helper: the two readings replace each other rather than stacking.
	DrawText(text, kMeterLightInk, geometry.originX,
		frameMeterDelayReadoutTopY(geometry, layout, labelHeight), font);
}

float AOGBrawlerUEHUD::meterLabelHeight()
{
	// A digit, because every band this places holds one and the small font is fixed
	// width; the string is a probe for the FONT's line height, not for this text.
	float probeWidth  = 0.f;
	float probeHeight = 0.f;
	GetTextSize(TEXT("0"), probeWidth, probeHeight, GEngine->GetSmallFont());

	return probeHeight;
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
	const FrameMeterRunLabel labelKind = frameMeterRunLabelOf(kind);
	if (labelKind == FrameMeterRunLabel::None)
		return;

	LaneRunList runs;
	collectLaneRuns(bar, runs);

	for (uint32_t index = 0u; index < runs.count; ++index)
	{
		const LaneRun& run = runs.runs[index];

		// The cause letter is always one character wide; a run length is as wide as it reads.
		const bool  isLetter = labelKind == FrameMeterRunLabel::CauseLetter;
		const TCHAR letter   = isLetter
			? static_cast<TCHAR>(relayMissLabelLetter(static_cast<RelayMissLabel>(run.label)))
			: TCHAR(0);

		// ⛔ A RUN WITH NO CAUSE TO NAME GETS NO LABEL: a hit names nothing, and a blank
		//   glyph drawn on it would read as a cause the display could not identify.
		if (isLetter && letter == TCHAR(0))
			continue;

		// Overlapping neighbours is expected at narrow cells and the reference does it
		// too; a label narrower than its own run is the only one worth drawing at all.
		if (!runLabelFits(geometry, run, isLetter ? 1u : decimalDigitCount(run.length)))
			continue;

		const LaneCellStyle style = styleOf(run.value);
		const FString       label = isLetter
			? FString::Printf(TEXT("%c"), letter)
			: FString::Printf(TEXT("%u"), run.length);

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

// [ringout task 6b] THE RING-OUT SCOREBOARD.
// ⛔ THE GATHER IS scoreboardVisualizationUImpl'S AND EVERY NUMBER IS THE PURE HEADER'S.
//   This method iterates nothing, sorts nothing, and computes no origin, row height or
//   column edge: it turns a placed layout and ordered rows into canvas calls.
void AOGBrawlerUEHUD::drawScoreboard()
{
	using namespace brawlerScoreboardVisualization;

	if (Canvas == nullptr || GEngine == nullptr)
		return;

	const UWorld* world = GetWorld();

	// ⛔ ONE LOCAL PLAYER'S HUD DRAWS, NOT EVERY SIBLING'S -- each local player owns a HUD,
	//   so without this every couch-co-op sibling stacks the same board on one screen.
	//   The SAME guard the two input-history displays make, through the same accessor.
	// ⚠ THIS IS THE ONLY THING THAT SELECTS A LOCAL PLAYER HERE. It does NOT filter the
	//   ROWS: a scoreboard draws everyone, which is exactly where this panel parts company
	//   with the single-character input-history pane.
	if (GetOwningPlayerController()
		!= inputHistoryVisualizationUImpl::firstLocalPlayerController(world))
	{
		return;
	}

	// ⛔ POINTER TO CONST, and it stays one: the board may not write a thing it draws.
	//   The same role expression every other manager lookup in this file uses.
	const ASimulationManagerUImpl* manager =
		ASimulationManagerUImpl::instanceFor(GetNetMode() != NM_Client);

	// ⭐ ALREADY ORDERED BY CHARACTER ID when it arrives, so two peers watching one match
	//   see the same player in the same row. Never re-sorted, and never sorted here.
	const std::vector<ScoreboardRow> rows =
		scoreboardVisualizationUImpl::gatherScoreboardRows(world, manager);
	if (rows.empty())
		return;

	// One call, so the scale cannot be applied after the placement: the layout arrives
	// already scaled AND already placed against this frame's own viewport.
	// ⛔ THE BOARD IS FLUSH RIGHT AND VERTICALLY CENTRED ON ITS DRAWN HEIGHT, and none of
	//   that is decided here -- `rowCount` is in the signature precisely because the
	//   centring is on the rows that exist, not on the eight a full board reserves.
	const ScoreboardLayout layout = placedScoreboardLayout(ScoreboardLayout{},
		scoreboardVisualizationUImpl::scale(), rows.size(),
		static_cast<float>(Canvas->SizeX), static_cast<float>(Canvas->SizeY));

	const std::size_t drawnRows = scoreboardDrawnRowCount(layout, rows.size());

	// ⛔ A FULLY TRANSPARENT BACKDROP IS NOT DRAWN. Zero is the shipped default, and an
	//   invisible rectangle is still a canvas call on every frame the board is up.
	const float backgroundAlpha = scoreboardVisualizationUImpl::backgroundAlpha();
	if (backgroundAlpha > 0.f)
	{
		DrawRect(FLinearColor(kScoreboardBackdropInk.r, kScoreboardBackdropInk.g,
			         kScoreboardBackdropInk.b, backgroundAlpha),
			layout.originX, layout.originY,
			layout.rowWidth, scoreboardHeight(layout, drawnRows));
	}

	UFont* const font = GEngine->GetSmallFont();

	for (std::size_t slot = 0u; slot < drawnRows; ++slot)
	{
		const ScoreboardRow& row = rows[slot];

		// ⛔ KEYED ON A BOOL, and the selector is the pure header's. This method names no
		//   colour of its own, which is what keeps the board outside
		//   `palette_legend_lint.ps1`'s enum-keyed-palette scope.
		const ScoreboardInk ink = scoreboardRowInk(row.isDead);
		const FLinearColor  color(ink.r, ink.g, ink.b, 1.f);

		const float textY = scoreboardRowTopY(layout, slot) + layout.textOffsetY;

		// ⛔ `layout.textScale` REACHES EVERY DrawText AND EVERY GetTextSize BELOW.
		//   `GetSmallFont()` is fixed-size, so a scale that reached only the geometry would
		//   give a bigger box holding the same tiny glyphs -- and a measure that omitted it
		//   would place each right-aligned column further out of alignment the larger the
		//   board got, which is invisible at the default scale most runs use.

		// Column 1 [ringout task 10] -- the fighter's COLOUR SWATCH, where the raw id used
		// to be drawn as text. A filled rectangle, so unlike columns two and three it needs
		// no font and no measure.
		// ⛔ THE COLOUR IS THE ROW'S OWN, THROUGH THE PURE SELECTOR, and the selector is
		//   deliberately independent of `row.isDead`: a dead fighter's swatch is its full
		//   tint, because a row is most in need of identifying while its owner is counting
		//   down. Status is carried by the text ink above and by column three below -- two
		//   cues, neither of them the identity channel. The argument is written out at
		//   `scoreboardRowSwatch`.
		// ⛔ `row.characterId` HAS NOT LEFT THE ROW. It is still the key the gather joined
		//   this row on and still the key the rows were ordered by, so two peers show the
		//   same player in the same row. It simply is not DRAWN any more.
		// ⛔ EVERY ONE OF THESE FOUR NUMBERS IS THE PURE HEADER'S. This method computes no
		//   geometry, here least of all: the swatch's height is derived from the row height
		//   and the inset, and deriving it here would be the only arithmetic in the file.
		const ScoreboardInk       swatch     = scoreboardRowSwatch(row);
		const ScoreboardSwatchRect swatchRect = scoreboardSwatchRect(layout, slot);
		DrawRect(FLinearColor(swatch.r, swatch.g, swatch.b, 1.f),
			swatchRect.x, swatchRect.y, swatchRect.width, swatchRect.height);

		// Column 2 -- the score. A RIGHT edge: `rightX - measuredWidth`, so a column of
		// two- and three-digit scores stays a column instead of jittering.
		const FString scoreText = FString::Printf(TEXT("%u"), row.score);
		float         scoreWidth = 0.f;
		float         scoreHeight = 0.f;
		GetTextSize(scoreText, scoreWidth, scoreHeight, font, layout.textScale);
		DrawText(scoreText, color,
			layout.originX + layout.scoreRightX - scoreWidth, textY, font, layout.textScale);

		// Column 3 -- the respawn countdown, and ONLY while this fighter is out.
		// ⛔ GATED ON THE LEVEL, THROUGH THE PURE PREDICATE, never on the countdown being
		//   non-zero: `ticksUntilRespawn` is stale by construction for a living fighter.
		if (scoreboardRowDrawsCountdown(row))
		{
			// ⚠ THE WORD IS NOT DECORATION. Columns two and three are both right-aligned
			//   integers, so a bare number here would be indistinguishable from a score
			//   that had simply moved right. `OUT` says which quantity this is.
			const FString statusText =
				FString::Printf(TEXT("OUT %u"), row.ticksUntilRespawn);
			float statusWidth = 0.f;
			float statusHeight = 0.f;
			GetTextSize(statusText, statusWidth, statusHeight, font, layout.textScale);
			DrawText(statusText, color,
				layout.originX + layout.statusRightX - statusWidth, textY, font,
				layout.textScale);
		}
	}
}
