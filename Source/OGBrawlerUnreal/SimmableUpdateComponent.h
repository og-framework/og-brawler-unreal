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