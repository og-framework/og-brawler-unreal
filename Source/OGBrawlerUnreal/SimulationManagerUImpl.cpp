// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationManagerUImpl-rationale.md · docs/SimulationManagerUImpl-guards.md

#include "SimulationManagerUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/SimulationComparison.h"
#include "Runtime/Engine/Public/Net/UnrealNetwork.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "GameFramework/PlayerController.h"
#include "OGSimulationUnreal/SimulationTimingRelay.h"
#include "OGSimulationUnreal/SimulationConnectionRelay.h"
#include "OGSimulationUnreal/SimulationInputRelay.h"

#include "OGBrawler/SimulatableBrawler.h"
#include "OGBrawler/DAttackRadialSimulation.h"
#include "OGBrawler/DAttackGuardSimulation.h"
#include "OGBrawler/BrawlerProjectileSimulation.h"
#include "OGBrawler/OGBrawlerLog.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "Components/CapsuleComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "OGBrawlerUnreal/UEBrawlerNetConfig.h"
#include "OGBrawlerUnreal/OGBuildIdentityUImpl.h"

#include "Runtime/Engine/Public/Net/NetPing.h"
#include "Runtime/Engine/Classes/Engine/NetConnection.h"
#include "Runtime/Engine/Classes/Engine/NetDriver.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "OGSimulation/RelayedInputRingCodec.h"
#include "OGSimulation/ResimGatePolicy.h"
#include "OGSimulation/OGAssert.h"

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

DEFINE_LOG_CATEGORY(LogOGSim);
DEFINE_LOG_CATEGORY(LogOGSimTick);
DEFINE_LOG_CATEGORY(LogOGMgmt);
DEFINE_LOG_CATEGORY(LogOGNet);
DEFINE_LOG_CATEGORY(LogOG);
DEFINE_LOG_CATEGORY(LogOGRelayProbe);
DEFINE_LOG_CATEGORY(LogOGDivergenceProbe);
DEFINE_LOG_CATEGORY(LogOGResimProbe);
DEFINE_LOG_CATEGORY(LogOGLatencyBudget);
DEFINE_LOG_CATEGORY(LogOGChaosDilation);
DEFINE_LOG_CATEGORY(LogOGBrawler);

#define HasAuthority HasAuthority_is_constant_true_on_ASimulationManagerUImpl_use_worldIsAuthority_or_runsPrediction

namespace
{
	void bindCorrectionFieldDiffGate()
	{
		// ⛔G-50  docs/SimulationManagerUImpl-guards.md
		correctionFieldDiff::setEnabledPredicate(
			[]() -> bool { return UE_LOG_ACTIVE(LogOGDivergenceProbe, Verbose); });
	}

	void RouteOGMessage(const char* msg)
	{
		FString fmsg(msg);

		ELogVerbosity::Type severity = ELogVerbosity::Log;
		FString body = fmsg;
		if (fmsg.StartsWith(TEXT("[Verbose]")))
		{
			severity = ELogVerbosity::Verbose;
			body = fmsg.RightChop(9);
		}
		else if (fmsg.StartsWith(TEXT("[Warning]")))
		{
			severity = ELogVerbosity::Warning;
			body = fmsg.RightChop(9);
		}

#define EMIT_OG(cat) \
		do { \
			switch (severity) { \
				case ELogVerbosity::Warning: UE_LOG(cat, Warning, TEXT("%s"), *fmsg); break; \
				case ELogVerbosity::Verbose: UE_LOG(cat, Verbose, TEXT("%s"), *fmsg); break; \
				default:                     UE_LOG(cat, Log,     TEXT("%s"), *fmsg); break; \
			} \
		} while (0)

		for (const OGLogRouteEntry& entry : kOGLogRoutes)
		{
			if (!body.StartsWith(entry.prefix))
				continue;

			if (severity == ELogVerbosity::Log)
				severity = entry.plainLevel;

			switch (entry.route)
			{
			case OGLogRoute::SimTick:          EMIT_OG(LogOGSimTick); break;
			case OGLogRoute::ResimProbe:       EMIT_OG(LogOGResimProbe); break;
			case OGLogRoute::Sim:              EMIT_OG(LogOGSim); break;
			case OGLogRoute::Net:              EMIT_OG(LogOGNet); break;
			case OGLogRoute::RelayProbe:       EMIT_OG(LogOGRelayProbe); break;
			case OGLogRoute::DivergenceProbe:  EMIT_OG(LogOGDivergenceProbe); break;
			case OGLogRoute::Mgmt:             EMIT_OG(LogOGMgmt); break;
			}
			return;
		}

		EMIT_OG(LogOG);

#undef EMIT_OG
	}

#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarTestGameThreadHitchMs(
		TEXT("og.Sim.TestGameThreadHitchMs"),
		0,
		TEXT("Test only (non-shipping): when above 0, the next OnPostPhysicsStep sleeps the game thread ")
		TEXT("this many milliseconds, once, and resets this variable to 0."),
		ECVF_Default);
#endif

}

OGSIM_OPTIMIZE_OFF

ASimulationManagerUImpl* ASimulationManagerUImpl::s_instances[] = {nullptr, nullptr};

ASimulationManagerUImpl::ASimulationManagerUImpl()
{
    bReplicates = false;
}

ASimulationManagerUImpl::~ASimulationManagerUImpl()
{
	// ⛔G-79  docs/SimulationManagerUImpl-guards.md
	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(true);
		ISimulationConnectionRelayListener::unregisterInstance(true);
		ISimulationInputRelayListener::unregisterInstance(true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(false);
		ISimulationConnectionRelayListener::unregisterInstance(false);
		ISimulationInputRelayListener::unregisterInstance(false);
	}
}

void ASimulationManagerUImpl::seedRingoutSpawnPointsFromLevel(UWorld& world)
{
	checkf(!m_ringoutSpawnPointsSeeded,
		TEXT("seedRingoutSpawnPointsFromLevel: the ring-out spawn table has already been ")
		TEXT("seeded. It is a ONE-TIME init, and a second pass would merge the level over its ")
		TEXT("own previous answer rather than over the authored defaults."));

	checkf(!m_manager.has_value() && !m_integrationLayer.has_value(),
		TEXT("seedRingoutSpawnPointsFromLevel: the manager or integration layer already ")
		TEXT("exists, so a simulation tick may already have read StaticData::spawnPoints. ")
		TEXT("This seed must run BEFORE the first integrate - move it back above the role ")
		TEXT("branches in BeginPlay rather than relaxing this check."));

	std::vector<brawlerRingout::LevelSpawnPoint> placements;
	for (TActorIterator<APlayerStart> it(&world); it; ++it)
	{
		const APlayerStart* playerStart = *it;
		if (playerStart == nullptr)
			continue;

		brawlerRingout::LevelSpawnPoint placement;
		placement.name     = TCHAR_TO_UTF8(*playerStart->GetName());
		placement.position = uglm::toGLMVec3(playerStart->GetActorLocation());
		placements.push_back(std::move(placement));
	}

	const std::array<glm::vec3, brawlerRingout::kMaxSpawnPoints> authoredFallback =
		m_staticData.m_ringoutStaticData.spawnPoints;

	m_staticData.m_ringoutStaticData.spawnPoints =
		brawlerRingout::spawnPointsFromLevelPlacements(placements, authoredFallback);

	m_ringoutSpawnPointsSeeded = true;

	UE_LOG(LogOGMgmt, Warning,
		TEXT("[Ringout.spawnPoints] seeded from the level: %d APlayerStart actor(s) found, ")
		TEXT("%d slot(s) filled, %d left authored (netMode=%d)"),
		static_cast<int32>(placements.size()),
		FMath::Min(static_cast<int32>(placements.size()),
			static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(brawlerRingout::kMaxSpawnPoints)
			- FMath::Min(static_cast<int32>(placements.size()),
				static_cast<int32>(brawlerRingout::kMaxSpawnPoints)),
		static_cast<int32>(GetNetMode()));

	for (uint32 slot = 0u; slot < brawlerRingout::kMaxSpawnPoints; ++slot)
	{
		const glm::vec3& point = m_staticData.m_ringoutStaticData.spawnPoints[slot];
		UE_LOG(LogOGMgmt, Warning,
			TEXT("[Ringout.spawnPoints]   slot %u -> (%.2f, %.2f, %.2f) [%s]"),
			slot, point.x, point.y, point.z,
			(slot < placements.size()) ? TEXT("level") : TEXT("authored"));
	}
}

void ASimulationManagerUImpl::BeginPlay()
{
	Super::BeginPlay();

	UWorld* uWorld = GetWorld();
	if (uWorld == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	const float engineGravityZ = uWorld->GetGravityZ();
	const float simGravityZ    = m_staticData.m_movementStaticData.gravity;
	checkf(FMath::Abs(simGravityZ - engineGravityZ) < 1e-3f,
		TEXT("SimulationManagerUImpl: the movement simulation's gravity (%f cm/s^2) disagrees with ")
		TEXT("the engine's (%f cm/s^2). The sim's value is read from UPhysicsSettings::DefaultGravityZ ")
		TEXT("at construction; this world overrides gravity in its WorldSettings. Either clear that ")
		TEXT("override or teach the movement StaticData about per-level gravity."),
		simGravityZ, engineGravityZ);

	seedRingoutSpawnPointsFromLevel(*uWorld);

	const float dt = UPhysicsSettings::Get()->AsyncFixedTimeStepSize;
	checkf(FMath::Abs(dt * static_cast<float>(UEBrawlerNetConfig::tickFrequencyHz) - 1.f) < 1e-4f,
		TEXT("SimulationManagerUImpl: UPhysicsSettings::AsyncFixedTimeStepSize (%f s) is not one tick of ")
		TEXT("UEBrawlerNetConfig::tickFrequencyHz (%d Hz). The manager's step dt and the tick rate the ")
		TEXT("per-connection templates are compiled for must agree. Was design D13 (task 18)."),
		dt, UEBrawlerNetConfig::tickFrequencyHz);

	const ENetMode worldNetMode = GetNetMode();
	const bool worldIsAuthority = (worldNetMode != NM_Client);

	createLatencyBudget(worldIsAuthority);
	m_latencyPostTickFlushHandle =
		uWorld->OnPostTickFlush().AddUObject(this, &ASimulationManagerUImpl::onLatencyPostTickFlush);

	int32 configuredRelayDelayFloorTicks = -1;
	// ⛔G-57  docs/SimulationManagerUImpl-guards.md
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniFloorTicks = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("RelayDelayFloorTicks"), iniFloorTicks, GEngineIni))
		{
			configuredRelayDelayFloorTicks = iniFloorTicks;
		}
	}

	// ⛔G-56  docs/SimulationManagerUImpl-guards.md

	int32 configuredCorrectionRotationK = -1;
	if (worldIsAuthority && GConfig != nullptr)
	{
		int32 iniK = 0;
		if (GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GGameIni) ||
			GConfig->GetInt(TEXT("OGNetcode"), TEXT("CorrectionRotationK"), iniK, GEngineIni))
		{
			configuredCorrectionRotationK = iniK;
		}
	}

	bool    hasIniResimTriggerPolicy = false;
	FString configuredResimTriggerPolicy;
	// ⛔G-58  docs/SimulationManagerUImpl-guards.md
	if (GConfig != nullptr)
	{
		FString iniPolicy;
		if (GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GGameIni) ||
			GConfig->GetString(TEXT("OGNetcode"), TEXT("ResimTriggerPolicy"), iniPolicy, GEngineIni))
		{
			hasIniResimTriggerPolicy = true;
			configuredResimTriggerPolicy = iniPolicy.TrimStartAndEnd();
		}
	}

	m_backend.buildWorld(*uWorld, dt, worldIsAuthority);

	if (worldIsAuthority)
	{
		if (s_instances[0] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[0] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/true, this);
		auto pctmloggerServer = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogServer = [](const char* msg) {
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
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(false, dt, ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmloggerServer) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmloggerServer));
		m_netSync.setLogger(std::function<void(const char*)>(pctmloggerServer));

		// ⛔G-59  docs/SimulationManagerUImpl-guards.md
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		if (configuredRelayDelayFloorTicks >= 0)
		{
			const int32 clampedFloor = clampRelayDelayFloorTicks(
				configuredRelayDelayFloorTicks, m_manager->getTimeConfig());
			if (clampedFloor != configuredRelayDelayFloorTicks)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[RelayDelayFloor] ini [OGNetcode] RelayDelayFloorTicks=%d out of range, clamped to %d ticks"),
					configuredRelayDelayFloorTicks, clampedFloor);
			}
			m_manager->setRelayDelayFloorTicks(clampedFloor);
			UE_LOG(LogOGNet, Log,
				TEXT("[RelayDelayFloor] session floor = %d ticks (ini override)"), clampedFloor);

			logRelayDelayFloorAdvisory(clampedFloor);
		}

		if (configuredCorrectionRotationK != -1)
		{
			const int32 clampedK = correctionRotation::clampK(configuredCorrectionRotationK);
			if (clampedK != configuredCorrectionRotationK)
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[StateRotation] ini [OGNetcode] CorrectionRotationK=%d out of range, clamped to %d"),
					configuredCorrectionRotationK, clampedK);
			}
			m_manager->setCorrectionRotationK(clampedK);
		}

		const int32 sessionCorrectionRotationK = m_manager->getTimeConfig().correctionRotationK;
		UE_LOG(LogOGNet, Warning,
			TEXT("[StateRotation] session K = %d (%s)"),
			sessionCorrectionRotationK,
			configuredCorrectionRotationK != -1 ? TEXT("ini override") : TEXT("compiled default"));

		const int32 sessionRelayDelayFloorTicks = m_manager->getTimeConfig().relayDelayFloorTicks;
		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->setRelayDelayFloorTicks((uint8)sessionRelayDelayFloorTicks);
		}
		else
		{
			UE_LOG(LogOGNet, Warning,
				TEXT("[RelayDelayFloor] no timing relay at manager BeginPlay — session floor %d NOT published"),
				sessionRelayDelayFloorTicks);
		}

		// ⛔G-61  docs/SimulationManagerUImpl-guards.md
		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/true, this);
			ISimulationInputRelayListener::registerInstance(/*isAuthority=*/true, this);

		m_receptionCoordinator.emplace(m_manager->getTimeConfig());
		m_receptionCoordinator->setLogger(std::function<void(const char*)>(pctmloggerServer));
		simlog::setGlobal(std::function<void(const char*)>(pctmloggerServer));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogServer));
		bindCorrectionFieldDiffGate();
	}
	else
	{
		if (s_instances[1] != nullptr)
		{
			checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));
		}
		s_instances[1] = this;
		ISimulationTimingRelayListener::registerInstance(/*isAuthority=*/false, this);
		auto pctmlogger = [](const char* msg) { RouteOGMessage(msg); };
		auto ogblogClient = [](const char* msg) {
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
		m_integrationLayer.emplace(m_storage, m_staticData, *m_physAdapter, *m_queryAdapter);
		m_systemsExec.emplace(std::piecewise_construct,
			BrawlerHitDetectionSystem(*m_physAdapter, *m_queryAdapter),
			brawlerHitRouting::System{}, brawlerRingout::ScoreSystem{});
		m_manager.emplace(/*usePrediction=*/true, dt, ManagerType::Params{
			*m_integrationLayer, m_netSync, m_inputResolution, m_reconciliation, *m_systemsExec,
			m_storage, m_staticData, std::function<void(const char*)>(pctmlogger) });
		m_reconciliation.setLogger(std::function<void(const char*)>(pctmlogger));
		m_inputResolution.setLogger(std::function<void(const char*)>(pctmlogger));
		m_netSync.setLogger(std::function<void(const char*)>(pctmlogger));

		// ⛔G-60  docs/SimulationManagerUImpl-guards.md
		m_inputResolution.setNeutralInput<SimulatableBrawler>(simulatableBrawler::getZeroPlayerInput());

		m_replicatedTierConsumer.emplace(m_manager->getTimeConfig());
		recomputeAndPublishEffectiveInputDelay();

		// ⛔G-62  docs/SimulationManagerUImpl-guards.md
		ISimulationConnectionRelayListener::registerInstance(/*isAuthority=*/false, this);
		ISimulationInputRelayListener::registerInstance(/*isAuthority=*/false, this);
		if (ASimulationConnectionRelay* connectionRelay =
				ASimulationConnectionRelay::findLocalClientRelay(uWorld))
		{
			connectionRelay->replayLatchedTier();
		}

		if (ASimulationTimingRelay* timingRelay = findTimingRelay())
		{
			timingRelay->replayLatchedRelayDelayFloor();
		}
		simlog::setGlobal(std::function<void(const char*)>(pctmlogger));
		ogblog::setGlobal(std::function<void(const char*)>(ogblogClient));
		bindCorrectionFieldDiffGate();

#if !NO_LOGGING
		UE_LOG(LogOGResimProbe, Warning,
			TEXT("[ResimProbe.Session] resim-gate probe LIVE — verbosity=%s verboseDetail=%s windowSamples=%u"),
			ToString(LogOGResimProbe.GetVerbosity()),
			LogOGResimProbe.IsSuppressed(ELogVerbosity::Verbose) ? TEXT("off") : TEXT("on"),
			static_cast<uint32>(kResimGateProbeWindowSamples));
#endif
	}

	// ⛔G-63  docs/SimulationManagerUImpl-guards.md
	{
		if (hasIniResimTriggerPolicy)
		{
			if (configuredResimTriggerPolicy.Equals(TEXT("OnDisagreement"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::OnDisagreement);
			}
			else if (configuredResimTriggerPolicy.Equals(TEXT("FrontierExact"), ESearchCase::IgnoreCase))
			{
				m_manager->setResimTriggerPolicy(TimeConfig::ResimTriggerPolicy::FrontierExact);
			}
			else
			{
				UE_LOG(LogOGNet, Warning,
					TEXT("[ResimGate] ini [OGNetcode] ResimTriggerPolicy='%s' unrecognised ")
					TEXT("(expected FrontierExact or OnDisagreement) — keeping the compiled default"),
					*configuredResimTriggerPolicy);
				hasIniResimTriggerPolicy = false;
			}
		}

		const TimeConfig& sessionTimeConfig = m_manager->getTimeConfig();
		const bool policyIsOnDisagreement =
			sessionTimeConfig.resimTriggerPolicy == TimeConfig::ResimTriggerPolicy::OnDisagreement;
		const FString depthPolicyText = policyIsOnDisagreement
			? FString::Printf(TEXT("skip beyond %d ticks"), sessionTimeConfig.rollbackWindowTicks)
			: FString(TEXT("off (inert under FrontierExact)"));
		UE_LOG(LogOGNet, Warning,
			TEXT("[ResimGate] session policy = %s (%s), depthPolicy = %s, rateLimit = none (structural: one resim in flight, one pending)"),
			policyIsOnDisagreement ? TEXT("OnDisagreement") : TEXT("FrontierExact"),
			hasIniResimTriggerPolicy ? TEXT("ini override") : TEXT("compiled default"),
			*depthPolicyText);
	}

	checkf(m_ringoutSpawnPointsSeeded,
		TEXT("SimulationManagerUImpl::BeginPlay: the ring-out spawn table was not seeded on this peer ")
		TEXT("(netMode=%d). Every peer reads StaticData::spawnPoints - a client predicts its own ")
		TEXT("respawn - so seedRingoutSpawnPointsFromLevel must not be role-gated. Was the NOT ")
		TEXT("ROLE-GATED fence at the seed call."),
		static_cast<int32>(worldNetMode));

	{
		const std::string_view fingerprint = buildIdentityUImpl::backendFingerprint();
		UE_LOG(LogOGSimHost, Warning, TEXT("[SimHost.Backend] backend=%hs fingerprint=%.*hs"),
			EngineTraits::kName, static_cast<int32>(fingerprint.size()), fingerprint.data());
	}

	m_backend.startStepping(*uWorld, dt, worldIsAuthority);
	UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: adapters, integration layer, and manager initialized"));
}

void ASimulationManagerUImpl::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// ⛔G-85  docs/SimulationManagerUImpl-guards.md
	m_backend.onEndPlayBegin();

	// ⛔G-64  docs/SimulationManagerUImpl-guards.md
	m_delayedInputComponentsById.clear();
	m_receptionCoordinator.reset();

	m_replicatedTierConsumer.reset();

	if (s_instances[0] == this)
	{
		s_instances[0] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/true);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/true);
	}
	if (s_instances[1] == this)
	{
		s_instances[1] = nullptr;
		ISimulationTimingRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationConnectionRelayListener::unregisterInstance(/*isAuthority=*/false);
		ISimulationInputRelayListener::unregisterInstance(/*isAuthority=*/false);
	}

	if (UWorld* World = GetWorld())
	{
		if (m_latencyPostTickFlushHandle.IsValid())
		{
			World->OnPostTickFlush().Remove(m_latencyPostTickFlushHandle);
			m_latencyPostTickFlushHandle.Reset();
		}

		m_backend.onEndPlayWorld(*World);
	}

	Super::EndPlay(EndPlayReason);

}

void ASimulationManagerUImpl::onConnectionTierReceived(uint8_t oldTier, uint8_t newTier)
{
    // ⛔G-65  docs/SimulationManagerUImpl-guards.md
    const bool hadAnyTier =
        m_replicatedTierConsumer.has_value() && m_replicatedTierConsumer->hasReceivedTier();

    applyReplicatedConnectionTier(newTier);

    applyTierTransitionStall(oldTier, newTier, hadAnyTier);
}

void ASimulationManagerUImpl::onConnectionTierReplayed(uint8_t tier)
{
    applyReplicatedConnectionTier(tier);
}

void ASimulationManagerUImpl::applyReplicatedConnectionTier(uint8 tier)
{
    if (!m_replicatedTierConsumer.has_value())
        return;

    m_replicatedTierConsumer->onReplicatedTierReceived((int32)tier);
    recomputeAndPublishEffectiveInputDelay();
}

void ASimulationManagerUImpl::onInputRelayHostReady(ASimulationInputRelay& host)
{
    AActor* ownerActor = host.GetOwner();
    if (ownerActor == nullptr)
        return;

    AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(ownerActor);
    if (character == nullptr)
        return;

    if (USimmableUpdateComponent* component =
            character->FindComponentByClass<USimmableUpdateComponent>())
    {
        component->attachInputRelayHost(&host);
    }
}

void ASimulationManagerUImpl::onRelayDelayFloorReceived(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/true);
}

void ASimulationManagerUImpl::onRelayDelayFloorReplayed(uint8_t floorTicks)
{
    applyReplicatedRelayDelayFloor((uint8)floorTicks, /*payForIncrease=*/false);
}

void ASimulationManagerUImpl::applyReplicatedRelayDelayFloor(uint8 floorTicks, bool payForIncrease)
{
    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 clampedFloor = clampRelayDelayFloorTicks((int32)floorTicks, cfg);
    if (clampedFloor != (int32)floorTicks)
    {
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] received floor %u out of range, clamped to %d ticks"),
            (unsigned int)floorTicks, clampedFloor);
    }

    m_manager->setRelayDelayFloorTicks(clampedFloor);

    logRelayDelayFloorAdvisory(clampedFloor);

    const int32 deltaDelayTicks = recomputeAndPublishEffectiveInputDelay();

    if (!payForIncrease || deltaDelayTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor = %d ticks, effective delay delta=%d, no stall"),
            clampedFloor, deltaDelayTicks);
        return;
    }

    requestInputDelayIncreaseStall(deltaDelayTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[RelayDelayFloor] floor = %d ticks, UPWARD, requesting %d-tick prediction stall"),
        clampedFloor, deltaDelayTicks);
}

void ASimulationManagerUImpl::logRelayDelayFloorAdvisory(int32 floorTicks)
{
    if (!m_manager.has_value())
        return;

    // ⛔G-66  docs/SimulationManagerUImpl-guards.md
    switch (classifyRelayDelayFloor(m_manager->getTimeConfig()))
    {
    case RelayDelayFloorAdvisory::BelowHiccupBaseline:
        UE_LOG(LogOGNet, Warning,
            TEXT("[RelayDelayFloor] floor=%d is below the hiccup-absorption baseline (>=2) and above off (0) — the one value that is neither"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::UniformDFairnessActive:
        UE_LOG(LogOGNet, Log,
            TEXT("[RelayDelayFloor] floor=%d >= max(rttTierInputDelays) — uniform-D fairness mode active; tiers and LAN override are inert"),
            floorTicks);
        break;
    case RelayDelayFloorAdvisory::None:
        break;
    }
}

int32 ASimulationManagerUImpl::recomputeAndPublishEffectiveInputDelay()
{
    if (!m_replicatedTierConsumer.has_value())
        return 0;

    const int32 delayTicks = (int32)m_replicatedTierConsumer->effectiveInputDelayTicks();
    const int32 deltaDelayTicks = delayTicks - m_lastPublishedEffectiveInputDelayTicks;

    m_lastPublishedEffectiveInputDelayTicks = delayTicks;
    publishClientEffectiveInputDelayTicks(delayTicks);

    UE_LOG(LogOGNet, Log,
        TEXT("[ConnectionTier] applied client input delay = %d ticks (tier %d, arrived=%d, floor %d)"),
        delayTicks,
        (int)m_replicatedTierConsumer->currentTierIndex(),
        m_replicatedTierConsumer->hasReceivedTier() ? 1 : 0,
        m_manager.has_value() ? (int)m_manager->getTimeConfig().relayDelayFloorTicks : 0);

    return deltaDelayTicks;
}

void ASimulationManagerUImpl::applyTierTransitionStall(uint8 oldTier, uint8 newTier, bool hadAnyTier)
{
    if (oldTier == newTier)
        return;

    if (!m_manager.has_value())
        return;

    const TimeConfig& cfg = m_manager->getTimeConfig();
    const int32 stallTicks = (int32)shouldStallForTierTransition(
        (int32)oldTier, (int32)newTier, hadAnyTier, cfg);

    if (stallTicks <= 0)
    {
        UE_LOG(LogOGNet, Log,
            TEXT("[ConnectionTier] tier %u -> %u hadAnyTier=%d, no stall"),
            (unsigned int)oldTier, (unsigned int)newTier, hadAnyTier ? 1 : 0);
        return;
    }

    requestInputDelayIncreaseStall(stallTicks);

    UE_LOG(LogOGNet, Warning,
        TEXT("[ConnectionTier] tier %u -> %u UPWARD, requesting %d-tick prediction stall"),
        (unsigned int)oldTier, (unsigned int)newTier, stallTicks);
}

ASimulationTimingRelay* ASimulationManagerUImpl::findTimingRelay()
{
    if (m_timingRelay) return m_timingRelay;
    for (TActorIterator<ASimulationTimingRelay> it(GetWorld()); it; ++it)
    {
        m_timingRelay = *it;
        UE_LOG(LogOGMgmt, Log, TEXT("SimulationManager: timing relay found"));
        return m_timingRelay;
    }
    return nullptr;
}

FSmallSimulationStateSyncBuffer& ASimulationManagerUImpl::getSyncedTimingBuffer()
{
    ASimulationTimingRelay* relay = findTimingRelay();
    checkf(relay != nullptr, TEXT("SimulationManagerUImpl::getSyncedTimingBuffer: relay not found"));
    return relay->editBuffer();
}

void ASimulationManagerUImpl::onPostSimulationGameThread()
{
    m_manager->onPostSimulationGameThread();
}

namespace
{
    void pushRingoutScoresToCharacters(
        const brawlerRingout::ScoreSystem& scoreSystem,
        const std::unordered_map<unsigned int, TWeakObjectPtr<USimmableUpdateComponent>>& routes)
    {
        for (const auto& entry : routes)
        {
            const unsigned int id = entry.first;

            USimmableUpdateComponent* component = entry.second.Get();
            if (component == nullptr)
                continue;

            AOGBrawlerUECharacter* character = Cast<AOGBrawlerUECharacter>(component->GetOwner());
            if (character == nullptr)
                continue;

            checkf(scoreSystem.hasScoreEntry(id),
                   TEXT("Ring-out score push: id=%u is in the authority route table but has no ")
                   TEXT("ScoreSystem roster entry. The game-thread read of the score table is ")
                   TEXT("safe only because the roster is seeded by onCharacterRegistered before ")
                   TEXT("tryRegister returns Ready - an unseeded id lets postIntegrate's ")
                   TEXT("operator[] INSERT, and rehash, on the physics thread under this walk."),
                   id);

            character->SetAuthoritativeRingoutScore(
                static_cast<int32>(scoreSystem.scoreOf(id)));
        }
    }
}

void ASimulationManagerUImpl::OnPostPhysicsStep()
{
#if !UE_BUILD_SHIPPING
	if (const int32 hitchMs = CVarTestGameThreadHitchMs.GetValueOnGameThread(); hitchMs > 0)
	{
		CVarTestGameThreadHitchMs->Set(0, ECVF_SetByConsole);
		FPlatformProcess::Sleep(static_cast<float>(hitchMs) / 1000.f);
	}
#endif

	m_backend.beforePresent_GameThread();

	onPostSimulationGameThread();

	if (m_manager.has_value() && !m_manager->runsPrediction())
	{
		if (ASimulationTimingRelay* relay = findTimingRelay())
			ServerTickClock::writeToSyncedBuffer(getServerClock(), relay->editBuffer(), 0);
	}

	m_backend.updateVisualization_GameThread();

	if (m_manager.has_value() && m_systemsExec.has_value() && !m_manager->runsPrediction())
	{
		pushRingoutScoresToCharacters(m_systemsExec->get<brawlerRingout::ScoreSystem>(),
			m_delayedInputComponentsById);
	}

	tickLatencyBudget_GameThread();
}

EngineTraits::VizQuery& ASimulationManagerUImpl::editVizQuery()
{
	return m_backend.editVizQuery();
}

const EngineTraits::VizReader& ASimulationManagerUImpl::getVizReader() const
{
	return m_backend.getVizReader();
}

std::optional<QueryVolumeId> ASimulationManagerUImpl::registerVizVolume(const QueryVolumeDescriptor& descriptor,
	AActor& owner)
{
	return m_backend.registerVizVolume(descriptor, owner);
}

SimCharacterId ASimulationManagerUImpl::allocateSimCharacterId()
{
    checkf(GetNetMode() != NM_Client,
           TEXT("allocateSimCharacterId called on a client world's manager. Only the authority assigns ")
           TEXT("simulation character ids; a client learns each one through the pawn's replicated ")
           TEXT("SimCharacterIdValue."));

    const SimCharacterId simId = m_simCharacterIds.allocate();
    // ⛔G-78  docs/SimulationManagerUImpl-guards.md
    if (simId == SimCharacterId::None)
    {
        UE_LOG(LogOGMgmt, Error,
            TEXT("allocateSimCharacterId: registration REFUSED. This authority has already assigned all %u ")
            TEXT("simulation character ids and never reuses one, so this character does not register. ")
            TEXT("Widening SimCharacterId or reusing ids is a user ruling (R2), not a fix to make here."),
            static_cast<unsigned int>(SimCharacterIdAllocator::kLastAssignable));
        OG_CHECK(false, "allocateSimCharacterId: all SimCharacterIds are spent; registration refused (R2)");
        return SimCharacterId::None;
    }

    UE_LOG(LogOGMgmt, Log, TEXT("allocateSimCharacterId: assigned id=%u"), toStorageKey(simId));
    return simId;
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
        if (!m_backend.createBodies_GameThread(owner, *character, *record.simulatable, id))
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

        const BodyId movementOwnBodyId =
            record.simulatable->getPhysicsComposite()
                .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
        const BodyId movementParentBodyId =
            record.simulatable->getPhysicsComposite()
                .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.parentBodyId;

        checkf(movementOwnBodyId == movementParentBodyId,
               TEXT("tryRegister: the movement declaration's own body (%u) is not the pawn's root ")
               TEXT("capsule (%u), id=%u. Either CharacterBindings is being stamped before the physics ")
               TEXT("fold populated bindings.ownBodyId, or the factory created a body instead of ")
               TEXT("adopting the capsule. Was the stamp-after-the-fold and isRoot fences."),
               movementOwnBodyId.value, movementParentBodyId.value, id);

        record.simulatable->setCharacterBindings({ /*.capsuleBodyId =*/ movementOwnBodyId });

        auto& movementIC = record.simulatable->editAllState().editState()
            .edit<brawlerMovementSimulation::InitialConditions>();
        movementIC.teleportPending = 1u;
        movementIC.teleportPos     =
            uglm::toGLMVec3(character->GetCapsuleComponent()->GetComponentLocation());

        if (record.isAuthority)
        {
            checkf(record.isAuthority == (GetNetMode() != NM_Client),
                   TEXT("tryRegister: isAuthority disagrees with the world-level net mode; the ")
                   TEXT("spawn-slot table must be populated on the authority world only"));

            auto& ringoutIC = record.simulatable->editAllState().editState()
                .edit<brawlerRingout::InitialConditions>();

            // ⛔G-67  docs/SimulationManagerUImpl-guards.md
            ringoutIC.spawnSlot = m_spawnSlots.acquire(id);

            UE_LOG(LogOGMgmt, Log,
                TEXT("tryRegister: ring-out spawnSlot=%u assigned to id=%u"),
                ringoutIC.spawnSlot, id);
        }

        record.bodiesCreated = true;
        return TryRegisterStatus::Pending;
    }

    const BodyId movementBodyId =
        record.simulatable->getPhysicsComposite()
            .get<brawlerMovementSimulation::PhysicsDeclaration>().bindings.ownBodyId;
    bool allResolvable = m_physAdapter->isBodyResolvable(movementBodyId);
    if (allResolvable)
    {
        record.simulatable->getPhysicsComposite().forEach([&](const auto& decl)
        {
            if (!m_physAdapter->isBodyResolvable(decl.bindings.ownBodyId))
                allResolvable = false;
        });
    }

    if (!allResolvable)
        return TryRegisterStatus::Pending;

    if (record.isAuthority)
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*predictionOwner=*/owner,
            /*authorityOwner=*/owner);
    }
    else
    {
        registerSimulatable<SimulatableBrawler>(
            m_storage, m_reconciliation, m_inputResolution, m_netSync,
            id, std::move(*record.simulatable),
            /*owner=*/owner,
            /*inputProvider=*/wrapInputProviderWithLatencyStamp(
                id, owner, std::move(record.inputProvider)));
    }
    UE_LOG(LogOGMgmt, Log, TEXT("tryRegister: registered simulatable id=%u isAuthority=%d"), id, isAuthority ? 1 : 0);

    // ⛔G-68  docs/SimulationManagerUImpl-guards.md
    if (record.isAuthority)
    {
        m_authorityRegisteredIds.insert(id);
        const int32 registered = static_cast<int32>(m_authorityRegisteredIds.size());
        if (registered > kPreDietCharacterCap)
        {
            UE_LOG(LogOGNet, Warning,
                TEXT("[PreDietCap] character %d exceeds pre-diet cap %d — input-loss margins "
                     "unsafe (T44/T38 §16); land item 40"),
                registered, kPreDietCharacterCap);
        }
    }

    // ⛔G-69  docs/SimulationManagerUImpl-guards.md
    m_manager->notifyCharacterRegistered(id);

    m_backend.onRegistered_GameThread(owner, id);

    m_pendingRegistrations.erase(it);
    return TryRegisterStatus::Ready;
}

void ASimulationManagerUImpl::noteDelayedInputComponent(
    SimCharacterId simId, USimmableUpdateComponent& component)
{
    m_delayedInputComponentsById[toStorageKey(simId)] = &component;
}

static_assert(
    RemoteInputDeliverySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputDeliverySink so "
    "ServerReceptionCoordinator can drive remote-input delivery through it");

void ASimulationManagerUImpl::deliverRemoteInput(
    unsigned int id, uint32 captureTick, const simulatableBrawler::PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->deliverDelayedRemoteInput(captureTick, input);
}

static_assert(
    RemoteInputRelaySink<ASimulationManagerUImpl, simulatableBrawler::PlayerInput>,
    "ASimulationManagerUImpl must satisfy RemoteInputRelaySink so "
    "ServerReceptionCoordinator can drive the input relay through it");

void ASimulationManagerUImpl::relayRemoteInput(
    unsigned int id, uint32 captureTick, uint8 dA,
    const simulatableBrawler::PlayerInput& input)
{
    const auto it = m_delayedInputComponentsById.find(id);
    if (it == m_delayedInputComponentsById.end())
        return;

    USimmableUpdateComponent* target = it->second.Get();
    if (target == nullptr)
    {
        m_delayedInputComponentsById.erase(it);
        return;
    }

    target->stageRelayedInput(captureTick, dA, input);

    {
        RelayWriteWindowSummary w;
        if (m_relayWriteProbe.noteWrite(
                id, static_cast<uint64>(GFrameCounter), captureTick, w))
        {
            char line[256];

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u runs=%u writes=%u observableWrites=%u "
                "captureSpan=%u receivedX1000=%u observableX1000=%u deliverableX1000=%u "
                "replaceLatestObservableX1000=%u",
                w.ownerId, w.runs, w.writes, w.observableWrites, w.captureSpan,
                w.receivedX1000, w.observableX1000, w.deliverableX1000,
                w.replaceLatestObservableX1000);
            RouteOGMessage(line);

            std::snprintf(line, sizeof(line),
                "[Warning][RelayProbe.Write] id=%u writesPerFrame p50=%u p99=%u%s "
                "max=%u emptyFrames=%u nonConsecutive=%u missedCaptureTicks=%u "
                "captures=[%u,%u] discont=%u",
                w.ownerId, w.p50, w.p99, w.p99Saturated ? "+" : "", w.maxRun,
                w.emptyFrames, w.nonConsecutiveWrites, w.missedCaptureTicks,
                w.firstCaptureTick, w.lastCaptureTick, w.discontinuities);
            RouteOGMessage(line);
        }
    }
}

void ASimulationManagerUImpl::onFrameStepsDue_GameThread(int32 firstUpcomingSimTick, int32 numSteps)
{
    static_assert(kFrameHealthProbeWindowSamples == kResimGateProbeWindowSamples,
        "PROBE A's window must stay the same length as ResimGateProbe's, so one [ResimProbe.Frame] "
        "window lines up against the surrounding [ResimProbe.Gate] windows without a resim-tick "
        "count on the line. Was the COMPARABLE WITH ResimGateProbe caution in PROBE A's banner.");
    {
        FrameHealthWindowSummary frameSummary;
        const bool frameWindowClosed = m_frameHealthProbe.noteFrame(
            static_cast<uint64>(GFrameCounter),
            static_cast<uint32>(firstUpcomingSimTick),
            static_cast<uint32>(numSteps),
            static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0),
            frameSummary);

        if (frameWindowClosed)
        {
            const bool isClient = m_manager.has_value() && m_manager->runsPrediction();
            char line[256];

            if (isClient)
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][ResimProbe.Frame] role=Client cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
            else
            {
                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server simTicks=%u frames=%u "
                    "meanTicksPerFrameX100=%u p50=%u p99=%u max=%u meanFrameUs=%u",
                    frameSummary.totalSimTicks, frameSummary.totalFrames,
                    frameSummary.meanTicksPerFrameX100, frameSummary.p50,
                    frameSummary.p99, frameSummary.maxTicksPerFrame,
                    frameSummary.meanFrameMicros);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Frame] role=Server cadence dFrame1=%u dFrame0=%u "
                    "dFrameGt1=%u discont=%u numSteps total=%u max=%u gt1=%u",
                    frameSummary.oncePerFrameSamples, frameSummary.sameFrameSamples,
                    frameSummary.skippedFrameSamples, frameSummary.discontinuities,
                    frameSummary.totalNumSteps, frameSummary.maxNumSteps,
                    frameSummary.numStepsAboveOne);
                RouteOGMessage(line);
            }
        }
    }

    // ⛔G-70  docs/SimulationManagerUImpl-guards.md
    if (!m_receptionCoordinator.has_value())
        return;

    if (const UWorld* world = GetWorld())
    {
        if (const UNetDriver* netDriver = world->GetNetDriver())
        {
            const uint32 tickRateHz =
                static_cast<uint32>(FMath::Max(1, netDriver->GetNetServerMaxTickRate()));
            const uint64 nowMicros =
                static_cast<uint64>(FPlatformTime::Seconds() * 1000000.0);

            for (UNetConnection* conn : netDriver->ClientConnections)
            {
                if (conn == nullptr)
                    continue;

                {
                    static bool s_loggedPacketBudget = false;
                    if (!s_loggedPacketBudget)
                    {
                        s_loggedPacketBudget = true;
                        const int32 usableBits = conn->GetMaxSingleBunchSizeBits();
                        UE_LOG(LogOGNet, Warning,
                            TEXT("[PacketBudget] usableSingleBunchBytes = %d, handlerBits = %d "
                                 "(maxPacket=%d, usableBits=%d)"),
                            // ∴D-50  docs/SimulationManagerUImpl-rationale.md
                            (usableBits / 32) * 4, conn->MaxPacketHandlerBits,
                            conn->MaxPacket, usableBits);
                    }
                }

                ConnectionBudgetWindowSummary budget;
                // ⛔G-72  docs/SimulationManagerUImpl-guards.md
                const bool budgetWindowClosed = m_connectionBudgetProbe.noteSample(
                    static_cast<uint32>(conn->GetUniqueID()),
                    conn->CurrentNetSpeed,
                    conn->QueuedBits,
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalBytes)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPackets)),
                    static_cast<uint64>(FMath::Max(0, conn->OutTotalPacketsLost)),
                    tickRateHz, nowMicros, budget);

                if (!budgetWindowClosed)
                    continue;

                char line[256];

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u frames=%u elapsedMs=%u "
                    "netSpeedBps=%d allowanceBytesPerTick=%u outBytes=%u "
                    "bytesPerTick=%u occupancyPctX10=%u",
                    budget.connectionId, budget.samples, budget.elapsedMs,
                    budget.netSpeedBps, budget.allowanceBytesPerTick,
                    budget.outBytes, budget.bytesPerSample, budget.occupancyPctX10);
                RouteOGMessage(line);

                std::snprintf(line, sizeof(line),
                    "[Warning][RelayProbe.Budget] conn=%u queuedBits min=%d max=%d "
                    "mean=%d notReadyFrames=%u outPackets=%u bytesPerPacket=%u lost=%u",
                    budget.connectionId, budget.queuedBitsMin, budget.queuedBitsMax,
                    budget.queuedBitsMean, budget.notReadySamples,
                    budget.outPackets, budget.bytesPerPacket, budget.outPacketsLost);
                RouteOGMessage(line);
            }
        }
    }
}

void ASimulationManagerUImpl::releaseDelayedInputsForStep(int32 firstTick, int32 numSteps)
{
    if (!m_receptionCoordinator.has_value())
        return;

    OG_LOG_ROUTED(LogOGNet, TEXT("[ReleaseBatch] first=%d numSteps=%d"), firstTick, numSteps);

    auto deliver = [this](unsigned int id, uint32 captureTick,
                          const simulatableBrawler::PlayerInput& input) -> bool
    {
        const auto it = m_delayedInputComponentsById.find(id);
        if (it == m_delayedInputComponentsById.end())
            return false;

        if (it->second.Get() == nullptr)
        {
            m_delayedInputComponentsById.erase(it);
            return false;
        }

        deliverRemoteInput(id, captureTick, input);
        return true;
    };

    m_receptionCoordinator->releaseDelayedInputs<SimulatableBrawler>(
        firstTick, numSteps, deliver);
}

void ASimulationManagerUImpl::unregisterFromNewFramework(
    SimCharacterId simId, USimmableUpdateComponent& owner, bool isAuthority)
{
    const unsigned int id = toStorageKey(simId);

    // ⛔G-73  docs/SimulationManagerUImpl-guards.md
    if (m_storage.has<SimulatableBrawler>(id))
    {
        m_manager->notifyCharacterUnregistered(id);
    }

    unregisterSimulatable<SimulatableBrawler>(
        m_storage, m_reconciliation, m_inputResolution, m_netSync,
        id,
        /*predictionOwner=*/&owner,
        /*authorityOwner=*/isAuthority ? &owner : nullptr);

    if (m_receptionCoordinator.has_value())
    {
        m_receptionCoordinator->forgetOwner(id);
    }
    m_delayedInputComponentsById.erase(id);

    m_relayWriteProbe.forgetOwner(id);

    m_authorityRegisteredIds.erase(id);

    // ⛔G-74  docs/SimulationManagerUImpl-guards.md
    m_spawnSlots.release(id);

    m_inputHistory.forgetCharacter(id);

    if (m_latencyProbe)
        m_latencyProbe->forgetLane(id);
    m_latencyLastHandedCorrectionTick.erase(id);

    m_backend.onUnregistered_GameThread(owner, id);

    UE_LOG(LogOGMgmt, Log, TEXT("NewFramework: unregistered simulatable id=%u"), id);
}

void ASimulationManagerUImpl::createLatencyBudget(bool isAuthority)
{
	m_latencyProbe = std::make_unique<latencyBudget::LatencyBudgetProbe>(
		isAuthority ? latencyBudget::kServerHops : latencyBudget::kClientHops);
	m_latencyMailbox      = std::make_unique<LatencyMailbox>();
	m_latencyClockMailbox = std::make_unique<LatencyClockMailbox>();
}

BrawlerInputProviderFn ASimulationManagerUImpl::wrapInputProviderWithLatencyStamp(
	unsigned int id, const USimmableUpdateComponent& owner, BrawlerInputProviderFn inputProvider)
{
	if (!inputProvider)
		return inputProvider;

	const UOGBrawlerInputCollectionComponent* inputCollection = owner.getOwnerInputCollection();
	BrawlerInputProviderFn wrapped =
		[this, id, inputCollection, inner = std::move(inputProvider)](
			const SimulationTimeStep& step,
			const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache)
		{
			simulatableBrawler::PlayerInput input = inner(step, localInputCache);
			const double capturedSeconds = FPlatformTime::Seconds();
			const uint32 tick = step.getTick();
			const double sampledSeconds =
				inputCollection != nullptr ? inputCollection->getInputSampledSeconds() : 0.0;
			if (sampledSeconds > 0.0)
			{
				postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, tick, latencyBudget::Point::InputSampled, sampledSeconds));
			}
			postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
				id, tick, latencyBudget::Point::Captured, capturedSeconds));
			return input;
		};

	checkf(static_cast<bool>(wrapped),
		TEXT("wrapInputProviderWithLatencyStamp: a present provider must stay present. Provider ")
		TEXT("presence is the core's local-vs-remote identity test, so the latency wrapper may ")
		TEXT("never turn an empty provider into a callable or a callable into an empty one."));
	return wrapped;
}

void ASimulationManagerUImpl::stampLatency(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point)
{
	checkf(IsInGameThread(),
		TEXT("ASimulationManagerUImpl::stampLatency is the GAME-THREAD door to the latency probe; ")
		TEXT("the physics thread posts through postLatencyEvent_Internal's mailbox instead."));
	if (m_latencyProbe)
		m_latencyProbe->stamp(stream, lane, tick, point, FPlatformTime::Seconds());
}

void ASimulationManagerUImpl::noteLatencyWireSample(double roundTripSeconds)
{
	checkf(IsInGameThread(),
		TEXT("ASimulationManagerUImpl::noteLatencyWireSample is game-thread only: the probe has one owner."));
	if (m_latencyProbe)
		m_latencyProbe->addSample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5);
}

void ASimulationManagerUImpl::postLatencyEvent_Internal(const latencyBudget::Event& event)
{
	if (m_latencyMailbox)
		m_latencyMailbox->tryPush(event);
}

void ASimulationManagerUImpl::stampLatencyAfterStep_Internal(double stepStartSeconds)
{
	if (!m_latencyMailbox || !m_manager.has_value())
		return;

	const uint32 tick = m_manager->currentIntegratedTick();

	if (!m_manager->runsPrediction())
	{
		m_storage.forEachSimulatable<SimulatableBrawler>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32 captureTick =
					m_inputResolution.getLastUsedCaptureTick<SimulatableBrawler>(id);
				if (captureTick != kNoInputCaptureTick)
				{
					postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, captureTick, latencyBudget::Point::Consumed, stepStartSeconds));
				}
			});
		return;
	}

	postLatencyEvent_Internal(latencyBudget::Event::stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::ClientReceived, latencyBudget::Point::Consumed, stepStartSeconds));

	m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_inputResolution.getDiagnostics().relayedReadObservations<SimulatableBrawler>(id);
			if (ring == nullptr)
				return;
			for (std::size_t index = 0; index < ring->size(); ++index)
			{
				const RelayedReadObservation* const observation = ring->at(index);
				if (observation == nullptr || observation->simTick != tick)
					continue;
				if (observation->hasAppliedCaptureTick)
				{
					postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, observation->appliedCaptureTick, latencyBudget::Point::Consumed,
						stepStartSeconds));
				}
				return;
			}
		});

	if (m_latencyClockMailbox)
	{
		const ClientPredictionClock& clock = m_manager->getClientClock();
		LatencyClockCounts counts;
		counts.skips       = clock.getDiagnostics().skipCount();
		counts.stalls      = clock.getDiagnostics().stallCount();
		counts.hardResyncs = clock.getDiagnostics().hardResyncCount();
		if (!(counts == m_latencyClockPosted_Physics) && m_latencyClockMailbox->tryPush(counts))
			m_latencyClockPosted_Physics = counts;
	}
}

void ASimulationManagerUImpl::stampLatencyStepEnd_Internal()
{
	if (!m_latencyMailbox || !m_manager.has_value())
		return;

	const double stepEndSeconds = FPlatformTime::Seconds();
	const uint32 tick = m_manager->currentIntegratedTick();
	m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			postLatencyEvent_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}

void ASimulationManagerUImpl::onLatencyPostTickFlush()
{
	if (!m_latencyProbe)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_latencyProbe->stampPending(latencyBudget::Stream::Input,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
	m_latencyProbe->stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
}

void ASimulationManagerUImpl::tickLatencyBudget_GameThread()
{
	if (!m_latencyProbe || !m_latencyMailbox || !m_manager.has_value())
		return;

	m_latencyMailbox->drain([this](const latencyBudget::Event& event) { m_latencyProbe->apply(event); });
	m_latencyMailboxDropped += m_latencyMailbox->takeDroppedCount();
	if (m_latencyClockMailbox)
	{
		m_latencyClockMailbox->drain(
			[this](const LatencyClockCounts& counts) { m_latencyClockLatest = counts; });
		m_latencyMailboxDropped += m_latencyClockMailbox->takeDroppedCount();
	}

	const double nowSeconds = FPlatformTime::Seconds();
	const bool isClient = m_manager->runsPrediction();

	if (!isClient)
	{
		for (const auto& entry : m_delayedInputComponentsById)
		{
			USimmableUpdateComponent* component = entry.second.Get();
			if (component == nullptr)
				continue;
			const uint32 correctionTick =
				component->getSyncedCorrectionStateBuffer().readFromBuffer<uint32>(
					correctionStateBuffer::kTickOffset);
			if (correctionTick == 0u)
				continue;
			uint32& lastHanded = m_latencyLastHandedCorrectionTick[entry.first];
			if (lastHanded == correctionTick)
				continue;
			lastHanded = correctionTick;
			m_latencyProbe->stamp(latencyBudget::Stream::State, entry.first, correctionTick,
				latencyBudget::Point::Handed, nowSeconds);
		}
	}

	m_latencyProbe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

	m_backend.sampleLatencyWindow_GameThread(isClient);

	latencyBudget::WindowReport report;
	if (!m_latencyProbe->closeWindowIfDue(nowSeconds, report))
		return;

	const TCHAR* role = isClient ? TEXT("Client") : TEXT("Server");
	constexpr double kMillis = 1000.0;
	for (std::size_t index = 0; index < report.hopCount; ++index)
	{
		const latencyBudget::HopSummary& hop = report.hops[index];
		UE_LOG(LogOGLatencyBudget, Warning,
			TEXT("[LatencyBudget] role=%s hop=%s p50=%.3f p95=%.3f p99=%.3f max=%.3f n=%u unit=ms ")
			TEXT("noStart=%u noEnd=%u outOfOrder=%u overflow=%u"),
			role, ANSI_TO_TCHAR(latencyBudget::hopName(hop.hop)),
			hop.p50 * kMillis, hop.p95 * kMillis, hop.p99 * kMillis, hop.max * kMillis, hop.n,
			hop.noStart, hop.noEnd, hop.outOfOrder, hop.overflow);
	}
	UE_LOG(LogOGLatencyBudget, Warning,
		TEXT("[LatencyBudget.Window] role=%s seconds=%.2f stale=%u duplicate=%u laneOverflow=%u ")
		TEXT("mailboxDropped=%llu"),
		role, report.windowEndSeconds - report.windowStartSeconds, report.staleStamps,
		report.duplicateStamps, report.laneOverflow,
		static_cast<unsigned long long>(m_latencyMailboxDropped));
	m_latencyMailboxDropped = 0u;

	m_backend.logLatencyWindow_GameThread(isClient);
}

OGSIM_OPTIMIZE_ON

#undef HasAuthority
