// SPDX-License-Identifier: MPL-2.0
// docs/UETickDriverHost-rationale.md · docs/UETickDriverHost-guards.md

#include "UETickDriverHost.h"

#include "Engine/Level.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

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

	TAutoConsoleVariable<int32> CVarSimRunInline(
		TEXT("og.Sim.RunInline"),
		0,
		TEXT("Jolt configuration only. 1 = a client steps inline on the game thread instead of on a worker task. ")
		TEXT("Read once, at the simulation manager's BeginPlay. The authority always steps inline."),
		ECVF_Default);
}

double SimulationEngineUImpl::nowSeconds() const
{
	return FPlatformTime::Seconds();
}

uint64_t SimulationEngineUImpl::frameNumber() const
{
	return GFrameCounter;
}

bool SimulationEngineUImpl::logEnabled(HostLogLevel level) const
{
	switch (level)
	{
	case HostLogLevel::Verbose:
		return UE_LOG_ACTIVE(LogOGSimHost, Verbose);
	case HostLogLevel::Log:
		return UE_LOG_ACTIVE(LogOGSimHost, Log);
	case HostLogLevel::Warning:
		return UE_LOG_ACTIVE(LogOGSimHost, Warning);
	}
	return false;
}

void SimulationEngineUImpl::log(HostLogLevel level, const char* line)
{
	switch (level)
	{
	case HostLogLevel::Verbose:
		UE_LOG(LogOGSimHost, Verbose, TEXT("%hs"), line);
		return;
	case HostLogLevel::Log:
		UE_LOG(LogOGSimHost, Log, TEXT("%hs"), line);
		return;
	case HostLogLevel::Warning:
		UE_LOG(LogOGSimHost, Warning, TEXT("%hs"), line);
		return;
	}
}

ENamedThreads::Type SimulationEngineUImpl::stepTaskPriority()
{
	return CPrio_OGSimStepTask.Get();
}

void SimulationEngineUImpl::waitFor(const Handle& steps)
{
	checkf(steps.IsValid(),
		TEXT("SimulationEngineUImpl::waitFor: the tick driver waits only for a step task it dispatched, so the event is ")
		TEXT("valid. Was the frame host's IsValid filter before its waits."));
	FTaskGraphInterface::Get().WaitUntilTaskCompletes(steps, ENamedThreads::GameThread_Local);
}

void FSimulationStartStepsTickFunctionUImpl::ExecuteTick(float DeltaTime, ELevelTick TickType,
	ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	check(startFrame != nullptr);
	startFrame(target, static_cast<double>(DeltaTime));
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
	check(endFrame != nullptr);
	endFrame(target, &MyCompletionGraphEvent, CVarSimBlockMode.GetValueOnGameThread());
}

FString FSimulationEndStepsTickFunctionUImpl::DiagnosticMessage()
{
	return TEXT("FSimulationEndStepsTickFunctionUImpl");
}

FName FSimulationEndStepsTickFunctionUImpl::DiagnosticContext(bool bDetailed)
{
	return FName(TEXT("OGSimEndSteps"));
}

int32 UETickDriverHost::requestedMaxCatchUpSteps()
{
	return CVarSimMaxCatchUpSteps.GetValueOnGameThread();
}

bool UETickDriverHost::runInline()
{
	return CVarSimRunInline.GetValueOnGameThread() != 0;
}

void UETickDriverHost::registerTickFunctions(UObject& owner, ULevel& level, void* target,
	StartFrameFn startFrame, EndFrameFn endFrame)
{
	checkf(m_owner == nullptr, TEXT("UETickDriverHost::begin: the frame host is already running."));

	m_owner = &owner;
	m_engine.setOwner(owner);

	m_startTick.target               = target;
	m_startTick.startFrame           = startFrame;
	m_startTick.bCanEverTick         = true;
	m_startTick.bStartWithTickEnabled = true;
	m_startTick.TickGroup            = TG_StartPhysics;
	m_startTick.EndTickGroup         = TG_StartPhysics;
	m_startTick.RegisterTickFunction(&level);

	m_endTick.target               = target;
	m_endTick.endFrame             = endFrame;
	m_endTick.bCanEverTick         = true;
	m_endTick.bStartWithTickEnabled = true;
	m_endTick.TickGroup            = TG_EndPhysics;
	m_endTick.EndTickGroup         = TG_EndPhysics;
	m_endTick.RegisterTickFunction(&level);
	m_endTick.AddPrerequisite(&owner, m_startTick);
}

void UETickDriverHost::unregisterTickFunctions()
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
