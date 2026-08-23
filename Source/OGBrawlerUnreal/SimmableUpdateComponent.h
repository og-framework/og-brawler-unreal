// SPDX-License-Identifier: BUSL-1.1

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

#include "DVolume/DVolumeAsset.h"
#include "OGBrawler/DAttackCircle.h"
#include "OGBrawler/DAttackRadialSequence.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackGuardSimulation.h"
#include "OGBrawler/DAttackMachineSimulation.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGBrawler/DAttackRadialVisualization.h"
#include "OGBrawler/BrawlerProjectileVisualization.h"
#include "OGBrawler/DAttackTargetVisualization.h"
#include "OGBrawler/DAttackTargetVisualizationTwo.h"
#include "OGBrawler/DAttackAimVisualization.h"
#include "OGBrawler/DAttackBlockPredictionVisualization.h"

#include <functional>
#include <optional>
#include <vector>
#include "InputActionValue.h"

#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/RelayedInputRing.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGSimulation/SimulationQueues.h"

#include "SimmableUpdateComponent.generated.h"

class DAttackCircle;
class UInputAction;
class UEnhancedInputComponent;
class USimmableUpdateComponent;
class ChaosTickMapper;
class UOGBrawlerInputCollectionComponent;
// [og-netcode-v2-input-relay T39] The relay ring's new carrier. Forward-declared
// rather than included: this header is pulled in very widely, and nothing here
// needs the actor's layout — the two accessors that dereference it are defined in
// the .cpp.
class ASimulationInputRelay;

UCLASS(ClassGroup = DPhysics, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class USimmableUpdateComponent : public UActorComponent
{
	GENERATED_BODY()
public:

	/**
	 * Default UObject constructor.
	 */
	USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//virtual bool ShouldCreatePhysicsState() const override { return true; }
	//virtual void OnCreatePhysicsState() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override final;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void setAttackAxisBody(FBodyInstance* attackAxisBody) { m_attackAxisBody = attackAxisBody; }
	void setAttackAxisBody(FBodyInstanceAsyncPhysicsTickHandle attackAxisPhysicsHandle) { m_attackAxisPhysicshandle = attackAxisPhysicsHandle; }

	const simulatableBrawler::StaticData& getStaticData() const { return *m_staticData; }

	// --- PredictionSyncedBufferOwnerConcept ---
	// Correction-STATE role stays FSimulationStateSyncBuffer.
	// [og-netcode-v2-input-relay T8] SyncedRemoteInputBufferType now names ONE
	// role, not two: the CLIENT->SERVER buffer below. Its server->client twin
	// (correction input) is retired.
	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using SyncedRemoteInputBufferType = FSimulationInputSyncBuffer;
	// [og-netcode-v2-input-relay / T5] The relay ring — the third replicated
	// channel when T5 landed, and post-T8 the second of two. Named through a
	// typedef for the same reason the ones above are: SimulationNetSync binds the
	// arrival callback and reads the ring at registration, and it must do so
	// without ever naming a UE type.
	using RelayedInputRingType        = FRelayedInputRing;

	void setOnCorrectionStateReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_onCorrectionStateReceivedCallback = std::move(fn);
	}

	void clearOnCorrectionStateReceivedCallback()
	{
		m_onCorrectionStateReceivedCallback = nullptr;
	}

	// [og-netcode-v2-input-relay T8] set/clearOnCorrectionInputReceivedCallback are
	// GONE with the SERVER->CLIENT correction-input channel. The two inbound
	// bindings a prediction owner still exposes are the correction STATE (above)
	// and the relay ring (below).

	FSimulationInputSyncBuffer* getClientToServerInputSyncedBuffer()
	{
		return &m_clientToServerInputSyncedBuffer;
	}

	// Stage 1 (Task 9): builds an FInputRedundancyBundle from the most-recent
	// `redundancyDepth` ticks still retained in `queue` and fires the unreliable
	// ServerReceiveRemoteMove RPC. Bridges the UE-free SimulationNetSync send path
	// (which hands us its core PendingInputQueue) to the UE wire type. Defined in
	// the .cpp so the InputRedundancyBundleBuilder include stays out of this header.
	void sendLocalInputToAuthority(
		const PendingInputQueue<simulatableBrawler::PlayerInput>& queue,
		uint32 currentTick,
		uint32 redundancyDepth);

	// --- AuthoritySyncedBufferOwnerConcept ---
	// Stage 1 (Task 9): per-slot inbound callback. ServerReceiveRemoteMove unpacks
	// the bundle and invokes this once per (capture_tick, input) slot.
	void setOnRemoteMoveReceivedCallback(
		std::function<void(uint32, const simulatableBrawler::PlayerInput&)> fn)
	{
		m_onRemoteMoveReceivedCallback = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback()
	{
		m_onRemoteMoveReceivedCallback = nullptr;
	}

	// [C.2 / T10 part 4] Delivery entry point for an input released from the
	// server's ServerInputDelayQueue. Called on the GAME THREAD by
	// ASimulationManagerUImpl::releaseDelayedInputsForStep, immediately before
	// the authority physics step.
	//
	// This deliberately routes through the SAME callback the RPC handler uses,
	// with the ORIGINAL captureTick — so from RemoteMoveQueue onwards a delayed
	// input is indistinguishable from an undelayed one, and its capture-tick
	// dedup and too-far-future guard keep working unchanged. The delay is
	// expressed purely as WHEN this is called, never as a modified tick number.
	void deliverDelayedRemoteInput(uint32 captureTick,
	                               const simulatableBrawler::PlayerInput& input)
	{
		if (m_onRemoteMoveReceivedCallback)
			m_onRemoteMoveReceivedCallback(captureTick, input);
	}

	// --- C.2 server-authoritative RTT tier (T10, Option A) -------------------
	//
	// Server → owning-client transport for the authoritative connection tier.
	// The SERVER derives the tier from its own per-connection FNetPing RoundTrip
	// (ServerReceiveRemoteMove resolves the RTT primitive and forwards it to
	// ServerReceptionCoordinator::noteRttSample) and publishes it; the client
	// only ever READS it. That is the whole point of Option A:
	// with one producer there is no second estimator to disagree with, so the
	// boundary-RTT tier split — and the recurring one-tick corrections it caused
	// — cannot happen.
	//
	// [og-netcode-v2-input-relay / T10] THE TIER NO LONGER RIDES THIS COMPONENT.
	// The replicated property, its OnRep, and the whole client-side consumption
	// path moved off this per-CHARACTER vehicle onto the per-CONNECTION
	// ASimulationConnectionRelay (RelayDelaySpectrumDesign.md §12): a tier is a
	// property of a WIRE, and one actor per wire makes "one wire = one tier"
	// structural rather than dedup-guarded. What survives here is the SINK — the
	// send boundary the core drives — because this is the object that can resolve
	// the wire (it owns the pawn whose net connection identifies it).

	// The component's `ConnectionTierSink` implementation (T23, re-routed by T10).
	// PURE TRANSPORT: resolve this character's ROOT connection, find-or-spawn that
	// wire's ASimulationConnectionRelay, write the tier there. The core
	// (`ServerReceptionCoordinator::noteRttSample`) owns the "no reading"
	// (rttMs < 0) skip AND the publish-only-on-change dedup, and calls this ONLY
	// when the tier for this owner actually changed — so there is no sentinel check
	// and no changed-vs-current test here. `id` is the sink target; this
	// component is its own target, so it asserts `id == GetUniqueID()`.
	//
	// WHY THE SINK STAYED ON THE COMPONENT (and did not move to the manager with
	// the rest of the channel): the sink must resolve id → owning wire, and this
	// object holds that link unconditionally. The manager's id→component map is
	// populated at REGISTER time, several frames after the first input RPC can
	// arrive — and because the core marks a tier published before calling the
	// sink, a publish the sink cannot route is never retried. Routing through a
	// map that may not be populated yet would turn a benign ordering race into a
	// permanently wrong tier.
	void sendConnectionTierToOwningClient(unsigned int id, uint8_t tier);

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_simulationStateCorrectionSyncedBuffer; }
	// [og-netcode-v2-input-relay T8] `getSyncedCorrectionInputBuffer` was here,
	// returning the retired `m_replicatedInputSyncedBuffer`. The authority's
	// outbound surface is now the correction STATE buffer above (carrying T4's
	// applied-capture-tick ref) plus the relay ring below.

	// --- [og-netcode-v2-input-relay / T1] outbound input relay ----------------
	//
	// The relay ring was added BESIDE the correction-input buffer, deliberately as
	// its own property, so that T3 could dual-write without disturbing the old
	// channel. [T8] The old channel is now gone and this is the only path by which
	// a character's input reaches other clients.
	//
	// ⭐ [T39] THE RING NO LONGER LIVES ON THIS COMPONENT. The UPROPERTY, its
	// OnRep and its DOREPLIFETIME registration moved to ASimulationInputRelay — a
	// per-character Iris DEPENDENT OBJECT — so that the small, irreplaceable relay
	// payload can be scheduled, ranked and skipped independently of the large,
	// self-healing correction state that shares this component. Read the header
	// block on that actor for the mechanism and for why a second component would
	// have bought nothing.
	//
	// WHAT DID NOT MOVE, AND MUST NOT: this concept surface. og-simulation sees
	// exactly the same four members it saw before — the accessor pair and the
	// callback pair — because `SimulationNetSync` must never name a UE type, and
	// because `SimmableUpdateComponentConceptTest.cpp`'s pins passing UNCHANGED is
	// the proof that the engine-facing move did not leak into the core contract.
	// Everything below is a forwarder to the linked host.
	//
	// Read handle for the ring. og-simulation reads it once at registration bind on
	// both roles; on a client the arrival callback below carries it thereafter.
	// Defined in the .cpp because it dereferences the forward-declared host.
	//
	// ⚠ [T34] NO LONGER THE SERVER WRITE HANDLE — see stageRelayedInput.
	FRelayedInputRing&       getRelayedInputRing();
	const FRelayedInputRing& getRelayedInputRing() const;

	// ⭐ [T34] THE SERVER-SIDE RELAY WRITE, under bare C1 flush-on-poll. The T3
	// relay sink calls this at RECEIPT of each newer capture tick; the arrival is
	// STAGED, and the host actor's PreReplication publishes the whole staged burst
	// into the replicated ring once per Iris poll. Before this the sink wrote the
	// replicated ring directly at a session-configurable retention depth (its old
	// identifier is on record in RN-13, ReviewNotes.md; item 63 retired the field
	// outright), which meant a second arrival in one server frame overwrote the
	// first in memory before replication ever compared the property — ~11.6 % of
	// relayed inputs, measured.
	//
	// NO DEPTH PARAMETER, and that is the fence: the stage's capacity is
	// `relayedInputRing::kMaxDepth`, taken as a constant inside the codec. A
	// depth parameter here would cap every round at one entry and reproduce
	// exactly the behaviour this replaces (T43 finding 1).
	//
	// Returns what the write did; `droppedOldest` means the burst exceeded
	// `kMaxDepth` in ONE frame and is already counted on the host.
	relayedInputRing::StageArrivalOutcome stageRelayedInput(
		uint32 captureTick, uint8 dA, const simulatableBrawler::PlayerInput& input);

	// Client-side arrival hook. Invoked with the freshly-replicated ring; T5 binds
	// this to populate its per-remote-character capture-tick store. Null until
	// then — an unbound arrival is a no-op, matching the correction-buffer
	// callbacks above.
	//
	// [T39] The callback is STORED HERE and pushed into the host at link time,
	// rather than being handed straight to the host. The two objects can be linked
	// in either order (the core binds at registration; the host may replicate in
	// before or after), so whichever happens second has to be able to complete the
	// wiring — and only this side can hold the value in the meantime.
	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn);

	void clearOnRelayedInputReceivedCallback();

	// [T39] Link this component to the ASimulationInputRelay that carries its
	// ring. Called from the authority spawn path, from the client-side pull at
	// registration, and from the manager's listener when a host replicates in.
	// IDEMPOTENT — re-linking the same host is a no-op — because all three paths
	// can fire, in any order, for the same pair.
	//
	// On a successful link the host's CURRENT ring is replayed into the bound core
	// callback. That is not latch machinery (see the note at
	// SimulationNetSync::registerPredictionOwner): the ring is a persistent
	// property, so reading it always yields the full current state, and replaying
	// it is how an arrival that landed before the link is recovered.
	void attachInputRelayHost(ASimulationInputRelay* host);

	// [T39] A ring arrival routed here from the host. Runs the owner-skip
	// divergence check and forwards to the core callback.
	void onRelayedInputRingArrived(const FRelayedInputRing& ring);

	// [hit-resolution T12] Game-thread-safe read of the character's machine sim
	// state via the viz-state snapshot the composite refreshes each physics tick
	// (SimulatableBrawler::getVizState). Safe from any game-thread context — input
	// callbacks (AOGBrawlerUECharacter::Move gates on HitFlinch/GuardFlinch),
	// Tick, viz. Returns DAttackState::Idle when the manager or storage entry is
	// not yet available (pre-registration ordering, or already unregistered) — the
	// safe default that keeps callers ungated. Non-const because SimulationManagerUImpl
	// currently only exposes editStorage(); read-only in intent.
	DAttackState getMachineVizState();

protected:
	// To add mapping context
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void tryInitializeWithManager();
	void tryRegisterWithNewFramework();
	void scheduleNextRegistrationAttempt();
	int32 m_initializationAttempts = 0;
	int32 m_registrationAttempts = 0;
	UPROPERTY(ReplicatedUsing = OnRep_CorrectionState)
	FSimulationStateSyncBuffer m_simulationStateCorrectionSyncedBuffer;
	UFUNCTION()
	void OnRep_CorrectionState();

	// wire-format compat fence. Set true the first time
	// OnRep_CorrectionState observes a correction-state buffer whose wire-format
	// version byte does not match FSimulationStateSyncBuffer::kWireFormatVersion
	// (a pre/post-Stage-1 build mismatch). Once set, subsequent OnRep callbacks
	// no-op so a mismatched peer cannot corrupt local state and the toast does not
	// re-fire (the on-screen message uses a stable key so it never spams per tick).
	bool m_wireFormatMismatchDetected = false;
	// Stable AddOnScreenDebugMessage key for the build-mismatch toast so repeated
	// calls replace rather than stack the message.
	static constexpr uint64 kWireFormatMismatchToastKey = 0x4F47574DF0000001ull; // 'OGWM' + tag

	// Stage 1 (Task 9): unreliable + redundancy input channel. Reliable -> Unreliable
	// (no head-of-line blocking on input RPCs � the R-T1 streeting-saturation fix);
	// payload FSimulationInputSyncBuffer -> FInputRedundancyBundle (the client re-sends
	// the last `redundancyDepthTicks` ticks each frame so a dropped datagram self-heals).
	UFUNCTION(Server, Unreliable)
	void ServerReceiveRemoteMove(const FInputRedundancyBundle& bundle);
	FSimulationInputSyncBuffer m_clientToServerInputSyncedBuffer;

	// [og-netcode-v2-input-relay T8] THE REPLICATED CORRECTION-INPUT PROPERTY IS
	// GONE. `UPROPERTY(ReplicatedUsing = OnRep_CorrectionInput)
	// FSimulationInputSyncBuffer m_replicatedInputSyncedBuffer;` and its
	// `OnRep_CorrectionInput()` UFUNCTION stood here, registered in
	// GetLifetimeReplicatedProps beside the state buffer. The server->client echo
	// of "the input I applied for you" is retired; a remote character's input
	// reaches peers on the relay ring below, and the correction state names which
	// capture it applied via T4's ref.
	//
	// NOTE the surviving `m_clientToServerInputSyncedBuffer` above is a DIFFERENT
	// member with the same type and the opposite direction — it is not replicated
	// and it is not affected.

	// [og-netcode-v2-input-relay / T1] The outbound input relay ring stood here as
	// `UPROPERTY(ReplicatedUsing = OnRep_RelayedInputRing) FRelayedInputRing
	// m_relayedInputRing;` with its `OnRep_RelayedInputRing()` UFUNCTION, and was
	// registered with a plain DOREPLIFETIME (no COND_) in
	// GetLifetimeReplicatedProps.
	//
	// ⭐ [T39] ALL THREE MOVED TO ASimulationInputRelay, IN ONE EDIT — property,
	// OnRep and registration together, per the T8 rule recorded in
	// GetLifetimeReplicatedProps: retiring a replicated property from an object
	// means retiring its registration in the same change, so neither half can be
	// forgotten and left pointing at nothing.
	//
	// TWO REASONS, both from T37/T38:
	//   * SCHEDULING. Sharing this component made the ring and the correction
	//     state one atomic Iris batch, so packet overflow killed them together —
	//     and a skipped snapshot is coalesced away, not deferred. The ring is the
	//     payload that cannot survive that: a dropped relayed input has no
	//     recovery path anywhere, while a missed correction costs latency only.
	//   * OWNERSHIP. The ring is now COND_SkipOwner on an actor OWNED by the
	//     character, which is what stops the ~85 B/connection/round echo to the
	//     one client that provably never reads it. There is no owner to skip
	//     against on a component of a pawn whose owner chain the condition would
	//     have applied to the state as well.
	//
	// [T39] THE LINK TO THAT HOST. Weak because the host is destroyed with the
	// character and this component's own teardown order against it is not
	// guaranteed. Set on the authority at spawn (registration completion, which is
	// strictly before the first relay write), and on a client by whichever of the
	// two link paths gets there first.
	TWeakObjectPtr<ASimulationInputRelay> m_inputRelayHost;

	// [T39] THE DETACHED FALLBACK. `getRelayedInputRing()` must return a reference
	// even when no host is linked — the core reads it once at registration bind,
	// on both roles, and the concept types it as a reference. This is that object:
	// a never-replicated, always-empty ring.
	//
	// It is deliberately NOT a silent stand-in for the real thing. A server-side
	// write that landed here would be invisible to every client, so the spawn is
	// sequenced BEFORE the relay tap's id->component route is registered
	// (tryRegisterWithNewFramework) precisely so that cannot happen. On a client
	// this is simply "no ring has arrived yet", which reads as version 0 and makes
	// the bind-time ingest a no-op — exactly what an unwritten ring did before.
	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayRing;

	// [T34] The same fallback, for the flush STAGE. `stageRelayedInput` must have
	// somewhere to write when no host is linked, and it must NOT be the detached
	// ring above: that one is what `getRelayedInputRing()` hands the core, so
	// staging into it would let an unpublished burst read back as if it had
	// replicated. Writes that land here are dropped at the next flush that never
	// comes, which is the same visibility a detached ring already had.
	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayStagingRing;

	// [T39] Provider presence, latched at registration. This is the CLIENT half of
	// the owner-skip precondition assert: "owning connection" (what COND_SkipOwner
	// narrows on, server-side) and "provider present" (what decides whether a
	// relay store exists, role-derived here) must name the same set, and nothing
	// asserted that they did. See onRelayedInputRingArrived.
	bool m_hasLocalInputProvider = false;
	bool m_loggedOwnerSkipDivergence = false;

	// [T10 / og-netcode-v2-input-relay] The replicated tier property, its OnRep,
	// the pre-OnRep latch, the observed-OnRep flag and the ReplicatedTierConsumer
	// that used to live here are GONE — the channel is ASimulationConnectionRelay
	// and the consumer is owned by ASimulationManagerUImpl. Only the send-side
	// sink (above) remains on this component.

	UOGBrawlerInputCollectionComponent* m_ownerInputCollection = nullptr;

	//Physics
	FBodyInstance* m_attackAxisBody;
	FBodyInstanceAsyncPhysicsTickHandle m_attackAxisPhysicshandle;

	//Visualization

	dAttackRadialVisualization::State m_visualizationState;
	brawlerProjectileVisualization::State m_projectileVisualizationState;

	std::vector<QueryVolumeId> m_targetVisualizationVolumeIds;
	std::optional<dAttackTargetVisualizationTwo::State> m_attackTargetVisualizationState;
	
	std::optional<dAttackAimVisualization::State> m_attackAimVisualizationState;

	std::optional<dAttackBlockPredictionVisualization::State> m_attackBlockPredictionVisualizationState;

	std::optional<simulatableBrawler::StaticData> m_staticData;

	// Callbacks registered by SimulationNetSync at registerSimulatable time.
	// Null until Task 7 wires the new path; OnRep_ handlers fall through to old logic when null.
	std::function<void(const FSimulationStateSyncBuffer&)> m_onCorrectionStateReceivedCallback;
	// [T8] m_onCorrectionInputReceivedCallback removed with its channel.
	// Per-slot (capture_tick, input) � invoked once per FInputRedundancyBundle slot.
	std::function<void(uint32, const simulatableBrawler::PlayerInput&)> m_onRemoteMoveReceivedCallback;
	// [T1] Relay-ring arrival hook; bound by T5's client store. [T39] Still stored
	// here — this is the CORE's callback, and the component is still the concept
	// surface. What changed is where the arrival comes FROM: the host actor's
	// OnRep, routed through onRelayedInputRingArrived, rather than this
	// component's own (now retired) OnRep.
	std::function<void(const FRelayedInputRing&)> m_onRelayedInputReceivedCallback;

};

// SimulatableOwnerTraits<SimulatableBrawler> is specialized in
// OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h � include that header at any
// site that instantiates SimulationNetSync<SimulatableBrawler>.