// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/UESimulationSession.h"

constexpr bool ueSessionClockLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Net && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kConnectionTierChange, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kConnectionTierTag, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayDelayFloorTag, ELogVerbosity::Log)
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kNoTransition, TEXT("%s floor"))
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kTransition, TEXT("[Warning]%s floor"))
	&& ogLogRouteStartsWithNarrow(ogsession::sessionClockLines::kDilationWindow, TEXT("[Warning]%s"))
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorClamped, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorBelowHiccupBaseline, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kRelayFloorUniformD, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kAppliedInputDelay, ELogVerbosity::Log)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kClockSourceWindow, ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute("[Warning][Dilation.Window] mode=%s", ELogVerbosity::Warning)
	&& ueSessionClockLineKeepsItsRoute(ogsession::sessionClockLines::kDilationAuthority, ELogVerbosity::Warning),
	"Each session-clock line (ogsession::sessionClockLines) reaches LogOGNet at the level its UE_LOG had: Log for the tier "
	"change, the no-transition line (no token, led by its tag: [ConnectionTier] or [RelayDelayFloor]), the uniform-D "
	"advisory and the applied delay; Warning for the transition line (a token, then its tag), the received floor's clamp, "
	"the hiccup advisory, [ClockSource.Window], the dilation window's own [Dilation.Window] line after its token, and "
	"[Dilation.Authority]. Was each line's UE_LOG(LogOGNet, <Level>, ...) in UESimulationSession (og-session-runtime task "
	"24)");
static_assert(ueSessionClockLineKeepsItsRoute(ogsession::sessionNetHooksLines::kReleaseBatch, ELogVerbosity::Verbose),
	"The net hooks' [ReleaseBatch] line (ogsession::sessionNetHooksLines) reaches LogOGNet at Verbose, the level its "
	"OG_LOG_ROUTED(LogOGNet, ...) took from the route table in releaseDelayedInputsForStep (og-session-runtime task 26)");

constexpr bool ueSessionLifecycleLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Mgmt && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kQueuedRegistrationDidNotHold,
		ELogVerbosity::Error)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRegistered, ELogVerbosity::Log)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRunnerRegisterRefused, ELogVerbosity::Error)
	&& ueSessionLifecycleLineKeepsItsRoute(ogsession::sessionLifecycleLines::kRosterVerdictMismatch, ELogVerbosity::Error),
	"Each lifecycle line (ogsession::sessionLifecycleLines) reaches LogOGMgmt at the level its UE_LOG had: Error for the "
	"queued registration that did not hold and for the runner registration the og-sim-net server refused, Log for "
	"tryRegister: registered (both roles). Was each line's UE_LOG(LogOGMgmt, <Level>, ...) in finishQueuedRegistration "
	"(og-session-runtime task 29) and tryRegisterOnRunner (task 30). The roster-verdict mismatch line is new (task 30): Error, "
	"before the check that stops on it, with the id and the with/without the check's old message printed");

constexpr bool ueSessionRuntimeLineKeepsItsRoute(const char* line, OGLogRoute expected, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == expected && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionRuntimeLineKeepsItsRoute(ogsession::sessionRuntimeLines::kBackend, OGLogRoute::SimHost, ELogVerbosity::Warning)
	&& ueSessionRuntimeLineKeepsItsRoute(ogsession::sessionRuntimeLines::kManagerInitialized, OGLogRoute::Mgmt, ELogVerbosity::Log),
	"The runtime's [SimHost.Backend] line reaches LogOGSimHost at Warning and its 'SimulationManager: ... initialized' "
	"line reaches LogOGMgmt at Log: the categories and levels their UE_LOGs had in UESimulationSession::beginPlay "
	"(og-session-runtime task 31)");

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "OGSimSession/SessionClock.h"
#include "OGSimSession/SessionKnobs.h"

#include <functional>
#include <string>
#include <vector>

namespace ueSessionClockLineRouteTest
{
struct FCapturedLine
{
	FString Text;
	ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
	FName Category;
};

class FClockLineCapture final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		static const TCHAR* const kTags[] = { TEXT("[ConnectionTier]"), TEXT("[RelayDelayFloor]"), TEXT("[ClockSource.Window]"),
			TEXT("[Dilation.Window]"), TEXT("[Dilation.Authority]") };
		bool clockLine = false;
		for (const TCHAR* tag : kTags)
			clockLine = clockLine || FCString::Strstr(V, tag) == V;
		if (!clockLine)
			return;
		FScopeLock lock(&m_lock);
		m_lines.Add(FCapturedLine{ FString(V), static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask),
			Category });
	}

	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnMultipleThreads() const override { return true; }

	TArray<FCapturedLine> TakeLines()
	{
		FScopeLock lock(&m_lock);
		return MoveTemp(m_lines);
	}

private:
	FCriticalSection m_lock;
	TArray<FCapturedLine> m_lines;
};

struct FLinePair
{
	std::function<void()>        logTheOldWay;
	std::function<std::string()> newLine;
};

namespace lines = ogsession::sessionClockLines;
using ogsession::sessionKnobsLines::formatLine;

constexpr char kWindowLine[] = "[Dilation.Window] mode=On setpoint=Common calls=1000 lead=3.512/3.650/3.790 stepLag=0.300";

// Each old UE_LOG is transcribed from OGSimulation dae3018's OGSimSession/SessionClock.h (og-session-runtime task 24 (a),
// the verbatim copy of UESimulationSession.h at 0249ba8), at the line named; one representative value set per line, and the
// two transition lines with each tag, upward and downward.
std::vector<FLinePair> linePairs()
{
	return {
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("%hs"), kWindowLine); }, // :75
			[] { return formatLine(lines::kDilationWindow, kWindowLine); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Authority] refusedRateChanges=%llu rateScale=%.5f"), 0ull, 1.0); }, // :91
			[] { return formatLine(lines::kDilationAuthority, 0ull, 1.0); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("[ConnectionTier] tier %u -> %u, effective delay delta=%d"), 0u, 2u, -1); }, // :115
			[] { return formatLine(lines::kConnectionTierChange, 0u, 2u, -1); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("%s floor = %d ticks, effective delay delta=0, no stall or skip"),
				TEXT("[ConnectionTier]"), 6); }, // :168
			[] { return formatLine(lines::kNoTransition, lines::kConnectionTierTag, 6); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("%s floor = %d ticks, effective delay delta=0, no stall or skip"),
				TEXT("[RelayDelayFloor]"), 3); }, // :168
			[] { return formatLine(lines::kNoTransition, lines::kRelayDelayFloorTag, 3); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
				TEXT("[ConnectionTier]"), 3, TEXT("DOWNWARD"), 1, TEXT("skip")); }, // :176
			[] { return formatLine(lines::kTransition, lines::kConnectionTierTag, 3, lines::kDownward, 1, lines::kSkip); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
				TEXT("[ConnectionTier]"), 0, TEXT("UPWARD"), 2, TEXT("stall")); }, // :176
			[] { return formatLine(lines::kTransition, lines::kConnectionTierTag, 0, lines::kUpward, 2, lines::kStall); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
				TEXT("[RelayDelayFloor]"), 6, TEXT("UPWARD"), 2, TEXT("stall")); }, // :176
			[] { return formatLine(lines::kTransition, lines::kRelayDelayFloorTag, 6, lines::kUpward, 2, lines::kStall); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
				TEXT("[RelayDelayFloor]"), 2, TEXT("DOWNWARD"), 2, TEXT("skip")); }, // :176
			[] { return formatLine(lines::kTransition, lines::kRelayDelayFloorTag, 2, lines::kDownward, 2, lines::kSkip); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"), 200u,
				12); }, // :149
			[] { return formatLine(lines::kRelayFloorClamped, 200u, 12); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and ")
				TEXT("above off (0) — the one value that is neither"), 1); }, // :193
			[] { return formatLine(lines::kRelayFloorBelowHiccupBaseline, 1); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode ")
				TEXT("active; tiers and LAN override are inert"), 6); }, // :198
			[] { return formatLine(lines::kRelayFloorUniformD, 6); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
				6, 1, 1, 6); }, // :225
			[] { return formatLine(lines::kAppliedInputDelay, 6, 1, 1, 6); } },
		{ [] { UE_LOG(LogOGNet, Warning,
				TEXT("[ClockSource.Window] source=%s engineRttMs=%.3f simClockRttMs=%.3f ackSrttMs=%.3f minRttOffsetUs=%.0f ")
				TEXT("owdVarUs=%.0f/%.0f smoothedRttMs=%.3f samples=%u invalid=%u pings=%llu pongs=%llu"),
				TEXT("SimClock"), 41.2, 38.9, -1.0, -12617.0, 120.0, 95.0, 38.9, 188u, 0u, 41ull, 40ull); }, // :266
			[] { return formatLine(lines::kClockSourceWindow, lines::kSourceSimClock, 41.2, 38.9, -1.0, -12617.0, 120.0, 95.0, 38.9,
				188u, 0u, 41ull, 40ull); } },
	};
}

class FWarningLinesTestBase : public FAutomationTestBase
{
public:
	FWarningLinesTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}

	virtual bool SuppressLogWarnings() override { return true; }
};
} // namespace ueSessionClockLineRouteTest

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FUESessionClockLineRouteTest,
	ueSessionClockLineRouteTest::FWarningLinesTestBase,
	"OGBrawler.Session.ClockLineRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESessionClockLineRouteTest::RunTest(const FString& Parameters)
{
	using namespace ueSessionClockLineRouteTest;

	const std::vector<FLinePair> pairs = linePairs();
	const ELogVerbosity::Type verbosityBefore = LogOGNet.GetVerbosity();
	LogOGNet.SetVerbosity(ELogVerbosity::Log);
	FClockLineCapture capture;
	GLog->AddOutputDevice(&capture);
	for (const FLinePair& pair : pairs)
	{
		pair.logTheOldWay();
		UESessionClockEngine engine;
		engine.logRouted(pair.newLine().c_str());
	}
	GLog->Flush();
	GLog->RemoveOutputDevice(&capture);
	LogOGNet.SetVerbosity(verbosityBefore);

	const TArray<FCapturedLine> captured = capture.TakeLines();
	TestEqual(TEXT("each of the 14 lines logged the old way and through the session clock's engine"), captured.Num(),
		2 * static_cast<int32>(pairs.size()));
	TestEqual(TEXT("14 lines"), static_cast<int32>(pairs.size()), 14);
	for (int32 index = 0; index + 1 < captured.Num(); index += 2)
	{
		const FCapturedLine& oldLine = captured[index];
		const FCapturedLine& newLine = captured[index + 1];
		TestEqual(TEXT("the routed line's text equals the old UE_LOG's"), newLine.Text, oldLine.Text);
		TestTrue(FString::Printf(TEXT("the routed line's category equals the old UE_LOG's: %s"), *oldLine.Text),
			newLine.Category == oldLine.Category);
		TestTrue(TEXT("the old UE_LOG's category is LogOGNet"), oldLine.Category == LogOGNet.GetCategoryName());
		TestEqual(FString::Printf(TEXT("the routed line's level equals the old UE_LOG's: %s"), *oldLine.Text),
			static_cast<int32>(newLine.Verbosity), static_cast<int32>(oldLine.Verbosity));
	}
	return true;
}

#endif
