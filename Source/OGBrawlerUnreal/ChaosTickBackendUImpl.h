// SPDX-License-Identifier: BUSL-1.1
// docs/ChaosTickBackendUImpl-rationale.md · docs/ChaosTickBackendUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackInput.h"
#include "Runtime/Experimental/Chaos/Public/Chaos/SimCallbackObject.h"

#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsBodyState.h"
#include "OGSimulation/QueryGeometry.h"
#include "OGSimulation/LatencyBudgetProbe.h"
#include "OGSimulationUnreal/UELatencyBudgetDiagnostics.h"
#include "OGSimulation/SimulationManager.h"
#include "OGSimulation/SimulationObjectStorage.h"
#include "OGSimulation/PCTimeManagement/ServerTickClock.h"
#include "OGSimulation/PCTimeManagement/ClientPredictionClock.h"
#include "OGSimulationUnreal/PCTimeManagement/ChaosTickMapper.h"
#include "OGBrawler/SimulatableBrawler.h"
#include "OGSimulationUnreal/ChaosEngineTraitsUImpl.h"

#include <cstdint>
#include <optional>
#include <type_traits>

#if OG_LATENCY_BUDGET_DIAGNOSTICS
DECLARE_LOG_CATEGORY_EXTERN(LogOGChaosDilation, Log, All);
#endif

class ASimulationManagerUImpl;
class ChaosTickBackendUImpl;
class USimmableUpdateComponent;
class AOGBrawlerUECharacter;
class AActor;

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
	ChaosTickBackendUImpl* m_manager = nullptr;

private:
};

struct FRewindPushProbeStashedBody
{
	PhysicsBodyState pushed;
	PhysicsBodyState beforePush;
	BodyId           bodyId{};
	unsigned int     simulatableId = 0;
	// ⛔G-01  docs/ChaosTickBackendUImpl-guards.md
	const char*      bodyName = nullptr;
	bool             wireCarriesRotationAndSpin = false;
	bool             movedAtPush = false;
};

class FSimulationManagerAsyncCallback : public Chaos::TSimCallbackObject<
	FSimulationInput2,
	FSimulationState2,
	Chaos::ESimCallbackOptions::Presimulate |
	Chaos::ESimCallbackOptions::Rewind |
	Chaos::ESimCallbackOptions::PostSolve>
{
public:
	virtual FName GetFNameForStatId() const override;

	void setManager(ChaosTickBackendUImpl* manager)
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

	ChaosTickBackendUImpl* m_manager = nullptr;

	TArray<FRewindPushProbeStashedBody> m_pushProbeStash;
	int32    m_pushProbeStashFrame      = INDEX_NONE;
	uint32_t m_pushProbeStashSimTick    = 0;
	int32    m_pushProbeStashBodies     = 0;
	int32    m_pushProbeStashUnresolved = 0;
	int32    m_pushProbeStashSkipped    = 0;

	void readPushProbeVerdict_Internal();
	void discardPushProbeStash(const TCHAR* reason);

	template <typename InputT, typename OutputT, Chaos::ESimCallbackOptions OptionsT>
	static constexpr Chaos::ESimCallbackOptions registeredOptionsOf(
		const Chaos::TSimCallbackObject<InputT, OutputT, OptionsT>*) { return OptionsT; }

	template <typename Self>
	static constexpr bool everyRegisteredHookIsOverridden()
	{
		using Opt = Chaos::ESimCallbackOptions;
		constexpr Opt options = registeredOptionsOf(static_cast<const Self*>(nullptr));
		static_assert(!EnumHasAnyFlags(options, Opt::Presimulate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreSimulate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::Presimulate but does not "
			"override OnPreSimulate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PreIntegrate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreIntegrate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PreIntegrate but does not "
			"override OnPreIntegrate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PostIntegrate)
			|| requires { requires std::is_same_v<decltype(&Self::OnPostIntegrate_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PostIntegrate but does not "
			"override OnPostIntegrate_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::MidPhaseModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnMidPhaseModification_Internal), void (Self::*)(Chaos::FMidPhaseModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::MidPhaseModification but does not "
			"override OnMidPhaseModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::CCDModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnCCDModification_Internal), void (Self::*)(Chaos::FCCDModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::CCDModification but does not "
			"override OnCCDModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::StrainModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnStrainModification_Internal), void (Self::*)(Chaos::FStrainModifierAccessor&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::StrainModification but does not "
			"override OnStrainModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::ContactModification)
			|| requires { requires std::is_same_v<decltype(&Self::OnContactModification_Internal), void (Self::*)(Chaos::FCollisionContactModifier&)>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::ContactModification but does not "
			"override OnContactModification_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PreSolve)
			|| requires { requires std::is_same_v<decltype(&Self::OnPreSolve_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PreSolve but does not "
			"override OnPreSolve_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		static_assert(!EnumHasAnyFlags(options, Opt::PostSolve)
			|| requires { requires std::is_same_v<decltype(&Self::OnPostSolve_Internal), void (Self::*)()>; },
			"FSimulationManagerAsyncCallback registers ESimCallbackOptions::PostSolve but does not "
			"override OnPostSolve_Internal. Each option bit adds this object to one more Chaos dispatch "
			"list, and the base hook is check(false): the first time that list runs, the process "
			"asserts. Add the override in the same edit, or drop the bit. Was the 'OPTION SET "
			"IS A CONTRACT' prose fence of SimulationManagerUImpl.h (task 11).");
		return true;
	}

	void assertEveryRegisteredHookIsOverridden()
	{
		static_assert(everyRegisteredHookIsOverridden<FSimulationManagerAsyncCallback>());
	}
};

struct ChaosTickBackendWorldsUImpl
{
};

class ChaosTickBackendUImpl
{
public:
	ChaosTickBackendUImpl(ASimulationManagerUImpl& owner, ChaosTickBackendWorldsUImpl&) : m_owner(owner) {}

	ChaosTickBackendUImpl(const ChaosTickBackendUImpl&) = delete;
	ChaosTickBackendUImpl& operator=(const ChaosTickBackendUImpl&) = delete;

	void buildWorld(UWorld& world, float dt, bool worldIsAuthority);
	void startStepping(UWorld& world, float dt, bool worldIsAuthority);
	void onEndPlayBegin() {}
	void onEndPlayWorld(UWorld& world);
	void beforePresent_GameThread() {}
	void updateVisualization_GameThread();

	ChaosEngineTraitsUImpl::VizQuery&        editVizQuery();
	const ChaosEngineTraitsUImpl::VizReader& getVizReader() const;
	std::optional<QueryVolumeId> registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner);

	bool createBodies_GameThread(USimmableUpdateComponent& owner, AOGBrawlerUECharacter& character,
		SimulatableBrawler& simulatable, unsigned int id);
	void onRegistered_GameThread(USimmableUpdateComponent&, unsigned int) {}
	void onUnregistered_GameThread(USimmableUpdateComponent&, unsigned int) {}

	class Diagnostics
	{
	public:
		explicit Diagnostics(ChaosTickBackendUImpl& host) : m_host(host) {}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
		void sampleLatencyWindow_GameThread(bool isClient);
		void logLatencyWindow_GameThread(bool isClient);
		void stampLatencyAfterStep_Internal(double stepStartSeconds);
		void stampLatencyStepEnd_Internal();
#else
		void sampleLatencyWindow_GameThread(bool) {}
		void logLatencyWindow_GameThread(bool) {}
		void stampLatencyAfterStep_Internal(double) {}
		void stampLatencyStepEnd_Internal() {}
#endif

	private:
		[[maybe_unused]] ChaosTickBackendUImpl& m_host;
	};
	Diagnostics editDiagnostics() { return Diagnostics(*this); }

	const ServerTickClock& getServerClock() const;
	const ClientPredictionClock& getClientClock() const;
	bool runsPrediction() const;
	SimulationObjectStorage<SimulatableBrawler>& editStorage();

	void onGameSimulation(const SimulationUpdateInfo& info);
	void onPostGameSimulation(const SimulationUpdateInfo& info);
	unsigned int onCheckIsSimilar();
	void prepareResimulation(int32_t chaosStep, uint32_t simTick);
	void noteResimRequest(unsigned int anchorTick, int32 lastCompletedStep, int32 requestedChaosFrame);
	void noteResimGrant(int32 grantedChaosFrame);

	void InjectInputs_External(int32 PhysicsStep, int32 NumSteps);

	ChaosTickMapper& editChaosTickMapper() { return m_chaosTickMapper; }
	const ChaosTickMapper& getChaosTickMapper() const { return m_chaosTickMapper; }

private:
	ASimulationManagerUImpl& m_owner;

	FSimulationManagerAsyncCallback* m_asyncCallback = nullptr;

	FDelegateHandle m_injectInputsExternalCallbackHandle;
	FDelegateHandle m_hysScenePostTickCallbackHandle;

	ChaosTickMapper m_chaosTickMapper;

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	// ⛔G-11  docs/ChaosTickBackendUImpl-guards.md
	latencyBudget::ScalarWindowStats m_chaosDilationWindow;
	std::optional<int32>             m_lastPhysicsTickOffset;
	uint32                           m_physicsTickOffsetResets = 0u;
#endif
};
