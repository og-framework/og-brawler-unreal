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
	// Server-side write handle. The T3 relay sink writes
	// (captureTick, dA, input) here at RECEIPT of each newer capture tick, with
	// `depth` read from TimeConfig::relayRedundancyDepthTicks.
	FRelayedInputRing& getRelayedInputRing() { return m_relayedInputRing; }
	const FRelayedInputRing& getRelayedInputRing() const { return m_relayedInputRing; }

	// Client-side arrival hook. Invoked from OnRep_RelayedInputRing with the
	// freshly-replicated ring; T5 binds this to populate its per-remote-character
	// capture-tick store. Null until then — an unbound OnRep is a no-op, matching
	// the correction-buffer callbacks above.
	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn)
	{
		m_onRelayedInputReceivedCallback = std::move(fn);
	}

	void clearOnRelayedInputReceivedCallback()
	{
		m_onRelayedInputReceivedCallback = nullptr;
	}

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

	// [og-netcode-v2-input-relay / T1] The outbound input relay ring — this
	// character's recent (captureTick, dA, input) entries, replicated to ALL
	// clients (registered with a plain DOREPLIFETIME, no COND_: a non-owning peer
	// is exactly who needs this). Written by T3's relay sink at receipt; read by
	// T5's client store. [T8] It was deliberately kept independent of the
	// correction-input buffer through the T3..T8 dual-write window, which is what
	// made retiring that buffer a pure deletion here.
	UPROPERTY(ReplicatedUsing = OnRep_RelayedInputRing)
	FRelayedInputRing m_relayedInputRing;
	UFUNCTION()
	void OnRep_RelayedInputRing();

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
	// [T1] Relay-ring arrival hook; bound by T5's client store.
	std::function<void(const FRelayedInputRing&)> m_onRelayedInputReceivedCallback;

};

// SimulatableOwnerTraits<SimulatableBrawler> is specialized in
// OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h � include that header at any
// site that instantiates SimulationNetSync<SimulatableBrawler>.