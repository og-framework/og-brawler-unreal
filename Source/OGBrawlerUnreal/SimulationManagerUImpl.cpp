// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationManagerUImpl-rationale.md · docs/SimulationManagerUImpl-guards.md

#include "SimulationManagerUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
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

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

OGSIM_OPTIMIZE_OFF

ASimulationManagerUImpl::ASimulationManagerUImpl()
{
    bReplicates = false;
}

BrawlerSessionTraitsUImpl::StaticData BrawlerSessionGameUImpl::makeStaticData()
{
    const FMovementStaticDataCVars movementStaticDataCVars = ASimulationManagerUImpl::readMovementStaticDataCVars();
    return BrawlerSessionTraitsUImpl::StaticData{
        movementStaticDataCVars.model,
        movementStaticDataCVars.maxWalkSpeed,
        movementStaticDataCVars.stepPeriodTicks,
        movementStaticDataCVars.stepSpeed,
        movementStaticDataCVars.gravity };
}

void ASimulationManagerUImpl::BeginPlay()
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
		TEXT("SimulationManagerUImpl: the movement simulation's gravity (%f cm/s^2) disagrees with ")
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

void ASimulationManagerUImpl::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

TryRegisterStatus ASimulationManagerUImpl::tryRegister(
    SimCharacterId simId,
    SimulatableBrawler simulatable,
    USimmableUpdateComponent& owner,
    BrawlerInputProviderFn inputProvider,
    bool isAuthority)
{
    checkf(simId != SimCharacterId::None,
           TEXT("tryRegister called with SimCharacterId::None; the component must wait until its ")
           TEXT("pawn's id is assigned (authority) or replicated (client)."));
    const unsigned int id = toStorageKey(simId);

    auto it = m_pendingRegistrations.find(id);
    if (it == m_pendingRegistrations.end())
    {
        PendingRegistration record;
        record.simulatable.emplace(std::move(simulatable));
        record.inputProvider = std::move(inputProvider);
        record.isAuthority   = isAuthority;
        auto inserted = m_pendingRegistrations.emplace(id, std::move(record));
        it = inserted.first;
    }

    PendingRegistration& record = it->second;

    if (!record.bodiesCreated)
    {
        AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(owner.GetOwner());
        checkf(character != nullptr,
               TEXT("USimmableUpdateComponent must be attached to an AOGBrawlerUECharacter — the ")
               TEXT("first-call body pass reads that class's own root capsule"));
        if (!m_session.m_backend.createBodies_GameThread(owner, *character, *record.simulatable, id))
            return TryRegisterStatus::Pending;

#if DO_CHECK
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            checkf(decl.bindings.ownBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.ownBodyId not populated by fold (id=%u)"), id);
            checkf(decl.bindings.parentBodyId.value != 0,
                   TEXT("PhysicsDeclaration bindings.parentBodyId not populated by fold (id=%u)"), id);
        });
#endif

        m_roster.onBodiesCreated(*record.simulatable, *character, id, record.isAuthority, GetNetMode());

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    const BodyId movementBodyId =
        record.simulatable->getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
    bool allResolvable = m_session.m_physAdapter->isBodyResolvable(movementBodyId);
    if (allResolvable)
    {
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            if (!m_session.m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
                allResolvable = false;
        });
    }

    if (!allResolvable)
        return TryRegisterStatus::Pending;

    if (record.isAuthority)
    {
        registerSimulatable<SimulatableBrawler>(
            m_session.m_storage, m_session.m_reconciliation, m_session.m_inputResolution, m_session.m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<SimulatableBrawler>(
            m_session.m_storage, m_session.m_reconciliation, m_session.m_inputResolution, m_session.m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/editLatencyBudgetDiagnostics().wrapInputProvider(
                id, owner, std::move(record.inputProvider)));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    m_roster.onRegistered(id, record.isAuthority);

    // ⛔G-69  docs/SimulationManagerUImpl-guards.md
    m_session.m_manager->notifyCharacterRegistered(id);

    m_session.m_backend.onRegistered_GameThread(owner, id);

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

void ASimulationManagerUImpl::noteDelayedInputComponent(
    SimCharacterId simId, USimmableUpdateComponent& component)
{
    m_session.noteDelayedInputComponent(toStorageKey(simId), component);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    SimCharacterId simId, USimmableUpdateComponent& owner, bool isAuthority)
{
    const unsigned int id = toStorageKey(simId);

    // ⛔G-73  docs/SimulationManagerUImpl-guards.md
    if (m_session.m_storage.has<SimulatableBrawler>(id))
    {
        m_session.m_manager->notifyCharacterUnregistered(id);
    }

    unregisterSimulatable<SimulatableBrawler>(
        m_session.m_storage, m_session.m_reconciliation, m_session.m_inputResolution, m_session.m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

    m_session.forgetReceptionOwner(id);

    m_roster.forgetCharacter(id);

    m_inputHistoryFeed.forgetCharacter(id);

    editLatencyBudgetDiagnostics().forgetCharacter(id);

    m_session.m_backend.onUnregistered_GameThread(owner, id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
