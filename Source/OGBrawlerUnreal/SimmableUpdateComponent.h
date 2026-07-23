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
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGSimulation/SimulationQueues.h"
#include "OGSimulation/Network/ReplicatedTierConsumer.h"

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
	// Correction-STATE role stays FSimulationStateSyncBuffer; the input role
	// (remote input + correction input) is now FSimulationInputSyncBuffer per the
	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using SyncedRemoteInputBufferType = FSimulationInputSyncBuffer;

	void setOnCorrectionStateReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_onCorrectionStateReceivedCallback = std::move(fn);
	}

	void clearOnCorrectionStateReceivedCallback()
	{
		m_onCorrectionStateReceivedCallback = nullptr;
	}

	void setOnCorrectionInputReceivedCallback(
		std::function<void(const FSimulationInputSyncBuffer&)> fn)
	{
		m_onCorrectionInputReceivedCallback = std::move(fn);
	}

	void clearOnCorrectionInputReceivedCallback()
	{
		m_onCorrectionInputReceivedCallback = nullptr;
	}

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
	// ServerReceptionCoordinator::noteRttSample) and publishes it here; the client
	// only ever READS it. That is the whole point of Option A:
	// with one producer there is no second estimator to disagree with, so the
	// boundary-RTT tier split — and the recurring one-tick corrections it caused
	// — cannot happen.
	//
	// WHY THIS COMPONENT AND NOT THE PAWN OR THE TIMING RELAY:
	//  - ASimulationTimingRelay is a single bAlwaysRelevant actor with ONE shared
	//    buffer. It is structurally incapable of carrying per-connection data.
	//  - The pawn (AOGBrawlerUECharacter) carries no netcode surface at all.
	//    This component already owns every other per-connection replicated
	//    channel (correction state, correction input) and already receives the
	//    client's input RPC, so the tier rides the channel its producer and its
	//    consumer both already touch.
	// The correction sync-buffer WIRE FORMAT is deliberately untouched — that is
	// Stage 4 territory. This is a separate, additive property.

	// The component's `ConnectionTierSink` implementation (T23). PURE TRANSPORT:
	// it just writes the owner-only replicated `m_replicatedConnectionTier`. The
	// core (`ServerReceptionCoordinator::noteRttSample`) now owns the "no reading"
	// (rttMs < 0) skip AND the publish-only-on-change dedup, and calls this ONLY
	// when the tier for this owner actually changed — so there is no sentinel check
	// and no changed-vs-current test left here. `id` is the sink target; this
	// component is its own target, so it asserts `id == GetUniqueID()` (a second
	// engine whose sink is a central manager would route on `id` instead).
	void sendConnectionTierToOwningClient(unsigned int id, uint8_t tier);

	// Client-side read (also valid on the server, where it mirrors what was
	// published). T9 consumes this at the integrator offset-read.
	uint8 getReplicatedConnectionTier() const { return m_replicatedConnectionTier; }

	// The tier value observed BEFORE the most recent OnRep. T11 keys its
	// upward-transition rollback off (previous → current); it is captured here
	// rather than recomputed because OnRep is the only place the pre-change
	// value still exists.
	uint8 getPreviousReplicatedConnectionTier() const { return m_previousReplicatedConnectionTier; }

	// --- C.2 client-side consumption (T9, Option A) --------------------------
	//
	// The client's ENTIRE share of the tier system. It holds the replicated tier
	// and derives behaviour from it through the shared lookups in
	// ConnectionTierTable.h — the same functions the server's table delegates to,
	// so both ends turn the identical tier value into the identical delay. The
	// client owns NO ConnectionTierTable and never calls onRttSample.
	//
	// std::nullopt until tryInitializeWithManager has resolved a manager: the
	// consumer binds `const TimeConfig&` from the manager and cannot be built
	// before one exists. Every accessor below therefore degrades to the no-tier
	// baseline while unbound, which is the same answer it would give pre-OnRep.

	// Effective Layer-1 input delay in ticks for this connection, per the
	// C2-locked formula: `rttTierInputDelays[tier]` once a tier has arrived
	// (REPLACES the baseline, never added to it), `forcedInputLatencyTicks`
	// before then. Returns 0 when neither a manager nor a config is available,
	// which is the correct un-delayed legacy behaviour for that state.
	int32 getEffectiveInputDelayTicks() const;

	// True once at least one authoritative tier has actually landed. Keyed on
	// ARRIVAL, not on the tier value — the replicated property defaults to 0,
	// which is also a legal tier, so a value test cannot tell the two apart.
	bool hasReceivedAuthoritativeTier() const;

	// [T9 part 3] Push getEffectiveInputDelayTicks() across to the simulation's
	// collect path (game thread -> a single std::atomic<int32> read once per tick
	// on the physics thread). Called from OnRep_ConnectionTier and once at
	// registration so the pre-arrival baseline is applied even if no tier ever
	// lands. No-ops when no manager has spawned yet — BeginPlay publishes the
	// baseline on its own, so nothing is lost by the early return.
	void publishEffectiveInputDelayToSimulation();

	// [T11] Turn one replicated-tier transition into a client prediction
	// rollback. Under Option A the client runs no RTT sampling of its own, so
	// the OnRep `oldTier -> newTier` delta IS the transition signal. Computes
	// the effective-delay delta through the shared `tierDelayDeltaTicks` lookup
	// (never off the raw config array — `lanZeroDelayOverride` makes those
	// disagree for every transition touching tier 0) and forwards a POSITIVE
	// delta to the client clock. Non-positive deltas are dropped by the clock.
	void applyTierTransitionRollback(uint8 oldTier, uint8 newTier);

	// Read-only handle for T11's transition logic and for tests.
	const ReplicatedTierConsumer* getReplicatedTierConsumer() const
	{
		return m_replicatedTierConsumer.has_value() ? &(*m_replicatedTierConsumer) : nullptr;
	}

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_simulationStateCorrectionSyncedBuffer; }
	FSimulationInputSyncBuffer& getSyncedCorrectionInputBuffer() { return m_replicatedInputSyncedBuffer; }

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

	UPROPERTY(ReplicatedUsing = OnRep_CorrectionInput)
	FSimulationInputSyncBuffer m_replicatedInputSyncedBuffer;
	UFUNCTION()
	void OnRep_CorrectionInput();

	// [T10] Authoritative connection tier, replicated COND_OwnerOnly (see
	// GetLifetimeReplicatedProps) — a tier is a property of ONE client's wire and
	// is meaningless to every other client, so sending it to anyone else would be
	// pure bandwidth waste. uint8 because the tier index is bounded by
	// ConnectionTierTable::kTierCount (4 today), so a byte is the natural width.
	// Tier 0 is the correct pre-arrival default: it is the lowest-latency tier,
	// and T9's consumer additionally falls back to forcedInputLatencyTicks until
	// the first OnRep actually lands, so a client that has not heard from the
	// server yet does not silently adopt tier-0 timing.
	UPROPERTY(ReplicatedUsing = OnRep_ConnectionTier)
	uint8 m_replicatedConnectionTier = 0;
	// Takes the OldValue overload UE offers for non-array replicated properties:
	// by the time OnRep runs the property already holds the NEW value, so the
	// engine-supplied previous value is the only place the transition delta can
	// be read without shadowing the property by hand.
	UFUNCTION()
	void OnRep_ConnectionTier(uint8 OldTier);

	// Pre-OnRep value, latched from OldTier so T11 can compute the transition
	// delta outside the OnRep call frame.
	uint8 m_previousReplicatedConnectionTier = 0;

	// [T9 part 3] True once OnRep_ConnectionTier has fired at least once, even if
	// that happened before the tier consumer was bound. Distinguishes "the server
	// sent tier 0" from "the property is still at its default 0", which the value
	// alone cannot.
	bool m_connectionTierOnRepObserved = false;

	// [T9] Client-side consumer of the replicated tier. Emplaced in
	// tryInitializeWithManager, bound to the manager's TimeConfig by reference.
	// Fed ONLY from OnRep_ConnectionTier (game thread) — it never samples.
	std::optional<ReplicatedTierConsumer> m_replicatedTierConsumer;

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
	std::function<void(const FSimulationInputSyncBuffer&)> m_onCorrectionInputReceivedCallback;
	// Per-slot (capture_tick, input) � invoked once per FInputRedundancyBundle slot.
	std::function<void(uint32, const simulatableBrawler::PlayerInput&)> m_onRemoteMoveReceivedCallback;

};

// SimulatableOwnerTraits<SimulatableBrawler> is specialized in
// OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h � include that header at any
// site that instantiates SimulationNetSync<SimulatableBrawler>.