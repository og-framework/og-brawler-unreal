// SPDX-License-Identifier: BUSL-1.1
// docs/ChaosTickBackendUImpl-rationale.md · docs/ChaosTickBackendUImpl-guards.md

#if OG_PHYSICS_BACKEND_CHAOS
#include "ChaosTickBackendUImpl.h"

#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerUECharacter.h"
#include "OGBrawlerUnreal/BrawlerChaosBindingUImpl.h"
#include "OGSimulation/CompilerControl.h"
#include "OGSimulation/SimulationComparison.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "Runtime/PhysicsCore/Public/Chaos/ChaosScene.h"
#include "Runtime/Engine/Public/Physics/NetworkPhysicsComponent.h"
#include "Runtime/Experimental/Chaos/Public/PBDRigidsSolver.h"
#include "Runtime/Experimental/Chaos/Public/RewindData.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

namespace
{
	const TCHAR* pushProbeResimTypeText(Chaos::EResimType type)
	{
		switch (type)
		{
		case Chaos::EResimType::FullResim:       return TEXT("FullResim");
		case Chaos::EResimType::ResimAsFollower: return TEXT("ResimAsFollower");
		default:                                 return TEXT("?");
		}
	}

	const TCHAR* pushProbeObjectStateText(Chaos::EObjectStateType state)
	{
		switch (state)
		{
		case Chaos::EObjectStateType::Static:    return TEXT("Static");
		case Chaos::EObjectStateType::Kinematic: return TEXT("Kinematic");
		case Chaos::EObjectStateType::Dynamic:   return TEXT("Dynamic");
		case Chaos::EObjectStateType::Sleeping:  return TEXT("Sleeping");
		default:                                 return TEXT("?");
		}
	}

	struct LivePushProbeRead
	{
		PhysicsBodyState startOfStep;
		glm::vec3        solvedP{0.f};
	};

	LivePushProbeRead readLiveBodyForPushProbe(Chaos::FSingleParticlePhysicsProxy& proxy)
	{
		LivePushProbeRead read;
		auto* ptApi = proxy.GetPhysicsThreadAPI();
		const Chaos::FConstGenericParticleHandle particle(proxy.GetHandle_LowLevel());
		// ⛔G-03  docs/ChaosTickBackendUImpl-guards.md
		read.startOfStep.position        = uglm::toGLMVec3(particle->GetX());
		read.startOfStep.rotation        = uglm::toGLMQuat(FQuat(particle->GetR()));
		read.startOfStep.linearVelocity  = uglm::toGLMVec3(ptApi->GetV());
		read.startOfStep.angularVelocity = uglm::toGLMVec3(ptApi->GetW());
		read.solvedP                     = uglm::toGLMVec3(particle->GetP());
		return read;
	}

	float pushProbeVectorDelta(const glm::vec3& a, const glm::vec3& b)
	{
		const float dx = std::abs(a.x - b.x);
		const float dy = std::abs(a.y - b.y);
		const float dz = std::abs(a.z - b.z);
		float worst = dx;
		if (dy > worst) worst = dy;
		if (dz > worst) worst = dz;
		return worst;
	}

	float pushProbeRotationDelta(const glm::quat& a, const glm::quat& b)
	{
		return std::abs(std::abs(glm::dot(a, b)) - 1.f);
	}

	// ⛔G-04  docs/ChaosTickBackendUImpl-guards.md
	bool pushProbeFieldsSimilar(const PhysicsBodyState& a, const PhysicsBodyState& b,
	                            bool wireCarriesRotationAndSpin)
	{
		return isSimilarToField(a.position,       b.position)
			&& isSimilarToField(a.linearVelocity, b.linearVelocity)
			&& (!wireCarriesRotationAndSpin
				|| (isSimilarToField(a.rotation,        b.rotation)
					&& isSimilarToField(a.angularVelocity, b.angularVelocity)));
	}

	float pushProbeWorstDelta(const PhysicsBodyState& a, const PhysicsBodyState& b,
	                          bool wireCarriesRotationAndSpin)
	{
		float worst = pushProbeVectorDelta(a.position, b.position);
		const float lin = pushProbeVectorDelta(a.linearVelocity, b.linearVelocity);
		if (lin > worst) worst = lin;
		if (wireCarriesRotationAndSpin)
		{
			const float rot = pushProbeRotationDelta(a.rotation,        b.rotation);
			const float ang = pushProbeVectorDelta  (a.angularVelocity, b.angularVelocity);
			if (rot > worst) worst = rot;
			if (ang > worst) worst = ang;
		}
		return worst;
	}

	// ⛔G-10  docs/ChaosTickBackendUImpl-guards.md
	bool pushProbeRestoreSkipFires(const PhysicsBodyState& pushed, const PhysicsBodyState& startOfStep,
	                               bool wireCarriesRotationAndSpin)
	{
		return pushed.position == startOfStep.position && pushed.linearVelocity == startOfStep.linearVelocity
			&& (!wireCarriesRotationAndSpin
				|| (pushed.rotation == startOfStep.rotation && pushed.angularVelocity == startOfStep.angularVelocity));
	}

	struct PushProbeTally
	{
		int32 bodies             = 0;
		int32 compared           = 0;
		int32 unresolved         = 0;
		int32 inert              = 0;
		int32 nonInert           = 0;
		int32 nonInertMatched    = 0;
		int32 nonInertMismatched = 0;
		int32 movedAtPush        = 0;
	};

	// ⛔G-05  docs/ChaosTickBackendUImpl-guards.md
	const TCHAR* pushProbeVerdictText(const PushProbeTally& tally)
	{
		if (tally.nonInert == 0)
			return TEXT("VACUOUS");
		return tally.nonInertMismatched == 0 ? TEXT("ALL_MATCH") : TEXT("MISMATCH");
	}

	void logPushProbeBody(
		PushProbeTally&                       tally,
		const Chaos::FGeometryParticleHandle& handle,
		const FRewindPushProbeStashedBody&    stashed,
		const LivePushProbeRead&              live,
		int32                                 pushFrame,
		int32                                 readFrame,
		uint32_t                              simTick)
	{
		const PhysicsBodyState& now    = live.startOfStep;
		const PhysicsBodyState& pushed = stashed.pushed;
		const PhysicsBodyState& before = stashed.beforePush;
		const bool              wire   = stashed.wireCarriesRotationAndSpin;

		const bool inert = pushProbeFieldsSimilar(before, pushed, wire);
		const bool match = pushProbeFieldsSimilar(now,    pushed, wire);

		++tally.compared;
		if (stashed.movedAtPush)
			++tally.movedAtPush;
		if (inert)
		{
			++tally.inert;
		}
		else
		{
			++tally.nonInert;
			if (match)
				++tally.nonInertMatched;
			else
				++tally.nonInertMismatched;
		}

		UE_LOG(LogOGResimProbe, Verbose,
			TEXT("[ResimProbe.PushTarget] pushFrame=%d readFrame=%d simTick=%u id=%u body=%hs bodyId=%u ")
			TEXT("inert=%d match=%d moved=%d cmp=%hs resimType=%s objState=%s eps=%.6f ")
			TEXT("dBefore=%.6f dBeforePos=%.6f dBeforeRot=%.6f dBeforeLin=%.6f dBeforeAng=%.6f ")
			TEXT("dPos=%.6f dRot=%.6f dLin=%.6f dAng=%.6f dXP=%.6f ")
			TEXT("pushedPos=(%.4f,%.4f,%.4f) livePos=(%.4f,%.4f,%.4f) ")
			TEXT("pushedRot=(%.4f,%.4f,%.4f,%.4f) liveRot=(%.4f,%.4f,%.4f,%.4f) ")
			TEXT("pushedLin=(%.4f,%.4f,%.4f) liveLin=(%.4f,%.4f,%.4f) ")
			TEXT("pushedAng=(%.4f,%.4f,%.4f) liveAng=(%.4f,%.4f,%.4f) ")
			TEXT("beforePos=(%.4f,%.4f,%.4f) beforeLin=(%.4f,%.4f,%.4f)"),
			pushFrame, readFrame, simTick, stashed.simulatableId,
			stashed.bodyName != nullptr ? stashed.bodyName : "?",
			static_cast<uint32>(stashed.bodyId.value),
			inert ? 1 : 0, match ? 1 : 0, stashed.movedAtPush ? 1 : 0,
			wire ? "pos,rot,lin,ang" : "pos,lin",
			pushProbeResimTypeText(handle.ResimType()),
			pushProbeObjectStateText(handle.ObjectState()),
			kDefaultSimilarityEpsilon,
			pushProbeWorstDelta   (before, pushed, wire),
			pushProbeVectorDelta  (before.position,        pushed.position),
			pushProbeRotationDelta(before.rotation,        pushed.rotation),
			pushProbeVectorDelta  (before.linearVelocity,  pushed.linearVelocity),
			pushProbeVectorDelta  (before.angularVelocity, pushed.angularVelocity),
			pushProbeVectorDelta  (now.position,        pushed.position),
			pushProbeRotationDelta(now.rotation,        pushed.rotation),
			pushProbeVectorDelta  (now.linearVelocity,  pushed.linearVelocity),
			pushProbeVectorDelta  (now.angularVelocity, pushed.angularVelocity),
			pushProbeVectorDelta  (now.position,        live.solvedP),
			pushed.position.x, pushed.position.y, pushed.position.z,
			now.position.x,    now.position.y,    now.position.z,
			pushed.rotation.x, pushed.rotation.y, pushed.rotation.z, pushed.rotation.w,
			now.rotation.x,    now.rotation.y,    now.rotation.z,    now.rotation.w,
			pushed.linearVelocity.x,  pushed.linearVelocity.y,  pushed.linearVelocity.z,
			now.linearVelocity.x,     now.linearVelocity.y,     now.linearVelocity.z,
			pushed.angularVelocity.x, pushed.angularVelocity.y, pushed.angularVelocity.z,
			now.angularVelocity.x,    now.angularVelocity.y,    now.angularVelocity.z,
			before.position.x,       before.position.y,       before.position.z,
			before.linearVelocity.x, before.linearVelocity.y, before.linearVelocity.z);
	}
}

OGSIM_OPTIMIZE_OFF

void FSimulationState2::Copy(const FSimulationState2& Value)
{
	bIsValid = Value.bIsValid;
	DeltaTime = Value.DeltaTime;
}

FName FSimulationManagerAsyncCallback::GetFNameForStatId() const
{
	const static FLazyName StaticName("FSimulationManagerAsyncCallback");
	return StaticName;
}

void FSimulationManagerAsyncCallback::OnPreSimulate_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting(); 
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	// ⛔G-02  docs/ChaosTickBackendUImpl-guards.md
	if (m_pushProbeStashFrame != INDEX_NONE)
	{
		if (isFirstResimulationFrame)
			readPushProbeVerdict_Internal();
		else
			discardPushProbeStash(TEXT("notResetting"));
	}

	if (!isResimulating)
	{
		const unsigned int chaosTick = solver.GetCurrentFrame();
		const unsigned int simulationTick = [&input]() {
			if (!input->m_manager->runsPrediction())
				return input->m_manager->getServerClock().getTick();
			else
				return input->m_manager->getClientClock().getPredictionTick();
		}();
		input->m_manager->editChaosTickMapper().update((int32_t)chaosTick, (int32_t)simulationTick);
	}

	const double stepStartSeconds = FPlatformTime::Seconds();
	input->m_manager->onGameSimulation(updateInfo);

	if (!isResimulating)
		input->m_manager->stampLatencyAfterStep_Internal(stepStartSeconds);
}

void FSimulationManagerAsyncCallback::OnPostSolve_Internal()
{
	const FSimulationInput2* input = this->GetConsumerInput_Internal();
	if (input == nullptr || input->m_manager == nullptr)
		return;

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	Chaos::FPBDRigidsEvolution* rigidsEvolution = solver.GetEvolution();

	const bool isResimulating = rigidsEvolution->IsResimming();
	const bool isFirstResimulationFrame = rigidsEvolution->IsResetting();
	SimulationUpdateInfo updateInfo(isResimulating, isFirstResimulationFrame);

	input->m_manager->onPostGameSimulation(updateInfo);

	if (!isResimulating)
		input->m_manager->stampLatencyStepEnd_Internal();
}

void FSimulationManagerAsyncCallback::ProcessInputs_Internal(int32 PhysicsStep)
{

}

void FSimulationManagerAsyncCallback::ProcessInputs_External(int32 PhysicsStep)
{

}

int32 FSimulationManagerAsyncCallback::TriggerRewindIfNeeded_Internal(int32 LastCompletedStep)
{

	ChaosTickBackendUImpl* manager = m_manager;
	if (manager == nullptr)
	{
		OG_LOG_ROUTED(LogOGSimTick, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d manager=null rewind=0"),
			LastCompletedStep);
		return INDEX_NONE;
	}

	if (!manager->runsPrediction())
		return INDEX_NONE;

	unsigned int correctionTick = manager->onCheckIsSimilar();
	const bool willRewind = correctionTick != std::numeric_limits<unsigned int>::max() && correctionTick != 0u;
	if (!willRewind)
	{
		OG_LOG_ROUTED(LogOGSimTick, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u rewind=0"),
			LastCompletedStep, correctionTick);
		return INDEX_NONE;
	}

	const int32 unrealTickDifferenceAdjustedTick = manager->getChaosTickMapper().toChaosTick(static_cast<int32_t>(correctionTick));
	UE_LOG(LogOGSimTick, Log, TEXT("[ResimCheck.TriggerRewind] lastCompletedStep=%d correctionTick=%u chaosTick=%d rewind=1"),
		LastCompletedStep, correctionTick, unrealTickDifferenceAdjustedTick);

	manager->noteResimRequest(correctionTick, LastCompletedStep, unrealTickDifferenceAdjustedTick);

	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.Request] requestedSimTick=%u requestedChaosFrame=%d lastCompletedStep=%d mapperOffset=%d"),
		correctionTick, unrealTickDifferenceAdjustedTick, LastCompletedStep,
		manager->getChaosTickMapper().toChaosTick(0));

	return unrealTickDifferenceAdjustedTick;
}

void FSimulationManagerAsyncCallback::ApplyCorrections_Internal(int32 PhysicsStep, Chaos::FSimCallbackInput* Input)
{

}

void FSimulationManagerAsyncCallback::FirstPreResimStep_Internal(int32 PhysicsStep)
{
	if (m_manager == nullptr || !m_manager->runsPrediction())
		return;

	// ⛔G-06  docs/ChaosTickBackendUImpl-guards.md
	m_manager->noteResimGrant(PhysicsStep);

	const uint32_t simTick = static_cast<uint32_t>(
		m_manager->getChaosTickMapper().toSimulationTick(static_cast<int32_t>(PhysicsStep)));

	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();

	const bool pushProbeActive = UE_LOG_ACTIVE(LogOGResimProbe, Verbose);

	if (m_pushProbeStashFrame != INDEX_NONE)
		discardPushProbeStash(TEXT("newPushBeforeRead"));

	if (pushProbeActive)
	{
		m_pushProbeStash.Reset();
		m_pushProbeStashFrame      = PhysicsStep;
		m_pushProbeStashSimTick    = simTick;
		m_pushProbeStashBodies     = 0;
		m_pushProbeStashUnresolved = 0;
		m_pushProbeStashSkipped    = 0;

		m_manager->editStorage().forEachSimulatable(
			[&](unsigned int id, SimulatableBrawler& simulatable)
			{
				simulatable.getPhysicsComposite().forEach([&](const auto& decl) {
					using D = std::decay_t<decltype(decl)>;
					using S = typename D::StateType;
					using BodyStateT = std::remove_cvref_t<
						decltype(D::bodyStateOf(std::declval<const S&>()))>;
					++m_pushProbeStashBodies;

					const BodyId bodyId = decl.bindings.ownBodyId;
					Chaos::FSingleParticlePhysicsProxy* proxy =
						solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
					if (proxy == nullptr || proxy->GetPhysicsThreadAPI() == nullptr)
					{
						++m_pushProbeStashUnresolved;
						return;
					}

					FRewindPushProbeStashedBody stashed;
					stashed.beforePush                 = readLiveBodyForPushProbe(*proxy).startOfStep;
					stashed.bodyId                     = bodyId;
					stashed.simulatableId              = id;
					stashed.bodyName                   = D::name;
					stashed.wireCarriesRotationAndSpin = std::is_same_v<BodyStateT, PhysicsBodyState>;
					m_pushProbeStash.Add(stashed);
				});
			});
	}

	// ⛔G-08  docs/ChaosTickBackendUImpl-guards.md
	m_manager->prepareResimulation(PhysicsStep, simTick);

	if (pushProbeActive)
	{
		int32 visited        = 0;
		bool  stashOrderHeld = true;
		m_manager->editStorage().forEachSimulatable(
			[&](unsigned int /*id*/, SimulatableBrawler& simulatable)
			{
				const simulatableBrawler::State& state = simulatable.getAllState().getState();
				simulatable.getPhysicsComposite().forEach([&](const auto& decl) {
					using D = std::decay_t<decltype(decl)>;
					using S = typename D::StateType;

					const BodyId bodyId = decl.bindings.ownBodyId;
					Chaos::FSingleParticlePhysicsProxy* proxy =
						solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
					if (proxy == nullptr || proxy->GetPhysicsThreadAPI() == nullptr)
						return;
					if (!stashOrderHeld || visited >= m_pushProbeStash.Num() || m_pushProbeStash[visited].bodyId != bodyId)
					{
						stashOrderHeld = false;
						return;
					}

					FRewindPushProbeStashedBody& stashed = m_pushProbeStash[visited++];
					stashed.pushed = D::bodyStateOf(state.template get<S>());
					const PhysicsBodyState afterPush = readLiveBodyForPushProbe(*proxy).startOfStep;
					stashed.movedAtPush = !(
						   isSimilarToField(afterPush.position,        stashed.beforePush.position)
						&& isSimilarToField(afterPush.rotation,        stashed.beforePush.rotation)
						&& isSimilarToField(afterPush.linearVelocity,  stashed.beforePush.linearVelocity)
						&& isSimilarToField(afterPush.angularVelocity, stashed.beforePush.angularVelocity));
					if (pushProbeRestoreSkipFires(stashed.pushed, stashed.beforePush, stashed.wireCarriesRotationAndSpin))
						++m_pushProbeStashSkipped;
				});
			});
		ensureMsgf(stashOrderHeld && visited == m_pushProbeStash.Num(),
			TEXT("FirstPreResimStep_Internal: the push probe's after pass must visit exactly the before pass's bodies, ")
			TEXT("in the same order (visited=%d stashed=%d orderHeld=%d)"),
			visited, m_pushProbeStash.Num(), stashOrderHeld ? 1 : 0);
	}
}

void FSimulationManagerAsyncCallback::readPushProbeVerdict_Internal()
{
	Chaos::FPBDRigidsSolver& solver = this->GetSolver()->CastChecked();
	const int32 readFrame = static_cast<int32>(solver.GetCurrentFrame());

	PushProbeTally tally;
	tally.bodies     = m_pushProbeStashBodies;
	tally.unresolved = m_pushProbeStashUnresolved;

	for (const FRewindPushProbeStashedBody& stashed : m_pushProbeStash)
	{
		Chaos::FSingleParticlePhysicsProxy* proxy =
			solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(stashed.bodyId.value)});
		Chaos::FGeometryParticleHandle* handle =
			proxy != nullptr ? proxy->GetHandle_LowLevel() : nullptr;
		if (handle == nullptr)
		{
			++tally.unresolved;
			continue;
		}

		const LivePushProbeRead live = readLiveBodyForPushProbe(*proxy);
		logPushProbeBody(tally, *handle, stashed, live,
		                 m_pushProbeStashFrame, readFrame, m_pushProbeStashSimTick);
	}

	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.PushVerdict] pushFrame=%d readFrame=%d simTick=%u verdict=%s ")
		TEXT("bodies=%d compared=%d inert=%d nonInert=%d nonInertMatched=%d ")
		TEXT("nonInertMismatched=%d unresolved=%d moved=%d skipped=%d eps=%.6f reason=-"),
		m_pushProbeStashFrame, readFrame, m_pushProbeStashSimTick,
		pushProbeVerdictText(tally),
		tally.bodies, tally.compared, tally.inert, tally.nonInert, tally.nonInertMatched,
		tally.nonInertMismatched, tally.unresolved, tally.movedAtPush, m_pushProbeStashSkipped,
		kDefaultSimilarityEpsilon);

	m_pushProbeStash.Reset();
	m_pushProbeStashFrame = INDEX_NONE;
}

void FSimulationManagerAsyncCallback::discardPushProbeStash(const TCHAR* reason)
{
	UE_LOG(LogOGResimProbe, Verbose,
		TEXT("[ResimProbe.PushVerdict] pushFrame=%d readFrame=-1 simTick=%u verdict=UNREAD ")
		TEXT("bodies=%d compared=0 inert=0 nonInert=0 nonInertMatched=0 ")
		TEXT("nonInertMismatched=0 unresolved=%d moved=0 skipped=%d eps=%.6f reason=%s"),
		m_pushProbeStashFrame, m_pushProbeStashSimTick,
		m_pushProbeStashBodies, m_pushProbeStashUnresolved, m_pushProbeStashSkipped,
		kDefaultSimilarityEpsilon, reason);

	m_pushProbeStash.Reset();
	m_pushProbeStashFrame = INDEX_NONE;
}

void ChaosTickBackendUImpl::InjectInputs_External(int32 PhysicsStep, int32 NumSteps)
{
	FSimulationInput2* asyncInput = m_asyncCallback->GetProducerInputData_External();
	asyncInput->Reset();
	asyncInput->bInitialized = true;
	asyncInput->m_world = m_owner.GetWorld();
	asyncInput->m_manager = this;

	// ⛔G-07  docs/ChaosTickBackendUImpl-guards.md
	const int32 firstUpcomingSimTick =
		static_cast<int32>(m_chaosTickMapper.toSimulationTick(static_cast<int32_t>(PhysicsStep))) + 1;

	m_owner.onFrameStepsDue_GameThread(firstUpcomingSimTick, NumSteps);

	if (m_owner.m_receptionCoordinator.has_value())
	{
		m_owner.releaseDelayedInputsForStep(firstUpcomingSimTick, NumSteps);
		m_owner.m_receptionCoordinator->reapConnections(firstUpcomingSimTick);
	}
}

const ServerTickClock& ChaosTickBackendUImpl::getServerClock() const
{
	return m_owner.getServerClock();
}

const ClientPredictionClock& ChaosTickBackendUImpl::getClientClock() const
{
	return m_owner.getClientClock();
}

bool ChaosTickBackendUImpl::runsPrediction() const
{
	return m_owner.runsPrediction();
}

SimulationObjectStorage<SimulatableBrawler>& ChaosTickBackendUImpl::editStorage()
{
	return m_owner.editStorage();
}

void ChaosTickBackendUImpl::stampLatencyAfterStep_Internal(double stepStartSeconds)
{
	m_owner.stampLatencyAfterStep_Internal(stepStartSeconds);
}

void ChaosTickBackendUImpl::stampLatencyStepEnd_Internal()
{
	m_owner.stampLatencyStepEnd_Internal();
}

void ChaosTickBackendUImpl::onGameSimulation(const SimulationUpdateInfo& info)
{
	m_owner.m_manager->onGameSimulation(info);
}

void ChaosTickBackendUImpl::onPostGameSimulation(const SimulationUpdateInfo& info)
{
	m_owner.m_manager->onPostGameSimulation(info);
}

unsigned int ChaosTickBackendUImpl::onCheckIsSimilar()
{
	return m_owner.m_manager->onCheckIsSimilar();
}

void ChaosTickBackendUImpl::prepareResimulation(int32_t chaosStep, uint32_t simTick)
{
	m_owner.m_manager->prepareResimulation(chaosStep, simTick);
}

void ChaosTickBackendUImpl::noteResimRequest(unsigned int anchorTick, int32 lastCompletedStep, int32 requestedChaosFrame)
{
	if (!m_owner.m_manager.has_value() || !m_owner.m_manager->runsPrediction())
		return;
	m_owner.m_manager->editResimGateProbe().noteRequest(
		static_cast<std::uint32_t>(anchorTick), lastCompletedStep, requestedChaosFrame);
}

void ChaosTickBackendUImpl::noteResimGrant(int32 grantedChaosFrame)
{
	if (!m_owner.m_manager.has_value() || !m_owner.m_manager->runsPrediction())
		return;
	m_owner.m_manager->editResimGateProbe().noteGrant(grantedChaosFrame);
}

void ChaosTickBackendUImpl::buildWorld(UWorld& world, float dt, bool)
{
	FPhysScene* physScene = world.GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	checkf(static_cast<float>(solver->GetAsyncDeltaTime()) == dt,
		TEXT("SimulationManagerUImpl: the Chaos solver's async dt (%f s) differs from ")
		TEXT("UPhysicsSettings::AsyncFixedTimeStepSize (%f s), which the manager now steps with. Was ")
		TEXT("design D13: the two are the same number."),
		static_cast<float>(solver->GetAsyncDeltaTime()), dt);

	Chaos::FPBDRigidsSolver& rigidsSolver = solver->CastChecked();
	m_owner.m_physAdapter.emplace(rigidsSolver);
	m_owner.m_physReaderAdapter.emplace(rigidsSolver);
	m_owner.m_queryAdapter.emplace(&world, BrawlerChaosBindingUImpl::categoryChannels());
}

void ChaosTickBackendUImpl::startStepping(UWorld& world, float, bool)
{
	FPhysScene* physScene = world.GetPhysicsScene();
	if (physScene == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	Chaos::FPhysicsSolver* solver = physScene->GetSolver();
	if (solver == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	m_hysScenePostTickCallbackHandle = physScene->OnPhysScenePostTick.AddWeakLambda(
		&m_owner, [this](FChaosScene*) { m_owner.OnPostPhysicsStep(); });

	m_asyncCallback = solver->CreateAndRegisterSimCallbackObject_External<FSimulationManagerAsyncCallback>();
	m_asyncCallback->setManager(this);

	FNetworkPhysicsCallback* solverCallback = static_cast<FNetworkPhysicsCallback*>(solver->GetRewindCallback());
	if (solverCallback == nullptr)
		checkf(false, TEXT("SimulationManagerUImpl: unexpected state"));

	m_injectInputsExternalCallbackHandle = solverCallback->InjectInputsExternal.AddWeakLambda(
		&m_owner, [this](int32 PhysicsStep, int32 NumSteps) { InjectInputs_External(PhysicsStep, NumSteps); });
}

void ChaosTickBackendUImpl::onEndPlayWorld(UWorld& world)
{
	if (FPhysScene* PhysScene = world.GetPhysicsScene())
	{
		if (Chaos::FPhysicsSolver* Solver = PhysScene->GetSolver())
		{
			if (m_injectInputsExternalCallbackHandle.IsValid())
			{
				if (FNetworkPhysicsCallback* SolverCallback = static_cast<FNetworkPhysicsCallback*>(Solver->GetRewindCallback()))
				{
					SolverCallback->InjectInputsExternal.Remove(m_injectInputsExternalCallbackHandle);
				}
			}

			if (m_hysScenePostTickCallbackHandle.IsValid())
			{
				PhysScene->OnPhysScenePostTick.Remove(m_hysScenePostTickCallbackHandle);
			}

			if (m_asyncCallback)
			{
				Solver->UnregisterAndFreeSimCallbackObject_External(m_asyncCallback);
			}
		}
	}
}

void ChaosTickBackendUImpl::updateVisualization_GameThread()
{
	updateVisualizationAll(m_owner.m_storage);
}

ChaosEngineTraitsUImpl::VizQuery& ChaosTickBackendUImpl::editVizQuery()
{
	return m_owner.m_queryAdapter.value();
}

const ChaosEngineTraitsUImpl::VizReader& ChaosTickBackendUImpl::getVizReader() const
{
	return m_owner.m_physReaderAdapter.value();
}

std::optional<QueryVolumeId> ChaosTickBackendUImpl::registerVizVolume(const QueryVolumeDescriptor& descriptor, AActor& owner)
{
	FCollisionQueryParams queryParams;
	queryParams.bTraceComplex = false;
	queryParams.AddIgnoredActor(&owner);
	return m_owner.m_queryAdapter->registerVolume(descriptor, queryParams, FActorInstanceHandle(&owner));
}

bool ChaosTickBackendUImpl::createBodies_GameThread(USimmableUpdateComponent& owner, AOGBrawlerUECharacter& character,
	SimulatableBrawler& simulatable, unsigned int id)
{
	FBodyInstanceAsyncPhysicsTickHandle parentHandle =
		character.GetCapsuleComponent()->GetBodyInstanceAsyncPhysicsTickHandle();
	const BodyId parentBodyId = m_owner.m_physAdapter->getBodyId(parentHandle);

	AActor* ownerActor = owner.GetOwner();
	UPrimitiveComponent* attachParent = character.GetCapsuleComponent();
	const simulatableBrawler::StaticData& staticData = owner.getStaticData();

	ChaosPhysicsFactory factory(*m_owner.m_physAdapter, *m_owner.m_queryAdapter, ownerActor, attachParent);

	simulatable.editPhysicsComposite().forEach([&](auto& decl)
	{
		using D = std::decay_t<decltype(decl)>;
		const auto& subStaticData = D::staticDataOf(staticData);
		auto r = factory.createPhysicalObject(D::descriptor(), D::name);
		decl.bindings.ownBodyId        = r.bodyId;
		decl.bindings.parentBodyId     = parentBodyId;
		decl.bindings.attachmentOffset = D::attachmentOffset(subStaticData);
		decl.bindings.shapeIds         = std::move(r.shapeIds);
		FCollisionQueryParams qp;
		qp.AddIgnoredActor(ownerActor);
		for (const auto& volDesc : D::queryVolumes(subStaticData))
			decl.bindings.queryVolumeIds.push_back(
				m_owner.m_queryAdapter->registerVolume(volDesc, qp, FActorInstanceHandle(ownerActor)));
	});
	return true;
}

void ChaosTickBackendUImpl::sampleLatencyWindow_GameThread(bool isClient)
{
	if (isClient)
	{
		FChaosScene* scene = m_owner.GetWorld() != nullptr ? m_owner.GetWorld()->GetPhysicsScene() : nullptr;
		if (scene != nullptr)
			m_chaosDilationWindow.add(static_cast<double>(scene->GetNetworkDeltaTimeScale()));

		const UWorld* world = m_owner.GetWorld();
		const APlayerController* localController =
			world != nullptr ? world->GetFirstPlayerController() : nullptr;
		if (localController != nullptr && localController->GetNetworkPhysicsTickOffsetAssigned())
		{
			const int32 offset = localController->GetNetworkPhysicsTickOffset();
			if (!m_lastPhysicsTickOffset.has_value() || *m_lastPhysicsTickOffset != offset)
			{
				m_lastPhysicsTickOffset = offset;
				++m_physicsTickOffsetResets;
			}
		}
	}
}

void ChaosTickBackendUImpl::logLatencyWindow_GameThread(bool isClient)
{
	if (isClient)
	{
		UE_LOG(LogOGChaosDilation, Warning,
			TEXT("[ChaosDilation] role=Client min=%.4f mean=%.4f max=%.4f samples=%u resets=%u ")
			TEXT("offset=%d skips=%u stalls=%u hardResyncs=%u"),
			m_chaosDilationWindow.min, m_chaosDilationWindow.mean(), m_chaosDilationWindow.max,
			m_chaosDilationWindow.samples, m_physicsTickOffsetResets,
			m_lastPhysicsTickOffset.value_or(0),
			m_owner.m_latencyClockLatest.skips - m_owner.m_latencyClockAtWindowStart.skips,
			m_owner.m_latencyClockLatest.stalls - m_owner.m_latencyClockAtWindowStart.stalls,
			m_owner.m_latencyClockLatest.hardResyncs - m_owner.m_latencyClockAtWindowStart.hardResyncs);
		m_chaosDilationWindow.reset();
		m_physicsTickOffsetResets   = 0u;
		m_owner.m_latencyClockAtWindowStart = m_owner.m_latencyClockLatest;
	}
}

OGSIM_OPTIMIZE_ON

#endif
