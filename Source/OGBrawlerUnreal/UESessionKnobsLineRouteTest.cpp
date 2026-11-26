// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimSession/SessionKnobs.h"

constexpr bool ueSessionKnobLineKeepsItsRoute(const char* line, ELogVerbosity::Type level)
{
	const OGLogRouteEntry* route = ogLogRouteForLevelLine(line);
	return route != nullptr && route->route == OGLogRoute::Net && ogLogLevelOfLevelLine(line) == level;
}

static_assert(ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRelayDelayFloorClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRelayDelayFloorSession, ELogVerbosity::Log)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kStateRotationClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kStateRotationSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kResimGateUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kResimGateSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsRedundancyClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsResendClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kLatencyKnobsSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRecentreClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kRecentreSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kJitterNotFinite, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kJitterClamped, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kClockRttSourceUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kClockSourceSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationNotApplicable, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationSetpointUnrecognised, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationSession, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoors, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoorNotFinite, ELogVerbosity::Warning)
	&& ueSessionKnobLineKeepsItsRoute(ogsession::sessionKnobsLines::kDilationDoorClamped, ELogVerbosity::Warning),
	"Each session-knob line (ogsession::sessionKnobsLines) reaches LogOGNet at the level its UE_LOG had: Log for the "
	"floor's ini line, Warning for every other. Was each line's UE_LOG(LogOGNet, <Level>, ...) in UESimulationSession "
	"(og-session-runtime task 23)");
static_assert(ogsession::SessionLogSink<UESessionLogSink>,
	"UESessionLogSink is the session knobs' log sink: og-sim-session's SessionLogSink");

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"
#include "OGSimSession/SessionKnobs.h"

#include <functional>
#include <string>
#include <vector>

namespace ueSessionKnobsLineRouteTest
{
struct FCapturedLine
{
	FString Text;
	ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
	FName Category;
};

class FKnobLineCapture final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		static const TCHAR* const kTags[] = { TEXT("[RelayDelayFloor]"), TEXT("[StateRotation]"), TEXT("[ResimGate]"),
			TEXT("[LatencyKnobs]"), TEXT("[ClockRecentre.Config]"), TEXT("[ClockSource.Config]"), TEXT("[Dilation.Config]") };
		bool knobLine = false;
		for (const TCHAR* tag : kTags)
			knobLine = knobLine || FCString::Strstr(V, tag) == V;
		if (!knobLine)
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

namespace lines = ogsession::sessionKnobsLines;
using lines::formatLine;

// Each old UE_LOG is transcribed from OGSimulation 0eb05d0's OGSimSession/SessionKnobs.h (the (a) commit's verbatim copy of
// UESimulationSession.h at ebd73bf), at the line named; the values are one representative set per line.
std::vector<FLinePair> linePairs()
{
	return {
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"), 99, 12); }, // :265
			[] { return formatLine(lines::kRelayDelayFloorClamped, 99, 12); } },
		{ [] { UE_LOG(LogOGNet, Log, TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), 6); }, // :270
			[] { return formatLine(lines::kRelayDelayFloorSession, 6); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"), -7, 1); }, // :281
			[] { return formatLine(lines::kStateRotationClamped, -7, 1); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[StateRotation] session K = %d (%s)"), 1, TEXT("compiled default")); }, // :289
			[] { return formatLine(lines::kStateRotationSession, 1, "compiled default"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
				TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"), TEXT("Bogus")); }, // :318
			[] { return formatLine(lines::kResimGateUnrecognised, "Bogus"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
				TEXT("OnDisagreement"), TEXT("ini override"), TEXT("skip beyond 12 ticks")); }, // :332
			[] { return formatLine(lines::kResimGateSession, "OnDisagreement", "ini override", "skip beyond 12 ticks"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d"), 12, 8); }, // :347
			[] { return formatLine(lines::kLatencyKnobsRedundancyClamped, 12, 8); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d"), -1, 0); }, // :359
			[] { return formatLine(lines::kLatencyKnobsResendClamped, -1, 0); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)"), 3, 2, TEXT("ini")); }, // :367
			[] { return formatLine(lines::kLatencyKnobsSession, 3, 2, "ini"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d"), -3, 0); }, // :378
			[] { return formatLine(lines::kRecentreClamped, -3, 0); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockRecentre.Config] recentreSettleSamples=%u (%s)"), 30u, TEXT("compiled")); }, // :384
			[] { return formatLine(lines::kRecentreSession, 30u, "compiled"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults")); }, // :394
			[] { return formatLine(lines::kJitterNotFinite); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier=%.3f out of range, clamped to %.3f"), 5.0, 4.0); }, // :402
			[] { return formatLine(lines::kJitterClamped, 5.0, 4.0); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockSource.Config] ini [OGNetcode] ClockRttSource='%s' unrecognised (expected Engine or SimClock), keeping ")
				TEXT("the compiled default"), TEXT("EnginePing")); }, // :434
			[] { return formatLine(lines::kClockRttSourceUnrecognised, "EnginePing"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[ClockSource.Config] clockRttSource=%s (%s) jitterMultiplier engine=%.3f simClock=%.3f (%s)"),
				TEXT("SimClock"), TEXT("compiled"), 2.0, 0.5, TEXT("compiled")); }, // :440
			[] { return formatLine(lines::kClockSourceSession, "SimClock", "compiled", 2.0, 0.5, "compiled"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] mode=n/a")); }, // :454
			[] { return formatLine(lines::kDilationNotApplicable); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] ini [OGNetcode] ClockDilation='%s' unrecognised (expected Off, Shadow or On), running Off"), TEXT("Onn")); }, // :478
			[] { return formatLine(lines::kDilationUnrecognised, "Onn"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='%s' unrecognised (expected OwnRtt or Common), ")
				TEXT("running %hs"), TEXT("Commons"), "OwnRtt"); }, // :512
			[] { return formatLine(lines::kDilationSetpointUnrecognised, "Commons", "OwnRtt"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] mode=%hs setpoint=%hs gainPerTick=%.4f maxDeviation=%.4f (%s)"),
				"On", "Common", 0.005, 0.01, TEXT("ini")); }, // :522
			[] { return formatLine(lines::kDilationSession, "On", "Common", 0.005, 0.01, "ini"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] doors gainPerTick=%.4f (%s) maxPercent=%.3f (%s) targetOffsetTicks=%.3f (%s) setpoint=%hs (%s) ")
				TEXT("commonLeadTicks=%.3f (%s)"), 0.005, TEXT("compiled"), 1.0, TEXT("compiled"), 0.0, TEXT("compiled"), "Common",
				TEXT("ini"), 3.65, TEXT("compiled")); }, // :526
			[] { return formatLine(lines::kDilationDoors, 0.005, "compiled", 1.0, "compiled", 0.0, "compiled", "Common", "ini", 3.65,
				"compiled"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] ini [OGNetcode] %s is not a finite number, keeping the compiled default"),
				TEXT("ClockDilationMaxPercent")); }, // :544
			[] { return formatLine(lines::kDilationDoorNotFinite, "ClockDilationMaxPercent"); } },
		{ [] { UE_LOG(LogOGNet, Warning, TEXT("[Dilation.Config] ini [OGNetcode] %s=%.4f out of range, clamped to %.4f"),
				TEXT("ClockDilationCommonLeadTicks"), 11.0, 10.0); }, // :551
			[] { return formatLine(lines::kDilationDoorClamped, "ClockDilationCommonLeadTicks", 11.0, 10.0); } },
	};
}

class FWarningLinesTestBase : public FAutomationTestBase
{
public:
	FWarningLinesTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}

	virtual bool SuppressLogWarnings() override { return true; }
};
} // namespace ueSessionKnobsLineRouteTest

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FUESessionKnobsLineRouteTest,
	ueSessionKnobsLineRouteTest::FWarningLinesTestBase,
	"OGBrawler.Session.KnobLineRoute",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESessionKnobsLineRouteTest::RunTest(const FString& Parameters)
{
	using namespace ueSessionKnobsLineRouteTest;

	const std::vector<FLinePair> pairs = linePairs();
	const ELogVerbosity::Type verbosityBefore = LogOGNet.GetVerbosity();
	LogOGNet.SetVerbosity(ELogVerbosity::Log);
	FKnobLineCapture capture;
	GLog->AddOutputDevice(&capture);
	for (const FLinePair& pair : pairs)
	{
		pair.logTheOldWay();
		UESessionLogSink sink;
		sink.logRouted(pair.newLine().c_str());
	}
	GLog->Flush();
	GLog->RemoveOutputDevice(&capture);
	LogOGNet.SetVerbosity(verbosityBefore);

	const TArray<FCapturedLine> captured = capture.TakeLines();
	TestEqual(TEXT("each of the 22 lines logged the old way and through the session's knob sink"), captured.Num(),
		2 * static_cast<int32>(pairs.size()));
	TestEqual(TEXT("22 lines"), static_cast<int32>(pairs.size()), 22);
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
