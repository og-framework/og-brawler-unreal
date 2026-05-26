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
#include "OGBrawler/DAttackTargetVisualization.h"
#include "OGBrawler/DAttackTargetVisualizationTwo.h"
#include "OGBrawler/DAttackAimVisualization.h"

#include <functional>
#include <optional>
#include <vector>
#include "InputActionValue.h"

#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"

#include "SimmableUpdateComponent.generated.h"

class DAttackCircle;
class UInputAction;
class UEnhancedInputComponent;
class USimmableUpdateComponent;
class ChaosTickMapper;

namespace DAttackMovementCVars
{
	//1 aim relative movement
	//2 movement relative strike
	//3 movementRelativeStrikeJoyAimRelativeMoveMouse
	extern TAutoConsoleVariable<int32> movementAndAimMode;
}

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

	//Input
	void setAttackAxisBody(FBodyInstance* attackAxisBody) { m_attackAxisBody = attackAxisBody; }
	void setAttackAxisBody(FBodyInstanceAsyncPhysicsTickHandle attackAxisPhysicsHandle) { m_attackAxisPhysicshandle = attackAxisPhysicsHandle; }
	void setInputComponent(UEnhancedInputComponent* inputComponent, const InputMappingUETranslator& translator);
	void setCameraForward(glm::vec3 cameraForward) { m_camForward = cameraForward; }
	void setMouseAimDirection(glm::vec3 mouseAimDirection) { m_mouseAimDirection = mouseAimDirection; }

	glm::vec3 buildAimDirection();
	static glm::vec3 getInputDirectionInCameraSpace(const glm::vec3& camForward, const glm::vec3& inputDirection);
	static glm::vec3 getInputDirectionInAimSpace(const glm::vec3& aimDirection, const glm::vec3& inputDirection);

	const simulatableBrawler::StaticData& getStaticData() const { return *m_staticData; }

	// --- PredictionSyncedBufferOwnerConcept ---
	using SyncedCorrectionBufferType  = FSimulationStateSyncBuffer;
	using SyncedRemoteInputBufferType = FSimulationStateSyncBuffer;

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
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_onCorrectionInputReceivedCallback = std::move(fn);
	}

	void clearOnCorrectionInputReceivedCallback()
	{
		m_onCorrectionInputReceivedCallback = nullptr;
	}

	FSimulationStateSyncBuffer* getClientToServerInputSyncedBuffer()
	{
		return &m_clientToServerInputSyncedBuffer;
	}

	void sendLocalInputToAuthority(const FSimulationStateSyncBuffer& buffer)
	{
		ServerReceiveRemoteMove(buffer);
	}

	// --- AuthoritySyncedBufferOwnerConcept ---
	void setOnRemoteMoveReceivedCallback(
		std::function<void(const FSimulationStateSyncBuffer&)> fn)
	{
		m_onRemoteMoveReceivedCallback = std::move(fn);
	}

	void clearOnRemoteMoveReceivedCallback()
	{
		m_onRemoteMoveReceivedCallback = nullptr;
	}

	FSimulationStateSyncBuffer& getSyncedCorrectionStateBuffer() { return m_simulationStateCorrectionSyncedBuffer; }
	FSimulationStateSyncBuffer& getSyncedCorrectionInputBuffer() { return m_replicatedInputSyncedBuffer; }

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

	UFUNCTION(Server, Reliable)
	void ServerReceiveRemoteMove(const FSimulationStateSyncBuffer& inputBuffer);
	FSimulationStateSyncBuffer m_clientToServerInputSyncedBuffer;

	UPROPERTY(ReplicatedUsing = OnRep_CorrectionInput)
	FSimulationStateSyncBuffer m_replicatedInputSyncedBuffer;
	UFUNCTION()
	void OnRep_CorrectionInput();

	//Input
	UEnhancedInputComponent* m_inputComponent;
	glm::vec3 m_aimDirection;
	bool m_leftAttackInput;
	bool m_rightAttachInput;
	glm::vec2 m_moveDirection;


	void setAimInput(const FInputActionValue& Value) { FVector2D AimAxisVector = Value.Get<FVector2D>(); m_aimDirection.x = AimAxisVector.X; m_aimDirection.y = AimAxisVector.Y; m_aimDirection.z = 0.f; }
	void setLeftAttackInput(const FInputActionValue& Value) { m_leftAttackInput = Value.Get<bool>(); }
	void setRightAttackInput(const FInputActionValue& Value) { m_rightAttachInput = Value.Get<bool>(); }
	void setMoveInput(const FInputActionValue& Value) { FVector2D valueVector = Value.Get<FVector2D>(); m_moveDirection.x = valueVector.X; m_moveDirection.y = valueVector.Y*(-1.f); }

	glm::vec3 m_camForward;
	glm::vec3 m_mouseAimDirection;

	//Physics
	FBodyInstance* m_attackAxisBody;
	FBodyInstanceAsyncPhysicsTickHandle m_attackAxisPhysicshandle;

	//Visualization

	dAttackRadialVisualization::State m_visualizationState;

	std::vector<QueryVolumeId> m_targetVisualizationVolumeIds;
	std::optional<dAttackTargetVisualizationTwo::State> m_attackTargetVisualizationState;
	
	std::optional<dAttackAimVisualization::State> m_attackAimVisualizationState;

	std::optional<simulatableBrawler::StaticData> m_staticData;

	// Callbacks registered by SimulationNetSync at registerSimulatable time.
	// Null until Task 7 wires the new path; OnRep_ handlers fall through to old logic when null.
	std::function<void(const FSimulationStateSyncBuffer&)> m_onCorrectionStateReceivedCallback;
	std::function<void(const FSimulationStateSyncBuffer&)> m_onCorrectionInputReceivedCallback;
	std::function<void(const FSimulationStateSyncBuffer&)> m_onRemoteMoveReceivedCallback;

};

// SimulatableOwnerTraits<SimulatableBrawler> is specialized in
// OGBrawlerUnreal/SimulatableBrawlerOwnerTraits.h — include that header at any
// site that instantiates SimulationNetSync<SimulatableBrawler>.