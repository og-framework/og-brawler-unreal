// SPDX-License-Identifier: BUSL-1.1

#include "SimmableUpdateComponent.h"
#include "OGSimulation/CompilerControl.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Chaos/Declares.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/DAttackRadialVisualization.h"
#include "OGBrawler/BrawlerProjectileVisualization.h"
#include "OGBrawler/DAttackAimVisualization.h"
#include "OGBrawler/BrawlerVisualizationInputSource.h"
#include "OGSimulation/DMathUtil.h"
#include "OGBrawler/DAttackMachineSimulationRuntimeTweakables.h"
// The relay ring's carrier: forward-declared in the header, needed whole here.
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
#include "Engine/Engine.h"   // GEngine->AddOnScreenDebugMessage (Task 11 build-mismatch toast)



// =============================================================================
// ORIENTATION -- USimmableUpdateComponent
// =============================================================================
// One instance per simulated character, on the character actor. This is the
// ADAPTER: it registers the character with the simulation, owns that character's
// wire surfaces, and is the RPC boundary for client->server input. It holds no
// netcode policy of its own -- all of it is core, reached through the manager.
//
// BINDING DECLARATION -- THIS FILE IS UE ADAPTER CODE. Every engine or project
// type named in this file's comments is one adapter's binding for the role it
// names -- `USimmableUpdateComponent`, `ASimulationManagerUImpl`, `GEngine`,
// `ASimulationInputRelay`, `ASimulationConnectionRelay`, `AOGBrawlerUECharacter`,
// and, equally one adapter's binding, the replication vocabulary: `UChildConnection`,
// `FNetPing`, `COND_SkipOwner`, `COND_OwnerOnly`, `NetSerialize`, `Iris`, `UObject`,
// `FSimulationStateSyncBuffer`, `FSimulationInputSyncBuffer` -- and another
// adapter substitutes its own. The engine-free core never sees any of them.
//
// ROLE. One instance runs on each side and they are not the same object. The
// role test is `GetNetMode() != NM_Client`, world-level, and it is the same
// predicate the manager picks its own role with.
//
// FOUR CHANNELS, and which way each one runs:
//
//   correction state    server -> owning client    replicated property + OnRep
//   input bundle        client -> server           unreliable server RPC
//   relayed input       server -> the OTHER peers  the relay host actor's ring
//   connection tier     server -> owning client    the connection relay actor
//
// PHASE ORDER. Every step polls, because every step can legitimately not be
// ready yet, and "not yet" is never an error:
//
//   BeginPlay
//     -> tryInitializeWithManager      until the manager exists; then the query
//                                      volumes and the visualization state
//     -> tryRegisterWithNewFramework   until the bodies resolve; then register,
//                                      latch the provider decision, spawn (or
//                                      find) the relay host, and -- authority
//                                      only -- register the id -> component route
//   TickComponent                      visualization only, both roles
//   EndPlay
//     -> unregister, then destroy (authority) or unbind (client) the relay host
//
// THE CONCEPT SURFACE, and it is why this file contains forwarders: the core
// reaches this object through an accessor pair and a callback pair, and nothing
// else. The relayed-input ring itself lives on a separate per-character actor,
// and the core never learns that.
//
// PROVIDER PRESENCE IS THE IDENTITY TEST. A locally-controlled character gets an
// input provider; every other character gets none, and provider-ABSENCE is what
// gives it a relay store for the core to predict from.
//
// Rationale, provenance and the worked derivations:
//   Source/OGBrawlerUnreal/docs/SimmableUpdateComponent-rationale.md
// The `§N` marks in this file point there.
// =============================================================================
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

namespace DAttackTargetVisualizationCVars
{
	bool legacyEnemyRangeArcsEnabled = false;   // OFF by default — new block-prediction viz is primary
	static FAutoConsoleVariableRef legacyEnemyRangeArcsEnabledCVar(
		TEXT("DAttackTargetVisualization.legacyEnemyRangeArcsEnabled"),
		legacyEnemyRangeArcsEnabled,
		TEXT("Enable the legacy per-enemy inner-circle arc segments (outlined + solid). Off by default; enable for A/B comparison against the new block-prediction viz. Slated for removal once the new viz is confirmed as an improvement."),
		ECVF_Default);
}

//Component
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ~10 seconds at 60 Hz -- long enough for scene startup, short enough to fail loudly.
constexpr int32 kMaxRegistrationAttempts = 600;

USimmableUpdateComponent::USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer)
	: UActorComponent(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);

	m_staticData.emplace();



	PrimaryComponentTick.SetTickFunctionEnable(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// ⛔ Tick group deliberate; NO prerequisite is installed and no LiveLink component exists. §2
	PrimaryComponentTick.TickGroup = ETickingGroup::TG_DuringPhysics;


}


void USimmableUpdateComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	// The manager may not exist yet (BeginPlay ordering race); poll per tick until it does. §3
	if (UWorld* world = GetWorld())
	{
		world->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateWeakLambda(this, [this]() { tryInitializeWithManager(); }));
	}

}

void USimmableUpdateComponent::tryInitializeWithManager()
{
	++m_initializationAttempts;

	// ⛔ NEVER HasAuthority() HERE -- it is always true on a non-replicated actor and would
	// disagree with the world-mode gate the manager picks its own role with. §3
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

	// ⛔ NOTHING TIER-RELATED IS BOUND HERE -- the tier is a WIRE property, bound once per world. §5

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
	// Block-prediction viz shares the same target-viz query-volume list.
	m_attackBlockPredictionVisualizationState.emplace(m_targetVisualizationVolumeIds);

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

	// ⛔ A SIMULATED PROXY MUST NOT REGISTER A PROVIDER -- absence is what buys it a relay store. §4
	const AActor* ownerActor = GetOwner();
	const bool isLocallyPredicted = !isAuthority
		&& ownerActor != nullptr
		&& ownerActor->GetLocalRole() == ROLE_AutonomousProxy;

	BrawlerInputProviderFn inputProvider;
	if (isLocallyPredicted)
	{
		UOGBrawlerInputCollectionComponent* ic = m_ownerInputCollection;
		const uint32 id = (unsigned int)GetUniqueID();
		// ⛔ DO NOT CAPTURE THE MANAGER -- this lambda reaches nothing outside its arguments. §4
		inputProvider = [ic, id](const SimulationTimeStep& step,
		                         const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache) {
			return ic->buildPlayerInput(step, id, localInputCache);
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

	// ⛔ LATCHED, NEVER RECOMPUTED AT ARRIVAL -- onRelayedInputRingArrived's divergence check
	// is only meaningful if it reads literally this evaluation. §6
	m_hasLocalInputProvider = isLocallyPredicted;

	// ⛔ SPAWN THE HOST BEFORE THE noteDelayedInputComponent ROUTE -- until that route exists,
	// relayRemoteInput stages into m_detachedRelayStagingRing, unseen and unlogged. §6
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
		// CLIENT, the PULL half. NEITHER ORDERING CAN BE RELIED ON, so push and pull are both
		// idempotent and both required. §6
		attachInputRelayHost(
			ASimulationInputRelay::findForOwner(GetWorld(), GetOwner()));
	}

	// ⛔ ROUTE REGISTERED ONCE, erased in unregisterFromNewFramework. Authority only. §7
	if (isAuthority)
		regManager->noteDelayedInputComponent((unsigned int)GetUniqueID(), *this);

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

	// ⛔ AUTHORITY DESTROYS, CLIENT ONLY UNBINDS -- a local destroy races the destruction bunch. §6
	if (ASimulationInputRelay* host = m_inputRelayHost.Get())
	{
		if (isAuthority)
			host->detachFromParentAndDestroy();      // RemoveDependentActor, then Destroy
		else
			host->clearOnRelayedInputReceivedCallback();
	}
	m_inputRelayHost = nullptr;

	UActorComponent::EndPlay(EndPlayReason);
}


void USimmableUpdateComponent::OnRep_CorrectionState()
{
	// ⛔ ONCE A MISMATCH IS LATCHED EVERY LATER OnRep NO-OPS -- a mismatched peer must not correct. §8
	if (m_wireFormatMismatchDetected)
		return;

	// The sender's wire-format version is byte 0, captured by the buffer's NetSerialize. §8
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

// ⛔ `OnRep_CorrectionInput` STOOD HERE, all of it retired -- see SimulationNetSync::sendCorrectionAll. §9

// ⛔ `OnRep_RelayedInputRing()` STOOD HERE -- it moved to the relay host with its property. §6

// --- Relay-ring forwarders ---------------------------------------------------
// The core sees only an accessor pair and a callback pair; the ring lives elsewhere. §6

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

// ⛔ NO DEPTH PARAMETER -- see the declaration. Host resolution matches the read accessors. §6
relayedInputRing::StageArrivalOutcome USimmableUpdateComponent::stageRelayedInput(
	uint32 captureTick, uint8 dA, const simulatableBrawler::PlayerInput& input)
{
	ASimulationInputRelay* host = m_inputRelayHost.Get();
	FRelayedInputRing& stage = (host != nullptr)
		? host->editRelayedInputStagingRing()
		: m_detachedRelayStagingRing;

	const relayedInputRing::StageArrivalOutcome outcome =
		stage.stageArrival<simulatableBrawler::PlayerInput>(captureTick, dA, input);

	// ⛔ COUNTED, NOT ABSORBED, and on the HOST -- the one input loss this side can see. §6
	if (outcome.droppedOldest && host != nullptr)
		host->noteStageOverflowDrop();

	return outcome;
}

void USimmableUpdateComponent::setOnRelayedInputReceivedCallback(
	std::function<void(const FRelayedInputRing&)> fn)
{
	m_onRelayedInputReceivedCallback = std::move(fn);

	// Re-read at bind -- the ring is a persistent property, so a missed arrival recovers in full. §6
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
		return;     // idempotent — three independent link paths can reach here

	m_inputRelayHost = host;

	// ⛔ WEAK CAPTURE, NEVER A RAW `this` -- the host can outlive this component by a frame. §6
	TWeakObjectPtr<USimmableUpdateComponent> weakSelf(this);
	host->setOnRelayedInputReceivedCallback(
		[weakSelf](const FRelayedInputRing& ring)
		{
			if (USimmableUpdateComponent* self = weakSelf.Get())
				self->onRelayedInputRingArrived(ring);
		});

	// REPLAY THE CURRENT RING -- a never-written ring reads as version 0 and the ingest no-ops. §6
	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
}

void USimmableUpdateComponent::onRelayedInputRingArrived(const FRelayedInputRing& ring)
{
	// ⚠ THE OWNER-SKIP PRECONDITION, ASSERTED FROM THE RECEIVING END. A ring with entries at a
	// provider-PRESENT character means COND_SkipOwner and provider-presence disagree. §6
	//
	// ⛔ DO NOT DROP THE `num() > 0` GATE -- the host replicates to its owner, so empty is normal.
	//
	// ⛔ `ensure`, NEVER `check`, one-shot -- a bandwidth regression must stay playable to diagnose.
	if (m_hasLocalInputProvider && ring.num() > 0 && !m_loggedOwnerSkipDivergence)
	{
		m_loggedOwnerSkipDivergence = true;
		UE_LOG(LogOGNet, Error,
			TEXT("[InputRelay] OWNER-SKIP DIVERGENCE id=%u — a relay ring with %u entries "
			     "arrived for a character that HAS a local input provider. COND_SkipOwner and "
			     "provider-presence disagree; check the relay host's Owner."),
			(unsigned int)GetUniqueID(), (unsigned int)ring.num());
		ensureMsgf(false,
			TEXT("[InputRelay] relay ring arrived for a provider-present character (id=%u)"),
			(unsigned int)GetUniqueID());
	}

	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(ring);
}

void USimmableUpdateComponent::sendLocalInputToAuthority(
	const PendingInputQueue<simulatableBrawler::PlayerInput>& queue,
	uint32 currentTick,
	uint32 redundancyDepth)
{
	// Bundle the most-recent `redundancyDepth` ticks (clamped inside the builder) and send. §7
	FInputRedundancyBundle bundle;
	buildRedundancyBundle<simulatableBrawler::PlayerInput>(
		queue, currentTick, static_cast<uint8>(redundancyDepth), bundle);
	ServerReceiveRemoteMove(bundle);
}

void USimmableUpdateComponent::ServerReceiveRemoteMove_Implementation(const FInputRedundancyBundle& bundle)
{
	// ⛔ AN EMPTY BUNDLE IS IDLE TRAFFIC, NOT A MISMATCH -- no wire header means no version byte. §8
	if (bundle.wireBytes.Num() == 0)
		return;

	// ⛔ REFUSE, BUT DO NOT DISCONNECT HERE -- the disconnect path is engine-managed. §8
	const uint8 clientVersion = bundle.getWireFormatVersion();
	if (clientVersion != FInputRedundancyBundle::kWireFormatVersion)
	{
		UE_LOG(LogOGNet, Error,
			TEXT("Wire-format mismatch on ServerReceiveRemoteMove: client version=%u, server expects=%u. Get matching builds from Saved/Archive/ — see PLAYTEST_PORTABLE_README.md."),
			(unsigned int)clientVersion, (unsigned int)FInputRedundancyBundle::kWireFormatVersion);
		return;
	}

	// ⛔ NO NETCODE POLICY BELOW THIS FENCE -- tier derivation, dedup, park/drain, the malformed-
	// slot fence and the per-slot loop are all core. This side resolves engine primitives only. §7
	//
	// ⛔ ORDER IS LOAD-BEARING: fence, then ONE RTT sample, then the core per-slot loop. §7
	ASimulationManagerUImpl* authorityManager =
		ASimulationManagerUImpl::instanceFor(/*isAuthority=*/true);
	ASimulationManagerUImpl::BrawlerReceptionCoordinator* coordinator =
		(authorityManager != nullptr) ? authorityManager->getReceptionCoordinator() : nullptr;

	// ⛔ RESOLVE THE ROOT CONNECTION ONCE -- split-screen siblings collapse to their shared wire. §7
	UNetConnection* rootConn =
		(coordinator != nullptr) ? GetRootNetConnection(GetOwner()) : nullptr;

	const unsigned int id = (unsigned int)GetUniqueID();

	// ⛔ THE NO-WIRE FALLBACK IS ADAPTER-SIDE -- the core only ever sees a valid wire. §7
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

	// ⛔ ONE RTT SAMPLE PER BUNDLE, NEVER PER SLOT -- per-slot couples the tier EMA to depth. §7
	coordinator->noteRttSample(
		handle, id, authorityManager->getServerReceptionTick(),
		readRoundTripMs(rootConn), *this);

	// Tier keys on the ROOT connection (latency is the link's); input keys on the slot too. §7
	const uint8 playerSlot = GetPlayerSlotForActor(GetOwner());

	// ⛔ THE DELAY IS APPLIED EXACTLY ONCE (park-to-release) -- a released input keeps its ORIGINAL
	// captureTick, and SimulationInputResolution::collectInputAll pops it in ARRIVAL order. §7
	//
	// The LAST argument is the RELAY sink: delivery routes an input IN, the relay forwards it OUT. §6
	coordinator->receiveInputBundle<SimulatableBrawler>(
		id, handle, playerSlot, bundle, *authorityManager, *authorityManager);
}

// Asserted here too, so a breaking signature change is a legible static_assert. §7
static_assert(ConnectionTierSink<USimmableUpdateComponent>,
	"USimmableUpdateComponent must satisfy ConnectionTierSink so ServerReceptionCoordinator "
	"can drive the tier send through it");

void USimmableUpdateComponent::sendConnectionTierToOwningClient(unsigned int id, uint8_t tier)
{
	// ⛔ PURE TRANSPORT -- the core applied the no-reading skip and the publish-only-on-change
	// dedup already, so no sentinel check and no changed-vs-current test belongs here. §7
	check(id == (unsigned int)GetUniqueID());

	// The tier is written to the wire's relay actor: siblings share one connection, one tier. §5
	UNetConnection* rootConnection = GetRootNetConnection(GetOwner());
	if (rootConnection == nullptr)
	{
		// No wire identity (standalone / listen-server local pawn). Defensive: the ONLY caller
		// already early-outs on a null root connection. §7
		return;
	}

	ASimulationConnectionRelay* relay =
		ASimulationConnectionRelay::findOrSpawnForConnection(GetWorld(), rootConnection);
	if (relay == nullptr)
		return;     // findOrSpawnForConnection logged the reason

	UE_LOG(LogOGNet, Log,
		TEXT("[ConnectionTier] id=%u server tier %u -> %u (relay %s)"),
		(unsigned int)GetUniqueID(),
		(unsigned int)relay->getConnectionTier(), (unsigned int)tier,
		*relay->GetName());

	relay->setConnectionTier(tier);
}

DAttackState USimmableUpdateComponent::getMachineVizState()
{
	// Mirrors the TickComponent viz lookup; returns Idle on any missing link. §10
	const bool vizIsAuthority = (GetNetMode() != NM_Client);
	ASimulationManagerUImpl* manager = ASimulationManagerUImpl::instanceFor(vizIsAuthority);
	if (manager == nullptr)
		return DAttackState::Idle;

	SimulationObjectStorage<SimulatableBrawler>& storage = manager->editStorage();
	if (!storage.has<SimulatableBrawler>((unsigned int)GetUniqueID()))
		return DAttackState::Idle;

	const SimulatableBrawler& simulatable = storage.get<SimulatableBrawler>((unsigned int)GetUniqueID());
	return simulatable.getVizState()
		.getState()
		.get<dAttackMachineSimulation::State>()
		.m_currentState;
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

		// --- Render-side input echo: one viz input source, local vs remote -----------
		// Resolved ONCE per render frame and shared by every input-carrying viz site. §10
		//
		// ⛔ hasInputComponent() IS THE LOCAL-vs-REMOTE DISCRIMINATOR -- the host's own pawn is LOCAL.
		//
		// ⛔ NOT the registration-time ROLE_AutonomousProxy test: that one excludes the host. §10
		//
		// REMOTE proxy: the relay store's LAST-KNOWN input, NOT the per-tick scheduled read. §10
		//
		// ⚠ ON THE AUTHORITY THE RELAY STORE EXISTS BUT IS NEVER WRITTEN, so this answers nullopt. §11
		//
		// ⛔ CONTINUOUS FIELDS ONLY -- discrete fields are pinned neutral; never fed to the sim. §10
		//
		// ⛔ NO TIER CONSULT HERE -- muting on a degraded tier is a separate, optional change.
		const bool hasLiveLocalInput =
			m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent();

		const std::optional<simulatableBrawler::PlayerInput> vizPlayerInput =
			simulatableBrawler::selectVisualizationInput(
				hasLiveLocalInput,
				[this]() { return m_ownerInputCollection->buildLatestVisualizationInput(); },
				vizManager->getLastRelayedInput((unsigned int)GetUniqueID()));

		// Attack aim viz, from the shared vizPlayerInput; skipped for a proxy with nothing relayed. §10
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
					attackSimAllState.getDerivedState().m_attackDerivedState,
					m_staticData->m_attackSimulationStaticData,
					m_attackAimVisualizationState.value());
			}
		}

		// currentTick + dt come from the SAME clock the sim uses. §10
		//
		// HOISTED so the projectile viz and the input-history poll share ONE read: a
		// second copy of this branch is a second convention waiting to drift. §10
		const SimulationTimeStep vizSimulationStep = vizManager->runsPrediction()
			? vizManager->getClientClock().getPredictionStep()
			: vizManager->getServerClock().getSimulationStep();

		// Projectile viz. §10
		{
			brawlerProjectileVisualization::Input projectileVisualizationInput(
				rendererFunctorImpl,
				m_staticData->m_projectileStaticData,
				vizSimulationStep.getTick(),
				vizSimulationStep.getDeltaSeconds());
			brawlerProjectileVisualization::visualize(projectileVisualizationInput,
				(*attackSimState).get<brawlerProjectileSimulation::State>(),
				attackSimAllState.getDerivedState().m_projectileDerivedState,
				m_projectileVisualizationState);
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

		// Block-prediction viz draws for EVERY character; only the input SOURCE splits. §10
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

		// --- Input-history poll: the display's only feed ------------------------------
		// The rings behind this are keyed per character id, so a sibling on the couch
		// would get its own the moment anything polled it.
		// ⛔ FIRST LOCAL PLAYER'S CHARACTER ONLY -- a SELECTION here, not a structural limit.
		//
		// ⛔ RENDER-RATE GAME-THREAD READ of a physics-written capture line: the accepted
		// tear argued at ASimulationManagerUImpl's CROSSING block. Nothing decides on it.
		//
		// ⛔ THE MASTER, READ ALONE, FIRST: with it off this costs one bool read and returns.
		if (!inputHistoryVisualizationUImpl::masterEnabled())
			return;

		// The panel and the three bars, one shared id walk. None on means none of the
		// feeds run. ⛔ THE DELAY BAR IS A THIRD BAR ON THE METER, so it implies the LANE
		//   poll runs even when the other two bars are off.
		{
			const bool feedRowPanel   = inputHistoryVisualizationUImpl::displayEnabled();
			const bool feedAnyBar     = inputHistoryVisualizationUImpl::anyBarEnabled();
			const bool feedInputDelay = inputHistoryVisualizationUImpl::inputDelayEnabled();

			// ⛔ ABOVE THE ID WALK BELOW, NEVER A CONJUNCT IN IT: off must not pay for that walk.
			if (!feedRowPanel && !feedAnyBar)
				return;

			const std::optional<unsigned int> historyCharacterId =
				inputHistoryVisualizationUImpl::firstLocalCharacterId(GetWorld());

			if (historyCharacterId.has_value()
				&& *historyCharacterId == (unsigned int)GetUniqueID())
			{
				if (feedRowPanel)
				{
					vizManager->pollInputHistory(*historyCharacterId,
						vizSimulationStep.getTick(),
						dAttackMachineSimulation::g_moveStickDeadzone.load());
				}

				// The machine state comes off the SAME attackSimState the block-prediction viz
				// above already reads, at the same site.
				// ⛔ NO NEW SEAM IS OPENED FOR IT.
				//
				// There is no per-tick machine-state history to read, so a tick this poll misses
				// stays a hole rather than being invented.
				// ⚠ SAMPLED LIVE AND NEVER BACK-FILLED.
				// The pause's input half is the PANEL'S OWN classification of this capture,
				// so "no input" means one thing across both displays.
				// ⛔ NO SECOND IDEA OF NEUTRAL IS DERIVED HERE.
				if (feedAnyBar)
				{
					std::optional<brawlerInputHistoryVisualization::CaptureRowFields> liveInput;
					if (vizPlayerInput.has_value())
					{
						liveInput = brawlerInputHistoryVisualization::captureRowFieldsOf(
							vizPlayerInput->get<dAttackMachineSimulation::PlayerInput>(),
							dAttackMachineSimulation::g_moveStickDeadzone.load());
					}

					vizManager->pollInputHistoryLanes(*historyCharacterId,
						vizSimulationStep.getTick(),
						(*attackSimState).get<dAttackMachineSimulation::State>().m_currentState,
						liveInput,
						inputHistoryVisualizationUImpl::pauseLanesWhileIdle(),
						feedInputDelay);
				}
			}
		}
	}




}

void USimmableUpdateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USimmableUpdateComponent, m_simulationStateCorrectionSyncedBuffer);

	// ⛔ THE `m_replicatedInputSyncedBuffer` REGISTRATION STOOD HERE -- removed in the SAME edit
	// as the member: one half alone is a compile error, the other a silently dead field. §9
	//
	// ⛔ REMOVING A PROPERTY NEEDS NO VERSION BUMP -- registrations are lifetime entries. §9

	// ⛔ `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` STOOD HERE, with NO COND_.
	// It is now COND_SkipOwner on the ring's own per-character dependent object. §6
	//
	// ⛔ THE COND_ COULD NOT HAVE BEEN ADDED HERE -- the shared atomic batch is what forced it. §6

	// ⛔ THE COND_OwnerOnly TIER REGISTRATION IS GONE -- owner-only RELEVANCY narrows it now. §5
}

// ⛔ CLOSES THE OGSIM_OPTIMIZE_OFF PAIR AT TRUE EOF. The file carried no closing pragma from
// its first commit, so the whole TU compiled unoptimized in every build. §2
OGSIM_OPTIMIZE_ON

