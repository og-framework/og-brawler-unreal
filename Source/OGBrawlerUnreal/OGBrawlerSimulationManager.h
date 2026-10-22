// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerSimulationManager-rationale.md · docs/OGBrawlerSimulationManager-guards.md

#pragma once

#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"
#include "OGSimulationUnreal/ISimulationConnectionRelayListener.h"
#include "OGSimulationUnreal/ISimulationInputRelayListener.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include <memory>
#include <set>
#include <iterator>
#include <array>
#include <cstddef>
#include "Engine/World.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulationLogRoutes.h"
#include "OGSimulation/RenderSnapshot.h"
#include "OGSimulation/SimulatableList.h"
#include "OGSimulation/SystemsExecutor.h"
#include "OGBrawlerUnreal/PhysicsBackendHostUImpl.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/SimulationManagerConcept.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationInputResolution.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGSimulation/Network/ConnectionSlotKey.h"
#include "OGSimulation/Network/ConnectionTierTable.h"
#include "OGSimulation/Network/ServerInputDelayQueue.h"
#include "OGSimulation/Network/ServerReceptionCoordinator.h"
#include "OGSimulation/Network/RelayWritePathProbe.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulationUnreal/UESimulationSession.h"
#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"
#include "OGBrawlerUnreal/BrawlerSessionGameUImpl.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/BrawlerHitDetectionSystem.h"
#include "OGBrawler/BrawlerHitRoutingSystem.h"
#include "OGBrawler/SimCharacterId.h"
#include "OGBrawler/BrawlerRingoutSimulation.h"
#include "OGBrawler/BrawlerRingoutScoreSystem.h"

#include "OGSimulationUnreal/OGSimulationUnreal.h"
#include "OGSimulationUnreal/UESimulationLogRoutes.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"
#include "OGBrawlerUnreal/BrawlerCompositionUImpl.h"
#include "OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h"
#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGBrawlerUnreal/BrawlerRosterUImpl.h"

#include "OGBrawlerSimulationManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

struct FMovementStaticDataCVars
{
	brawlerMovementSimulation::MovementModel model;
	float    maxWalkSpeed;
	uint32_t stepPeriodTicks;
	float    stepSpeed;
	float    gravity;
};

// ⛔G-01  docs/OGBrawlerSimulationManager-guards.md
using BrawlerInputProviderFn = std::function<simulatableBrawler::PlayerInput(
	const SimulationTimeStep&,
	const LocalInputCache<simulatableBrawler::PlayerInput>&)>;

class UOGBrawlerSimulatableComponent;
class AOGBrawlerSimulationManager;
class ASimulationTimingRelay;
class ASimulationConnectionRelay;
class AOGBrawlerUECharacter;

extern template void UESimulationSession<BrawlerSessionTraitsUImpl>::onInputRelayHostReady(ASimulationInputRelay& host);

UCLASS()
class AOGBrawlerSimulationManager : public AActor
{
    GENERATED_BODY()

public:
    AOGBrawlerSimulationManager();

    static AOGBrawlerSimulationManager* instanceFor(bool isAuthority)
    {
        UESimulationSession<BrawlerSessionTraitsUImpl>* session =
            UESimulationSession<BrawlerSessionTraitsUImpl>::instanceFor(isAuthority);
        return session != nullptr ? &session->owner() : nullptr;
    }

    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    const UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>& getLatencyBudgetDiagnostics() const { return m_session.getLatencyBudgetDiagnostics(); }
    UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>&       editLatencyBudgetDiagnostics()       { return m_session.editLatencyBudgetDiagnostics(); }

    TryRegisterStatus tryRegister(
        SimCharacterId simId,
        SimulatableBrawler simulatable,
        UOGBrawlerSimulatableComponent& owner,
        BrawlerInputProviderFn inputProvider,
        bool isAuthority);

    void unregisterFromNewFramework(SimCharacterId simId, UOGBrawlerSimulatableComponent& owner, bool isAuthority);

    const BrawlerRosterUImpl& roster() const { return m_roster; }
    BrawlerRosterUImpl&       roster()       { return m_roster; }

    const BrawlerInputHistoryFeedUImpl& inputHistoryFeed() const { return m_inputHistoryFeed; }
    BrawlerInputHistoryFeedUImpl&       inputHistoryFeed()       { return m_inputHistoryFeed; }

    const UESimulationSession<BrawlerSessionTraitsUImpl>& session() const { return m_session; }
    UESimulationSession<BrawlerSessionTraitsUImpl>&       session()       { return m_session; }

    void noteDelayedInputComponent(SimCharacterId simId, UOGBrawlerSimulatableComponent& component);

private:
    friend class BrawlerSessionGameUImpl;

    BrawlerRosterUImpl m_roster;

    BrawlerInputHistoryFeedUImpl m_inputHistoryFeed{ std::as_const(m_session) };

    static FMovementStaticDataCVars readMovementStaticDataCVars();

    BrawlerSessionGameUImpl m_game{ m_roster, m_inputHistoryFeed };

    UESimulationSession<BrawlerSessionTraitsUImpl> m_session{ *this, m_game };

    static constexpr bool compositionContractsHold()
    {
        static_assert(std::is_same_v<BrawlerInputProviderFn, UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>::InputProvider>,
            "tryRegister hands a client's BrawlerInputProviderFn to UESimulationSession::tryRegister, whose parameter is "
            "the latency member's InputProvider; the session keeps it in the pending record, hands it to "
            "UELatencyBudgetDiagnostics::wrapInputProvider and registers what that returns. The two spellings must name "
            "ONE std::function type: a different one would convert, wrapping the provider in a second heap-allocated "
            "callable on every call. Added by og-tickdriver task 18; the hand-off re-stated by "
            "og-ue-simulationcomponentandmanagersplit task 28.");
        return true;
    }

    void assertCompositionContracts()
    {
        static_assert(compositionContractsHold());
    }
};