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
#include "OGBrawler/OGBrawlerLog.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "Components/CapsuleComponent.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

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

BrawlerSessionTraitsUImpl::StaticData BrawlerSessionGameUImpl::makeStaticData()
{
    const FMovementStaticDataCVars movementStaticDataCVars = AOGBrawlerSimulationManager::readMovementStaticDataCVars();
    return BrawlerSessionTraitsUImpl::StaticData{
        movementStaticDataCVars.model,
        movementStaticDataCVars.maxWalkSpeed,
        movementStaticDataCVars.stepPeriodTicks,
        movementStaticDataCVars.stepSpeed,
        movementStaticDataCVars.gravity };
}

void AOGBrawlerSimulationManager::BeginPlay()
{
	Super::BeginPlay();

	m_session.beginPlay();
}

void BrawlerSessionGameUImpl::beforeWorldBuild(UWorld& world, ENetMode netMode, bool compositionExists,
	BrawlerSessionTraitsUImpl::StaticData& staticData)
{
	const float engineGravityZ = world.GetGravityZ();
	const float simGravityZ    = staticData.m_movementStaticData.gravity;
	checkf(FMath::Abs(simGravityZ - engineGravityZ) < 1e-3f,
		TEXT("OGBrawlerSimulationManager: the movement simulation's gravity (%f cm/s^2) disagrees with ")
		TEXT("the engine's (%f cm/s^2). The sim's value is read from UPhysicsSettings::DefaultGravityZ ")
		TEXT("at construction; this world overrides gravity in its WorldSettings. Either clear that ")
		TEXT("override or teach the movement StaticData about per-level gravity."),
		simGravityZ, engineGravityZ);

	m_roster.seedSpawnPointsFromLevel(world, netMode, compositionExists, staticData);
}

void BrawlerSessionGameUImpl::emplaceSystems(std::optional<BrawlerCompositionUImpl::SystemsExec>& systemsExec,
	EngineTraits::BodyAdapter& bodyAdapter, EngineTraits::QueryAdapter& queryAdapter)
{
	systemsExec.emplace(std::piecewise_construct,
		BrawlerHitDetectionSystem(bodyAdapter, queryAdapter),
		brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
}

void BrawlerSessionGameUImpl::installGameLoggers(bool isAuthority)
{
	auto ogblogSink = [](const char* msg) {
		FString fmsg(msg);
		if (fmsg.StartsWith(TEXT("[Verbose]")))
		{
			UE_LOG(LogOGBrawler, Verbose, TEXT("%s"), *fmsg);
		}
		else if (fmsg.StartsWith(TEXT("[Warning]")))
		{
			UE_LOG(LogOGBrawler, Warning, TEXT("%s"), *fmsg);
		}
		else
		{
			UE_LOG(LogOGBrawler, Log, TEXT("%s"), *fmsg);
		}
	};
	ogblog::setGlobal(std::function<void(const char*)>(ogblogSink));
}

void BrawlerSessionGameUImpl::afterSessionConfigured(ENetMode netMode)
{
	m_roster.checkSeeded(netMode);
}

std::string_view BrawlerSessionGameUImpl::backendFingerprint()
{
	return buildIdentityUImpl::backendFingerprint();
}

void AOGBrawlerSimulationManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	m_session.endPlay();

	Super::EndPlay(EndPlayReason);

}

template void UESimulationSession<BrawlerSessionTraitsUImpl>::onInputRelayHostReady(ASimulationInputRelay& host);

void BrawlerSessionGameUImpl::afterVisualization(const UESimulationSession<BrawlerSessionTraitsUImpl>& session)
{
	if (session.getManager().has_value() && session.getSystemsExec().has_value() && !session.getManager()->runsPrediction())
	{
		BrawlerRosterUImpl::pushRingoutScores(session.getSystemsExec()->get<brawlerRingout::ScoreSystem>(),
			session.getDelayedInputComponentsById());
	}
}

TryRegisterStatus AOGBrawlerSimulationManager::tryRegister(
    SimCharacterId simId,
    SimulatableBrawler simulatable,
    UOGBrawlerSimulatableComponent& owner,
    BrawlerInputProviderFn inputProvider,
    bool isAuthority)
{
    checkf(simId != SimCharacterId::None,
           TEXT("tryRegister called with SimCharacterId::None; the component must wait until its ")
           TEXT("pawn's id is assigned (authority) or replicated (client)."));
    const unsigned int id = toStorageKey(simId);

    return m_session.tryRegister(id, std::move(simulatable), owner, std::move(inputProvider), isAuthority);
}

BodyId BrawlerSessionGameUImpl::rootBodyId(const SimulatableBrawler& simulatable)
{
    return simulatable.getPhysicsComposite()
        .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
}

void BrawlerSessionGameUImpl::afterBodiesCreated(SimulatableBrawler& simulatable, AOGBrawlerUECharacter& character,
    unsigned int id, bool isAuthority, ENetMode netMode)
{
    m_roster.onBodiesCreated(simulatable, character, id, isAuthority, netMode);
}

void BrawlerSessionGameUImpl::afterRegistered(unsigned int id, bool isAuthority)
{
    m_roster.onRegistered(id, isAuthority);
}

void AOGBrawlerSimulationManager::noteDelayedInputComponent(
    SimCharacterId simId, UOGBrawlerSimulatableComponent& component)
{
    m_session.noteDelayedInputComponent(toStorageKey(simId), component);
}

void AOGBrawlerSimulationManager::unregisterFromNewFramework(
    SimCharacterId simId, UOGBrawlerSimulatableComponent& owner, bool isAuthority)
{
    const unsigned int id = toStorageKey(simId);

    m_session.unregisterFromNewFramework(id, owner, isAuthority);
}

void BrawlerSessionGameUImpl::onUnregistered(unsigned int id)
{
    m_roster.forgetCharacter(id);

    m_inputHistoryFeed.forgetCharacter(id);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
