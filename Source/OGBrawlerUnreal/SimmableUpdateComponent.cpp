// SPDX-License-Identifier: BUSL-1.1

#include "SimmableUpdateComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/Declares.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/DAttackRadialVisualization.h"
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
#include "OGBrawlerUnreal/DAttackCircularVisualizationUimpl.h"
#include "OGSimulationUnreal/LoggingFunctorUImpl.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include <stdexcept>
#include <variant>

#include "Net/UnrealNetwork.h"



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

//namespace DAttackMovementCVars
//{
//	int movementAndAimMode = 0;
//	//1 aim relative movement
//	//2 movement relative strike
//	//3 movementRelativeStrikeJoyAimRelativeMoveMouse
//	static FAutoConsoleVariableRef movementAndAimModeCVar(
//		TEXT("DAttackMovementCVars.movementAndAimMode"),
//		movementAndAimMode,
//		TEXT("test)"),
//		ECVF_Default);
//
//	bool aimRelativeMovement = false;
//	static FAutoConsoleVariableRef aimRelativeMovementCVar(
//		TEXT("DAttackMovementCVars.aimRelativeMovement"),
//		aimRelativeMovement,
//		TEXT("test)"),
//		ECVF_Default);
//
//	bool movementRelativeStrike = true;
//	static FAutoConsoleVariableRef movementRelativeStrikeCVar(
//		TEXT("DAttackMovementCVars.movementRelativeStrike"),
//		movementRelativeStrike,
//		TEXT("test)"),
//		ECVF_Default);
//
//	bool movementRelativeStrikeJoyAimRelativeMoveMouse = true;
//	static FAutoConsoleVariableRef movementRelativeStrikeJoyAimRelativeMoveMouseCVar(
//		TEXT("DAttackMovementCVars.movementRelativeStrike"),
//		movementRelativeStrike,
//		TEXT("test)"),
//		ECVF_Default);
//}

namespace DAttackMovementCVars
{
	TAutoConsoleVariable<int32> movementAndAimMode(
		TEXT("DAttackMovementCVars.movementAndAimMode"),
		2,
		TEXT("test)"),
		ECVF_Default);
}

//Component
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ~10 seconds at 60 Hz — generous enough for scene startup, small enough to fail loudly on genuine bugs.
constexpr int32 kMaxRegistrationAttempts = 600;

USimmableUpdateComponent::USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
	, m_inputComponent(nullptr)
	, m_aimDirection(0.f, 0.f, 0.f)
	, m_camForward(0.f, 0.f, 0.f)
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

	const uint32 componentId = (unsigned int)GetUniqueID();
	std::function<simulatableBrawler::PlayerInput(const SimulationTimeStep&)> inputProvider;
	if (isLocallyPredicted)
	{
		inputProvider = [this, componentId](const SimulationTimeStep& step) -> simulatableBrawler::PlayerInput
		{
			if (m_inputComponent != nullptr)
			{
				const glm::vec3 aimDirection = buildAimDirection();
				const glm::vec3 moveDirectionWorld = [this, &aimDirection]() {
					if (dAttackMachineSimulation::MovementAndAimModeTest == 1)
						return getInputDirectionInAimSpace(aimDirection, glm::vec3(m_moveDirection, 0.0f));
					else
						return getInputDirectionInCameraSpace(m_camForward, glm::vec3(m_moveDirection, 0.0f));
				}();
				UE_LOG(LogOGSimTick, Log,
					TEXT("[ClientPrediction] id=%u tick=%u attackLeft=%d"),
					componentId, step.getTick(), m_leftAttackInput ? 1 : 0);
				return simulatableBrawler::PlayerInput(
					dAttackRadialSimulation::PlayerInput(aimDirection, m_leftAttackInput, m_rightAttachInput),
					dAttackMachineSimulation::PlayerInput(aimDirection, m_leftAttackInput, m_rightAttachInput, m_moveDirection, moveDirectionWorld),
					dAttackGuardSimulation::PlayerInput(aimDirection));
			}
			return simulatableBrawler::getZeroPlayerInput();
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

void USimmableUpdateComponent::setInputComponent(UEnhancedInputComponent* inputComponent, const InputMappingUETranslator& translator)
{
	m_inputComponent = inputComponent;

	UInputAction* LeftAttackAction = translator.getAction(dInput::gameMapping::LeftAttack);
	UInputAction* RightAttackAction = translator.getAction(dInput::gameMapping::RightAttack);
	UInputAction* AimAction = translator.getAction(dInput::gameMapping::Aim);
	UInputAction* MoveAction = translator.getAction(dInput::gameMapping::Move);

	m_inputComponent->BindAction(LeftAttackAction, ETriggerEvent::Triggered, this, &USimmableUpdateComponent::setLeftAttackInput);
	m_inputComponent->BindAction(LeftAttackAction, ETriggerEvent::Completed, this, &USimmableUpdateComponent::setLeftAttackInput);
	m_inputComponent->BindAction(RightAttackAction, ETriggerEvent::Triggered, this, &USimmableUpdateComponent::setRightAttackInput);
	m_inputComponent->BindAction(RightAttackAction, ETriggerEvent::Completed, this, &USimmableUpdateComponent::setRightAttackInput);
	m_inputComponent->BindAction(AimAction, ETriggerEvent::Triggered, this, &USimmableUpdateComponent::setAimInput);
	m_inputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &USimmableUpdateComponent::setMoveInput);
	m_inputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &USimmableUpdateComponent::setMoveInput);

}

void USimmableUpdateComponent::OnRep_CorrectionState()
{
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

void USimmableUpdateComponent::ServerReceiveRemoteMove_Implementation(const FSimulationStateSyncBuffer& inputBuffer)
{
	{
		simulatableBrawler::PlayerInput peeked;
		const uint32 tick = inputBuffer.readInto(peeked);
		UE_LOG(LogOGNet, Log,
			TEXT("[ServerReceive] id=%u tick=%u attackLeft=%d"),
			(unsigned int)GetUniqueID(), tick,
			peeked.get<dAttackRadialSimulation::PlayerInput>().attackLeft ? 1 : 0);
	}

	if (m_onRemoteMoveReceivedCallback)
		m_onRemoteMoveReceivedCallback(inputBuffer);
}

//
//glm::vec3 USimmableUpdateComponent::buildAimDirection()
//{
//	glm::vec3 camForward = m_camForward;
//	camForward.z = 0.f;
//	camForward = glm::normalize(camForward);
//
//	glm::mat4 camRotationMatrix;
//	dMathUtil::getRotationMatrix(glm::vec3(0.f, -1.f, 0.f), camForward, camRotationMatrix);
//
//	if (glm::length(m_aimDirection) > 0.2f)
//	{
//		glm::vec3 normalizedAimDirection = glm::normalize(m_aimDirection);
//		glm::vec4 aimDirection4(normalizedAimDirection, 0.f);
//		glm::vec3 worldAimDirection = camRotationMatrix * aimDirection4;
//		return worldAimDirection;
//	}
//	else if (glm::length(m_mouseAimDirection) > 0.2f)
//	{
//		glm::vec3 normalizedAimDirection = glm::normalize(m_mouseAimDirection);
//		return normalizedAimDirection;
//	}
//	else
//	{
//		return camForward;
//	}
//}


glm::vec3 USimmableUpdateComponent::buildAimDirection()
{
	if (glm::length(m_aimDirection) > 0.2f)
	{
		return getInputDirectionInCameraSpace(m_camForward, m_aimDirection);
	}
	else if (glm::length(m_mouseAimDirection) > 0.2f)
	{
		return glm::normalize(m_mouseAimDirection);
	}
	else
	{
		glm::vec3 camForward = m_camForward;
		camForward.z = 0.f;
		camForward = glm::normalize(camForward);
		return camForward;
	}
}


glm::vec3 USimmableUpdateComponent::getInputDirectionInCameraSpace(const glm::vec3& camForward, const glm::vec3& inputDirection)
{
	glm::vec3 camForwardNormalized = camForward;
	camForwardNormalized.z = 0.f;
	camForwardNormalized = glm::normalize(camForwardNormalized);

	glm::mat4 camRotationMatrix;
	dMathUtil::getRotationMatrix(glm::vec3(0.f, -1.f, 0.f), camForwardNormalized, camRotationMatrix);

	glm::vec3 normalizedDirection = glm::normalize(inputDirection);
	glm::vec4 direction4(normalizedDirection, 0.f);
	glm::vec3 worldDirection = camRotationMatrix * direction4;

	return worldDirection;
}

glm::vec3 USimmableUpdateComponent::getInputDirectionInAimSpace(const glm::vec3& aimDirection, const glm::vec3& inputDirection)
{
	glm::vec3 aimDirectionNormalized = aimDirection;
	aimDirectionNormalized.z = 0.f;
	aimDirectionNormalized = glm::normalize(aimDirectionNormalized);

	glm::mat4 aimRotationMatrix;
	dMathUtil::getRotationMatrix(glm::vec3(0.f, -1.f, 0.f), aimDirectionNormalized, aimRotationMatrix);

	glm::vec3 normalizedDirection = glm::normalize(inputDirection);
	glm::vec4 direction4(normalizedDirection, 0.f);
	glm::vec3 worldDirection = aimRotationMatrix * direction4;

	return worldDirection;
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
		const glm::vec3 aimDirection = buildAimDirection();
		const glm::vec3 moveDirectionWorld = [this, &aimDirection]() {
			if (DAttackMovementCVars::movementAndAimMode.GetValueOnAnyThread() == 1)
				return getInputDirectionInAimSpace(aimDirection, glm::vec3(m_moveDirection, 0.0f));
			else
				return getInputDirectionInCameraSpace(m_camForward, glm::vec3(m_moveDirection, 0.0f));
			}();

		LoggingFunctorUImpl loggingFunctor(DAttackRadialVisualizationCVars::loggingEnabled);
		DAttackRendererFunctorUImpl rendererFunctorImpl(GetWorld());

		const simulatableBrawler::AllState& attackSimAllState = simulatable.getVizState();
		const simulatableBrawler::State* attackSimState = &(attackSimAllState.getState());

		glm::vec3 tmpAimInput = glm::vec3(1.f, 0.f, 0.f);
		if (m_inputComponent != nullptr)
			tmpAimInput = aimDirection;

		{

			dAttackRadialVisualization::Input attackCircularVisualizationInput(DeltaTime,
				tmpAimInput,
				rendererFunctorImpl,
				loggingFunctor);
			if (dAttackMachineSimulation::MovementAndAimModeTest == 2)
			{
				dAttackRadialVisualization::visualize2(attackCircularVisualizationInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_visualizationState);
			}
			else if (dAttackMachineSimulation::MovementAndAimModeTest == 1)
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

		if (m_inputComponent != nullptr)
		{
			dAttackAimVisualization::Input aimInput(DeltaTime,
				aimDirection,
				rendererFunctorImpl,
				loggingFunctor,
				m_moveDirection,
				moveDirectionWorld);

			switch (dAttackMachineSimulation::MovementAndAimModeTest)
			{
				case 1:
				dAttackAimVisualization::visualize(aimInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
				break;
				case 2:
				dAttackAimVisualization::visualize(aimInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
				break;
			default:
				dAttackAimVisualization::visualize(aimInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
				break;
			}
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

