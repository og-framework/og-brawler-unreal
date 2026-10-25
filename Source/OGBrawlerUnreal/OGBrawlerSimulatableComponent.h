// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerSimulatableComponent-rationale.md · docs/OGBrawlerSimulatableComponent-guards.md

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

#include "OGBrawlerSimulatableComponent.generated.h"

class DAttackCircle;
class UInputAction;
class UEnhancedInputComponent;
class UOGBrawlerSimulatableComponent;
class UOGBrawlerInputCollectionComponent;
class ASimulationInputRelay;
class AOGBrawlerSimulationManager;

extern template class UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>;

UCLASS(ClassGroup = DPhysics, BlueprintType, Blueprintable, EditInlineNew, meta = (BlueprintSpawnableComponent))
class UOGBrawlerSimulatableComponent : public UActorComponent, public UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>
{
	GENERATED_BODY()
public:

	UOGBrawlerSimulatableComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());


	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override final;

	const simulatableBrawler::StaticData& getStaticData() const { return *m_staticData; }

	const UOGBrawlerInputCollectionComponent* getOwnerInputCollection() const { return m_ownerInputCollection; }


protected:
	virtual void BeginPlay();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend class UESimulatableOwnerBinding<UOGBrawlerSimulatableComponent, BrawlerSessionTraitsUImpl>;

	ueSimulatableOwnerRegistration::Initialization initializeWithManager(AOGBrawlerSimulationManager& manager, AActor& owner);
	ueSimulatableOwnerRegistration::ResolvedKey resolveRegistrationKey(AOGBrawlerSimulationManager& manager, bool isAuthority);
	UELatencyBudgetDiagnostics<BrawlerSessionTraitsUImpl>::InputProvider makeInputProvider(unsigned int key);
	SimulatableBrawler makeSimulatable();
	SimCharacterId simCharacterId() const;

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
