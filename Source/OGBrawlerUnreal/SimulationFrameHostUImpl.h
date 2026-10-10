// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationFrameHostUImpl-rationale.md · docs/SimulationFrameHostUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineBaseTypes.h"
#include "Async/TaskGraphInterfaces.h"

#include "OGSimulation/Mailbox.h"
#include "OGSimulation/SimulationScheduler.h"
#include "OGSimulation/PhysicsWorldAdapter.h"
#include "OGSimulation/StepHooks.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <vector>

class ASimulationManagerUImpl;
class SimulationFrameHostUImpl;
class ULevel;

struct OccupancyCommandUImpl
{
	uint32_t slot     = 0;
	bool     occupied = false;
};

struct StepCostSampleUImpl
{
	double   preSeconds    = 0.0;
	double   stepSeconds   = 0.0;
	uint32_t replayedTicks = 0u;
	bool     resimRefused  = false;
};

struct FSimulationStartStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	SimulationFrameHostUImpl* host = nullptr;
};

struct FSimulationEndStepsTickFunctionUImpl : public FTickFunction
{
	virtual void ExecuteTick(float DeltaTime, ELevelTick TickType, ENamedThreads::Type CurrentThread,
		const FGraphEventRef& MyCompletionGraphEvent) override;
	virtual FString DiagnosticMessage() override;
	virtual FName DiagnosticContext(bool bDetailed) override;

	SimulationFrameHostUImpl* host = nullptr;
};

class SimulationFrameHostUImpl
{
public:
	static constexpr uint32_t kMaxStepsPerFrame = 240u;
	static constexpr size_t   kOccupancyCommandCapacity = 64u;
	static constexpr size_t   kStepCostSampleCapacity   = 256u;
	static constexpr double   kWindowSeconds            = 10.0;
	static constexpr uint32_t kMaxWindowSamples         = 16384u;

	struct Config
	{
		double dtSeconds   = 0.0;
		bool   isAuthority = false;
		bool   stepsInline = true;
	};

	SimulationFrameHostUImpl() = default;
	SimulationFrameHostUImpl(const SimulationFrameHostUImpl&) = delete;
	SimulationFrameHostUImpl& operator=(const SimulationFrameHostUImpl&) = delete;

	void begin(ASimulationManagerUImpl& owner, ULevel& level, const Config& config);
	void unregisterTickFunctions();
	void waitForOutstandingSteps();

	void pushOccupancy_GameThread(uint32_t slot, bool occupied);
	void noteWorldMutexWait_GameThread(double waitSeconds);
	void noteShadowRestore_GameThread(double restoreSeconds, bool refused);

	bool isRunningBatchOnThisThread() const;
	bool isStepRunningOnThisThread() const;

	double hostNowSeconds_GameThread() const;
	double stepIntervalSeconds_GameThread();

	void applyOccupancyCommands_Step();
	void releaseDelayedInputsForStep(SimTick authorityTick, uint32_t numSteps);
	void stampLatencyAfterStep_Step(double stepStartSeconds);
	void stampLatencyStepEnd_Step();
	void publishTickOffset_Step(int64_t offset);
	void publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds);
	void publishShadowSlot_Step(const TickOutcome& outcome);
	void pushStepCost_Step(const StepCostSampleUImpl& sample);

	void startSteps_GameThread(float deltaSeconds);
	void endSteps_GameThread(const FGraphEventRef& completion);

private:
	struct StepBatch
	{
		uint64_t first = 0u;
		uint32_t count = 0u;
		std::array<double, kMaxStepsPerFrame> deadlines{};
	};

	struct WindowSamplesUImpl
	{
		std::vector<float> values;
		uint32_t           overflow = 0u;

		void add(double value);
	};

	struct HostWindowCountsUImpl
	{
		double   startSeconds     = 0.0;
		uint32_t frames           = 0u;
		uint64_t steps            = 0u;
		uint32_t maxStepsPerFrame = 0u;
		uint32_t endWaits         = 0u;
		uint32_t j2Waits          = 0u;
		uint32_t replays          = 0u;
		uint64_t replayedTicks    = 0u;
		uint32_t maxReplayDepth   = 0u;
		uint32_t resimRefused     = 0u;
		uint32_t shadowRefused    = 0u;
		LostTime lostAtStart;
		uint64_t ignoredAtStart   = 0u;
		uint64_t j1DropsAtStart   = 0u;
		uint64_t j6DropsAtStart   = 0u;
	};

	struct HostWindowSamplesUImpl
	{
		WindowSamplesUImpl frameMs;
		WindowSamplesUImpl startPhysicsUs;
		WindowSamplesUImpl endWaitUs;
		WindowSamplesUImpl postPhysicsUs;
		WindowSamplesUImpl j2WaitUs;
		WindowSamplesUImpl stepUs;
		WindowSamplesUImpl preUs;
		WindowSamplesUImpl shadowRestoreUs;

		std::array<WindowSamplesUImpl*, 8> all();
	};

	SimulationScheduler& schedulerOnOwningThread();
	uint32_t pumpAndRunSteps_GameThread(float deltaSeconds);
	void runBatch(const StepBatch& batch);
	void runPostPhysicsStep_GameThread(std::optional<double> waitStartSeconds);
	void drainStepCosts_GameThread();
	void startWindow_GameThread(double nowSeconds);
	void logWindow_GameThread(double nowSeconds);

	ASimulationManagerUImpl*               m_owner       = nullptr;
	bool                                   m_isAuthority = false;
	bool                                   m_stepsInline = true;
	bool                                   m_tickFunctionsRegistered = false;
	bool                                   m_loggedFirstStep = false;
	double                                 m_hostTimeSeconds = 0.0;
	double                                 m_lastFrameStartSeconds = 0.0;
	std::optional<SimulationScheduler>     m_scheduler;
	SpscRing<OccupancyCommandUImpl, kOccupancyCommandCapacity> m_occupancyCommands;
	SpscRing<StepCostSampleUImpl, kStepCostSampleCapacity>     m_stepCosts;
	std::atomic<int64_t>                   m_tickOffset{0};
	HostWindowCountsUImpl                  m_windowCounts;
	HostWindowSamplesUImpl                 m_windowSamples;
	FGraphEventRef                         m_pendingSteps;
	FGraphEventRef                         m_blockingSteps;
	FSimulationStartStepsTickFunctionUImpl m_startTick;
	FSimulationEndStepsTickFunctionUImpl   m_endTick;
};
