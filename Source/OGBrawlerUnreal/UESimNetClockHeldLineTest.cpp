// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/UESimNetLanes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/ScopeLock.h"

#include <array>
#include <cstdint>
#include <cstring>

namespace ueSimNetClockHeldLineTest
{
struct FCapturedLine
{
	FString Text;
	ELogVerbosity::Type Verbosity = ELogVerbosity::NoLogging;
	FName Category;
};

class FHeldLineCapture final : public FOutputDevice
{
public:
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override
	{
		if (FCString::Strstr(V, TEXT("[ClockSource.Held]")) == nullptr)
			return;
		FScopeLock lock(&m_lock);
		m_lines.Add(FCapturedLine{FString(V), static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask), Category});
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

struct FHeldValues
{
	uint32 AuthorityTick = 0u;
	double RttMs = 0.0;
	double HeldMs = 0.0;
};

// The old line, transcribed from UESimNetLanesClientHost::onTimingInfo (UESimNetLanes.h:238-241 at 3b8a211).
void logTheOldWay(const FHeldValues& values)
{
	UE_LOG(LogOGNet, Warning,
		TEXT("[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick %u: ")
		TEXT("rttMs=%.3f heldMs=%.1f"),
		values.AuthorityTick, values.RttMs, values.HeldMs);
}

FString formatTheOldWay(const FHeldValues& values)
{
	return FString::Printf(
		TEXT("[ClockSource.Held] fed the SimClock sample held while no world session was bound, ahead of authority tick %u: ")
		TEXT("rttMs=%.3f heldMs=%.1f"),
		values.AuthorityTick, values.RttMs, values.HeldMs);
}

constexpr std::array<FHeldValues, 9> kHeldValues = {{
	{1502u, 71.0, 32.0},
	{0u, 0.0, 0.0},
	{4294967295u, 87.123456, 1234.56},
	{1500u, 0.0005, 0.05},
	{1501u, 0.0015, 0.15},
	{77u, -3.25, -0.04},
	{20000u, 999999.9995, 99999.95},
	{3u, 123456789012.3456, 98765432109.87},
	{88u, 16.66666666666, 4.25},
}};
class FWarningLinesTestBase : public FAutomationTestBase
{
public:
	FWarningLinesTestBase(const FString& InName, const bool bInComplexTask) : FAutomationTestBase(InName, bInComplexTask) {}

	virtual bool SuppressLogWarnings() override { return true; }
};
} // namespace ueSimNetClockHeldLineTest

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(
	FUESimNetClockHeldLineTest,
	ueSimNetClockHeldLineTest::FWarningLinesTestBase,
	"OGBrawler.SimNet.ClockHeldLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESimNetClockHeldLineTest::RunTest(const FString& Parameters)
{
	using namespace ueSimNetClockHeldLineTest;

	for (const FHeldValues& values : kHeldValues)
	{
		const std::array<char, ogsimnet::kClockSourceHeldLineBytes> line =
			ogsimnet::formatClockSourceHeld(values.AuthorityTick, values.RttMs, values.HeldMs);
		const FString old = formatTheOldWay(values);
		const FTCHARToUTF8 oldBytes(*old);
		TestTrue(FString::Printf(TEXT("og-sim-net's line has the old UE_LOG format's bytes: %s"), *old),
			oldBytes.Length() == static_cast<int32>(std::strlen(line.data()))
				&& std::memcmp(oldBytes.Get(), line.data(), oldBytes.Length()) == 0);
	}

	FHeldLineCapture capture;
	GLog->AddOutputDevice(&capture);
	for (const FHeldValues& values : kHeldValues)
	{
		logTheOldWay(values);
		ueSimNetLanes::routeClientLine(
			ogsimnet::formatClockSourceHeld(values.AuthorityTick, values.RttMs, values.HeldMs).data());
	}
	GLog->Flush();
	GLog->RemoveOutputDevice(&capture);

	const TArray<FCapturedLine> lines = capture.TakeLines();
	TestEqual(TEXT("each value set logged the old way and through the client host's route"), lines.Num(),
		2 * static_cast<int32>(kHeldValues.size()));
	for (int32 index = 0; index + 1 < lines.Num(); index += 2)
	{
		const FCapturedLine& oldLine = lines[index];
		const FCapturedLine& newLine = lines[index + 1];
		TestEqual(TEXT("the routed line's text equals the old UE_LOG's"), newLine.Text, oldLine.Text);
		TestTrue(TEXT("the routed line's category equals the old UE_LOG's (LogOGNet)"), newLine.Category == oldLine.Category);
		TestTrue(TEXT("the old UE_LOG's category is LogOGNet"), oldLine.Category == LogOGNet.GetCategoryName());
		TestEqual(TEXT("the routed line's level equals the old UE_LOG's"), static_cast<int32>(newLine.Verbosity),
			static_cast<int32>(oldLine.Verbosity));
		TestEqual(TEXT("the old UE_LOG's level is Warning"), static_cast<int32>(oldLine.Verbosity),
			static_cast<int32>(ELogVerbosity::Warning));
	}
	return true;
}

#endif
