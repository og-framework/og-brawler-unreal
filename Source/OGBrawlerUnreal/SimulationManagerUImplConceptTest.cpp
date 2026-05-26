// SPDX-License-Identifier: BUSL-1.1

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
// SimulationManagerUImpl.h pulls in all peer headers, adapter headers, and owner traits.
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"

// ---------------------------------------------------------------------------
// Compile-time concept checks
// ---------------------------------------------------------------------------

static_assert(SimulationManagerOwnerConcept<ASimulationManagerUImpl>,
    "ASimulationManagerUImpl must satisfy SimulationManagerOwnerConcept");

static_assert(
    SimulationNetSyncConcept<
        SimulationNetSync<SimulatableBrawler>,
        SimulatableBrawler>,
    "SimulationNetSync<SimulatableBrawler> must satisfy SimulationNetSyncConcept");

static_assert(
    SimulationReconciliationConcept<
        SimulationReconciliation<SimulatableBrawler>,
        SimulatableBrawler>,
    "SimulationReconciliation<SimulatableBrawler> must satisfy SimulationReconciliationConcept");

static_assert(
    SimulationIntegrationExecutorConcept<
        SimulationIntegrationExecutor<simulatableBrawler::StaticData, ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter, SimulatableBrawler>>,
    "SimulationIntegrationExecutor must satisfy SimulationIntegrationExecutorConcept");

// ---------------------------------------------------------------------------
// Runtime placeholder — concept checks are compile-time only
// ---------------------------------------------------------------------------
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FSimulationManagerUImplConceptTest,
    "OGBrawlerUnreal.SimulationManagerUImpl.ConceptSatisfied",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSimulationManagerUImplConceptTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("SimulationManagerOwnerConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationNetSyncConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationReconciliationConcept satisfied at compile time"), true);
    TestTrue(TEXT("SimulationIntegrationExecutorConcept satisfied at compile time"), true);
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
