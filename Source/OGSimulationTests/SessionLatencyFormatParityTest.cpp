// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS

#include <cfloat>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "CoreMinimal.h"
#include "catch_amalgamated.hpp"
#include "OGSimSession/SessionLatencyDiagnostics.h"

namespace sessionLatencyFormatParityTest
{
namespace lines = ogsession::sessionLatencyLines;
using latencyBudget::Hop;
using latencyBudget::HopSummary;
using latencyBudget::WindowReport;

std::string utf8(const FString& text)
{
	return std::string(TCHAR_TO_UTF8(*text));
}

FString throughTheSink(const char* line)
{
	return FString::Printf(TEXT("%s"), ANSI_TO_TCHAR(line));
}

FString oldHopLine(const TCHAR* role, const HopSummary& hop)
{
	constexpr double kMillis = 1000.0;
	if (hop.hop == Hop::H6L)
	{
		return FString::Printf(
			TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
			TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u healedByUse=%u healedByCorrection=%u"),
			role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
			hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
			hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow, hop.healedByUse, hop.healedByCorrection);
	}
	return FString::Printf(
		TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
		TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u"),
		role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
		hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
		hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow);
}

FString oldWindowLine(const TCHAR* role, const WindowReport& report, uint64 mailboxDropped)
{
	return FString::Printf(
		TEXT("[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u ")
		TEXT("mailboxDropped=%llu"),
		role, report.windowEndSeconds - report.windowStartSeconds, report.staleStamps,
		report.duplicateStamps, report.laneOverflow,
		static_cast<unsigned long long>(mailboxDropped));
}

const std::vector<double> kSeconds = { 0.0, -0.0, 0.0000001, 0.0000004999, 0.0000005, 0.0000015, 0.00013, 0.0011234,
	0.0099995, 0.1, 0.123456789, 1.0, 9.99, 150.052, 12345.678, 1e9, -0.25, DBL_MAX, -DBL_MAX,
	std::numeric_limits<double>::infinity() };
const std::vector<uint32> kCounts = { 0u, 1u, 7u, 599u, 4096u, 65535u, 2147483647u, 4294967295u };
const std::vector<uint64> kDropped = { 0ull, 1ull, 4096ull, 4294967296ull, 18446744073709551615ull };
const std::vector<double> kWindowSpans = { 0.0, 0.005, 0.0049999, 9.999, 10.0, 10.004999, 10.005, 10.01, 123.456, 1e12 };
} // namespace sessionLatencyFormatParityTest

using namespace sessionLatencyFormatParityTest;

TEST_CASE("SessionLatencyFormatParity.FStringPrintfEqualsSnprintfForTheThreeWindowFormats", "[Session][SessionLatency]")
{
	// UELatencyBudgetDiagnostics.h at root 7d58a26: :495 (role), :502-507 (the H6L line), :510-515 (the hop line),
	// :517-522 (the window line, mailboxDropped through its unsigned long long cast)
	uint32 compared = 0u;
	char line[lines::kLineBytes];
	for (const bool isClient : { true, false })
	{
		const TCHAR* oldRole = isClient ? TEXT("Client") : TEXT("Server");
		const char* newRole = isClient ? lines::kClient : lines::kServer;
		for (std::size_t hopIndex = 0u; hopIndex < latencyBudget::kHopCount; ++hopIndex)
		{
			for (std::size_t valueIndex = 0u; valueIndex < kSeconds.size(); ++valueIndex)
			{
				HopSummary hop;
				hop.hop = static_cast<Hop>(hopIndex);
				hop.p50 = kSeconds[valueIndex];
				hop.p95 = kSeconds[(valueIndex + 3u) % kSeconds.size()];
				hop.p99 = kSeconds[(valueIndex + 7u) % kSeconds.size()];
				hop.max = kSeconds[(valueIndex + 11u) % kSeconds.size()];
				hop.n                  = kCounts[valueIndex % kCounts.size()];
				hop.noStart            = kCounts[(valueIndex + 1u) % kCounts.size()];
				hop.noEnd              = kCounts[(valueIndex + 2u) % kCounts.size()];
				hop.outOfOrder         = kCounts[(valueIndex + 3u) % kCounts.size()];
				hop.overflow           = kCounts[(valueIndex + 4u) % kCounts.size()];
				hop.healedByUse        = kCounts[(valueIndex + 5u) % kCounts.size()];
				hop.healedByCorrection = kCounts[(valueIndex + 6u) % kCounts.size()];
				lines::formatHop(line, newRole, hop);
				const FString old = oldHopLine(oldRole, hop);
				INFO("old: " << utf8(old));
				INFO("new: " << line);
				CHECK(utf8(throughTheSink(line)) == utf8(old));
				++compared;
			}
		}
		for (const double span : kWindowSpans)
		{
			for (std::size_t countIndex = 0u; countIndex < kCounts.size(); ++countIndex)
			{
				for (const uint64 dropped : kDropped)
				{
					WindowReport report;
					report.windowStartSeconds = 1000.0;
					report.windowEndSeconds   = 1000.0 + span;
					report.staleStamps        = kCounts[countIndex];
					report.duplicateStamps    = kCounts[(countIndex + 3u) % kCounts.size()];
					report.laneOverflow       = kCounts[(countIndex + 5u) % kCounts.size()];
					lines::formatWindow(line, newRole, report, dropped);
					const FString old = oldWindowLine(oldRole, report, dropped);
					INFO("old: " << utf8(old));
					INFO("new: " << line);
					CHECK(utf8(throughTheSink(line)) == utf8(old));
					++compared;
				}
			}
		}
	}
	CHECK(compared == 2u * (latencyBudget::kHopCount * kSeconds.size() + kWindowSpans.size() * kCounts.size() * kDropped.size()));
}

#endif // WITH_LOW_LEVEL_TESTS
