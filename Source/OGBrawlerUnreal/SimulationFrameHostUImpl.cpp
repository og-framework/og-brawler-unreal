// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationFrameHostUImpl-rationale.md · docs/SimulationFrameHostUImpl-guards.md

#if !OG_PHYSICS_BACKEND_CHAOS
#include "SimulationFrameHostUImpl.h"

#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "Engine/Level.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "OGSimulation/LatencyBudgetProbe.h"

#include <algorithm>

namespace
{
	TAutoConsoleVariable<int32> CVarSimMaxCatchUpSteps(
		TEXT("og.Sim.MaxCatchUpSteps"),
		60,
		TEXT("Jolt configuration only. The most physics steps one frame may run; the scheduler drops the rest of a ")
		TEXT("longer frame and counts it as lost time. Read once, at the simulation manager's BeginPlay. 60 is ")
		TEXT("Chaos's one-second clamp at 60 Hz, on both roles."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarSimBlockMode(
		TEXT("og.Sim.BlockMode"),
		0,
		TEXT("Jolt configuration only, for debugging. 0 = end of physics waits only for the step tasks of earlier ")
		TEXT("frames (Chaos block mode 0). 1 = it also waits for this frame's step task."),
		ECVF_Default);

	FAutoConsoleTaskPriority CPrio_OGSimStepTask(
		TEXT("TaskGraph.TaskPriorities.OGSimStepTask"),
		TEXT("Task and thread priority for the client's Jolt step task (mirrors TaskGraph.TaskPriorities.PhysicsTickTask)"),
		ENamedThreads::HighThreadPriority,
		ENamedThreads::NormalTaskPriority,
		ENamedThreads::HighTaskPriority);

	thread_local const SimulationFrameHostUImpl* t_batchHost = nullptr;

	class BatchScopeUImpl
	{
	public:
		explicit BatchScopeUImpl(const SimulationFrameHostUImpl& host)
			: m_previous(t_batchHost)
		{
			t_batchHost = &host;
		}

		~BatchScopeUImpl()
		{
			t_batchHost = m_previous;
		}

		BatchScopeUImpl(const BatchScopeUImpl&) = delete;
		BatchScopeUImpl& operator=(const BatchScopeUImpl&) = delete;

	private:
		const SimulationFrameHostUImpl* m_previous;
	};

	struct PercentilesUImpl
	{
		uint32 n   = 0u;
		double p50 = 0.0;
		double p99 = 0.0;
		double max = 0.0;
	};

	PercentilesUImpl percentilesOf(std::vector<float>& values)
	{
		PercentilesUImpl result;
		result.n = static_cast<uint32>(values.size());
		if (result.n == 0u)
			return result;

		std::sort(values.begin(), values.end());
		result.p50 = values[latencyBudget::nearestRankIndex(result.n, 500u)];
		result.p99 = values[latencyBudget::nearestRankIndex(result.n, 990u)];
		result.max = values.back();
		return result;
	}

	FString formatPercentiles(std::vector<float>& values, bool twoDecimals)
	{
		const PercentilesUImpl p = percentilesOf(values);
		if (p.n == 0u)
			return TEXT("-");
		return twoDecimals ? FString::Printf(TEXT("%.2f/%.2f/%.2f"), p.p50, p.p99, p.max)
		                   : FString::Printf(TEXT("%.1f/%.1f/%.1f"), p.p50, p.p99, p.max);
	}
}

void SimulationFrameHostUImpl::WindowSamplesUImpl::add(double value)
{
	if (values.size() < kMaxWindowSamples)
		values.push_back(static_cast<float>(value));
	else
		++overflow;
}

std::array<SimulationFrameHostUImpl::WindowSamplesUImpl*, 8> SimulationFrameHostUImpl::HostWindowSamplesUImpl::all()
{
	return { &frameMs, &startPhysicsUs, &endWaitUs, &postPhysicsUs, &j2WaitUs, &stepUs, &preUs, &shadowRestoreUs };
}

void FSimulationStartStepsTickFunctionUImpl::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(host != nullptr);
	host->startSteps_GameThread(DeltaTime);
}

FString FSimulationStartStepsTickFunctionUImpl::DiagnosticMessage()
{
	return TEXT("FSimulationStartStepsTickFunctionUImpl");
}

FName FSimulationStartStepsTickFunctionUImpl::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("OGSimStartSteps"));
}

void FSimulationEndStepsTickFunctionUImpl::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(host != nullptr);
	host->endSteps_GameThread(MyCompletionGraphEvent);
}

FString FSimulationEndStepsTickFunctionUImpl::DiagnosticMessage()
{
	return TEXT("FSimulationEndStepsTickFunctionUImpl");
}

FName FSimulationEndStepsTickFunctionUImpl::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("OGSimEndSteps"));
}

void SimulationFrameHostUImpl::begin(ASimulationManagerUImpl& owner, ULevel& level, const Config& config)
{
	checkf(m_owner == nullptr, TEXT("SimulationFrameHostUImpl::begin: the frame host is already running."));
	checkf(!config.isAuthority || config.stepsInline,
		TEXT("SimulationFrameHostUImpl::begin: the authority must step inline on the game thread. Its input release ")
		TEXT("delivers into UObjects. Was design D2 (task 18)."));

	m_owner       = &owner;
	m_isAuthority = config.isAuthority;
	m_stepsInline = config.stepsInline;

	const int32 requestedCap = CVarSimMaxCatchUpSteps.GetValueOnGameThread();
	const uint32 cap = static_cast<uint32>(std::clamp<int32>(requestedCap, 1, static_cast<int32>(kMaxStepsPerFrame)));
	if (static_cast<int32>(cap) != requestedCap)
	{
		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.Frame] og.Sim.MaxCatchUpSteps=%d is outside [1, %u]; using %u"),
			requestedCap, kMaxStepsPerFrame, cap);
	}

	m_hostTimeSeconds = 0.0;
	m_lastFrameStartSeconds = 0.0;
	m_scheduler.emplace(SchedulerConfig{ config.dtSeconds, MaxCatchUpSteps{ cap }, 0.75, 1.25 }, m_hostTimeSeconds);
	for (WindowSamplesUImpl* samples : m_windowSamples.all())
		samples->values.reserve(kMaxWindowSamples);
	startWindow_GameThread(FPlatformTime::Seconds());

	m_startTick.host                 = this;
	m_startTick.bCanEverTick         = true;
	m_startTick.bStartWithTickEnabled = true;
	m_startTick.TickGroup            = TG_StartPhysics;
	m_startTick.EndTickGroup         = TG_StartPhysics;
	m_startTick.RegisterTickFunction(&level);

	m_endTick.host                 = this;
	m_endTick.bCanEverTick         = true;
	m_endTick.bStartWithTickEnabled = true;
	m_endTick.TickGroup            = TG_EndPhysics;
	m_endTick.EndTickGroup         = TG_EndPhysics;
	m_endTick.RegisterTickFunction(&level);
	m_endTick.AddPrerequisite(&owner, m_startTick);
	m_tickFunctionsRegistered = true;

	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Frame] role=%s stepping=%s dt=%.6f maxCatchUpSteps=%u rateScale=[0.75,1.25] timeBase=worldDelta"),
		m_isAuthority ? TEXT("Authority") : TEXT("Client"),
		m_stepsInline ? TEXT("inline") : TEXT("worker"),
		config.dtSeconds, cap);
}

void SimulationFrameHostUImpl::unregisterTickFunctions()
{
	if (m_owner == nullptr)
		return;

	m_tickFunctionsRegistered = false;
	if (m_endTick.IsTickFunctionRegistered())
	{
		m_endTick.RemovePrerequisite(m_owner, m_startTick);
		m_endTick.UnRegisterTickFunction();
	}
	if (m_startTick.IsTickFunctionRegistered())
		m_startTick.UnRegisterTickFunction();
}

void SimulationFrameHostUImpl::waitForOutstandingSteps()
{
	if (m_owner == nullptr)
		return;

	checkf(!m_owner->isJoltWorldMutexHeldOnThisThread(),
		TEXT("SimulationFrameHostUImpl::waitForOutstandingSteps: this thread holds the Jolt world mutex (J2) and is ")
		TEXT("about to wait for a step task that takes it: a deadlock. Was frame-host G-05."));

	FGraphEventArray outstanding;
	if (m_pendingSteps.IsValid())
		outstanding.Add(m_pendingSteps);
	if (m_blockingSteps.IsValid())
		outstanding.Add(m_blockingSteps);
	if (!outstanding.IsEmpty())
	{
		// ⛔G-05  docs/SimulationFrameHostUImpl-guards.md
		FTaskGraphInterface::Get().WaitUntilTasksComplete(outstanding, ENamedThreads::GameThread_Local);
	}

	m_pendingSteps  = nullptr;
	m_blockingSteps = nullptr;
}

void SimulationFrameHostUImpl::pushOccupancy_GameThread(uint32_t slot, bool occupied)
{
	const bool pushed = m_occupancyCommands.tryPush(OccupancyCommandUImpl{ slot, occupied });
	checkf(pushed,
		TEXT("SimulationFrameHostUImpl::pushOccupancy_GameThread: the occupancy ring (J1, %u entries) is full; slot %u ")
		TEXT("occupied=%d would be lost and the step world would keep the wrong body set. No step has drained it."),
		static_cast<uint32>(kOccupancyCommandCapacity), slot, occupied ? 1 : 0);
}

void SimulationFrameHostUImpl::noteWorldMutexWait_GameThread(double waitSeconds)
{
	++m_windowCounts.j2Waits;
	m_windowSamples.j2WaitUs.add(waitSeconds * 1.0e6);
}

void SimulationFrameHostUImpl::noteShadowRestore_GameThread(double restoreSeconds, bool refused)
{
	m_windowSamples.shadowRestoreUs.add(restoreSeconds * 1.0e6);
	m_windowCounts.shadowRefused += refused ? 1u : 0u;
}

bool SimulationFrameHostUImpl::isRunningBatchOnThisThread() const
{
	return t_batchHost == this;
}

bool SimulationFrameHostUImpl::isStepRunningOnThisThread() const
{
	return m_owner != nullptr && m_owner->isJoltStepRunningOnThisThread();
}

void SimulationFrameHostUImpl::applyOccupancyCommands_Step()
{
	checkf(isStepRunningOnThisThread(),
		TEXT("SimulationFrameHostUImpl::applyOccupancyCommands_Step: called outside a step of this manager. It is the ")
		TEXT("step driver's occupancy door, and only the step (runJoltStep_Step, under the world mutex) may touch the ")
		TEXT("driver: from anywhere else it races the running step, and the occupancy timeline no longer records the ")
		TEXT("body set the world stepped with."));
	while (std::optional<OccupancyCommandUImpl> command = m_occupancyCommands.tryPop())
		m_owner->m_stepDriver->noteOccupancy(command->slot, command->occupied);
}

void SimulationFrameHostUImpl::releaseDelayedInputsForStep(SimTick authorityTick, uint32_t numSteps)
{
	m_owner->releaseDelayedInputsForStep(static_cast<int32>(authorityTick), static_cast<int32>(numSteps));
}

void SimulationFrameHostUImpl::stampLatencyAfterStep_Step(double stepStartSeconds)
{
	m_owner->stampLatencyAfterStep_Internal(stepStartSeconds);
}

void SimulationFrameHostUImpl::stampLatencyStepEnd_Step()
{
	m_owner->stampLatencyStepEnd_Internal();
}

void SimulationFrameHostUImpl::publishTickOffset_Step(int64_t offset)
{
	m_tickOffset.store(offset, std::memory_order_relaxed);
}

void SimulationFrameHostUImpl::publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds)
{
	m_owner->publishRenderSnapshot_Step(outcome, stepDeadlineSeconds);
}

void SimulationFrameHostUImpl::publishShadowSlot_Step(const TickOutcome& outcome)
{
	m_owner->publishShadowSlot_Step(outcome);
}

void SimulationFrameHostUImpl::pushStepCost_Step(const StepCostSampleUImpl& sample)
{
	m_stepCosts.tryPush(sample);
}

double SimulationFrameHostUImpl::hostNowSeconds_GameThread() const
{
	checkf(IsInGameThread(),
		TEXT("SimulationFrameHostUImpl::hostNowSeconds_GameThread: the host time is the game thread's accumulated world ")
		TEXT("delta, written at TG_StartPhysics; read off the game thread it races that write."));
	return m_hostTimeSeconds;
}

double SimulationFrameHostUImpl::stepIntervalSeconds_GameThread()
{
	const SimulationScheduler& scheduler = schedulerOnOwningThread();
	return scheduler.config().dtSeconds / scheduler.rateScale();
}

SimulationScheduler& SimulationFrameHostUImpl::schedulerOnOwningThread()
{
	checkf(IsInGameThread(),
		TEXT("SimulationFrameHostUImpl: the scheduler was reached off the game thread. The game thread owns it in ")
		TEXT("every M1 stepping mode: it pumps it at TG_StartPhysics, and the step reads only the batch it was handed ")
		TEXT("by value. It has no synchronization, so a call from the step or any other thread (a rate change from an ")
		TEXT("RPC included) races the pump."));
	checkf(m_scheduler.has_value(), TEXT("SimulationFrameHostUImpl: the scheduler was reached before begin()."));
	return *m_scheduler;
}

void SimulationFrameHostUImpl::startSteps_GameThread(float deltaSeconds)
{
	const double startSeconds = FPlatformTime::Seconds();
	if (m_lastFrameStartSeconds > 0.0)
		m_windowSamples.frameMs.add((startSeconds - m_lastFrameStartSeconds) * 1000.0);
	m_lastFrameStartSeconds = startSeconds;

	const uint32 numSteps = pumpAndRunSteps_GameThread(deltaSeconds);

	++m_windowCounts.frames;
	m_windowCounts.steps += numSteps;
	m_windowCounts.maxStepsPerFrame = FMath::Max(m_windowCounts.maxStepsPerFrame, numSteps);
	m_windowSamples.startPhysicsUs.add((FPlatformTime::Seconds() - startSeconds) * 1.0e6);
}

uint32_t SimulationFrameHostUImpl::pumpAndRunSteps_GameThread(float deltaSeconds)
{
	m_blockingSteps = m_pendingSteps;

	SimulationScheduler& scheduler = schedulerOnOwningThread();
	m_hostTimeSeconds += static_cast<double>(deltaSeconds);
	// ⛔G-04  docs/SimulationFrameHostUImpl-guards.md
	const uint32 numSteps = scheduler.pump(m_hostTimeSeconds);
	if (numSteps == 0u)
		return 0u;

	StepBatch batch;
	batch.first = scheduler.physicsStepCount() - numSteps;
	batch.count = numSteps;
	for (uint32 index = 0u; index < numSteps; ++index)
		batch.deadlines[index] = scheduler.stepDeadline(batch.first + index);

	// ⛔G-03  docs/SimulationFrameHostUImpl-guards.md
	const int32 firstUpcomingSimTick =
		static_cast<int32>(static_cast<int64_t>(batch.first) + m_tickOffset.load(std::memory_order_relaxed));

	// ⛔G-02  docs/SimulationFrameHostUImpl-guards.md
	m_owner->onFrameStepsDue_GameThread(firstUpcomingSimTick, static_cast<int32>(numSteps));

	// ⛔G-01  docs/SimulationFrameHostUImpl-guards.md
	if (m_stepsInline)
	{
		runBatch(batch);
		if (ASimulationManagerUImpl::BrawlerReceptionCoordinator* coordinator = m_owner->getReceptionCoordinator())
		{
			// ⛔G-06  docs/SimulationFrameHostUImpl-guards.md
			coordinator->reapConnections(firstUpcomingSimTick);
		}
		return numSteps;
	}

	FGraphEventArray prerequisites;
	if (m_pendingSteps.IsValid() && !m_pendingSteps->IsComplete())
		prerequisites.Add(m_pendingSteps);

	m_pendingSteps = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[this, batch]() { runBatch(batch); },
		TStatId{}, &prerequisites, CPrio_OGSimStepTask.Get());
	return numSteps;
}

void SimulationFrameHostUImpl::endSteps_GameThread(const FGraphEventRef& completion)
{
	checkf(!m_owner->isJoltWorldMutexHeldOnThisThread(),
		TEXT("SimulationFrameHostUImpl::endSteps_GameThread: the game thread holds the Jolt world mutex (J2) at end of ")
		TEXT("physics, where it waits for step tasks that take it: a deadlock. Was frame-host G-05."));

	const double endStartSeconds = FPlatformTime::Seconds();
	const FGraphEventRef waitFor = CVarSimBlockMode.GetValueOnGameThread() == 1 ? m_pendingSteps : m_blockingSteps;
	if (!waitFor.IsValid() || waitFor->IsComplete())
	{
		runPostPhysicsStep_GameThread(std::nullopt);
		return;
	}

	if (!completion.IsValid())
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(waitFor, ENamedThreads::GameThread_Local);
		runPostPhysicsStep_GameThread(endStartSeconds);
		return;
	}

	FGraphEventArray prerequisites;
	prerequisites.Add(waitFor);
	TWeakObjectPtr<ASimulationManagerUImpl> owner(m_owner);
	completion->DontCompleteUntil(FFunctionGraphTask::CreateAndDispatchWhenReady(
		[owner, endStartSeconds]()
		{
			ASimulationManagerUImpl* const manager = owner.Get();
			if (manager == nullptr || !manager->m_frameHost.m_tickFunctionsRegistered)
				return;
			manager->m_frameHost.runPostPhysicsStep_GameThread(endStartSeconds);
		},
		TStatId{}, &prerequisites, ENamedThreads::GameThread));
}

void SimulationFrameHostUImpl::runPostPhysicsStep_GameThread(std::optional<double> waitStartSeconds)
{
	const double postStartSeconds = FPlatformTime::Seconds();
	if (waitStartSeconds.has_value())
	{
		++m_windowCounts.endWaits;
		m_windowSamples.endWaitUs.add((postStartSeconds - *waitStartSeconds) * 1.0e6);
	}

	m_owner->OnPostPhysicsStep();

	const double nowSeconds = FPlatformTime::Seconds();
	m_windowSamples.postPhysicsUs.add((nowSeconds - postStartSeconds) * 1.0e6);
	drainStepCosts_GameThread();
	if (nowSeconds - m_windowCounts.startSeconds >= kWindowSeconds)
		logWindow_GameThread(nowSeconds);
}

void SimulationFrameHostUImpl::drainStepCosts_GameThread()
{
	while (std::optional<StepCostSampleUImpl> sample = m_stepCosts.tryPop())
	{
		m_windowSamples.stepUs.add(sample->stepSeconds * 1.0e6);
		if (m_isAuthority || sample->replayedTicks > 0u)
			m_windowSamples.preUs.add(sample->preSeconds * 1.0e6);
		if (sample->replayedTicks > 0u)
		{
			++m_windowCounts.replays;
			m_windowCounts.replayedTicks += sample->replayedTicks;
			m_windowCounts.maxReplayDepth = FMath::Max(m_windowCounts.maxReplayDepth, sample->replayedTicks);
		}
		m_windowCounts.resimRefused += sample->resimRefused ? 1u : 0u;
	}
}

void SimulationFrameHostUImpl::startWindow_GameThread(double nowSeconds)
{
	const SimulationScheduler& scheduler = schedulerOnOwningThread();

	m_windowCounts                = HostWindowCountsUImpl{};
	m_windowCounts.startSeconds   = nowSeconds;
	m_windowCounts.lostAtStart    = scheduler.lostTime();
	m_windowCounts.ignoredAtStart = scheduler.ignoredTimeSamples();
	m_windowCounts.j1DropsAtStart = m_occupancyCommands.drops();
	m_windowCounts.j6DropsAtStart = m_stepCosts.drops();
	for (WindowSamplesUImpl* samples : m_windowSamples.all())
	{
		samples->values.clear();
		samples->overflow = 0u;
	}
}

void SimulationFrameHostUImpl::logWindow_GameThread(double nowSeconds)
{
	const SimulationScheduler& scheduler = schedulerOnOwningThread();
	const HostWindowCountsUImpl& counts = m_windowCounts;
	HostWindowSamplesUImpl& samples = m_windowSamples;

	const LostTime lost = scheduler.lostTime();
	uint32 overflow = 0u;
	for (const WindowSamplesUImpl* windowSamples : samples.all())
		overflow += windowSamples->overflow;

	const FString replayDepth = counts.replays == 0u
		? FString(TEXT("-"))
		: FString::Printf(TEXT("%.2f/%u"),
			static_cast<double>(counts.replayedTicks) / static_cast<double>(counts.replays), counts.maxReplayDepth);
	const uint32 costSamples = static_cast<uint32>(samples.stepUs.values.size());
	const uint32 preTicks    = static_cast<uint32>(samples.preUs.values.size());
	const uint32 restores    = static_cast<uint32>(samples.shadowRestoreUs.values.size());

	UE_LOG(LogOGSimHost, Warning,
		TEXT("[SimHost.Window] role=%s stepping=%s seconds=%.2f frames=%u frameMs=%s startPhysicsUs=%s endWaits=%u ")
		TEXT("endWaitUs=%s postPhysicsUs=%s steps=%llu maxStepsPerFrame=%u lostSteps=%llu lostSeconds=%.4f ")
		TEXT("ignoredTimeSamples=%llu j1Drops=%llu j2Waits=%u j2WaitUs=%s costSamples=%u stepUs=%s preTicks=%u preUs=%s ")
		TEXT("replayDepth=%s resimRefused=%u j6Drops=%llu shadowRestores=%u shadowRestoreUs=%s shadowRefused=%u ")
		TEXT("sampleOverflow=%u"),
		m_isAuthority ? TEXT("Authority") : TEXT("Client"),
		m_stepsInline ? TEXT("inline") : TEXT("worker"),
		nowSeconds - counts.startSeconds, counts.frames,
		*formatPercentiles(samples.frameMs.values, true),
		*formatPercentiles(samples.startPhysicsUs.values, false),
		counts.endWaits,
		*formatPercentiles(samples.endWaitUs.values, false),
		*formatPercentiles(samples.postPhysicsUs.values, false),
		static_cast<unsigned long long>(counts.steps), counts.maxStepsPerFrame,
		static_cast<unsigned long long>(lost.steps - counts.lostAtStart.steps),
		lost.seconds - counts.lostAtStart.seconds,
		static_cast<unsigned long long>(scheduler.ignoredTimeSamples() - counts.ignoredAtStart),
		static_cast<unsigned long long>(m_occupancyCommands.drops() - counts.j1DropsAtStart),
		counts.j2Waits,
		*formatPercentiles(samples.j2WaitUs.values, false),
		costSamples,
		*formatPercentiles(samples.stepUs.values, false),
		preTicks,
		*formatPercentiles(samples.preUs.values, false),
		*replayDepth, counts.resimRefused,
		static_cast<unsigned long long>(m_stepCosts.drops() - counts.j6DropsAtStart),
		restores,
		*formatPercentiles(samples.shadowRestoreUs.values, false),
		counts.shadowRefused, overflow);

	startWindow_GameThread(nowSeconds);
}

void SimulationFrameHostUImpl::runBatch(const StepBatch& batch)
{
	if (!m_loggedFirstStep)
	{
		m_loggedFirstStep = true;
		UE_LOG(LogOGSimHost, Warning,
			TEXT("[SimHost.FirstStep] role=%s thread=%s physicsStep=%llu batch=%u"),
			m_isAuthority ? TEXT("Authority") : TEXT("Client"),
			IsInGameThread() ? TEXT("game") : TEXT("worker"),
			static_cast<unsigned long long>(batch.first), batch.count);
	}

	const BatchScopeUImpl batchScope(*this);
	for (uint32 index = 0u; index < batch.count; ++index)
		m_owner->runJoltStep_Step(batch.first + index, batch.deadlines[index]);
}

#endif
