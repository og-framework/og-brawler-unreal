// SPDX-License-Identifier: MPL-2.0
#if WITH_LOW_LEVEL_TESTS

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include "CoreMinimal.h"
#include "catch_amalgamated.hpp"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulation/TickDriverWindow.h"

namespace tickDriverFormatParityTest
{
    constexpr uint32 kMaxStepsPerFrame = 240u;
    constexpr size_t kSeriesCount = tickDriverWindow::kSeriesCount;

    struct PercentilesUImpl
    {
        uint32 n   = 0u;
        double p50 = 0.0;
        double p99 = 0.0;
        double max = 0.0;
    };

    PercentilesUImpl percentilesOf(std::vector<float>& values)
    {
        PercentilesUImpl result;
        result.n = static_cast<uint32>(values.size());
        if (result.n == 0u)
            return result;

        std::sort(values.begin(), values.end());
        result.p50 = values[latencyBudget::nearestRankIndex(result.n, 500u)];
        result.p99 = values[latencyBudget::nearestRankIndex(result.n, 990u)];
        result.max = values.back();
        return result;
    }

    FString formatPercentiles(std::vector<float>& values, bool twoDecimals)
    {
        const PercentilesUImpl p = percentilesOf(values);
        if (p.n == 0u)
            return TEXT("-");
        return twoDecimals ? FString::Printf(TEXT("%.2f/%.2f/%.2f"), p.p50, p.p99, p.max)
                           : FString::Printf(TEXT("%.1f/%.1f/%.1f"), p.p50, p.p99, p.max);
    }

    struct ValueSet
    {
        std::string                                   name;
        bool                                          isAuthority = false;
        bool                                          stepsInline = true;
        double                                        nowSeconds  = 0.0;
        TickDriverWindowCounts                        counts;
        std::array<std::vector<float>, kSeriesCount>  samples;
        std::array<uint32, kSeriesCount>              overflow{};
        TickDriverWindowTotals                        atLog;
        double                                        dtSeconds     = 1.0 / 60.0;
        int32                                         requestedCap  = 60;
        uint32                                        cap           = 60u;
        bool                                          onOwnerThread = true;
        uint64                                        physicsStep   = 0u;
        uint32                                        batch         = 1u;
    };

    std::vector<float>& series(ValueSet& set, TickDriverWindowSeries which)
    {
        return set.samples[static_cast<size_t>(which)];
    }

    std::string utf8(const FString& line)
    {
        return std::string(TCHAR_TO_UTF8(*line));
    }

    std::string oldWindowLine(ValueSet set)
    {
        const bool m_isAuthority = set.isAuthority;
        const bool m_stepsInline = set.stepsInline;
        const TickDriverWindowCounts& counts = set.counts;
        const double nowSeconds = set.nowSeconds;
        const LostTime lost = set.atLog.lost;
        uint32 overflow = 0u;
        for (const uint32 seriesOverflow : set.overflow)
            overflow += seriesOverflow;

        const FString replayDepth = counts.replays == 0u
            ? FString(TEXT("-"))
            : FString::Printf(TEXT("%.2f/%u"),
                static_cast<double>(counts.replayedTicks) / static_cast<double>(counts.replays), counts.maxReplayDepth);
        const uint32 costSamples = static_cast<uint32>(series(set, TickDriverWindowSeries::StepUs).size());
        const uint32 preTicks    = static_cast<uint32>(series(set, TickDriverWindowSeries::PreUs).size());
        const uint32 restores    = static_cast<uint32>(series(set, TickDriverWindowSeries::ShadowRestoreUs).size());

        return utf8(FString::Printf(
            TEXT("[SimHost.Window] role=%s stepping=%s seconds=%.2f frames=%u frameMs=%s startPhysicsUs=%s endWaits=%u ")
            TEXT("endWaitUs=%s postPhysicsUs=%s steps=%llu maxStepsPerFrame=%u lostSteps=%llu lostSeconds=%.4f ")
            TEXT("ignoredTimeSamples=%llu j1Drops=%llu j2Waits=%u j2WaitUs=%s costSamples=%u stepUs=%s preTicks=%u preUs=%s ")
            TEXT("replayDepth=%s resimRefused=%u j6Drops=%llu shadowRestores=%u shadowRestoreUs=%s shadowRefused=%u ")
            TEXT("sampleOverflow=%u"),
            m_isAuthority ? TEXT("Authority") : TEXT("Client"),
            m_stepsInline ? TEXT("inline") : TEXT("worker"),
            nowSeconds - counts.startSeconds, counts.frames,
            *formatPercentiles(series(set, TickDriverWindowSeries::FrameMs), true),
            *formatPercentiles(series(set, TickDriverWindowSeries::StartPhysicsUs), false),
            counts.endWaits,
            *formatPercentiles(series(set, TickDriverWindowSeries::EndWaitUs), false),
            *formatPercentiles(series(set, TickDriverWindowSeries::PostPhysicsUs), false),
            static_cast<unsigned long long>(counts.steps), counts.maxStepsPerFrame,
            static_cast<unsigned long long>(lost.steps - counts.atStart.lost.steps),
            lost.seconds - counts.atStart.lost.seconds,
            static_cast<unsigned long long>(set.atLog.ignoredTimeSamples - counts.atStart.ignoredTimeSamples),
            static_cast<unsigned long long>(set.atLog.j1Drops - counts.atStart.j1Drops),
            counts.j2Waits,
            *formatPercentiles(series(set, TickDriverWindowSeries::J2WaitUs), false),
            costSamples,
            *formatPercentiles(series(set, TickDriverWindowSeries::StepUs), false),
            preTicks,
            *formatPercentiles(series(set, TickDriverWindowSeries::PreUs), false),
            *replayDepth, counts.resimRefused,
            static_cast<unsigned long long>(set.atLog.j6Drops - counts.atStart.j6Drops),
            restores,
            *formatPercentiles(series(set, TickDriverWindowSeries::ShadowRestoreUs), false),
            counts.shadowRefused, overflow));
    }

    std::string oldCapWarningLine(const ValueSet& set)
    {
        const int32 requestedCap = set.requestedCap;
        const uint32 cap = set.cap;
        return utf8(FString::Printf(
            TEXT("[SimHost.Frame] og.Sim.MaxCatchUpSteps=%d is outside [1, %u]; using %u"),
            requestedCap, kMaxStepsPerFrame, cap));
    }

    std::string oldFrameLine(const ValueSet& set)
    {
        const bool m_isAuthority = set.isAuthority;
        const bool m_stepsInline = set.stepsInline;
        const uint32 cap = set.cap;
        return utf8(FString::Printf(
            TEXT("[SimHost.Frame] role=%s stepping=%s dt=%.6f maxCatchUpSteps=%u rateScale=[0.75,1.25] timeBase=worldDelta"),
            m_isAuthority ? TEXT("Authority") : TEXT("Client"),
            m_stepsInline ? TEXT("inline") : TEXT("worker"),
            set.dtSeconds, cap));
    }

    std::string oldFirstStepLine(const ValueSet& set)
    {
        const bool m_isAuthority = set.isAuthority;
        return utf8(FString::Printf(
            TEXT("[SimHost.FirstStep] role=%s thread=%s physicsStep=%llu batch=%u"),
            m_isAuthority ? TEXT("Authority") : TEXT("Client"),
            set.onOwnerThread ? TEXT("game") : TEXT("worker"),
            static_cast<unsigned long long>(set.physicsStep), set.batch));
    }

    std::string newWindowLine(const ValueSet& set)
    {
        TickDriverWindow window(set.isAuthority, set.stepsInline);
        window.restart(set.counts.startSeconds, set.counts.atStart);
        for (size_t index = 0u; index < kSeriesCount; ++index)
        {
            const TickDriverWindowSeries which = static_cast<TickDriverWindowSeries>(index);
            for (const float value : set.samples[index])
                window.samples(which).add(static_cast<double>(value));
            for (uint32 extra = 0u; extra < set.overflow[index]; ++extra)
                window.samples(which).add(0.0);
        }
        window.counts() = set.counts;
        return window.formatWindowLine(set.nowSeconds, set.atLog);
    }

    std::string newCapWarningLine(const ValueSet& set)
    {
        TickDriverWindow window(set.isAuthority, set.stepsInline);
        return window.formatCatchUpCapWarningLine(set.requestedCap, kMaxStepsPerFrame, set.cap);
    }

    std::string newFrameLine(const ValueSet& set)
    {
        TickDriverWindow window(set.isAuthority, set.stepsInline);
        return window.formatFrameLine(set.dtSeconds, set.cap);
    }

    std::string newFirstStepLine(const ValueSet& set)
    {
        TickDriverFirstStepLine firstStep(set.isAuthority);
        return firstStep.format(set.onOwnerThread, set.physicsStep, set.batch);
    }

    void fillAll(ValueSet& set, std::initializer_list<float> values)
    {
        for (std::vector<float>& samples : set.samples)
            samples.assign(values.begin(), values.end());
    }

    TickDriverWindowTotals totals(uint64 lostSteps, double lostSeconds, uint64 ignored, uint64 j1Drops, uint64 j6Drops)
    {
        TickDriverWindowTotals result;
        result.lost.steps         = lostSteps;
        result.lost.seconds       = lostSeconds;
        result.ignoredTimeSamples = ignored;
        result.j1Drops            = j1Drops;
        result.j6Drops            = j6Drops;
        return result;
    }

    struct Lcg
    {
        uint64 state;

        uint64 next()
        {
            state = state * 6364136223846793005ull + 1442695040888963407ull;
            return state >> 11;
        }

        double unit() { return static_cast<double>(next() & ((1ull << 52) - 1ull)) / static_cast<double>(1ull << 52); }
    };

    std::vector<ValueSet> valueSets()
    {
        constexpr uint32 kU32Max = std::numeric_limits<uint32>::max();
        constexpr uint64 kU64Max = std::numeric_limits<uint64>::max();
        std::vector<ValueSet> sets;

        {
            ValueSet set;
            set.name = "zeros, empty sample lists";
            set.requestedCap = 0;
            set.cap = 1u;
            set.dtSeconds = 0.0;
            set.batch = 0u;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "empty sample lists, non-zero counts and totals";
            set.isAuthority = true;
            set.nowSeconds = 12.25;
            set.counts.startSeconds = 2.0;
            set.counts.frames = 600u;
            set.counts.steps = 600u;
            set.counts.maxStepsPerFrame = 2u;
            set.counts.atStart = totals(1u, 0.5, 2u, 3u, 4u);
            set.atLog = totals(4u, 0.5625, 5u, 3u, 9u);
            set.requestedCap = 999;
            set.cap = 240u;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "sub-microsecond samples";
            set.stepsInline = false;
            set.nowSeconds = 10.0;
            fillAll(set, { 0.001f, 0.004f, 0.005f, 0.0049f, 0.05f, 0.049f, 0.06f, 0.0f });
            set.dtSeconds = 1.0e-7;
            set.onOwnerThread = false;
            set.physicsStep = 1u;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "binary ties at the rounding digit";
            set.isAuthority = true;
            set.nowSeconds = 10.125;
            fillAll(set, { 0.125f, 0.375f, 0.625f, 2.5f, 0.25f, 0.75f, 1.005f, 0.015f, 1.125f });
            set.counts.replays = 8u;
            set.counts.replayedTicks = 1u;
            set.counts.maxReplayDepth = 1u;
            set.atLog.lost.seconds = 0.00005;
            set.dtSeconds = 0.0000005;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "large counts";
            set.nowSeconds = 1.0e9;
            set.counts.frames = 1000000000u;
            set.counts.steps = 1000000000000000ull;
            set.counts.maxStepsPerFrame = 240u;
            set.counts.endWaits = 123456789u;
            set.counts.j2Waits = 987654321u;
            set.counts.replays = 3u;
            set.counts.replayedTicks = 10u;
            set.counts.maxReplayDepth = 5u;
            set.counts.resimRefused = 4000000000u;
            set.counts.shadowRefused = 4000000001u;
            set.atLog = totals(5000000000ull, 123456.789, 7000000000ull, 8000000000ull, 9000000000ull);
            fillAll(set, { 1.0e6f, 2.5e7f, 3.0e8f });
            set.physicsStep = 123456789012345ull;
            set.batch = 15u;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "UINT64 and UINT32 maxima";
            set.isAuthority = true;
            set.counts.frames = kU32Max;
            set.counts.steps = kU64Max;
            set.counts.maxStepsPerFrame = kU32Max;
            set.counts.endWaits = kU32Max;
            set.counts.j2Waits = kU32Max;
            set.counts.replays = kU32Max;
            set.counts.replayedTicks = kU64Max;
            set.counts.maxReplayDepth = kU32Max;
            set.counts.resimRefused = kU32Max;
            set.counts.shadowRefused = kU32Max;
            set.atLog = totals(kU64Max, DBL_MAX, kU64Max, kU64Max, kU64Max);
            set.nowSeconds = DBL_MAX;
            fillAll(set, { FLT_MAX, -FLT_MAX, FLT_MIN, -FLT_MIN });
            set.requestedCap = std::numeric_limits<int32>::max();
            set.cap = kU32Max;
            set.dtSeconds = DBL_MAX;
            set.physicsStep = kU64Max;
            set.batch = kU32Max;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "deltas that wrap and go negative";
            set.stepsInline = false;
            set.nowSeconds = 1.0;
            set.counts.startSeconds = 5.0;
            set.counts.atStart = totals(10u, 2.0, 10u, 10u, 10u);
            set.atLog = totals(3u, 1.0, 0u, 9u, 0u);
            set.requestedCap = std::numeric_limits<int32>::min();
            set.dtSeconds = -1.0 / 60.0;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "denormal and negative samples";
            fillAll(set, { 1.0e-45f, -1.0e-45f, -0.05f, -0.049f, -123.456f, 7.0e-39f });
            set.nowSeconds = -0.005;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "one full series, one overflow";
            set.nowSeconds = 10.0;
            std::vector<float>& frameMs = series(set, TickDriverWindowSeries::FrameMs);
            for (uint32 index = 0u; index < tickDriverWindow::kMaxSamplesPerSeries; ++index)
                frameMs.push_back(static_cast<float>(index % 97u) * 0.37f);
            set.overflow[static_cast<size_t>(TickDriverWindowSeries::FrameMs)] = 1u;
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "every series full, uneven overflow";
            set.isAuthority = true;
            set.nowSeconds = 10.0;
            for (size_t index = 0u; index < kSeriesCount; ++index)
            {
                for (uint32 sample = 0u; sample < tickDriverWindow::kMaxSamplesPerSeries; ++sample)
                    set.samples[index].push_back(static_cast<float>((sample * 31u + index) % 5003u) * 1.37f);
                set.overflow[index] = static_cast<uint32>(index * 3u + 1u);
            }
            sets.push_back(set);
        }
        {
            ValueSet set;
            set.name = "one sample in each series, two in some";
            set.stepsInline = false;
            set.nowSeconds = 10.000001;
            fillAll(set, { 16.6667f });
            series(set, TickDriverWindowSeries::StepUs).push_back(812.25f);
            series(set, TickDriverWindowSeries::PreUs).push_back(33.35f);
            set.counts.replays = 1u;
            set.counts.replayedTicks = 7u;
            set.counts.maxReplayDepth = 7u;
            sets.push_back(set);
        }

        Lcg lcg{ 0x5eedf00du };
        for (int index = 0; index < 16; ++index)
        {
            ValueSet set;
            set.name = "pseudo-random set " + std::to_string(index);
            set.isAuthority = (lcg.next() & 1u) != 0u;
            set.stepsInline = (lcg.next() & 1u) != 0u;
            set.counts.startSeconds = lcg.unit() * 1.0e4;
            set.nowSeconds = set.counts.startSeconds + 10.0 + lcg.unit() * 0.1;
            set.counts.frames = static_cast<uint32>(lcg.next() % 2000u);
            set.counts.steps = lcg.next() % 5000u;
            set.counts.maxStepsPerFrame = static_cast<uint32>(lcg.next() % 241u);
            set.counts.endWaits = static_cast<uint32>(lcg.next() % 600u);
            set.counts.j2Waits = static_cast<uint32>(lcg.next() % 10u);
            set.counts.replays = static_cast<uint32>(lcg.next() % 50u);
            set.counts.replayedTicks = set.counts.replays * (lcg.next() % 9u);
            set.counts.maxReplayDepth = static_cast<uint32>(lcg.next() % 9u);
            set.counts.resimRefused = static_cast<uint32>(lcg.next() % 3u);
            set.counts.shadowRefused = static_cast<uint32>(lcg.next() % 3u);
            set.counts.atStart = totals(lcg.next() % 100u, lcg.unit(), lcg.next() % 100u, lcg.next() % 4u, lcg.next() % 4u);
            set.atLog = totals(set.counts.atStart.lost.steps + lcg.next() % 20u, set.counts.atStart.lost.seconds + lcg.unit(),
                set.counts.atStart.ignoredTimeSamples + lcg.next() % 20u, set.counts.atStart.j1Drops,
                set.counts.atStart.j6Drops + lcg.next() % 3u);
            for (std::vector<float>& samples : set.samples)
            {
                const uint64 count = lcg.next() % 60u;
                const double scale = std::pow(10.0, static_cast<double>(lcg.next() % 9u) - 2.0);
                for (uint64 sample = 0u; sample < count; ++sample)
                    samples.push_back(static_cast<float>(lcg.unit() * scale));
            }
            set.dtSeconds = 1.0 / static_cast<double>(30u + lcg.next() % 211u);
            set.requestedCap = static_cast<int32>(lcg.next() % 400u) - 100;
            set.cap = static_cast<uint32>(std::clamp<int32>(set.requestedCap, 1, static_cast<int32>(kMaxStepsPerFrame)));
            set.onOwnerThread = (lcg.next() & 1u) != 0u;
            set.physicsStep = lcg.next();
            set.batch = static_cast<uint32>(lcg.next() % 241u);
            sets.push_back(set);
        }
        return sets;
    }
} // namespace tickDriverFormatParityTest

using namespace tickDriverFormatParityTest;

TEST_CASE("TickDriverFormatParity.FStringPrintfEqualsFormatHostLine", "[TickDriverWindow]")
{
    const std::vector<ValueSet> sets = valueSets();
    REQUIRE(sets.size() >= 20u);

    uint32 compared = 0u;
    for (const ValueSet& set : sets)
    {
        INFO(set.name);
        CHECK(newWindowLine(set) == oldWindowLine(set));
        CHECK(newFrameLine(set) == oldFrameLine(set));
        CHECK(newCapWarningLine(set) == oldCapWarningLine(set));
        CHECK(newFirstStepLine(set) == oldFirstStepLine(set));
        compared += 4u;
    }
    CHECK(compared == 4u * static_cast<uint32>(sets.size()));
}

#endif // WITH_LOW_LEVEL_TESTS
