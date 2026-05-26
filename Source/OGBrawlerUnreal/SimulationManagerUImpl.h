// SPDX-License-Identifier: BUSL-1.1

#include "OGSimulationUnreal/ISimulationTimingRelayListener.h"

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include <unordered_map>
#include <functional>
#include <optional>
#include "Engine/World.h"
#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackInput.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationManager.h"
#include "OGSimulationUnreal/PCTimeManagement/ChaosTickMapper.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulation/SimulationManagerConcept.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/SimulationReconciliation.h"
#include "OGSimulation/SimulationNetSync.h"
#include "OGSimulation/SimulationIntegrationExecutor.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/SimulatableBrawler.h"

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h"

#include "SimulationManagerUImpl.generated.h"

// Rare simulation lifecycle: TimeResync.*, Resim.*, ResimCheck.Divergence, ResimCheck.PrepareRestore
DECLARE_LOG_CATEGORY_EXTERN(LogOGSim, Log, All);
// Per-tick simulation chatter: AuthoritySimulation, ClientPrediction, CollectInput, ResimCheck.*
DECLARE_LOG_CATEGORY_EXTERN(LogOGSimTick, Log, All);
// Manager / simulatable lifecycle: SimulationManager:*, tryRegister:*, NewFramework:*
DECLARE_LOG_CATEGORY_EXTERN(LogOGMgmt, Log, All);
// Replication-channel events: ServerReceive, Send*ToClients, ReceiveCorrection*, InjectCorrection*
DECLARE_LOG_CATEGORY_EXTERN(LogOGNet, Log, All);
// Fallback for unrecognized prefixes
DECLARE_LOG_CATEGORY_EXTERN(LogOG, Log, All);
// Game-rule logging (DAttackMachine/Radial/Guard via OGBLOG_G)
DECLARE_LOG_CATEGORY_EXTERN(LogOGBrawler, Log, All);

enum class TryRegisterStatus { Pending, Ready };

class USimmableUpdateComponent;
class ASimulationManagerUImpl;
class ASimulationTimingRelay;

struct FSimulationState2 : public Chaos::FSimCallbackOutput
{
	FSimulationState2()
		: FSimCallbackOutput()
		, bIsValid(false)
	{
	}

	virtual ~FSimulationState2() {}

public:
	void Reset() { bIsValid = false; }
	void Copy(const FSimulationState2& Value);
	bool IsValid() const { return bIsValid; }
	bool bIsValid;
	float DeltaTime;
};

struct FSimulationInput2 : public Chaos::FSimCallbackInput
{
	virtual ~FSimulationInput2() {}

	void Reset()
	{
		bInitialized = false;
		m_world = nullptr;
		m_manager = nullptr;
	}

	bool bInitialized = false;
	UWorld* m_world;
	ASimulationManagerUImpl* m_manager = nullptr;

private:
};

class ASimulationManagerUImpl;

class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FSimulationInput2,
	FSimulationState2,
	Chaos::ESimCallbackOptions::Presimulate |
	Chaos::ESimCallbackOptions::ContactModification |
	Chaos::ESimCallbackOptions::Rewind |
	Chaos::ESimCallbackOptions::PostSolve>
{
public:
	virtual FName GetFNameForStatId() const override;

	void setManager(ASimulationManagerUImpl* manager)
	{
		m_manager = manager;
	}

private:
	virtual void OnPreSimulate_Internal() override;
	virtual void OnPostSolve_Internal() override;
	virtual void ProcessInputs_Internal(int32 PhysicsStep);
	virtual void ProcessInputs_External(int32 PhysicsStep);

	virtual int32 TriggerRewindIfNeeded_Internal(int32 LastCompletedStep);

	virtual void ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input) override;

	virtual void FirstPreResimStep_Internal(int32 PhysicsStep);

	ASimulationManagerUImpl* m_manager = nullptr;
};

UCLASS()
class ASimulationManagerUImpl : public AActor, public ISimulationTimingRelayListener
{
    GENERATED_BODY()

public:
    ASimulationManagerUImpl();
    ~ASimulationManagerUImpl();

    // Slot 0 = authority world's manager (dedicated server / listen server / standalone).
    // Slot 1 = non-authority (pure client) world's manager.
    // PIE single-process mode can fill both concurrently — one per world.
    static ASimulationManagerUImpl* instanceFor(bool isAuthority)
    {
        return isAuthority ? s_instances[0] : s_instances[1];
    }

    // Clock accessors — forwarded from the manager (type-erased here for callers outside the template).
    const ServerTickClock& getServerClock() const { return m_manager->getServerClock(); }
    ServerTickClock& editServerClock()             { return m_manager->editServerClock(); }
    const ClientPredictionClock& getClientClock() const { return m_manager->getClientClock(); }
    bool runsPrediction() const { return m_manager->runsPrediction(); }
    void onGameSimulation(const SimulationUpdateInfo& info) { m_manager->onGameSimulation(info); }
    void onPostGameSimulation(const SimulationUpdateInfo& info) { m_manager->onPostGameSimulation(info); }
    unsigned int onCheckIsSimilar() { return m_manager->onCheckIsSimilar(); }
    void prepareResimulation(int32_t chaosStep, uint32_t simTick) { m_manager->prepareResimulation(chaosStep, simTick); }
    // Defined in .cpp — requires full USimmableUpdateComponent definition for NetSync template instantiation.
    void onPostSimulationGameThread();

    virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // SimulationManagerOwnerConcept members — timing buffer forwarded through relay.
    FSmallSimulationStateSyncBuffer& getSyncedTimingBuffer();
    void setOnTimingInfoReceivedCallback(std::function<void(uint32_t, double)> fn)
    {
        m_onTimingInfoReceivedCallback = std::move(fn);
    }

    // Called from ASimulationTimingRelay::OnRep_Buffer on clients via ISimulationTimingRelayListener.
    virtual void onTimingInfoReceived(uint32_t authorityTick, double roundTripTime) override
    {
        if (m_onTimingInfoReceivedCallback)
        {
            m_onTimingInfoReceivedCallback(authorityTick, roundTripTime);
            return;
        }
        if (!m_manager.has_value())
            return;
        m_manager->editNetworkEstimator().updateRTT(roundTripTime);
        m_manager->editNetworkEstimator().recordAuthorityTick(authorityTick);
    }

    void OnPhysicsPreTick(FPhysScene* Scene, float DeltaTime);
    void OnPhysicsStep(FPhysScene* Scene, float DeltaTime);
    void OnPostPhysicsStep(FChaosScene* Scene);

    TryRegisterStatus tryRegister(
        unsigned int id,
        SimulatableBrawler simulatable,
        USimmableUpdateComponent& owner,
        std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider,
        bool isAuthority);

    void unregisterFromNewFramework(unsigned int id, USimmableUpdateComponent& owner, bool isAuthority);

    void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);

    ChaosTickMapper& editChaosTickMapper() { return m_chaosTickMapper; }
    const ChaosTickMapper& getChaosTickMapper() const { return m_chaosTickMapper; }

    ChaosPhysicsBodyAdapter&  editPhysicsBodyAdapter() { return m_physAdapter.value(); }
    ChaosSpatialQueryAdapter& editQueryAdapter()       { return m_queryAdapter.value(); }
    SimulationObjectStorage<SimulatableBrawler>& editStorage() { return m_storage; }

private:
    FSimulationManagerAsyncCallback* m_asyncCallback;

    FDelegateHandle m_injectInputsExternalCallbackHandle;
    FDelegateHandle m_hysScenePostTickCallbackHandle;

    std::function<void(uint32_t, double)> m_onTimingInfoReceivedCallback;

    ASimulationTimingRelay* m_timingRelay = nullptr;
    ASimulationTimingRelay* findTimingRelay();

    // Adapters require the Chaos solver — emplaced in BeginPlay.
    std::optional<ChaosPhysicsBodyAdapter>   m_physAdapter;
    std::optional<ChaosSpatialQueryAdapter>  m_queryAdapter;

    // Simulation peers — construction order: storage → reconciliation → netSync.
    SimulationObjectStorage<SimulatableBrawler>  m_storage;
    SimulationReconciliation<SimulatableBrawler> m_reconciliation{ m_storage };
    SimulationNetSync<SimulatableBrawler>        m_netSync{ m_storage, m_reconciliation };

    // Integration layer and manager require adapters — emplaced in BeginPlay.
    using IntegrationLayerType = SimulationIntegrationExecutor<
        simulatableBrawler::StaticData, ChaosPhysicsBodyAdapter, ChaosSpatialQueryAdapter, SimulatableBrawler>;
    std::optional<IntegrationLayerType> m_integrationLayer;

    using ManagerType = SimulationManager<IntegrationLayerType, decltype(m_netSync), decltype(m_reconciliation)>;
    std::optional<ManagerType> m_manager;

    static ASimulationManagerUImpl* s_instances[2];

    ChaosTickMapper m_chaosTickMapper;

    struct PendingRegistration
    {
        std::optional<SimulatableBrawler> simulatable;
        bool bodiesCreated = false;
        BodyId parentBodyId;
        std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider;
        bool isAuthority = false;
    };
    std::unordered_map<unsigned int, PendingRegistration> m_pendingRegistrations;
};