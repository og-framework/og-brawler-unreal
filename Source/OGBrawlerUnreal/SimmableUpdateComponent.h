// SPDX-License-Identifier: BUSL-1.1
// docs/SimmableUpdateComponent-rationale.md · docs/SimmableUpdateComponent-guards.md

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
#include "OGBrawler/SimCharacterId.h"

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
class ASimulationInputRelay;

UCLASS(ClassGroup = DPhysics, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class USimmableUpdateComponent : public UActorComponent
{
	GENERATED_BODY()
public:

	USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override final;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void setAttackAxisBody(FBodyInstance* attackAxisBody) { m_attackAxisBody = attackAxisBody; }
	void setAttackAxisBody(FBodyInstanceAsyncPhysicsTickHandle attackAxisPhysicsHandle) { m_attackAxisPhysicshandle = attackAxisPhysicsHandle; }

	const simulatableBrawler::StaticData& getStaticData() const { return *m_staticData; }

	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using SyncedRemoteInputBufferType = FSimulationInputSyncBuffer;
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


	FSimulationInputSyncBuffer* getClientToServerInputSyncedBuffer()
	{
		return &m_clientToServerInputSyncedBuffer;
	}

	void sendLocalInputToAuthority(
		const PendingInputQueue<simulatableBrawler::PlayerInput>& queue,
		uint32 currentTick,
		uint32 redundancyDepth);

	void setOnRemoteMoveReceivedCallback(
		std::function<void(uint32, const simulatableBrawler::PlayerInput&)> fn)
	{
		m_onRemoteMoveReceivedCallback = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback()
	{
		m_onRemoteMoveReceivedCallback = nullptr;
	}

	void deliverDelayedRemoteInput(uint32 captureTick,
	                               const simulatableBrawler::PlayerInput& input)
	{
		if (m_onRemoteMoveReceivedCallback)
			m_onRemoteMoveReceivedCallback(captureTick, input);
	}

	void sendConnectionTierToOwningClient(unsigned int id, uint8_t tier);

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_simulationStateCorrectionSyncedBuffer; }

	FRelayedInputRing&       getRelayedInputRing();
	const FRelayedInputRing& getRelayedInputRing() const;

	relayedInputRing::StageArrivalOutcome stageRelayedInput(
		uint32 captureTick, uint8 dA, const simulatableBrawler::PlayerInput& input);

	void setOnRelayedInputReceivedCallback(
		std::function<void(const FRelayedInputRing&)> fn);

	void clearOnRelayedInputReceivedCallback();

	void attachInputRelayHost(ASimulationInputRelay* host);

	void onRelayedInputRingArrived(const FRelayedInputRing& ring);


protected:
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void tryInitializeWithManager();
	void tryRegisterWithNewFramework();
	void scheduleNextRegistrationAttempt();
	SimCharacterId simCharacterId() const;
	int32 m_initializationAttempts = 0;
	int32 m_registrationAttempts = 0;
	UPROPERTY(ReplicatedUsing = OnRep_CorrectionState)
	FSimulationStateSyncBuffer m_simulationStateCorrectionSyncedBuffer;
	UFUNCTION()
	void OnRep_CorrectionState();

	bool m_wireFormatMismatchDetected = false;
	static constexpr uint64 kWireFormatMismatchToastKey = 0x4F47574DF0000001ull;

	UFUNCTION(Server, Unreliable)
	void ServerReceiveRemoteMove(const FInputRedundancyBundle& bundle);
	FSimulationInputSyncBuffer m_clientToServerInputSyncedBuffer;

	// ⛔G-36  docs/SimmableUpdateComponent-guards.md

	// ⛔G-37  docs/SimmableUpdateComponent-guards.md

	TWeakObjectPtr<ASimulationInputRelay> m_inputRelayHost;

	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayRing;

	UPROPERTY(Transient)
	FRelayedInputRing m_detachedRelayStagingRing;

	bool m_hasLocalInputProvider = false;
	bool m_loggedOwnerSkipDivergence = false;


	UOGBrawlerInputCollectionComponent* m_ownerInputCollection = nullptr;

	FBodyInstance* m_attackAxisBody;
	FBodyInstanceAsyncPhysicsTickHandle m_attackAxisPhysicshandle;


	dAttackRadialVisualization::State m_visualizationState;
	brawlerProjectileVisualization::State m_projectileVisualizationState;

	std::vector<QueryVolumeId> m_targetVisualizationVolumeIds;
	std::optional<dAttackTargetVisualizationTwo::State> m_attackTargetVisualizationState;
	
	std::optional<dAttackAimVisualization::State> m_attackAimVisualizationState;

	std::optional<dAttackBlockPredictionVisualization::State> m_attackBlockPredictionVisualizationState;

	std::optional<simulatableBrawler::StaticData> m_staticData;

	std::function<void(const FSimulationStateSyncBuffer&)> m_onCorrectionStateReceivedCallback;
	std::function<void(uint32, const simulatableBrawler::PlayerInput&)> m_onRemoteMoveReceivedCallback;
	std::function<void(const FRelayedInputRing&)> m_onRelayedInputReceivedCallback;

};
