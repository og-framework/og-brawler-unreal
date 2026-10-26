// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerSimulatableComponent-rationale.md · docs/OGBrawlerSimulatableComponent-guards.md

#include "OGBrawlerSimulatableComponent.h"
#include "OGSimulation/CompilerControl.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/DAttackRadialVisualization.h"
#include "OGBrawler/BrawlerProjectileVisualization.h"
#include "OGBrawler/BrawlerMovementVisualization.h"
#include "OGBrawler/DAttackAimVisualization.h"
#include "OGBrawler/BrawlerVisualizationInputSource.h"
#include "OGSimulation/DMathUtil.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
#include "OGSimulationUnreal/SimulationInputRelay.h"

#include "Logging/LogMacros.h"


#include "Math/Vector.h"
#include "Runtime/Core/Public/Templates/SharedPointer.h"
#include "Runtime/Engine/Public/Physics/PhysicsFiltering.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "OGBrawlerUnreal/DAttackCircleUImplementation.h"
#include "OGBrawlerUnreal/DShapeUImplementation.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/DAttackCircularVisualizationUimpl.h"
#include "OGSimulationUnreal/LoggingFunctorUImpl.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGBrawlerUnreal/OGBrawlerSimulationManager.h"
#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGSimulation/SimulationFrame.h"
#include "OGSimulationUnreal/InputRedundancyBundleBuilder.h"
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"

#include "glm/ext/matrix_transform.hpp"
#include "glm/mat4x4.hpp"
#include <stdexcept>
#include <variant>

#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"



OGSIM_OPTIMIZE_OFF

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

namespace OGBrawlerMovementVisualizationCVars
{
	bool movementVizEnabled = false;
	static FAutoConsoleVariableRef movementVizEnabledCVar(
		TEXT("OGBrawler.Viz.Movement"),
		movementVizEnabled,
		TEXT("Draw the movement sub-simulation's debug readout for every simulated character: the "
			"capsule outline at the SIM's position (coloured by SupportState, red while frozen), the "
			"sim velocity, the surface normal and the (u, v, up) frame as four SEPARATE rays at the "
			"probe point, the ride-height / probe-reach band marks, and the signed servo error "
			"(rideHeight - clearance). Off by default; costs one bool read when off."),
		ECVF_Default);
}

namespace DAttackTargetVisualizationCVars
{
	bool legacyEnemyRangeArcsEnabled = false;
	static FAutoConsoleVariableRef legacyEnemyRangeArcsEnabledCVar(
		TEXT("DAttackTargetVisualization.legacyEnemyRangeArcsEnabled"),
		legacyEnemyRangeArcsEnabled,
		TEXT("Enable the legacy per-enemy inner-circle arc segments (outlined + solid). Off by default; enable for A/B comparison against the new block-prediction viz. Slated for removal once the new viz is confirmed as an improvement."),
		ECVF_Default);
}


UOGBrawlerSimulatableComponent::UOGBrawlerSimulatableComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	m_staticData.emplace();



	PrimaryComponentTick.SetTickFunctionEnable(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;


}


void UOGBrawlerSimulatableComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	beginRegistration();

}

ueSimulatableOwnerRegistration::Initialization UOGBrawlerSimulatableComponent::initializeWithManager(
	AOGBrawlerSimulationManager& manager, AActor& owner)
{
	if (AOGBrawlerUECharacter* brawlerOwner = Cast<AOGBrawlerUECharacter>(&owner))
		m_ownerInputCollection = brawlerOwner->getInputCollection();

	{
		QueryVolumeDescriptor targetVisDescriptor{
			SphereGeometry{m_staticData->m_attackCircle.getOuterRadius() * 2.f},
			collisionCategory::bodyAndGuard,
			glm::mat4(1.f),
			collisionCategory::queryRouting};

		const std::optional<QueryVolumeId> targetVolume = manager.session().registerVizVolume(targetVisDescriptor, owner);
		if (!targetVolume.has_value())
			return ueSimulatableOwnerRegistration::Initialization::Retry;
		m_targetVisualizationVolumeIds.push_back(*targetVolume);
	}
	m_attackTargetVisualizationState.emplace(m_targetVisualizationVolumeIds);
	m_attackAimVisualizationState.emplace();
	m_attackBlockPredictionVisualizationState.emplace(m_targetVisualizationVolumeIds);

	return ueSimulatableOwnerRegistration::Initialization::Ready;
}

SimCharacterId UOGBrawlerSimulatableComponent::simCharacterId() const
{
	const AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(GetOwner());
	return (character != nullptr) ? character->GetSimCharacterId() : SimCharacterId::None;
}

ueSimulatableOwnerRegistration::ResolvedKey UOGBrawlerSimulatableComponent::resolveRegistrationKey(
	AOGBrawlerSimulationManager& manager, bool isAuthority)
{
	AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(GetOwner());
	checkf(character != nullptr,
		TEXT("UOGBrawlerSimulatableComponent must be attached to an AOGBrawlerUECharacter: the pawn carries ")
		TEXT("the replicated SimCharacterId this component registers under"));

	if (isAuthority && character->GetSimCharacterId() == SimCharacterId::None)
	{
		const SimCharacterId assigned = manager.roster().allocateSimCharacterId(manager.GetNetMode());
		// ⛔G-01  docs/OGBrawlerSimulatableComponent-guards.md
		if (assigned == SimCharacterId::None)
			return ueSimulatableOwnerRegistration::ResolvedKey::abandon();
		character->SetAuthoritativeSimCharacterId(assigned);
	}

	const SimCharacterId simId = character->GetSimCharacterId();
	if (simId == SimCharacterId::None)
		return ueSimulatableOwnerRegistration::ResolvedKey::retry();

	return ueSimulatableOwnerRegistration::ResolvedKey::ready(toStorageKey(simId));
}

UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>::InputProvider UOGBrawlerSimulatableComponent::makeInputProvider(unsigned int key)
{
	UOGBrawlerInputCollectionComponent* ic = m_ownerInputCollection;
	const uint32 id = key;
	// ⛔G-05  docs/OGBrawlerSimulatableComponent-guards.md
	return [ic, id](const SimulationTimeStep& step,
	                const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache) {
		return ic->buildPlayerInput(step, id, localInputCache);
	};
}

SimulatableBrawler UOGBrawlerSimulatableComponent::makeSimulatable()
{
	return SimulatableBrawler(*m_staticData);
}

void UOGBrawlerSimulatableComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const bool isAuthority = (GetNetMode() != NM_Client);
	AOGBrawlerSimulationManager* manager = AOGBrawlerSimulationManager::instanceFor(isAuthority);
	if (manager != nullptr)
		manager->unregisterFromNewFramework(simCharacterId(), *this, isAuthority);

	// ⛔G-38  docs/OGBrawlerSimulatableComponent-guards.md
	if (isNetSyncBound())
		netSync().detachInputRelayHost(isAuthority);

	unbindNetSync();

	UActorComponent::EndPlay(EndPlayReason);
}


unsigned int UOGBrawlerSimulatableComponent::storageKeyOfOwner() const
{
	return toStorageKey(simCharacterId());
}

template class UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>;

void UOGBrawlerSimulatableComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool vizIsAuthority = (GetNetMode() != NM_Client);
	AOGBrawlerSimulationManager* vizManager = AOGBrawlerSimulationManager::instanceFor(vizIsAuthority);
	if (vizManager == nullptr)
		return;

	SimulationObjectStorage<SimulatableBrawler>& storage = vizManager->session().editStorage();
	const unsigned int ownKey = toStorageKey(simCharacterId());
	if (!storage.has<SimulatableBrawler>(ownKey))
		return;
	SimulatableBrawler& simulatable = storage.get<SimulatableBrawler>(ownKey);

	{
		const glm::vec3 aimDirection = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->buildAimDirection() : simFrame::kForward;
		const glm::vec2 moveStick = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->getMoveStick() : glm::vec2(0.f);
		const glm::vec3 moveDirectionWorld = (m_ownerInputCollection != nullptr) ? m_ownerInputCollection->buildMoveDirectionWorld() : simFrame::kRight;

		LoggingFunctorUImpl loggingFunctor(DAttackRadialVisualizationCVars::loggingEnabled);
		DAttackRendererFunctorUImpl rendererFunctorImpl(GetWorld());

		const simulatableBrawler::AllState& attackSimAllState = simulatable.getVizState();
		const simulatableBrawler::State* attackSimState = &(attackSimAllState.getState());

		glm::vec3 tmpAimInput = simFrame::kForward;
		if (m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent())
			tmpAimInput = aimDirection;

		{

			dAttackRadialVisualization::Input attackCircularVisualizationInput(DeltaTime,
				tmpAimInput,
				rendererFunctorImpl,
				loggingFunctor);
			if (dAttackMachineSimulation::isAimRelativeFamily(dAttackMachineSimulation::g_movementScheme.load()))
			{
				dAttackRadialVisualization::visualize2(attackCircularVisualizationInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().get<dAttackRadialSimulation::DerivedState>(),
					m_staticData->m_attackSimulationStaticData,
					m_visualizationState);
			}
			else
			{
				dAttackRadialVisualization::visualize(attackCircularVisualizationInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().get<dAttackRadialSimulation::DerivedState>(),
					m_staticData->m_attackSimulationStaticData,
					m_visualizationState);
			}

		}

		// ⛔G-24  docs/OGBrawlerSimulatableComponent-guards.md
		const bool hasLiveLocalInput =
			m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent();

		// ⛔G-25  docs/OGBrawlerSimulatableComponent-guards.md
		const std::optional<simulatableBrawler::PlayerInput> vizPlayerInput =
			simulatableBrawler::selectVisualizationInput(
				hasLiveLocalInput,
				[this]() { return m_ownerInputCollection->buildLatestVisualizationInput(); },
				vizManager->session().getLastRelayedInput(ownKey));

		{
			if (vizPlayerInput.has_value())
			{
				dAttackAimVisualization::Input aimInput(DeltaTime,
					vizPlayerInput->aimDirection,
					rendererFunctorImpl,
					loggingFunctor,
					vizPlayerInput->moveStick,
					vizPlayerInput->moveDirectionWorld);

				dAttackAimVisualization::visualize(aimInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().get<dAttackRadialSimulation::DerivedState>(),
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
			}
		}

		const SimulationTimeStep vizSimulationStep = vizManager->session().runsPrediction()
			? vizManager->session().getClientClock().getPredictionStep()
			: vizManager->session().getServerClock().getSimulationStep();

		{
			brawlerProjectileVisualization::Input projectileVisualizationInput(
				rendererFunctorImpl,
				m_staticData->m_projectileStaticData,
				vizSimulationStep.getTick(),
				vizSimulationStep.getDeltaSeconds());
			brawlerProjectileVisualization::visualize(projectileVisualizationInput,
				(*attackSimState).get<brawlerProjectileSimulation::State>(),
				attackSimAllState.getDerivedState().get<brawlerProjectileSimulation::DerivedState>(),
				m_projectileVisualizationState);
		}

		// ⛔G-26  docs/OGBrawlerSimulatableComponent-guards.md
		if (OGBrawlerMovementVisualizationCVars::movementVizEnabled)
		{
			brawlerMovementVisualization::Input<DAttackRendererFunctorUImpl> movementVisualizationInput(
				rendererFunctorImpl);
			brawlerMovementVisualization::visualize(movementVisualizationInput,
				(*attackSimState).get<brawlerMovementSimulation::State>(),
				attackSimAllState.getDerivedState().get<brawlerMovementSimulation::DerivedState>(),
				m_staticData->m_movementStaticData);
		}

		{
			dAttackTargetVisualizationTwo::Input attackTargetVisualizationInput(DeltaTime,
				tmpAimInput,
				vizManager->session().editVizQuery(),
				rendererFunctorImpl,
				DAttackTargetVisualizationCVars::legacyEnemyRangeArcsEnabled);
			dAttackTargetVisualizationTwo::visualize(attackTargetVisualizationInput,
				m_attackTargetVisualizationState.value(),
				m_staticData->m_attackSimulationStaticData,
				(*attackSimState).get<dAttackMachineSimulation::State>(),
				(*attackSimState).get<dAttackRadialSimulation::State>(),
				(*attackSimState).get<dAttackGuardSimulation::State>());
		}

		{
			if (vizPlayerInput.has_value())
			{
				dAttackBlockPredictionVisualization::Input blockPredInput(DeltaTime,
					vizPlayerInput->aimDirection,
					vizPlayerInput->moveStick,
					vizPlayerInput->moveDirectionWorld,
					vizManager->session().getVizReader(),
					vizManager->session().editVizQuery(),
					rendererFunctorImpl);
				dAttackBlockPredictionVisualization::visualize(blockPredInput,
					m_attackBlockPredictionVisualizationState.value(),
					m_staticData->m_attackSimulationStaticData,
					(*attackSimState).get<dAttackRadialSimulation::State>());
			}
		}

		// ⛔G-27  docs/OGBrawlerSimulatableComponent-guards.md
		if (!inputHistoryVisualizationUImpl::masterEnabled())
			return;

		{
			const bool feedRowPanel   = inputHistoryVisualizationUImpl::displayEnabled();
			const bool feedAnyBar     = inputHistoryVisualizationUImpl::anyBarEnabled();
			const bool feedInputDelay = inputHistoryVisualizationUImpl::inputDelayEnabled();

			// ⛔G-28  docs/OGBrawlerSimulatableComponent-guards.md
			if (!feedRowPanel && !feedAnyBar)
				return;

			const unsigned int ownCharacterId = ownKey;

			if (feedRowPanel)
			{
				const std::optional<unsigned int> rowCharacterId =
					inputHistoryVisualizationUImpl::firstLocalCharacterId(GetWorld());

				// ⛔G-29  docs/OGBrawlerSimulatableComponent-guards.md
				if (rowCharacterId.has_value() && *rowCharacterId == ownCharacterId)
				{
					vizManager->inputHistoryFeed().pollInputHistory(*rowCharacterId,
						vizSimulationStep.getTick(),
						dAttackMachineSimulation::g_moveStickDeadzone.load());
				}
			}

			if (feedAnyBar)
			{
				std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput;
				if (vizPlayerInput.has_value())
				{
					// ⛔G-31  docs/OGBrawlerSimulatableComponent-guards.md
					liveInput = brawlerInputHistoryVisualization::captureRowFieldsOf(
						*vizPlayerInput,
						dAttackMachineSimulation::g_moveStickDeadzone.load());
				}

				// ⛔G-30  docs/OGBrawlerSimulatableComponent-guards.md
				vizManager->inputHistoryFeed().pollInputHistoryLanes(ownCharacterId,
					vizSimulationStep.getTick(),
					(*attackSimState).get<dAttackMachineSimulation::State>().m_currentState,
					liveInput,
					inputHistoryVisualizationUImpl::pauseLanesWhileIdle(),
					feedInputDelay);
			}
		}
	}




}

// ⛔G-35  docs/OGBrawlerSimulatableComponent-guards.md
OGSIM_OPTIMIZE_ON

