// SPDX-License-Identifier: BUSL-1.1

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
// OGBrawlerSimulationManager.h pulls in all peer headers, adapter headers, and owner traits.
#include "OGBrawlerUnreal/OGBrawlerSimulationManager.h"

// ---------------------------------------------------------------------------
// Compile-time concept checks
// ---------------------------------------------------------------------------

// SimulationManagerOwnerConcept (a writable timing buffer and the timing callback) was asserted
// on the session here from og-ue-simulationcomponentandmanagersplit task 25. Since og-input-latency
// task P5-7 the server clock is og-sim-net's ClockPublish message (SimNetServer::publishClock), so
// the session holds no timing buffer and no longer models that concept.

static_assert(
    SimulationNetSyncConcept<
        SimulationNetSync<SimulatableBrawler>,
        SimulatableBrawler>,
    "SimulationNetSync<SimulatableBrawler> must satisfy SimulationNetSyncConcept");

// [item 87] The resolution peer's own SimulatableTs-exact concept
// (SimulationInputResolution.h) — the same adapter-side proof pattern this
// task's other three assertions already use, extended to the new peer.
static_assert(
    SimulationInputResolutionConcept<
        SimulationInputResolution<SimulatableBrawler>,
        SimulatableBrawler>,
    "SimulationInputResolution<SimulatableBrawler> must satisfy SimulationInputResolutionConcept");

static_assert(
    SimulationReconciliationConcept<
        SimulationReconciliation<SimulatableBrawler>,
        SimulatableBrawler>,
    "SimulationReconciliation<SimulatableBrawler> must satisfy SimulationReconciliationConcept");

static_assert(
    SimulationIntegrationExecutorConcept<
        SimulationIntegrationExecutor<simulatableBrawler::StaticData, EngineTraits::BodyAdapter, EngineTraits::QueryAdapter, SimulatableBrawler>>,
    "SimulationIntegrationExecutor must satisfy SimulationIntegrationExecutorConcept");

// ---------------------------------------------------------------------------
// Runtime placeholder — concept checks are compile-time only
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FOGBrawlerSimulationManagerConceptTest,
    "OGBrawlerUnreal.OGBrawlerSimulationManager.ConceptSatisfied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FOGBrawlerSimulationManagerConceptTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("SimulationNetSyncConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationInputResolutionConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationReconciliationConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationIntegrationExecutorConcept satisfied at compile time"), true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
