// SPDX-License-Identifier: BUSL-1.1
// docs/SimmableUpdateComponent-rationale.md · docs/SimmableUpdateComponent-guards.md

#include "SimmableUpdateComponent.h"
#include "OGSimulation/CompilerControl.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/Declares.h"
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
#include "OGBrawlerUnreal/InputHistoryVisualizationUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
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


constexpr int32 kMaxRegistrationAttempts = 600;

USimmableUpdateComponent::USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	m_staticData.emplace();



	PrimaryComponentTick.SetTickFunctionEnable(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;


}


void USimmableUpdateComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { tryInitializeWithManager(); }));
	}

}

void USimmableUpdateComponent::tryInitializeWithManager()
{
	++m_initializationAttempts;

	// ⛔G-02  docs/SimmableUpdateComponent-guards.md
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

	// ⛔G-03  docs/SimmableUpdateComponent-guards.md

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
	m_attackBlockPredictionVisualizationState.emplace(m_targetVisualizationVolumeIds);

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

SimCharacterId USimmableUpdateComponent::simCharacterId() const
{
	const AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(GetOwner());
	return (character != nullptr) ? character->GetSimCharacterId() : SimCharacterId::None;
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
				kMaxRegistrationAttempts, toStorageKey(simCharacterId()));
		scheduleNextRegistrationAttempt();
		return;
	}

	AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(GetOwner());
	checkf(character != nullptr,
		TEXT("USimmableUpdateComponent must be attached to an AOGBrawlerUECharacter: the pawn carries ")
		TEXT("the replicated SimCharacterId this component registers under"));

	if (isAuthority && character->GetSimCharacterId() == SimCharacterId::None)
	{
		const SimCharacterId assigned = regManager->allocateSimCharacterId();
		// ⛔G-01  docs/SimmableUpdateComponent-guards.md
		if (assigned == SimCharacterId::None)
			return;
		character->SetAuthoritativeSimCharacterId(assigned);
	}

	const SimCharacterId simId = character->GetSimCharacterId();
	if (simId == SimCharacterId::None)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: the pawn's SimCharacterId was not replicated after %d attempts"),
				kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	const AActor* ownerActor = GetOwner();
	// ⛔G-04  docs/SimmableUpdateComponent-guards.md
	const bool isLocallyPredicted = !isAuthority
		&& ownerActor != nullptr
		&& ownerActor->GetLocalRole() == ROLE_AutonomousProxy;

	BrawlerInputProviderFn inputProvider;
	if (isLocallyPredicted)
	{
		UOGBrawlerInputCollectionComponent* ic = m_ownerInputCollection;
		const uint32 id = toStorageKey(simId);
		// ⛔G-05  docs/SimmableUpdateComponent-guards.md
		inputProvider = [ic, id](const SimulationTimeStep& step,
		                         const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache) {
			return ic->buildPlayerInput(step, id, localInputCache);
		};
	}

	SimulatableBrawler newSimulatable(*m_staticData);

	const TryRegisterStatus status = regManager->tryRegister(
		simId,
		std::move(newSimulatable),
		*this,
		std::move(inputProvider),
		isAuthority);

	if (status == TryRegisterStatus::Pending)
	{
		if (m_registrationAttempts >= kMaxRegistrationAttempts)
			checkf(false,
				TEXT("DeferredReg: id=%u not ready after %d attempts"),
				toStorageKey(simId), kMaxRegistrationAttempts);
		scheduleNextRegistrationAttempt();
		return;
	}

	m_hasLocalInputProvider = isLocallyPredicted;

	// ⛔G-06  docs/SimmableUpdateComponent-guards.md
	if (isAuthority)
	{
		if (UWorld* world = GetWorld())
		{
			if (AActor* ownerActor2 = GetOwner())
			{
				attachInputRelayHost(
					ASimulationInputRelay::spawnForCharacter(*world, *ownerActor2));
			}
		}
	}
	else
	{
		attachInputRelayHost(
			ASimulationInputRelay::findForOwner(GetWorld(), GetOwner()));
	}

	// ⛔G-07  docs/SimmableUpdateComponent-guards.md
	if (isAuthority)
		regManager->noteDelayedInputComponent(simId, *this);

	UE_LOG(LogOGMgmt, Log,
		TEXT("NewFramework: registered id=%u isAuthority=%d isLocallyPredicted=%d"),
		toStorageKey(simId), isAuthority ? 1 : 0, isLocallyPredicted ? 1 : 0);
}

void USimmableUpdateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	const bool isAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* manager = ASimulationManagerUImpl::instanceFor(isAuthority);
	if (manager != nullptr)
		manager->unregisterFromNewFramework(simCharacterId(), *this, isAuthority);

	if (ASimulationInputRelay* host = m_inputRelayHost.Get())
	{
		// ⛔G-08  docs/SimmableUpdateComponent-guards.md
		if (isAuthority)
			host->detachFromParentAndDestroy();
		else
			host->clearOnRelayedInputReceivedCallback();
	}
	m_inputRelayHost = nullptr;

	UActorComponent::EndPlay(EndPlayReason);
}


void USimmableUpdateComponent::OnRep_CorrectionState()
{
	// ⛔G-09  docs/SimmableUpdateComponent-guards.md
	if (m_wireFormatMismatchDetected)
		return;

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
			toStorageKey(simCharacterId()), tick);
	}
	if (m_onCorrectionStateReceivedCallback)
		m_onCorrectionStateReceivedCallback(m_simulationStateCorrectionSyncedBuffer);
}

// ⛔G-10  docs/SimmableUpdateComponent-guards.md

// ⛔G-11  docs/SimmableUpdateComponent-guards.md


FRelayedInputRing& USimmableUpdateComponent::getRelayedInputRing()
{
	if (ASimulationInputRelay* host = m_inputRelayHost.Get())
		return host->editRelayedInputRing();
	return m_detachedRelayRing;
}

const FRelayedInputRing& USimmableUpdateComponent::getRelayedInputRing() const
{
	if (const ASimulationInputRelay* host = m_inputRelayHost.Get())
		return host->getRelayedInputRing();
	return m_detachedRelayRing;
}

relayedInputRing::StageArrivalOutcome USimmableUpdateComponent::stageRelayedInput(
	uint32 captureTick, uint8 dA, const simulatableBrawler::PlayerInput& input)
{
	ASimulationInputRelay* host = m_inputRelayHost.Get();
	// ⛔G-12  docs/SimmableUpdateComponent-guards.md
	FRelayedInputRing& stage = (host != nullptr)
		? host->editRelayedInputStagingRing()
		: m_detachedRelayStagingRing;

	const relayedInputRing::StageArrivalOutcome outcome =
		stage.stageArrival<simulatableBrawler::PlayerInput>(captureTick, dA, input);

	// ⛔G-13  docs/SimmableUpdateComponent-guards.md
	if (outcome.droppedOldest && host != nullptr)
		host->noteStageOverflowDrop();

	return outcome;
}


static_assert(std::is_same_v<decltype(&USimmableUpdateComponent::stageRelayedInput),
		relayedInputRing::StageArrivalOutcome (USimmableUpdateComponent::*)(
			uint32, uint8, const simulatableBrawler::PlayerInput&)>,
	"USimmableUpdateComponent::stageRelayedInput takes NO depth parameter. The stage's capacity is "
	"relayedInputRing::kMaxDepth, fixed inside the codec; a depth passed down here would cap every "
	"round at one entry and silently restore the replace-latest behaviour the staging removed. "
	"Was the 'NO DEPTH PARAMETER, and that is the fence' prose fence of SimmableUpdateComponent.h "
	"and its .cpp one-liner (task 25 conversion).");

void USimmableUpdateComponent::setOnRelayedInputReceivedCallback(
	std::function<void(const FRelayedInputRing&)> fn)
{
	m_onRelayedInputReceivedCallback = std::move(fn);

	if (const ASimulationInputRelay* host = m_inputRelayHost.Get())
	{
		if (m_onRelayedInputReceivedCallback)
			m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
	}
}

void USimmableUpdateComponent::clearOnRelayedInputReceivedCallback()
{
	m_onRelayedInputReceivedCallback = nullptr;
}

void USimmableUpdateComponent::attachInputRelayHost(ASimulationInputRelay* host)
{
	if (host == nullptr || m_inputRelayHost.Get() == host)
		return;

	m_inputRelayHost = host;

	TWeakObjectPtr<USimmableUpdateComponent> weakSelf(this);
	// ⛔G-14  docs/SimmableUpdateComponent-guards.md
	host->setOnRelayedInputReceivedCallback(
		[weakSelf](const FRelayedInputRing& ring)
		{
			if (USimmableUpdateComponent* self = weakSelf.Get())
				self->onRelayedInputRingArrived(ring);
		});

	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
}

void USimmableUpdateComponent::onRelayedInputRingArrived(const FRelayedInputRing& ring)
{
	// ⛔G-15  docs/SimmableUpdateComponent-guards.md
	if (m_hasLocalInputProvider && ring.num() > 0 && !m_loggedOwnerSkipDivergence)
	{
		m_loggedOwnerSkipDivergence = true;
		UE_LOG(LogOGNet, Error,
			TEXT("[InputRelay] OWNER-SKIP DIVERGENCE id=%u — a relay ring with %u entries "
			     "arrived for a character that HAS a local input provider. COND_SkipOwner and "
			     "provider-presence disagree; check the relay host's Owner."),
			toStorageKey(simCharacterId()), (unsigned int)ring.num());
		// ⛔G-16  docs/SimmableUpdateComponent-guards.md
		ensureMsgf(false,
			TEXT("[InputRelay] relay ring arrived for a provider-present character (id=%u)"),
			toStorageKey(simCharacterId()));
	}

	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(ring);
}

void USimmableUpdateComponent::sendLocalInputToAuthority(
	const PendingInputQueue<simulatableBrawler::PlayerInput>& queue,
	uint32 currentTick,
	uint32 redundancyDepth)
{
	FInputRedundancyBundle bundle;
	buildRedundancyBundle<simulatableBrawler::PlayerInput>(
		queue, currentTick, static_cast<uint8>(redundancyDepth), bundle);
	ServerReceiveRemoteMove(bundle);
}

void USimmableUpdateComponent::ServerReceiveRemoteMove_Implementation(const FInputRedundancyBundle& bundle)
{
	// ⛔G-17  docs/SimmableUpdateComponent-guards.md
	if (bundle.wireBytes.Num() == 0)
		return;

	const uint8 clientVersion = bundle.getWireFormatVersion();
	if (clientVersion != FInputRedundancyBundle::kWireFormatVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on ServerReceiveRemoteMove: client version=%u, server expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)clientVersion, (unsigned int)FInputRedundancyBundle::kWireFormatVersion);
		// ⛔G-18  docs/SimmableUpdateComponent-guards.md
		return;
	}

	// ⛔G-19  docs/SimmableUpdateComponent-guards.md
	ASimulationManagerUImpl* authorityManager =
		ASimulationManagerUImpl::instanceFor(/*isAuthority=*/true);
	ASimulationManagerUImpl::BrawlerReceptionCoordinator* coordinator =
		(authorityManager != nullptr) ? authorityManager->getReceptionCoordinator() : nullptr;

	// ⛔G-20  docs/SimmableUpdateComponent-guards.md
	UNetConnection* rootConn =
		(coordinator != nullptr) ? GetRootNetConnection(GetOwner()) : nullptr;

	const unsigned int id = toStorageKey(simCharacterId());

	// ⛔G-21  docs/SimmableUpdateComponent-guards.md
	if (coordinator == nullptr || rootConn == nullptr)
	{
		bundle.forEachSlot<simulatableBrawler::PlayerInput>(
			[this, id](uint32 captureTick, const simulatableBrawler::PlayerInput& input)
			{
				UE_LOG(LogOGNet, Log, TEXT("[ServerReceive] id=%u tick=%u"), id, captureTick);
				if (m_onRemoteMoveReceivedCallback)
					m_onRemoteMoveReceivedCallback(captureTick, input);
			});
		return;
	}

	const FUEConnectionHandle handle(rootConn);

	// ⛔G-22  docs/SimmableUpdateComponent-guards.md
	coordinator->noteRttSample(
		handle, id, authorityManager->getServerReceptionTick(),
		readRoundTripMs(rootConn), *this);

	const uint8 playerSlot = GetPlayerSlotForActor(GetOwner());

	// ⛔G-23  docs/SimmableUpdateComponent-guards.md
	coordinator->receiveInputBundle<SimulatableBrawler>(
		id, handle, playerSlot, bundle, *authorityManager, *authorityManager);
}

static_assert(ConnectionTierSink<USimmableUpdateComponent>,
	"USimmableUpdateComponent must satisfy ConnectionTierSink so ServerReceptionCoordinator "
	"can drive the tier send through it");

void USimmableUpdateComponent::sendConnectionTierToOwningClient(unsigned int id, uint8_t tier)
{
	check(id == toStorageKey(simCharacterId()));

	UNetConnection* rootConnection = GetRootNetConnection(GetOwner());
	if (rootConnection == nullptr)
	{
		return;
	}

	ASimulationConnectionRelay* relay =
		ASimulationConnectionRelay::findOrSpawnForConnection(GetWorld(), rootConnection);
	if (relay == nullptr)
		return;

	UE_LOG(LogOGNet, Log,
		TEXT("[ConnectionTier] id=%u server tier %u -> %u (relay %s)"),
		toStorageKey(simCharacterId()),
		(unsigned int)relay->getConnectionTier(), (unsigned int)tier,
		*relay->GetName());

	relay->setConnectionTier(tier);
}

void USimmableUpdateComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool vizIsAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* vizManager = ASimulationManagerUImpl::instanceFor(vizIsAuthority);
	if (vizManager == nullptr)
		return;

	SimulationObjectStorage<SimulatableBrawler>& storage = vizManager->editStorage();
	const unsigned int ownKey = toStorageKey(simCharacterId());
	if (!storage.has<SimulatableBrawler>(ownKey))
		return;
	SimulatableBrawler& simulatable = storage.get<SimulatableBrawler>(ownKey);

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

		// ⛔G-24  docs/SimmableUpdateComponent-guards.md
		const bool hasLiveLocalInput =
			m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent();

		// ⛔G-25  docs/SimmableUpdateComponent-guards.md
		const std::optional<simulatableBrawler::PlayerInput> vizPlayerInput =
			simulatableBrawler::selectVisualizationInput(
				hasLiveLocalInput,
				[this]() { return m_ownerInputCollection->buildLatestVisualizationInput(); },
				vizManager->getLastRelayedInput(ownKey));

		{
			if (vizPlayerInput.has_value())
			{
				const auto& machineInput = vizPlayerInput->get<dAttackMachineSimulation::PlayerInput>();
				dAttackAimVisualization::Input aimInput(DeltaTime,
					machineInput.aimDirection,
					rendererFunctorImpl,
					loggingFunctor,
					machineInput.moveDirection,
					machineInput.moveDirectionWorld);

				dAttackAimVisualization::visualize(aimInput,
					(*attackSimState).get<dAttackRadialSimulation::State>(),
					(*attackSimState).get<dAttackRadialSimulation::InitialConditions>(),
					attackSimAllState.getDerivedState().get<dAttackRadialSimulation::DerivedState>(),
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
			}
		}

		const SimulationTimeStep vizSimulationStep = vizManager->runsPrediction()
			? vizManager->getClientClock().getPredictionStep()
			: vizManager->getServerClock().getSimulationStep();

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

		// ⛔G-26  docs/SimmableUpdateComponent-guards.md
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
				vizManager->editQueryAdapter(),
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
				const auto& machineInput = vizPlayerInput->get<dAttackMachineSimulation::PlayerInput>();
				dAttackBlockPredictionVisualization::Input blockPredInput(DeltaTime,
					machineInput.aimDirection,
					machineInput.moveDirection,
					machineInput.moveDirectionWorld,
					vizManager->getPhysicsBodyReaderAdapter(),
					vizManager->editQueryAdapter(),
					rendererFunctorImpl);
				dAttackBlockPredictionVisualization::visualize(blockPredInput,
					m_attackBlockPredictionVisualizationState.value(),
					m_staticData->m_attackSimulationStaticData,
					(*attackSimState).get<dAttackRadialSimulation::State>());
			}
		}

		// ⛔G-27  docs/SimmableUpdateComponent-guards.md
		if (!inputHistoryVisualizationUImpl::masterEnabled())
			return;

		{
			const bool feedRowPanel   = inputHistoryVisualizationUImpl::displayEnabled();
			const bool feedAnyBar     = inputHistoryVisualizationUImpl::anyBarEnabled();
			const bool feedInputDelay = inputHistoryVisualizationUImpl::inputDelayEnabled();

			// ⛔G-28  docs/SimmableUpdateComponent-guards.md
			if (!feedRowPanel && !feedAnyBar)
				return;

			const unsigned int ownCharacterId = ownKey;

			if (feedRowPanel)
			{
				const std::optional<unsigned int> rowCharacterId =
					inputHistoryVisualizationUImpl::firstLocalCharacterId(GetWorld());

				// ⛔G-29  docs/SimmableUpdateComponent-guards.md
				if (rowCharacterId.has_value() && *rowCharacterId == ownCharacterId)
				{
					vizManager->pollInputHistory(*rowCharacterId,
						vizSimulationStep.getTick(),
						dAttackMachineSimulation::g_moveStickDeadzone.load());
				}
			}

			if (feedAnyBar)
			{
				std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput;
				if (vizPlayerInput.has_value())
				{
					// ⛔G-31  docs/SimmableUpdateComponent-guards.md
					liveInput = brawlerInputHistoryVisualization::captureRowFieldsOf(
						vizPlayerInput->get<dAttackMachineSimulation::PlayerInput>(),
						dAttackMachineSimulation::g_moveStickDeadzone.load());
				}

				// ⛔G-30  docs/SimmableUpdateComponent-guards.md
				vizManager->pollInputHistoryLanes(ownCharacterId,
					vizSimulationStep.getTick(),
					(*attackSimState).get<dAttackMachineSimulation::State>().m_currentState,
					liveInput,
					inputHistoryVisualizationUImpl::pauseLanesWhileIdle(),
					feedInputDelay);
			}
		}
	}




}

void USimmableUpdateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USimmableUpdateComponent, m_simulationStateCorrectionSyncedBuffer);

	// ⛔G-32  docs/SimmableUpdateComponent-guards.md

	// ⛔G-33  docs/SimmableUpdateComponent-guards.md

	// ⛔G-34  docs/SimmableUpdateComponent-guards.md

}

// ⛔G-35  docs/SimmableUpdateComponent-guards.md
OGSIM_OPTIMIZE_ON

