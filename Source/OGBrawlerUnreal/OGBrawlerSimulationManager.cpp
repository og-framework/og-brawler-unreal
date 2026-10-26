// SPDX-License-Identifier: BUSL-1.1
// docs/OGBrawlerSimulationManager-rationale.md · docs/OGBrawlerSimulationManager-guards.md

#include "OGBrawlerSimulationManager.h"
#include "OGSimulation/CompilerControl.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "OGBrawlerUnreal/OGBrawlerSimulatableComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGBrawlerUnreal/BrawlerSessionGameUImpl.h"
#include "GameFramework/PlayerController.h"
#include "OGSimulationUnreal/SimulationInputRelay.h"

#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackGuardSimulation.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "Components/CapsuleComponent.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"

#include "OGSimulation/OGAssert.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGBrawler);

#define HasAuthority HasAuthority_is_constant_true_on_AOGBrawlerSimulationManager_use_worldIsAuthority_or_runsPrediction

OGSIM_OPTIMIZE_OFF

AOGBrawlerSimulationManager::AOGBrawlerSimulationManager()
{
    bReplicates = false;
}

void AOGBrawlerSimulationManager::BeginPlay()
{
	Super::BeginPlay();

	m_session.beginPlay();
}

void AOGBrawlerSimulationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	m_session.endPlay();

	Super::EndPlay(EndPlayReason);

}

template void UESimulationSession<BrawlerSessionTraitsUImpl>::onInputRelayHostReady(ASimulationInputRelay& host);

void AOGBrawlerSimulationManager::unregisterFromNewFramework(
    SimCharacterId simId, UOGBrawlerSimulatableComponent& owner, bool isAuthority)
{
    const unsigned int id = toStorageKey(simId);

    m_session.unregisterFromNewFramework(id, owner, isAuthority);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
