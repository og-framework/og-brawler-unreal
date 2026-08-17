// SPDX-License-Identifier: BUSL-1.1

#include "SimmableUpdateComponent.h"
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
// [T39] The relay ring's new carrier — forward-declared in the header, needed
// whole here (spawn, find, the ring accessors, the callback install).
#include "OGSimulationUnreal/SimulationInputRelay.h"

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
#include "OGSimulationUnreal/UEConnectionHandle.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"

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

	// [T10 / og-netcode-v2-input-relay] The client-side tier consumer used to be
	// bound here, lazily, per character. It now lives on the manager (one per
	// world, bound in its BeginPlay where the TimeConfig is created) because the
	// tier is a WIRE property, not a character property — see
	// ASimulationConnectionRelay. Nothing tier-related is initialized here anymore.

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

	// On a pure client only the locally-controlled character (ROLE_AutonomousProxy)
	// feeds a live input provider. Simulated proxies of other players must NOT
	// register a provider — provider-ABSENCE is what gives a character a
	// RemoteInputCache, and collectInputAll's proxy branch then predicts it from
	// that store via the scheduled read ([T7]; it used to hold the correction
	// cache's last server-reported input instead).
	const AActor* ownerActor = GetOwner();
	const bool isLocallyPredicted = !isAuthority
		&& ownerActor != nullptr
		&& ownerActor->GetLocalRole() == ROLE_AutonomousProxy;

	BrawlerInputProviderFn inputProvider;
	if (isLocallyPredicted)
	{
		UOGBrawlerInputCollectionComponent* ic = m_ownerInputCollection;
		const uint32 id = (unsigned int)GetUniqueID();
		// [T15] The manager is no longer captured. The matcher's input history used
		// to be fetched from it (manager -> reconciliation -> correction cache);
		// collectInputAll now hands the character's own raw-capture delay line
		// straight in, so this lambda reaches for nothing outside its arguments.
		inputProvider = [ic, id](const SimulationTimeStep& step,
		                         const LocalInputCache<simulatableBrawler::PlayerInput>& delayLine) {
			return ic->buildPlayerInput(step, id, delayLine);
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

	// [T39] Latch the provider decision. This is the client half of the owner-skip
	// precondition (see onRelayedInputRingArrived): a ring arriving for a
	// provider-PRESENT character means COND_SkipOwner and provider-presence have
	// diverged. Latched here rather than recomputed at arrival so the two decisions
	// are literally the same evaluation, taken once.
	m_hasLocalInputProvider = isLocallyPredicted;

	// ⭐ [T39] THE RELAY RING'S HOST — created here, on the authority, at the
	// moment registration completes.
	//
	// ORDER IS LOAD-BEARING AND IS THE REASON THIS SITS ABOVE THE ROUTE BELOW.
	// `noteDelayedInputComponent` is what makes `relayRemoteInput` able to find
	// this component and write the ring. If the host did not exist by then, the
	// first writes would land in `m_detachedRelayRing` and be invisible to every
	// client, with nothing logged. Spawning first closes that window by
	// construction rather than by timing.
	//
	// This is also why the host is NOT spawned on demand from the write path the
	// way ASimulationConnectionRelay is: that actor has no always-reached creation
	// point earlier than its first write (and PostLogin misses seamless travel),
	// whereas this one does.
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
		// CLIENT: the PULL half of the link. The host may already have replicated
		// in and pushed at the manager's listener before this component registered
		// (in which case attachInputRelayHost already ran and this is a no-op), or
		// it may not have arrived yet (in which case this finds nothing and the
		// listener's push does the linking later). Both paths are idempotent; the
		// pair exists because neither ordering can be relied on.
		attachInputRelayHost(
			ASimulationInputRelay::findForOwner(GetWorld(), GetOwner()));
	}

	// [T24] Register the id -> component delivery route ONCE, at register-time,
	// paired with the forgetOwner()/erase at unregisterFromNewFramework. This
	// replaces the former per-park noteDelayedInputComponent call inside the receive
	// loop: the route the coordinator's drain (and the malformed-slot deliver-now
	// fallback) resolves against now exists for the whole registered lifetime of the
	// component, not just after its first parked slot. Authority only — the
	// coordinator, the delay queue, and the drain are all std::nullopt / no-op on a
	// pure client, and m_onRemoteMoveReceivedCallback (the delivery terminus) is
	// itself only wired at registration, so the route and its target come up together.
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

	// [T39] The ring host dies with the character it carries input for.
	//
	// AUTHORITY DESTROYS; a client only drops its reference. A replicated actor is
	// removed on clients by the destruction bunch, and destroying it locally would
	// race that. This mirrors the split every other replicated actor in this
	// codebase uses, and is the reason ASimulationConnectionRelay's self-reap is
	// authority-gated too.
	//
	// EXPLICIT, NOT SELF-REAPING. The connection relay polls a 1 Hz timer because
	// its owning PlayerController's death does not destroy it and there is no other
	// reliable signal. This host has one: the component whose character it belongs
	// to is ending play, right here, synchronously.
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

// [og-netcode-v2-input-relay T8] `OnRep_CorrectionInput` stood here. It peeked the
// replicated input for the `[ReceiveCorrectionInput]` trace and forwarded the
// buffer to the core's injectCorrectionInput binding. Property, OnRep, trace and
// binding are all retired — see the retirement block at
// SimulationNetSync::sendCorrectionAll.

// [og-netcode-v2-input-relay / T1] Arrival of the outbound relay ring on a peer.
//
// Deliberately thin: T1 only opened the channel. The ring's entries are keyed by
// CAPTURE tick and stamped with the schedule `dA`, so the consumer — not this
// handler — decides what to do with them (T5 stores them per remote character; T7
// reads them through the schedule). [T8] It is now the ONLY inbound input channel
// a peer has for a character it does not control.
//
// [T39] `OnRep_RelayedInputRing()` stood here. The OnRep moved to
// ASimulationInputRelay with the property it notifies; what arrives here now is
// onRelayedInputRingArrived, routed from that actor through the callback
// attachInputRelayHost installs. The BODY is unchanged in substance — forward the
// ring to the core if anything is bound — with the owner-skip divergence check
// added, because this is the first build in which a ring arriving at a
// locally-predicted character is a detectable fault rather than the norm.

// --- [T39] Relay-ring forwarders --------------------------------------------
//
// The three functions below are the whole of what "the component stays the
// concept surface" means in code: og-simulation still sees an accessor pair and a
// callback pair on this object, and never learns that the ring is carried by a
// different UObject entirely.

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

// ⭐ [T34] THE STAGED RELAY WRITE. See the declaration for why there is no depth
// parameter. The host resolution is the same pair the read accessors use, so a
// component that is somehow unlinked degrades identically on both paths.
relayedInputRing::StageArrivalOutcome USimmableUpdateComponent::stageRelayedInput(
	uint32 captureTick, uint8 dA, const simulatableBrawler::PlayerInput& input)
{
	ASimulationInputRelay* host = m_inputRelayHost.Get();
	FRelayedInputRing& stage = (host != nullptr)
		? host->editRelayedInputStagingRing()
		: m_detachedRelayStagingRing;

	const relayedInputRing::StageArrivalOutcome outcome =
		stage.stageArrival<simulatableBrawler::PlayerInput>(captureTick, dA, input);

	// A burst longer than the stage is the ONE input loss this side of R = 0 can
	// see, so it is counted rather than absorbed. Counted on the HOST because the
	// host is what a run inspects, and because a drop against the detached fallback
	// is already covered by the louder fault that no host exists.
	if (outcome.droppedOldest && host != nullptr)
		host->noteStageOverflowDrop();

	return outcome;
}

void USimmableUpdateComponent::setOnRelayedInputReceivedCallback(
	std::function<void(const FRelayedInputRing&)> fn)
{
	m_onRelayedInputReceivedCallback = std::move(fn);

	// If the host is ALREADY linked, the core has just bound after an arrival may
	// have been dropped. The ring is a persistent property, so re-reading it is a
	// complete recovery — the same argument registerPredictionOwner's bind-time
	// populate rests on, applied to the other ordering.
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

	// Route the host's arrivals at this component. Weak capture: the host outlives
	// nothing, but it CAN outlive this component by a frame during teardown, and a
	// raw `this` would then be a dangling call from a replicated OnRep.
	TWeakObjectPtr<USimmableUpdateComponent> weakSelf(this);
	host->setOnRelayedInputReceivedCallback(
		[weakSelf](const FRelayedInputRing& ring)
		{
			if (USimmableUpdateComponent* self = weakSelf.Get())
				self->onRelayedInputRingArrived(ring);
		});

	// REPLAY THE CURRENT RING. Covers the ordering where the host replicated (and
	// dropped one or more OnReps) before this link existed. A never-written ring
	// reads as version 0 and the ingest no-ops, so this is free on the authority
	// and on a freshly spawned host.
	if (m_onRelayedInputReceivedCallback)
		m_onRelayedInputReceivedCallback(host->getRelayedInputRing());
}

void USimmableUpdateComponent::onRelayedInputRingArrived(const FRelayedInputRing& ring)
{
	// ⚠ [T39] THE OWNER-SKIP PRECONDITION, ASSERTED FROM THE RECEIVING END.
	//
	// The design's owner-echo removal rests on an equivalence nothing checked:
	// "owning connection" — the set COND_SkipOwner narrows on, decided server-side
	// from the host's owner chain — and "provider present" — the set that decides
	// whether `SimulationNetSync::registerPredictionOwner` builds a
	// RemoteInputCache at all, decided here from the local role. If they ever
	// name different sets, the failure is silent in BOTH directions: a character
	// in the first set but not the second still pays the echo, and a character in
	// the second but not the first loses its relayed input entirely.
	//
	// THIS is the observable half. A ring with resident entries arriving for a
	// character that has a local input provider means the skip did not apply where
	// this build believes it does — most likely an unset or wrong Owner on the
	// host (see the producing-end warning in ASimulationInputRelay::
	// spawnForCharacter). Nothing consumes the payload either way: the core binds
	// no store for a provider-present id, so this is pure diagnosis.
	//
	// GATED ON `num() > 0` DELIBERATELY. An empty ring is not evidence: the host
	// actor itself is bAlwaysRelevant and DOES replicate to the owning client (only
	// the property is skipped), so an owner legitimately holds a host carrying a
	// never-written ring, and asserting on that would fire on every clean run.
	//
	// ONE-SHOT, and `ensure` rather than `check`: this is a bandwidth and
	// correctness regression, not a memory-safety fault, and a session that hits
	// it should stay playable enough to be diagnosed.
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

	// [T21/T24] THIN TRANSPORT ADAPTER. Everything below the wire-format fence is
	// pure engine-primitive acquisition plus a forward into the engine-agnostic
	// ServerReceptionCoordinator. NO netcode policy lives here: the tier
	// derivation/EMA, capture-tick dedup, park/drain, the malformed-slot fence, AND
	// (T24) the per-slot receive loop itself are all in the core coordinator. This
	// side resolves only the primitives a second engine must also supply — Address
	// (root connection), player slot, RTT, server sim tick — and the wire buffer,
	// then forwards them. The id->component delivery route is registered ONCE at
	// register-time (tryRegisterWithNewFramework), not per slot.
	//
	// The receive-path order is documented and load-bearing: fence (above) -> RTT
	// SAMPLE (once per bundle) -> the core per-slot loop (receiveInputBundle).
	ASimulationManagerUImpl* authorityManager =
		ASimulationManagerUImpl::instanceFor(/*isAuthority=*/true);
	ASimulationManagerUImpl::BrawlerReceptionCoordinator* coordinator =
		(authorityManager != nullptr) ? authorityManager->getReceptionCoordinator() : nullptr;

	// Resolve the ROOT connection ONCE — split-screen siblings (UChildConnection)
	// collapse to the single wire entry their shared connection deserves; tier and
	// slot key are both derived from it. A null connection is the documented
	// "no wire identity" sentinel (standalone / listen-server local pawn).
	UNetConnection* rootConn =
		(coordinator != nullptr) ? GetRootNetConnection(GetOwner()) : nullptr;

	const unsigned int id = (unsigned int)GetUniqueID();

	// THE NO-WIRE FALLBACK BOUNDARY IS HERE, adapter-side (fable ruling). When there
	// is no coordinator or no wire (standalone / listen-server local pawn), every
	// slot takes the legacy undelayed delivery path directly, WITHOUT the coordinator
	// — the core only ever sees a valid wire (task AC "Preserve fallback").
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

	// (1) RTT SAMPLE — ONCE PER BUNDLE, before the per-slot loop. A bundle is one
	// datagram (one arrival event) and FNetPing's RoundTrip only advances on ack
	// receipt anyway; sampling per slot would feed the same reading into the tier
	// EMA up to kMaxSlots times and couple smoothing to redundancy depth.
	// noteRttSample DRIVES THE SEND itself: it derives the tier and, on a change for
	// this owner, fires sendConnectionTierToOwningClient through the sink (*this).
	// The "no reading" skip and the publish-only-on-change dedup are core-owned (T23).
	coordinator->noteRttSample(
		handle, id, authorityManager->getServerReceptionTick(),
		readRoundTripMs(rootConn), *this);

	// (2) Slot primitive: WHICH local player on that wire (the child-connection id).
	// Tier is keyed on the ROOT connection (latency is a property of the link);
	// INPUT is per character, so the queue is keyed on the slot too.
	const uint8 playerSlot = GetPlayerSlotForActor(GetOwner());

	// (3) The whole per-slot loop is now core (T24): receiveInputBundle decodes the
	// wire buffer, parks each slot in the ServerInputDelayQueue, and on the ONE
	// non-parked (malformed-slot) path delivers immediately through the delivery
	// sink — here the manager, which routes id->component via the register-time
	// route. THE DELAY IS APPLIED EXACTLY ONCE (park-to-release): a released input
	// keeps its ORIGINAL captureTick, and the server's consumer
	// (SimulationNetSync::collectInputAll, authority branch) pops RemoteMoveQueue in
	// ARRIVAL ORDER, so it contributes no offset of its own.
	//
	// [T3] The LAST argument is the RELAY sink — the same manager again, bound to a
	// different boundary: on each genuinely-new capture tick the core taps the
	// receipt path and calls relayRemoteInput, which writes (captureTick, dA, input)
	// into that character's replicated relay ring for the OTHER clients. Two
	// arguments, one object, deliberately: delivery routes an input INTO the
	// simulation, the relay forwards it OUT. Nothing about the delivery half — or
	// about sendCorrectionAll's still-live input write (dual-write until T8) — is
	// changed by its presence.
	coordinator->receiveInputBundle<SimulatableBrawler>(
		id, handle, playerSlot, bundle, *authorityManager, *authorityManager);
}

// This component IS a ConnectionTierSink — the concept is compile-checked at the
// noteRttSample call site below, but assert it here too so a breaking signature
// change surfaces as a legible static_assert rather than an opaque template error.
static_assert(ConnectionTierSink<USimmableUpdateComponent>,
	"USimmableUpdateComponent must satisfy ConnectionTierSink so ServerReceptionCoordinator "
	"can drive the tier send through it");

void USimmableUpdateComponent::sendConnectionTierToOwningClient(unsigned int id, uint8_t tier)
{
	// PURE TRANSPORT (T23). The core already applied the "no reading" skip and the
	// publish-only-on-change dedup, and only calls this when the tier for this
	// owner actually changed — so there is NO sentinel check and NO changed-vs-
	// current test here. This component is its own sink target, so the id must be
	// its own unique id (a central-manager sink in another engine would route on it).
	check(id == (unsigned int)GetUniqueID());

	// [T10 / og-netcode-v2-input-relay] The tier is written to the wire's relay
	// actor, not to a property of this component. Split-screen siblings resolve to
	// the SAME root connection and therefore to the SAME relay — one wire, one
	// tier property, which is what makes the sibling-starvation class the core's
	// per-owner dedup guards against structurally impossible on the transport side.
	UNetConnection* rootConnection = GetRootNetConnection(GetOwner());
	if (rootConnection == nullptr)
	{
		// No wire identity (standalone / listen-server-local pawn). Defensive: the
		// only caller is the RPC receive path, which already early-outs on a null
		// root connection before it can reach noteRttSample.
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
	// Mirrors the TickComponent viz lookup below (same pattern, same authority
	// resolution). Returns Idle on any missing link so callers stay ungated
	// during registration ordering races.
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

		// --- D5.4 render-side input echo (T13): local-vs-remote viz input source ---
		//
		// Resolved ONCE per render frame and shared by every input-carrying viz site
		// below, so the aim indicator and the block-prediction wedge can never read
		// two different samples of the same frame.
		//
		// LOCAL character: sample live, at THIS render frame's rate. hasInputComponent()
		// is the local-vs-remote discriminator and is exactly the test the radial viz
		// above already uses (line ~555) — setupBindings() is reached only from
		// AOGBrawlerUECharacter::SetupPlayerInputComponent, which UE calls only for a
		// locally-controlled pawn. It is true for a pure client's autonomous proxy AND
		// for the listen-server host's own pawn; it is false for every simulated proxy.
		// (The registration-time ROLE_AutonomousProxy test used in tryRegisterWithManager
		// is deliberately NOT reused here: it excludes the host, which we now want to echo.)
		//
		// REMOTE simulated proxy: [T7] RE-SOURCED. This used to read the correction
		// cache's input column (`editReconciliation().getLatestInput(...)`) — fed by
		// the SERVER->CLIENT correction-input channel. It now reads the relay store's
		// LAST-KNOWN relayed input, which is the same character's real input as
		// relayed by the server, and is the source the sim's own proxy prediction
		// resolves from. [T8] That channel is now DELETED, so this re-point is
		// irreversible: a viz left on the column would today be rendering this
		// client's own prediction of the proxy, silently, dressed as authority.
		// [T16] And the column itself is now gone, so "left on the column" is no
		// longer even expressible — this is the only remote source there is.
		// Still the only input that exists for someone else's character, and still
		// tick-quantized. NOT the per-tick scheduled read: the viz wants "what is this
		// player doing", not "which input does tick N run on".
		//
		// LISTEN-SERVER IMPROVEMENT (intended, T13, and PRESERVED by the re-source):
		// the host's own pawn takes the LOCAL path here, so the remote-source read is
		// not what it renders from. Its fallback still holds either way — the authority
		// allocates no relay stores at all (registerPredictionOwner's provider-absent
		// branch is the only site that creates one, and it runs on the client), so this
		// call answers nullopt on the authority for exactly the same structural reason
		// getLatestInput did when the caches were what was missing (T16 has since
		// retired getLatestInput along with the whole input column; the structural
		// argument for THIS accessor's nullopt is unaffected — it is about stores).
		//
		// The echoed value carries CONTINUOUS FIELDS ONLY (T12's makeVisualizationPlayerInput
		// pins every discrete field neutral), so attack / Hadouken edges structurally cannot
		// render-echo. Cosmetic only — never fed to the sim, the RPC or a cache.
		//
		// No tier consult here: muting on a degraded tier is optional task T15.
		const bool hasLiveLocalInput =
			m_ownerInputCollection != nullptr && m_ownerInputCollection->hasInputComponent();

		const std::optional<simulatableBrawler::PlayerInput> vizPlayerInput =
			simulatableBrawler::selectVisualizationInput(
				hasLiveLocalInput,
				[this]() { return m_ownerInputCollection->buildLatestVisualizationInput(); },
				vizManager->getLastRelayedInput((unsigned int)GetUniqueID()));

		// Attack aim visualization — attack-direction indicator (red L / 3-point path)
		// and reference arcs. Sourced per-character via the shared vizPlayerInput above:
		// render-rate live sample for the local character, held-constant LAST RELAYED
		// input for remote simulated proxies ([T7] — was the correction cache's input
		// column, ~1 RTT behind; the relayed one is fresher by roughly the return leg).
		// For a remote proxy with nothing relayed yet, the viz is skipped for that frame
		// — unchanged pre-existing behaviour, preserved by the nullopt contract.
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
				rendererFunctorImpl,
				DAttackTargetVisualizationCVars::legacyEnemyRangeArcsEnabled);
			dAttackTargetVisualizationTwo::visualize(attackTargetVisualizationInput,
				m_attackTargetVisualizationState.value(),
				m_staticData->m_attackSimulationStaticData,
				(*attackSimState).get<dAttackMachineSimulation::State>(),
				(*attackSimState).get<dAttackRadialSimulation::State>(),
				(*attackSimState).get<dAttackGuardSimulation::State>());
		}

		// Attacker-side block-prediction viz. NO local-player gate on WHETHER it draws —
		// it draws for every character; the local-vs-remote split is only about WHERE the
		// input comes from, and that decision is made once in vizPlayerInput above
		// (render-rate live sample locally, server correction ~1 RTT behind for remote
		// simulated proxies). The old "skipped on the authority" limitation called out in
		// Task 8's PIE matrix is now gone for the HOST'S OWN character, which has live
		// input; it still applies to a remote proxy whose cache has no entry yet.
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

	// [og-netcode-v2-input-relay T8] The `m_replicatedInputSyncedBuffer`
	// registration stood on the line above. Retiring a replicated property means
	// retiring its registration in the same edit: a DOREPLIFETIME naming a member
	// that no longer exists is a compile error (so it cannot be forgotten), while
	// keeping the member and dropping only the registration would have left a
	// silently-never-replicated field. Both halves went together.
	//
	// FENCE COHERENCE. Removing a property leaves NO hole and NO stale version
	// expectation: DOREPLIFETIME registrations are per-property lifetime entries,
	// not fixed wire offsets, and each surviving property carries its own
	// self-describing NetSerialize (watermark-trimmed length prefix). The
	// increment's SINGLE wire fence is
	// FSimulationStateSyncBuffer::kWireFormatVersion, which T4 bumped 1 -> 2 for
	// the applied-capture-tick ref and which T8 deliberately does NOT bump: the
	// state payload's layout is unchanged by this task, and every build that
	// speaks version 2 already agrees the input value is not on the wire — the
	// relay ring, the input RPC and the state buffer all ride behind that one
	// fence, so a mismatched peer is still refused loudly at the first correction
	// OnRep. FSimulationInputSyncBuffer's own kWireFormatVersion is likewise
	// untouched; that struct survives in its CLIENT->SERVER role.

	// [og-netcode-v2-input-relay / T1] The outbound input relay ring's
	// `DOREPLIFETIME(USimmableUpdateComponent, m_relayedInputRing)` stood on the
	// line above, with NO COND_.
	//
	// ⭐ [T39] GONE, IN THE SAME EDIT AS THE MEMBER — the rule this block already
	// states for the T8 removal, applied again. The ring is now
	// `DOREPLIFETIME_CONDITION(ASimulationInputRelay, m_relayedInputRing,
	// COND_SkipOwner)` on its own per-character Iris dependent object, which is
	// what lets it be scheduled and prioritised independently of the correction
	// state above (they used to share one atomic batch and die together under
	// packet pressure — T37) and what finally lets the owner echo be skipped (the
	// owning client provably never reads its own ring).
	//
	// ⛔ THE COND_ COULD NOT HAVE BEEN ADDED HERE INSTEAD. A condition on this
	// component would resolve against the CHARACTER's owning connection, which is
	// the same relationship — but the whole reason for the move is the atomic
	// batch, not the condition, and adding the condition without the move would
	// have reclaimed ~85 B/round while leaving the ring dying with the state on
	// every overflow frame. The saving is the smaller half of this change.

	// [T10 / og-netcode-v2-input-relay] The COND_OwnerOnly connection-tier
	// registration that used to sit here is GONE. The tier replicates from
	// ASimulationConnectionRelay, whose owner-only RELEVANCY does the narrowing
	// the condition used to do.
}

