// SPDX-License-Identifier: BUSL-1.1

#include "SimmableUpdateComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/Declares.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/DAttackRadialVisualization.h"
#include "OGBrawler/BrawlerProjectileVisualization.h"
#include "OGBrawler/DAttackAimVisualization.h"
#include "OGSimulation/DMathUtil.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"

//#include "Runtime/Core/Public/Logging/StructeredLog.h"
//#include "Logging/StructeredLogFormat.h"
#include "Logging/LogMacros.h"


#include "Chaos/Box.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/Vector.h"
#include "Math/Vector.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Experimental/ChaosCore/Public/Chaos/Real.h"
#include "Runtime/Experimental/ChaosCore/Public/Chaos/Vector.h"
#include "Runtime/Experimental/ChaosCore/Public/Chaos/Core.h"
#include "Runtime/Engine/Public/Physics/PhysicsFiltering.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosEngineInterface.h"
#include "PBDRigidsSolver.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Physics/Experimental/PhysScene_Chaos.h"

#include "OGBrawlerUnreal/DAttackCircleUImplementation.h"
#include "OGBrawlerUnreal/DShapeUImplementation.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/DAttackCircularVisualizationUimpl.h"
#include "OGSimulationUnreal/LoggingFunctorUImpl.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "OGSimulationUnreal/InputRedundancyBundleBuilder.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include <stdexcept>
#include <variant>

#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"   // GEngine->AddOnScreenDebugMessage (Task 11 build-mismatch toast)



#pragma optimize( "", off )

namespace DAttackFakeInputCVars
{
	bool rightAttackInput = false;
	static FAutoConsoleVariableRef CRightAttackInput(
		TEXT("DAttackFakeInput.RightAttack"),
		rightAttackInput,
		TEXT("test\n")
		TEXT("test)"),
		ECVF_Default);

	bool leftAttackInput = false;
	static FAutoConsoleVariableRef CLeftAttackInputCVar(
		TEXT("DAttackFakeInput.LightAttack"),
		leftAttackInput,
		TEXT("test\n")
		TEXT("test)"),
		ECVF_Default);	
	
}
namespace DAttackRadialSimulationCVars
{
	int attackSegments = 16;
	static FAutoConsoleVariableRef attackSegmentsCVar(
		TEXT("DAttackRadialSimulation.attackSegments"),
		attackSegments,
		TEXT("test)"),
		ECVF_Default);	
	
	float innerRadius = 90.f;
	static FAutoConsoleVariableRef innerRadiusCVar(
		TEXT("DAttackRadialSimulation.innerRadius"),
		innerRadius,
		TEXT("test)"),
		ECVF_Default);	
	
	float outerRadius = 300.f;
	static FAutoConsoleVariableRef outerRadiusCVar(
		TEXT("DAttackRadialSimulation.outerRadius"),
		outerRadius,
		TEXT("test)"),
		ECVF_Default);

	float thickness = 70.f;
	static FAutoConsoleVariableRef thicknessCVar(
		TEXT("DAttackRadialSimulation.thickness"),
		thickness,
		TEXT("test)"),
		ECVF_Default);

	bool offsetWithSegmentHalf = true;
	static FAutoConsoleVariableRef offsetWithSegmentHalfCVar(
		TEXT("DAttackRadialSimulation.offsetWithSegmentHalf"),
		offsetWithSegmentHalf,
		TEXT("test)"),
		ECVF_Default);

	float forwardRangeMultiplier = 1.f;
	static FAutoConsoleVariableRef forwardRangeMultiplierCVar(
		TEXT("DAttackRadialSimulation.forwardRangeMultiplier"),
		forwardRangeMultiplier,
		TEXT("test)"),
		ECVF_Default);
}

namespace DAttackRadialVisualizationCVars
{
	bool loggingEnabled = false;
	static FAutoConsoleVariableRef loggingEnabledCVar(
		TEXT("DAttackRadialVisualization.loggingEnabled"),
		loggingEnabled,
		TEXT("test)"),
		ECVF_Default);
}

//Component
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ~10 seconds at 60 Hz — generous enough for scene startup, small enough to fail loudly on genuine bugs.
constexpr int32 kMaxRegistrationAttempts = 600;

USimmableUpdateComponent::USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	m_staticData.emplace();



	PrimaryComponentTick.SetTickFunctionEnable(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// This component is designed to tick after the LiveLink component, which uses TG_PrePhysics
	// We also use a tick prerequisite on LiveLink components, so technically this could also use TG_PrePhysics
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;
	//AddTickPrerequisiteComponent(LiveLinkComponent);


}


void USimmableUpdateComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	// Manager may not yet exist (ordering race between ASimulationManagerUImpl::BeginPlay
	// and character BeginPlay). Poll via SetTimerForNextTick until it is available, then
	// do all manager-dependent setup and kick off registration polling.
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { tryInitializeWithManager(); }));
	}

}

void USimmableUpdateComponent::tryInitializeWithManager()
{
	++m_initializationAttempts;

	// World-level authority — same predicate used when picking the manager's slot
	// in ASimulationManagerUImpl::BeginPlay. HasAuthority() on non-replicated
	// actors is always true and would disagree with the world-mode gate.
	const bool isAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* manager = ASimulationManagerUImpl::instanceFor(isAuthority);
	AActor* Owner = GetOwner();
	if (manager == nullptr)
	{
		if (m_initializationAttempts >= kMaxRegistrationAttempts)
			checkf(false, TEXT("USimmableUpdateComponent: manager never spawned after %d attempts (netmode=%d)"),
				kMaxRegistrationAttempts, (int)GetNetMode());
		if (UWorld* world = GetWorld())
		{
			world->GetTimerManager().SetTimerForNextTick(
				FTimerDelegate::CreateWeakLambda(this, [this]() { tryInitializeWithManager(); }));
		}
		return;
	}

	if (Owner == nullptr)
		return;

	if (AOGBrawlerUECharacter* brawlerOwner = Cast<AOGBrawlerUECharacter>(Owner))
		m_ownerInputCollection = brawlerOwner->getInputCollection();

	ChaosSpatialQueryAdapter& queryAdapter = manager->editQueryAdapter();

	{
		FCollisionQueryParams queryParams;
		queryParams.bTraceComplex = false;
		queryParams.AddIgnoredActor(Owner);

		QueryVolumeDescriptor targetVisDescriptor{
			SphereGeometry{m_staticData->m_attackCircle.getOuterRadius() * 2.f},
			collisionCategory::bodyAndGuard,
			glm::mat4(1.f),
			collisionCategory::queryRouting};

		m_targetVisualizationVolumeIds.push_back(
			queryAdapter.registerVolume(targetVisDescriptor, queryParams, FActorInstanceHandle(Owner)));
	}
	m_attackTargetVisualizationState.emplace(m_targetVisualizationVolumeIds);
	m_attackAimVisualizationState.emplace();

	// Kick off body creation + resolvability polling via manager->tryRegister.
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { tryRegisterWithNewFramework(); }));
	}
}

void USimmableUpdateComponent::scheduleNextRegistrationAttempt()
{
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { tryRegisterWithNewFramework(); }));
	}
}

void USimmableUpdateComponent::tryRegisterWithNewFramework()
{
	++m_registrationAttempts;

	const bool isAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* regManager = ASimulationManagerUImpl::instanceFor(isAuthority);
	if (regManager == nullptr)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false, TEXT("DeferredReg: manager never spawned after %d attempts (id=%u)"),
				kMaxRegistrationAttempts, GetUniqueID());
		scheduleNextRegistrationAttempt();
		return;
	}

	// On a pure client only the locally-controlled character (ROLE_AutonomousProxy)
	// feeds a live input provider. Simulated proxies of other players must NOT
	// register a provider — collectInputAll then falls through to the cache's
	// getLastCorrectionInput, which predicts the remote character forward with the
	// input the server last reported.
	const AActor* ownerActor = GetOwner();
	const bool isLocallyPredicted = !isAuthority
		&& ownerActor != nullptr
		&& ownerActor->GetLocalRole() == ROLE_AutonomousProxy;

	std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider;
	if (isLocallyPredicted)
	{
		UOGBrawlerInputCollectionComponent* ic = m_ownerInputCollection;
		const uint32 id = (unsigned int)GetUniqueID();
		inputProvider = [ic, id, mgr = regManager](const SimulationTimeStep& step) {
			return ic->buildPlayerInput(step, id, mgr);
		};
	}

	SimulatableBrawler newSimulatable(*m_staticData);

	const TryRegisterStatus status = regManager->tryRegister(
		(unsigned int)GetUniqueID(),
		std::move(newSimulatable),
		*this,
		std::move(inputProvider),
		isAuthority);

	if (status == TryRegisterStatus::Pending)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: id=%u not ready after %d attempts"),
				GetUniqueID(), kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	UE_LOG(LogOGMgmt, Log,
		TEXT("NewFramework: registered id=%u isAuthority=%d isLocallyPredicted=%d"),
		GetUniqueID(), isAuthority ? 1 : 0, isLocallyPredicted ? 1 : 0);
}

void USimmableUpdateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const bool isAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* manager = ASimulationManagerUImpl::instanceFor(isAuthority);
	if (manager != nullptr)
		manager->unregisterFromNewFramework((unsigned int)GetUniqueID(), *this, isAuthority);

	UActorComponent::EndPlay(EndPlayReason);
}


void USimmableUpdateComponent::OnRep_CorrectionState()
{
	// Stage 1 (Task 11) — wire-format compat fence (client side). Once a mismatch
	// has been detected, no-op every subsequent OnRep so a mismatched peer cannot
	// drive local correction logic.
	if (m_wireFormatMismatchDetected)
		return;

	// Version byte detection: the buffer's NetSerialize captured the sender's
	// wire-format version byte (byte 0 of the wire payload). If it disagrees with
	// what this build expects, refuse to process and surface a loud build-mismatch
	// error + one-time on-screen toast (risks_and_plan.md §5.2).
	const uint8 expectedVersion = FSimulationStateSyncBuffer::kWireFormatVersion;
	const uint8 wireVersion = m_simulationStateCorrectionSyncedBuffer.getReceivedWireFormatVersion();
	if (wireVersion != expectedVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on OnRep_CorrectionState: server version=%u, client expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)wireVersion, (unsigned int)expectedVersion);

		if (GEngine != nullptr)
		{
			GEngine->AddOnScreenDebugMessage(
				kWireFormatMismatchToastKey, /*TimeToDisplay*/ 15.f, FColor::Red,
				TEXT("Build mismatch — please get the latest archive (see PLAYTEST_PORTABLE_README.md)"));
		}

		m_wireFormatMismatchDetected = true;
		return;
	}

	{
		simulatableBrawler::State peeked;
		const uint32 tick = m_simulationStateCorrectionSyncedBuffer.readInto(peeked);
		UE_LOG(LogOGNet, Log,
			TEXT("[ReceiveCorrectionState] id=%u tick=%u"),
			(unsigned int)GetUniqueID(), tick);
	}
	if (m_onCorrectionStateReceivedCallback)
		m_onCorrectionStateReceivedCallback(m_simulationStateCorrectionSyncedBuffer);
}

void USimmableUpdateComponent::OnRep_CorrectionInput()
{
	{
		simulatableBrawler::PlayerInput peeked;
		const uint32 tick = m_replicatedInputSyncedBuffer.readInto(peeked);
		UE_LOG(LogOGNet, Log,
			TEXT("[ReceiveCorrectionInput] id=%u tick=%u attackLeft=%d"),
			(unsigned int)GetUniqueID(), tick,
			peeked.get<dAttackRadialSimulation::PlayerInput>().attackLeft ? 1 : 0);
	}
	if (m_onCorrectionInputReceivedCallback)
		m_onCorrectionInputReceivedCallback(m_replicatedInputSyncedBuffer);
}

void USimmableUpdateComponent::sendLocalInputToAuthority(
	const PendingInputQueue<simulatableBrawler::PlayerInput>& queue,
	uint32 currentTick,
	uint32 redundancyDepth)
{
	// Build the redundancy bundle from the most-recent `redundancyDepth` ticks
	// (clamped to kMaxSlots inside the builder) and fire the unreliable RPC.
	FInputRedundancyBundle bundle;
	buildRedundancyBundle<simulatableBrawler::PlayerInput>(
		queue, currentTick, static_cast<uint8>(redundancyDepth), bundle);
	ServerReceiveRemoteMove(bundle);
}

void USimmableUpdateComponent::ServerReceiveRemoteMove_Implementation(const FInputRedundancyBundle& bundle)
{
	// Stage 1 (Task 11) — wire-format compat fence (server side).
	// An empty bundle (the client had no pending input in the redundancy window)
	// carries no wire header and therefore no version byte; getWireFormatVersion()
	// returns 0 for it. That is normal idle traffic, NOT a mismatch — skip it
	// silently (forEachSlot is a no-op on it anyway) so we don't log a false
	// mismatch every idle frame.
	if (bundle.wireBytes.Num() == 0)
		return;

	// Version byte detection: refuse to process a bundle whose wire-format version
	// disagrees with this build (a pre/post-Stage-1 mismatch). Surface a loud error
	// and early-return — do NOT disconnect the client here; the disconnect path is
	// engine-managed and is a Stage 6 dedicated-server-validation concern
	// (risks_and_plan.md §5.2).
	const uint8 clientVersion = bundle.getWireFormatVersion();
	if (clientVersion != FInputRedundancyBundle::kWireFormatVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on ServerReceiveRemoteMove: client version=%u, server expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)clientVersion, (unsigned int)FInputRedundancyBundle::kWireFormatVersion);
		return;
	}

	// Dispatch each redundancy slot to the per-slot inbound callback registered by
	// SimulationNetSync. Capture-tick dedup on the queue side lands in Task 10.
	bundle.forEachSlot<simulatableBrawler::PlayerInput>(
		[this](uint32 captureTick, const simulatableBrawler::PlayerInput& input)
		{
			UE_LOG(LogOGNet, Log,
				TEXT("[ServerReceive] id=%u tick=%u attackLeft=%d"),
				(unsigned int)GetUniqueID(), captureTick,
				input.get<dAttackRadialSimulation::PlayerInput>().attackLeft ? 1 : 0);

			if (m_onRemoteMoveReceivedCallback)
				m_onRemoteMoveReceivedCallback(captureTick, input);
		});
}

void USimmableUpdateComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool vizIsAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* vizManager = ASimulationManagerUImpl::instanceFor(vizIsAuthority);
	if (vizManager == nullptr)
		return;

	SimulationObjectStorage<SimulatableBrawler>& storage = vizManager->editStorage();
	if (!storage.has<SimulatableBrawler>((unsigned int)GetUniqueID()))
		return;
	SimulatableBrawler& simulatable = storage.get<SimulatableBrawler>((unsigned int)GetUniqueID());

	{
		const glm::vec3 aimDirection = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->buildAimDirection() : glm::vec3(1.f, 0.f, 0.f);
		const glm::vec2 moveStick = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->getMoveStick() : glm::vec2(0.f);
		const glm::vec3 moveDirectionWorld = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->buildMoveDirectionWorld() : glm::vec3(0.f, 1.f, 0.f);

		LoggingFunctorUImpl loggingFunctor(DAttackRadialVisualizationCVars::loggingEnabled);
		DAttackRendererFunctorUImpl rendererFunctorImpl(GetWorld());

		const simulatableBrawler::AllState& attackSimAllState = simulatable.getVizState();
		const simulatableBrawler::State* attackSimState = &(attackSimAllState.getState());

		glm::vec3 tmpAimInput = glm::vec3(1.f, 0.f, 0.f);
		if (m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent())
			tmpAimInput = aimDirection;

		{

			dAttackRadialVisualization::Input attackCircularVisualizationInput(DeltaTime,
				tmpAimInput,
				rendererFunctorImpl,
				loggingFunctor);
			if (dAttackMachineSimulation::g_movementScheme == dAttackMachineSimulation::MovementScheme::AimRelative)
			{
				dAttackRadialVisualization::visualize2(attackCircularVisualizationInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_visualizationState);
			}
			else
			{
				dAttackRadialVisualization::visualize(attackCircularVisualizationInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_visualizationState);
			}

		}

		if (m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent())
		{
			dAttackAimVisualization::Input aimInput(DeltaTime,
				aimDirection,
				rendererFunctorImpl,
				loggingFunctor,
				moveStick,
				moveDirectionWorld);

			dAttackAimVisualization::visualize(aimInput,
				(*attackSimState).get<dAttackRadialSimulation::State>(),
				(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
				attackSimAllState.getDerivedState().m_attackDerivedState,
				m_staticData->m_attackSimulationStaticData,
				m_attackAimVisualizationState.value());
		}

		// Projectile visualization — debug sphere per alive (flying) projectile slot,
		// plus tick-stamped hit/block indicators (T30). currentTick + dt come from the
		// SAME clock the sim uses (prediction tick on clients, server tick on the
		// authority), so the viz residual-lifetime maths line up with the sim's pruning.
		{
			const SimulationTimeStep projectileVizStep = vizManager->runsPrediction()
				? vizManager->getClientClock().getPredictionStep()
				: vizManager->getServerClock().getSimulationStep();

			brawlerProjectileVisualization::Input projectileVisualizationInput(
				rendererFunctorImpl,
				m_staticData->m_projectileStaticData,
				projectileVizStep.getTick(),
				projectileVizStep.getDeltaSeconds());
			brawlerProjectileVisualization::visualize(projectileVisualizationInput,
				(*attackSimState).get<brawlerProjectileSimulation::State>(),
				attackSimAllState.getDerivedState().m_projectileDerivedState,
				m_projectileVisualizationState);
		}

		{
			dAttackTargetVisualizationTwo::Input attackTargetVisualizationInput(DeltaTime,
				tmpAimInput,
				vizManager->editQueryAdapter(),
				rendererFunctorImpl);
			dAttackTargetVisualizationTwo::visualize(attackTargetVisualizationInput,
				m_attackTargetVisualizationState.value(),
				m_staticData->m_attackSimulationStaticData,
				(*attackSimState).get<dAttackMachineSimulation::State>(),
				(*attackSimState).get<dAttackRadialSimulation::State>(),
				(*attackSimState).get<dAttackGuardSimulation::State>());
		}
	}

	//if (GetNetMode() == NM_DedicatedServer || GetNetMode() == NM_ListenServer)
	//{
	//	m_simulationStateSyncedBuffer.writeToBuffer(0, 1337.f);
	//	m_simulationStateSyncedBuffer.writeToBuffer(0 + sizeof(float), 1336.f);

	//}
	//else
	//{
	//	LoggingFunctorUImpl loggingFunctor(true);

	//	loggingFunctor.logFloat("first synced Float", m_simulationStateSyncedBuffer.readFromBuffer<float>(0));
	//	const uint32 floatSize = sizeof(float);
	//	loggingFunctor.logFloat("first synced Float", m_simulationStateSyncedBuffer.readFromBuffer<float>(0 + floatSize/*sizeof(float)*/));
	//}

	//EpicGamesAssignment::runAssignment();
}

void USimmableUpdateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USimmableUpdateComponent, m_simulationStateCorrectionSyncedBuffer);
	DOREPLIFETIME(USimmableUpdateComponent, m_replicatedInputSyncedBuffer);

	
}

