// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS

#include <cfloat>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "CoreMinimal.h"
#include "catch_amalgamated.hpp"
#include "OGSimSession/SessionClock.h"
#include "OGSimSession/SessionKnobs.h"
#include "OGSimSession/SessionNetHooks.h"

namespace sessionKnobsFormatParityTest
{
using ogsession::sessionKnobsLines::formatLine;
namespace lines = ogsession::sessionKnobsLines;

enum class OldLevel : uint8_t { Log, Warning, None };

struct Tally
{
	uint32 compared = 0u;
	uint32 formats  = 0u;
};

std::string utf8(const FString& text)
{
	return std::string(TCHAR_TO_UTF8(*text));
}

FString wide(const std::string& text)
{
	return FString(UTF8_TO_TCHAR(text.c_str()));
}

void checkLine(Tally& tally, const FString& old, OldLevel level, const std::string& routed)
{
	const bool hasWarningToken = routed.rfind("[Warning]", 0) == 0;
	const std::string body = hasWarningToken ? routed.substr(9) : routed;
	INFO("old: " << utf8(old));
	INFO("new: " << routed);
	CHECK(hasWarningToken == (level == OldLevel::Warning));
	CHECK(body == utf8(old));
	++tally.compared;
}

const std::vector<int32> kInts = { 0, 1, -1, 7, 99, -5, 1000000, std::numeric_limits<int32>::min(),
	std::numeric_limits<int32>::max() };
const std::vector<double> kDoubles = { 0.0, -0.0, 0.0005, 0.00005, 0.0015, 0.05, 2.5, 3.65, 0.0123456, -2.5, 123456.789,
	1e-310, 1e308, -1e308, DBL_MAX, -DBL_MAX, 99999.99995 };
const std::vector<std::string> kWords = { "", "Bogus", "OnDisagreement", "simclock", "O\xC3\xB1", "\xC5\x8Cn",
	"100% %d %s", "\xE2\x80\x94", std::string(3000u, 'x') };
const std::vector<std::string> kTokens = { "ini override", "compiled default", "ini", "compiled", "FrontierExact",
	"OnDisagreement", "Engine", "SimClock" };
const std::vector<const char*> kModeWords = { "Off", "Shadow", "On", "OwnRtt", "Common" };
} // namespace sessionKnobsFormatParityTest

TEST_CASE("SessionKnobsFormatParity.FStringPrintfEqualsSnprintfForEveryMovedLine", "[Session][SessionKnobs]")
{
	using namespace sessionKnobsFormatParityTest;
	Tally tally;

	// Each old format is transcribed from OGSimulation 0eb05d0's OGSimSession/SessionKnobs.h (the (a) commit's verbatim copy
	// of UESimulationSession.h at ebd73bf), at the line named.

	++tally.formats; // :265-267, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			checkLine(tally, FString::Printf(TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"), a, b),
				OldLevel::Warning, formatLine(lines::kRelayDelayFloorClamped, a, b));

	++tally.formats; // :270-271, Log
	for (const int32 a : kInts)
		checkLine(tally, FString::Printf(TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), a), OldLevel::Log,
			formatLine(lines::kRelayDelayFloorSession, a));

	++tally.formats; // :281-283, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			checkLine(tally, FString::Printf(TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"), a, b),
				OldLevel::Warning, formatLine(lines::kStateRotationClamped, a, b));

	++tally.formats; // :289-292, Warning
	for (const int32 a : kInts)
		for (const std::string& token : kTokens)
			checkLine(tally, FString::Printf(TEXT("[StateRotation] session K = %d (%s)"), a, *wide(token)), OldLevel::Warning,
				formatLine(lines::kStateRotationSession, a, token.c_str()));

	++tally.formats; // :318-321, Warning
	for (const std::string& word : kWords)
		checkLine(tally, FString::Printf(TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
				TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"), *wide(word)),
			OldLevel::Warning, formatLine(lines::kResimGateUnrecognised, word.c_str()));

	++tally.formats; // :330, the depth text inside the session line
	for (const int32 a : kInts)
		checkLine(tally, FString::Printf(TEXT("skip beyond %d ticks"), a), OldLevel::Log, formatLine(lines::kResimGateDepthSkip, a));

	++tally.formats; // :332-336, Warning
	for (const std::string& policy : kTokens)
		for (const std::string& source : kTokens)
			for (const std::string& depth : { std::string("off (inert under FrontierExact)"), std::string("skip beyond 12 ticks") })
				checkLine(tally, FString::Printf(TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
						*wide(policy), *wide(source), *wide(depth)),
					OldLevel::Warning, formatLine(lines::kResimGateSession, policy.c_str(), source.c_str(), depth.c_str()));

	++tally.formats; // :347-349, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			checkLine(tally, FString::Printf(TEXT("[LatencyKnobs] ini [OGNetcode] RedundancyFrames=%d out of range, clamped to %d"), a, b),
				OldLevel::Warning, formatLine(lines::kLatencyKnobsRedundancyClamped, a, b));

	++tally.formats; // :359-361, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			checkLine(tally, FString::Printf(TEXT("[LatencyKnobs] ini [OGNetcode] RelayResendEntries=%d out of range, clamped to %d"), a, b),
				OldLevel::Warning, formatLine(lines::kLatencyKnobsResendClamped, a, b));

	++tally.formats; // :367-371, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			for (const std::string& token : kTokens)
				checkLine(tally, FString::Printf(TEXT("[LatencyKnobs] redundancyFrames=%d relayResendEntries=%d (%s)"), a, b, *wide(token)),
					OldLevel::Warning, formatLine(lines::kLatencyKnobsSession, a, b, token.c_str()));

	++tally.formats; // :378-380, Warning
	for (const int32 a : kInts)
		for (const int32 b : kInts)
			checkLine(tally, FString::Printf(TEXT("[ClockRecentre.Config] ini [OGNetcode] RecentreSettleSamples=%d out of range, clamped to %d"), a, b),
				OldLevel::Warning, formatLine(lines::kRecentreClamped, a, b));

	++tally.formats; // :384-387, Warning
	for (const uint32 a : { 0u, 1u, 30u, 4294967295u })
		for (const std::string& token : kTokens)
			checkLine(tally, FString::Printf(TEXT("[ClockRecentre.Config] recentreSettleSamples=%u (%s)"), a, *wide(token)),
				OldLevel::Warning, formatLine(lines::kRecentreSession, a, token.c_str()));

	++tally.formats; // :394-395, Warning
	checkLine(tally, FString::Printf(TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier is not a finite number, keeping the compiled defaults")),
		OldLevel::Warning, formatLine(lines::kJitterNotFinite));

	++tally.formats; // :402-404, Warning
	for (const double a : kDoubles)
		for (const double b : kDoubles)
			checkLine(tally, FString::Printf(TEXT("[ClockSource.Config] ini [OGNetcode] JitterMultiplier=%.3f out of range, clamped to %.3f"), a, b),
				OldLevel::Warning, formatLine(lines::kJitterClamped, a, b));

	++tally.formats; // :434-437, Warning
	for (const std::string& word : kWords)
		checkLine(tally, FString::Printf(TEXT("[ClockSource.Config] ini [OGNetcode] ClockRttSource='%s' unrecognised (expected Engine or SimClock), keeping ")
				TEXT("the compiled default"), *wide(word)),
			OldLevel::Warning, formatLine(lines::kClockRttSourceUnrecognised, word.c_str()));

	++tally.formats; // :440-446, Warning
	for (const std::string& source : kTokens)
		for (const double a : kDoubles)
			for (const double b : { 0.5, 2.0, 0.0, 4.0, 1e308 })
				checkLine(tally, FString::Printf(TEXT("[ClockSource.Config] clockRttSource=%s (%s) jitterMultiplier engine=%.3f simClock=%.3f (%s)"),
						*wide(source), *wide(source), a, b, *wide(source)),
					OldLevel::Warning, formatLine(lines::kClockSourceSession, source.c_str(), source.c_str(), a, b, source.c_str()));

	++tally.formats; // :454, Warning
	checkLine(tally, FString::Printf(TEXT("[Dilation.Config] mode=n/a")), OldLevel::Warning, formatLine(lines::kDilationNotApplicable));

	++tally.formats; // :478-480, Warning
	for (const std::string& word : kWords)
		checkLine(tally, FString::Printf(TEXT("[Dilation.Config] ini [OGNetcode] ClockDilation='%s' unrecognised (expected Off, Shadow or On), running Off"), *wide(word)),
			OldLevel::Warning, formatLine(lines::kDilationUnrecognised, word.c_str()));

	++tally.formats; // :512-515, Warning
	for (const std::string& word : kWords)
		for (const char* setpoint : kModeWords)
			checkLine(tally, FString::Printf(TEXT("[Dilation.Config] ini [OGNetcode] ClockDilationSetpoint='%s' unrecognised (expected OwnRtt or Common), ")
					TEXT("running %hs"), *wide(word), setpoint),
				OldLevel::Warning, formatLine(lines::kDilationSetpointUnrecognised, word.c_str(), setpoint));

	++tally.formats; // :522-525, Warning
	for (const char* mode : kModeWords)
		for (const double a : kDoubles)
			for (const std::string& token : kTokens)
				checkLine(tally, FString::Printf(TEXT("[Dilation.Config] mode=%hs setpoint=%hs gainPerTick=%.4f maxDeviation=%.4f (%s)"),
						mode, mode, a, a / 3.0, *wide(token)),
					OldLevel::Warning, formatLine(lines::kDilationSession, mode, mode, a, a / 3.0, token.c_str()));

	++tally.formats; // :526-533, Warning
	for (const double a : kDoubles)
		for (const char* setpoint : kModeWords)
			checkLine(tally, FString::Printf(TEXT("[Dilation.Config] doors gainPerTick=%.4f (%s) maxPercent=%.3f (%s) targetOffsetTicks=%.3f (%s) setpoint=%hs (%s) ")
					TEXT("commonLeadTicks=%.3f (%s)"),
					a, TEXT("ini"), a * 100.0, TEXT("compiled"), -a, TEXT("ini"), setpoint, TEXT("compiled"), a + 3.65, TEXT("ini")),
				OldLevel::Warning, formatLine(lines::kDilationDoors, a, "ini", a * 100.0, "compiled", -a, "ini", setpoint, "compiled",
					a + 3.65, "ini"));

	const std::vector<std::string> doorKeys = { "ClockDilationGainPerTick", "ClockDilationMaxPercent",
		"ClockDilationTargetOffsetTicks", "ClockDilationCommonLeadTicks" };

	++tally.formats; // :544-545, Warning
	for (const std::string& key : doorKeys)
		checkLine(tally, FString::Printf(TEXT("[Dilation.Config] ini [OGNetcode] %s is not a finite number, keeping the compiled default"), *wide(key)),
			OldLevel::Warning, formatLine(lines::kDilationDoorNotFinite, key.c_str()));

	++tally.formats; // :551-552, Warning
	for (const std::string& key : doorKeys)
		for (const double a : kDoubles)
			for (const double b : kDoubles)
				checkLine(tally, FString::Printf(TEXT("[Dilation.Config] ini [OGNetcode] %s=%.4f out of range, clamped to %.4f"), *wide(key), a, b),
					OldLevel::Warning, formatLine(lines::kDilationDoorClamped, key.c_str(), a, b));

	CHECK(tally.formats == 23u);
	CHECK(tally.compared == 4271u);
}

TEST_CASE("SessionClockFormatParity.FStringPrintfEqualsSnprintfForEveryMovedLine", "[Session][SessionClock]")
{
	using namespace sessionKnobsFormatParityTest;
	namespace clockLines = ogsession::sessionClockLines;
	Tally tally;

	// Each old format is transcribed from OGSimulation dae3018's OGSimSession/SessionClock.h (og-session-runtime task 24 (a),
	// the verbatim copy of UESimulationSession.h at 0249ba8), at the line named; split literals are adjacent TEXT literals.
	const std::vector<uint8_t> tiers = { 0u, 1u, 2u, 3u, 4u, 7u, 255u };
	const std::vector<int32> deltas = { 0, 1, 2, 6, -1, -2, -6, std::numeric_limits<int32>::min() + 1,
		std::numeric_limits<int32>::max() };
	const std::vector<std::pair<const TCHAR*, const char*>> tags = { { TEXT("[ConnectionTier]"), clockLines::kConnectionTierTag },
		{ TEXT("[RelayDelayFloor]"), clockLines::kRelayDelayFloorTag } };

	++tally.formats; // :75, Warning: the dilation window's own line through %hs
	for (const std::string& window : { std::string("[Dilation.Window] mode=On setpoint=Common calls=1000 lead=3.512/3.650/3.790"),
			 std::string("[Dilation.Window] mode=Shadow setpoint=OwnRtt calls=0 frozenBy=sensor:0,recentre:0 stepLag=0.300"),
			 std::string("[Dilation.Window] 100% %d %s"), std::string(3000u, 'w') })
		checkLine(tally, FString::Printf(TEXT("%hs"), window.c_str()), OldLevel::Warning,
			formatLine(clockLines::kDilationWindow, window.c_str()));

	++tally.formats; // :91-92, Warning
	for (const uint64_t refused : { uint64_t{ 0u }, uint64_t{ 1u }, uint64_t{ 37u }, std::numeric_limits<uint64_t>::max() })
		for (const double scale : { 1.0, 0.99, 1.012345, 0.995, 1.000005, 1.0000049999, 0.0, -0.0 })
			checkLine(tally, FString::Printf(TEXT("[Dilation.Authority] refusedRateChanges=%llu rateScale=%.5f"),
					static_cast<unsigned long long>(refused), scale),
				OldLevel::Warning,
				formatLine(clockLines::kDilationAuthority, static_cast<unsigned long long>(refused), scale));

	++tally.formats; // :115-116, Log
	for (const uint8_t oldTier : tiers)
		for (const uint8_t newTier : tiers)
			for (const int32 delta : deltas)
				checkLine(tally, FString::Printf(TEXT("[ConnectionTier] tier %u -> %u, effective delay delta=%d"),
						(unsigned int)oldTier, (unsigned int)newTier, delta),
					OldLevel::Log,
					formatLine(clockLines::kConnectionTierChange, (unsigned int)oldTier, (unsigned int)newTier, delta));

	++tally.formats; // :168-170, Log: delta 0, each tag
	for (const auto& [oldTag, newTag] : tags)
		for (const int32 floorTicks : kInts)
			checkLine(tally, FString::Printf(TEXT("%s floor = %d ticks, effective delay delta=0, no stall or skip"), oldTag,
					floorTicks),
				OldLevel::Log, formatLine(clockLines::kNoTransition, newTag, floorTicks));

	++tally.formats; // :176-180, Warning: +n (stall) and -n (skip), each tag
	for (const auto& [oldTag, newTag] : tags)
		for (const int32 floorTicks : kInts)
			for (const int32 delta : deltas)
			{
				if (delta == 0)
					continue;
				checkLine(tally, FString::Printf(TEXT("%s floor = %d ticks, %s, requesting %d-tick prediction %s"),
						oldTag, floorTicks, delta > 0 ? TEXT("UPWARD") : TEXT("DOWNWARD"),
						delta > 0 ? delta : -delta,
						delta > 0 ? TEXT("stall") : TEXT("skip")),
					OldLevel::Warning,
					formatLine(clockLines::kTransition, newTag, floorTicks, delta > 0 ? clockLines::kUpward : clockLines::kDownward,
						delta > 0 ? delta : -delta, delta > 0 ? clockLines::kStall : clockLines::kSkip));
			}

	++tally.formats; // :149-151, Warning
	for (const uint8_t received : tiers)
		for (const int32 clamped : kInts)
			checkLine(tally, FString::Printf(TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
					(unsigned int)received, clamped),
				OldLevel::Warning, formatLine(clockLines::kRelayFloorClamped, (unsigned int)received, clamped));

	++tally.formats; // :193-195, Warning
	for (const int32 floorTicks : kInts)
		checkLine(tally, FString::Printf(TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and ")
				TEXT("above off (0) — the one value that is neither"), floorTicks),
			OldLevel::Warning, formatLine(clockLines::kRelayFloorBelowHiccupBaseline, floorTicks));

	++tally.formats; // :198-200, Log
	for (const int32 floorTicks : kInts)
		checkLine(tally, FString::Printf(TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness ")
				TEXT("mode active; tiers and LAN override are inert"), floorTicks),
			OldLevel::Log, formatLine(clockLines::kRelayFloorUniformD, floorTicks));

	++tally.formats; // :225-230, Log
	for (const int32 delay : kInts)
		for (const int32 tier : { 0, 1, 3, -1 })
			for (const int arrived : { 0, 1 })
				for (const int32 floorTicks : { 0, 3, 6, std::numeric_limits<int32>::max() })
					checkLine(tally, FString::Printf(TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, ")
							TEXT("arrived=%d, floor %d)"), delay, tier, arrived, floorTicks),
						OldLevel::Log, formatLine(clockLines::kAppliedInputDelay, delay, tier, arrived, floorTicks));

	++tally.formats; // :266-280, Warning
	const std::vector<double> milliseconds = { -1.0, 0.0, 0.0005, 41.2, 123456.789, 1e308, -0.0 };
	const std::vector<double> microseconds = { 0.0, -12617.4, 0.5, 1.5, -0.5, 2.5, 1e308, -1e308 };
	for (const bool simClock : { true, false })
		for (const double a : milliseconds)
			for (const double b : microseconds)
				for (const unsigned int samples : { 0u, 188u, std::numeric_limits<unsigned int>::max() })
					for (const uint64_t pings : { uint64_t{ 0u }, uint64_t{ 41u }, std::numeric_limits<uint64_t>::max() })
						checkLine(tally,
							FString::Printf(TEXT("[ClockSource.Window] source=%s engineRttMs=%.3f simClockRttMs=%.3f ackSrttMs=%.3f minRttOffsetUs=%.0f ")
								TEXT("owdVarUs=%.0f/%.0f smoothedRttMs=%.3f samples=%u invalid=%u pings=%llu pongs=%llu"),
								simClock ? TEXT("SimClock") : TEXT("Engine"), a, a * 0.5, -a, b, b * 2.0, -b, a, samples, samples / 2u,
								static_cast<unsigned long long>(pings), static_cast<unsigned long long>(pings / 3u)),
							OldLevel::Warning,
							formatLine(clockLines::kClockSourceWindow, simClock ? clockLines::kSourceSimClock : clockLines::kSourceEngine,
								a, a * 0.5, -a, b, b * 2.0, -b, a, samples, samples / 2u, static_cast<unsigned long long>(pings),
								static_cast<unsigned long long>(pings / 3u)));

	CHECK(tally.formats == 10u);
	CHECK(tally.compared == 2016u);
}

TEST_CASE("SessionNetHooksFormatParity.FStringPrintfEqualsSnprintfForTheReleaseLine", "[Session][SessionTickGame]")
{
	using namespace sessionKnobsFormatParityTest;
	Tally tally;

	// The old format is transcribed from OGSimulation ab09ccb's OGSimSession/SessionNetHooks.h :46 (og-session-runtime task 26 (a),
	// the verbatim copy of UESimulationSession.h at 06ad51c): OG_LOG_ROUTED(LogOGNet, ...), no level token (the route's Verbose).
	++tally.formats;
	for (const int32 first : kInts)
		for (const int32 steps : { 0, 1, 2, -1, std::numeric_limits<int32>::max() })
			checkLine(tally, FString::Printf(TEXT("[ReleaseBatch] first=%d numSteps=%d"), first, steps), OldLevel::Log,
				formatLine(ogsession::sessionNetHooksLines::kReleaseBatch, first, steps));

	CHECK(tally.formats == 1u);
	CHECK(tally.compared == 45u);
}

#endif
