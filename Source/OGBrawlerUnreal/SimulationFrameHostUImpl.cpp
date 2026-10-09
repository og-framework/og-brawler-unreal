// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationFrameHostUImpl-rationale.md · docs/SimulationFrameHostUImpl-guards.md

#if !OG_PHYSICS_BACKEND_CHAOS
#include "SimulationFrameHostUImpl.h"

#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "Engine/Level.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/WeakObjectPtrTemplates.h"

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
	m_scheduler.emplace(SchedulerConfig{ config.dtSeconds, MaxCatchUpSteps{ cap }, 0.75, 1.25 }, m_hostTimeSeconds);

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

void SimulationFrameHostUImpl::applyOccupancyCommands_Step()
{
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

void SimulationFrameHostUImpl::startSteps_GameThread(float deltaSeconds)
{
	m_blockingSteps = m_pendingSteps;

	m_hostTimeSeconds += static_cast<double>(deltaSeconds);
	// ⛔G-04  docs/SimulationFrameHostUImpl-guards.md
	const uint32 numSteps = m_scheduler->pump(m_hostTimeSeconds);
	if (numSteps == 0u)
		return;

	StepBatch batch;
	batch.first = m_scheduler->physicsStepCount() - numSteps;
	batch.count = numSteps;
	for (uint32 index = 0u; index < numSteps; ++index)
		batch.deadlines[index] = m_scheduler->stepDeadline(batch.first + index);

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
		return;
	}

	FGraphEventArray prerequisites;
	if (m_pendingSteps.IsValid() && !m_pendingSteps->IsComplete())
		prerequisites.Add(m_pendingSteps);

	m_pendingSteps = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[this, batch]() { runBatch(batch); },
		TStatId{}, &prerequisites, CPrio_OGSimStepTask.Get());
}

void SimulationFrameHostUImpl::endSteps_GameThread(const FGraphEventRef& completion)
{
	checkf(!m_owner->isJoltWorldMutexHeldOnThisThread(),
		TEXT("SimulationFrameHostUImpl::endSteps_GameThread: the game thread holds the Jolt world mutex (J2) at end of ")
		TEXT("physics, where it waits for step tasks that take it: a deadlock. Was frame-host G-05."));

	const FGraphEventRef waitFor = CVarSimBlockMode.GetValueOnGameThread() == 1 ? m_pendingSteps : m_blockingSteps;
	if (!waitFor.IsValid() || waitFor->IsComplete())
	{
		m_owner->OnPostPhysicsStep();
		return;
	}

	if (!completion.IsValid())
	{
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(waitFor, ENamedThreads::GameThread_Local);
		m_owner->OnPostPhysicsStep();
		return;
	}

	FGraphEventArray prerequisites;
	prerequisites.Add(waitFor);
	TWeakObjectPtr<ASimulationManagerUImpl> owner(m_owner);
	completion->DontCompleteUntil(FFunctionGraphTask::CreateAndDispatchWhenReady(
		[owner]()
		{
			if (ASimulationManagerUImpl* manager = owner.Get())
				manager->OnPostPhysicsStep();
		},
		TStatId{}, &prerequisites, ENamedThreads::GameThread));
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

	for (uint32 index = 0u; index < batch.count; ++index)
		m_owner->runJoltStep_Step(batch.first + index, batch.deadlines[index]);
}

#endif
