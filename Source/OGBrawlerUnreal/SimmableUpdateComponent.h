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

#include "OGBrawlerUnreal/PhysicsBackendUImpl.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"
#include "OGSimulationUnreal/SimulatableNetSyncComponent.h"
#include "OGSimulationUnreal/UESimulatableOwnerBinding.h"
#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"
#include "OGSimulationUnreal/RelayedInputRing.h"
#include "OGSimulationUnreal/InputMappingUETranslator.h"
#include "OGBrawler/InputMapping/GameInputMapping.h"
#include "OGSimulation/SimulationQueues.h"

#include "SimmableUpdateComponent.generated.h"

class DAttackCircle;
class UInputAction;
class UEnhancedInputComponent;
class USimmableUpdateComponent;
class UOGBrawlerInputCollectionComponent;
class ASimulationInputRelay;

extern template class UESimulatableOwnerBinding<USimmableUpdateComponent, BrawlerSessionTraitsUImpl>;

UCLASS(ClassGroup = DPhysics, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class USimmableUpdateComponent : public UActorComponent, public UESimulatableOwnerBinding<USimmableUpdateComponent, BrawlerSessionTraitsUImpl>
{
	GENERATED_BODY()
public:

	USimmableUpdateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override final;

	const simulatableBrawler::StaticData& getStaticData() const { return *m_staticData; }

	const UOGBrawlerInputCollectionComponent* getOwnerInputCollection() const { return m_ownerInputCollection; }


protected:
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UESimulatableOwnerBinding<USimmableUpdateComponent, BrawlerSessionTraitsUImpl>;

	void tryInitializeWithManager();
	void tryRegisterWithNewFramework();
	void scheduleNextInitializationAttempt();
	void scheduleNextRegistrationAttempt();
	SimCharacterId simCharacterId() const;
	int32 m_initializationAttempts = 0;
	int32 m_registrationAttempts = 0;

	unsigned int storageKeyOfOwner() const;

	UOGBrawlerInputCollectionComponent* m_ownerInputCollection = nullptr;


	dAttackRadialVisualization::State m_visualizationState;
	brawlerProjectileVisualization::State m_projectileVisualizationState;

	std::vector<QueryVolumeId> m_targetVisualizationVolumeIds;
	std::optional<dAttackTargetVisualizationTwo::State> m_attackTargetVisualizationState;
	
	std::optional<dAttackAimVisualization::State> m_attackAimVisualizationState;

	std::optional<dAttackBlockPredictionVisualization::State> m_attackBlockPredictionVisualizationState;

	std::optional<simulatableBrawler::StaticData> m_staticData;

};
