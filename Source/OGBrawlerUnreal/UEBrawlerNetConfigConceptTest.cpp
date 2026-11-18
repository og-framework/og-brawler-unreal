// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"

#include <concepts>
#include <functional>
#include <type_traits>

// ---------------------------------------------------------------------------
// Compile anchor for the UE NetConfig binding (task 2 of
// og-netcode-v2-arch-latency; proposal §2.1 / D3.2).
//
// WHY THIS FILE EXISTS: it was written while nothing in the game included the
// header-only UEBrawlerNetConfig.h. UBT compiles .cpp files — a header no TU
// includes is never parsed, so the `static_assert(NetConfig<UEBrawlerNetConfig>)`
// inside it would have been dead text. Production includes it now
// (BrawlerSessionTraitsUImpl.h), so that guarantee no longer depends on this TU;
// what this TU still adds is the per-requirement assertions on the Address and
// the runtime sentinel check below.
//
// Mirrors the existing OGBrawlerSimulationManagerConceptTest.cpp pattern, with one
// deliberate difference: the static_asserts sit OUTSIDE the automation-test
// guard so Server and Client targets (which build with
// WITH_DEV_AUTOMATION_TESTS=0) verify the concept too, not just Editor.
// ---------------------------------------------------------------------------

// The binding satisfies the concept. (Also asserted inside the header; repeated
// here so this file's purpose is legible on its own.)
static_assert(NetConfig<UEBrawlerNetConfig>,
    "UEBrawlerNetConfig must satisfy the NetConfig concept");

// The Address contract, asserted directly on the handle so a regression in
// ogsimnet::SimNetConnectionHandle points at the specific broken requirement
// rather than at the aggregate concept failure. (The handle is og-sim-net's since
// og-input-latency task P5-7; it was the UE FUEConnectionHandle before, deleted by P5-9.)
static_assert(std::is_same_v<UEBrawlerNetConfig::Address, ogsimnet::SimNetConnectionHandle>);
static_assert(std::regular<ogsimnet::SimNetConnectionHandle>);
static_assert(std::default_initializable<ogsimnet::SimNetConnectionHandle>);
static_assert(std::is_convertible_v<
    decltype(std::hash<ogsimnet::SimNetConnectionHandle>{}(std::declval<const ogsimnet::SimNetConnectionHandle&>())),
    std::size_t>);
static_assert(std::is_convertible_v<
    decltype(std::declval<const ogsimnet::SimNetConnectionHandle&>().isAlive()),
    bool>);

// Coherence with the engine-free binding used by the Catch2 suite and with
// TimeConfig::tickFrequency. If a future task retunes the tick rate, all three
// must move together.
static_assert(UEBrawlerNetConfig::tickFrequencyHz == 60);

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUEBrawlerNetConfigConceptTest,
    "OGBrawlerUnreal.UEBrawlerNetConfig.ConceptSatisfied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUEBrawlerNetConfigConceptTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("NetConfig<UEBrawlerNetConfig> satisfied at compile time"), true);

    // Runtime coverage the static_asserts cannot give: the sentinel. A default
    // handle names no connection (kInvalidSimConnection, no liveness table), so it
    // is never alive, and every default handle compares equal: a server-local
    // character has no wire, and all such characters collapse to one table key.
    const ogsimnet::SimNetConnectionHandle sentinel{};
    TestFalse(TEXT("Sentinel handle is not alive"), sentinel.isAlive());
    TestTrue(TEXT("Sentinel equals a default-constructed handle"), sentinel == ogsimnet::SimNetConnectionHandle{});

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
