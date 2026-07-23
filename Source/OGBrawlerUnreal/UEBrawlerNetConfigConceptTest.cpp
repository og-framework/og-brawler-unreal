// SPDX-License-Identifier: BUSL-1.1

#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"

#include <concepts>
#include <functional>
#include <type_traits>

// ---------------------------------------------------------------------------
// Compile anchor for the UE NetConfig binding (task 2 of
// og-netcode-v2-arch-latency; proposal §2.1 / D3.2).
//
// WHY THIS FILE EXISTS: UEBrawlerNetConfig.h and UEConnectionHandle.h are
// header-only and, until T4/T5 wire up real consumers, nothing in the game
// includes them. UBT compiles .cpp files — a header no TU includes is never
// parsed, so the `static_assert(NetConfig<UEBrawlerNetConfig>)` inside
// UEBrawlerNetConfig.h would be dead text and the task's "build fails if the
// concept is broken" guarantee would be vacuous. This TU includes the header
// and thereby makes that guarantee real. Delete it once a production consumer
// includes the config.
//
// Mirrors the existing SimulationManagerUImplConceptTest.cpp pattern, with one
// deliberate difference: the static_asserts sit OUTSIDE the automation-test
// guard so Server and Client targets (which build with
// WITH_DEV_AUTOMATION_TESTS=0) verify the concept too, not just Editor.
// ---------------------------------------------------------------------------

// The binding satisfies the concept. (Also asserted inside the header; repeated
// here so this file's purpose is legible on its own.)
static_assert(NetConfig<UEBrawlerNetConfig>,
    "UEBrawlerNetConfig must satisfy the NetConfig concept");

// The Address contract, asserted directly on the UE handle so a regression in
// FUEConnectionHandle points at the specific broken requirement rather than at
// the aggregate concept failure.
static_assert(std::is_same_v<UEBrawlerNetConfig::Address, FUEConnectionHandle>);
static_assert(std::regular<FUEConnectionHandle>);
static_assert(std::default_initializable<FUEConnectionHandle>);
static_assert(std::is_convertible_v<
    decltype(std::hash<FUEConnectionHandle>{}(std::declval<const FUEConnectionHandle&>())),
    std::size_t>);
static_assert(std::is_convertible_v<
    decltype(std::declval<const FUEConnectionHandle&>().isAlive()),
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

    // Runtime coverage the static_asserts cannot give: the sentinel path.
    // FromRootOf(nullptr) must yield the default handle rather than crash —
    // this is the standalone / local-PIE-host case called out in
    // UEConnectionHandle.h, and the one place the helper is reachable without
    // a live net driver.
    const FUEConnectionHandle sentinel = FUEConnectionHandle::FromRootOf(nullptr);
    TestFalse(TEXT("Sentinel handle is not alive"), sentinel.isAlive());
    TestTrue(TEXT("Sentinel equals a default-constructed handle"), sentinel == FUEConnectionHandle{});
    TestNull(TEXT("GetRootNetConnection(nullptr) is null"), GetRootNetConnection(nullptr));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
