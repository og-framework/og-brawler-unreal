#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulationUnreal.h"
#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "OGSimulationUnreal/SyncedSimulationStateBuffer.h"

#include "SimulationTimingRelay.generated.h"

UCLASS()
class OGSIMULATIONUNREAL_API ASimulationTimingRelay : public AInfo
{
    GENERATED_BODY()

public:
    ASimulationTimingRelay();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    FSmallSimulationStateSyncBuffer& editBuffer() { return m_buffer; }
    const FSmallSimulationStateSyncBuffer& readBuffer() const { return m_buffer; }

private:
    UFUNCTION()
    void OnRep_Buffer();

    UPROPERTY(ReplicatedUsing = OnRep_Buffer)
    FSmallSimulationStateSyncBuffer m_buffer;
};
