// SPDX-License-Identifier: MPL-2.0

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Engine/World.h"
#include "PhysicsPublic.h"
#include "Runtime/PhysicsCore/Public/PhysicsInterfaceDeclaresCore.h"
#include "Runtime/Engine/Public/Physics/Experimental/PhysScene_Chaos.h"
#include "Chaos/ChaosEngineInterface.h"
#include "Runtime/CoreUObject/Public/UObject/Object.h"

#include "OGSimulation/SimulationTimeContext.h"

#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"

//#include "SimulationTimeContextUImpl.generated.h"
//
//UCLASS()
//class ASimulationTimeContextUImpl : public AActor
//{
//    GENERATED_BODY()
//
//public:
//    ASimulationTimeContextUImpl();
//    ~ASimulationTimeContextUImpl();
//
//    virtual void BeginPlay() override;
//
//    static ASimulationTimeContextUImpl* instance(ENetMode netMode) { return netMode == NM_DedicatedServer ? s_instances[0] : s_instances[1]; }
//
//    void OnPhysicsStep(FPhysScene* Scene, float DeltaTime);
//
//    const AuthoritySimulationTimeManager& getTimingContext() const { return *m_timingInfo; }
//
//    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
//
//    UFUNCTION()
//    void OnRep_TimingInfo();
//
//private:
//    std::optional<AuthoritySimulationTimeManager> m_timingInfo;
//
//    UPROPERTY(ReplicatedUsing = OnRep_TimingInfo)
//    FSmallSimulationStateSyncBuffer m_syncedBuffer;
//
//    static ASimulationTimeContextUImpl* s_instances[2];
//};