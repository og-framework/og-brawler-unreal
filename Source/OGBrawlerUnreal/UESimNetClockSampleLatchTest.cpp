// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/UESimNetLanes.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace ueSimNetClockSampleLatchTest
{
struct FEvent
{
	bool isSample = false;
	double rttSeconds = 0.0;
	uint32_t authorityTick = 0u;
};

struct FFakeSession
{
	std::vector<FEvent> events;

	void onSimClockSample(const ogsimnet::SimNetClockSample& sample) { events.push_back(FEvent{true, sample.rttSeconds, 0u}); }
	void onTimingInfoReceived(uint32_t authorityTick, double) { events.push_back(FEvent{false, 0.0, authorityTick}); }
};

ogsimnet::SimNetClockSample sampleWithRtt(double rttSeconds)
{
	ogsimnet::SimNetClockSample sample;
	sample.rttSeconds = rttSeconds;
	sample.t1 = rttSeconds / 2.0;
	sample.t2 = rttSeconds / 2.0;
	sample.t3 = rttSeconds;
	return sample;
}
} // namespace ueSimNetClockSampleLatchTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FUESimNetClockSampleLatchTest,
	"OGBrawler.SimNet.ClockSampleLatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUESimNetClockSampleLatchTest::RunTest(const FString& Parameters)
{
	using namespace ueSimNetClockSampleLatchTest;

	{
		FClockSampleLatch latch;
		FFakeSession session;
		FFakeSession* const unbound = nullptr;
		latch.onSample(unbound, sampleWithRtt(0.087));
		latch.onSample(unbound, sampleWithRtt(0.071));
		latch.onTimingInfo(unbound, 1500u, 0.0);
		TestTrue(TEXT("unbound: the newest sample is held and nothing reaches a session"),
			latch.holdsSample() && session.events.empty());

		const std::optional<ogsimnet::SimNetClockSample> fed = latch.onTimingInfo(&session, 1502u, 0.0);
		const std::optional<ogsimnet::SimNetClockSample> fedAgain = latch.onTimingInfo(&session, 1503u, 0.0);
		TestTrue(TEXT("the first bound tick reports the held sample it fed (the [ClockSource.Held] line's input)"),
			fed.has_value() && fed->rttSeconds == 0.071 && !fedAgain.has_value());
		TestEqual(TEXT("bound: the held sample, then the first tick, then the next tick"), session.events.size(), size_t{3});
		if (session.events.size() == 3u)
		{
			TestTrue(TEXT("the first event is the newest unbound sample (71 ms, not 87 ms)"),
				session.events[0].isSample && session.events[0].rttSeconds == 0.071);
			TestTrue(TEXT("the first authority tick follows the sample"),
				!session.events[1].isSample && session.events[1].authorityTick == 1502u);
			TestTrue(TEXT("the second tick is not preceded by the sample again"),
				!session.events[2].isSample && session.events[2].authorityTick == 1503u);
		}
		TestFalse(TEXT("the sample is fed once"), latch.holdsSample());
	}

	{
		FClockSampleLatch latch;
		FFakeSession session;
		FFakeSession* const unbound = nullptr;
		latch.onSample(unbound, sampleWithRtt(0.090));
		latch.onSample(&session, sampleWithRtt(0.075));
		TestFalse(TEXT("nothing held is reported"), latch.onTimingInfo(&session, 1600u, 0.0).has_value());
		TestEqual(TEXT("a bound sample supersedes the held one: sample, tick"), session.events.size(), size_t{2});
		if (session.events.size() == 2u)
		{
			TestTrue(TEXT("the bound sample reaches the session at once"),
				session.events[0].isSample && session.events[0].rttSeconds == 0.075);
			TestTrue(TEXT("the tick follows with no replay of the older held sample"),
				!session.events[1].isSample && session.events[1].authorityTick == 1600u);
		}
	}

	{
		FClockSampleLatch latch;
		FFakeSession session;
		latch.onTimingInfo(&session, 1700u, 0.0);
		TestTrue(TEXT("with nothing held a tick arrives alone"),
			session.events.size() == 1u && !session.events[0].isSample && session.events[0].authorityTick == 1700u);
	}

	return true;
}

#endif
