// SPDX-License-Identifier: MPL-2.0
// docs/UETickDriverHost-rationale.md · docs/UETickDriverHost-guards.md

#pragma once

#include "OGSimulationUnreal.h"
#include "Engine/EngineBaseTypes.h"
#include "Async/TaskGraphInterfaces.h"
#include "UObject/WeakObjectPtrTemplates.h"

#include "OGSimulation/TickDriverConcepts.h"
#include "OGSimulation/TickDriverLog.h"

#include <cstdint>
#include <utility>

class ULevel;
class UObject;

class OGSIMULATIONUNREAL_API SimulationEngineUImpl
{
public:
	using Handle     = FGraphEventRef;
	using Completion = const FGraphEventRef;

	static constexpr bool kHostDiagnostics = !UE_BUILD_SHIPPING;

	SimulationEngineUImpl() = default;
	SimulationEngineUImpl(const SimulationEngineUImpl&) = delete;
	SimulationEngineUImpl& operator=(const SimulationEngineUImpl&) = delete;

	double nowSeconds() const;
	uint64_t frameNumber() const;
	bool logEnabled(HostLogLevel level) const;
	void log(HostLogLevel level, const char* line);

	bool isPending(const Handle& steps) const { return steps.IsValid() && !steps->IsComplete(); }
	void waitFor(const Handle& steps);

	template <typename F>
	Handle dispatchAfter(const Handle& previous, F&& work)
	{
		FGraphEventArray prerequisites;
		if (previous.IsValid())
			prerequisites.Add(previous);
		return FFunctionGraphTask::CreateAndDispatchWhenReady(std::forward<F>(work), TStatId{}, &prerequisites,
			stepTaskPriority());
	}

	template <typename F>
	bool deferUntil(Completion& completion, const Handle& steps, F&& continuation)
	{
		if (!completion.IsValid())
			return false;

		FGraphEventArray prerequisites;
		prerequisites.Add(steps);
		completion->DontCompleteUntil(FFunctionGraphTask::CreateAndDispatchWhenReady(
			[owner = m_owner, continuation = std::forward<F>(continuation)]() mutable
			{
				if (owner.Get() == nullptr)
					return;
				continuation();
			},
			TStatId{}, &prerequisites, ENamedThreads::GameThread));
		return true;
	}

	void setOwner(UObject& owner) { m_owner = &owner; }

private:
	static ENamedThreads::Type stepTaskPriority();

	TWeakObjectPtr<UObject> m_owner;
};

static_assert(TickDriverEngine<SimulationEngineUImpl>,
	"SimulationEngineUImpl must model og-simulation's TickDriverEngine: the clock, the frame number, the log sink, a "
	"constant kHostDiagnostics and the task-graph executor half.");

struct OGSIMULATIONUNREAL_API FSimulationStartStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	void* target = nullptr;
	void (*startFrame)(void* target, double frameDeltaSeconds) = nullptr;
};

struct OGSIMULATIONUNREAL_API FSimulationEndStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	void* target = nullptr;
	void (*endFrame)(void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode) = nullptr;
};

class OGSIMULATIONUNREAL_API UETickDriverHost
{
public:
	using StartFrameFn = void (*)(void* target, double frameDeltaSeconds);
	using EndFrameFn   = void (*)(void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode);

	UETickDriverHost() = default;
	UETickDriverHost(const UETickDriverHost&) = delete;
	UETickDriverHost& operator=(const UETickDriverHost&) = delete;

	static int32 requestedMaxCatchUpSteps();
	static bool  runInline();

	SimulationEngineUImpl& engine() { return m_engine; }

	template <typename DriverT>
	void begin(UObject& owner, ULevel& level, DriverT& driver)
	{
		registerTickFunctions(owner, level, &driver,
			[](void* target, double frameDeltaSeconds) { static_cast<DriverT*>(target)->startFrame(frameDeltaSeconds); },
			[](void* target, SimulationEngineUImpl::Completion* completion, int32 blockMode)
			{
				static_cast<DriverT*>(target)->endFrame(completion, blockMode);
			});
	}

	void unregisterTickFunctions();

private:
	void registerTickFunctions(UObject& owner, ULevel& level, void* target, StartFrameFn startFrame, EndFrameFn endFrame);

	SimulationEngineUImpl                  m_engine;
	UObject*                               m_owner = nullptr;
	FSimulationStartStepsTickFunctionUImpl m_startTick;
	FSimulationEndStepsTickFunctionUImpl   m_endTick;
};
