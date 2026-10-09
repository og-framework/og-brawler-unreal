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

class ASimulationManagerUImpl;
class SimulationFrameHostUImpl;
class ULevel;

struct OccupancyCommandUImpl
{
	uint32_t slot     = 0;
	bool     occupied = false;
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

	void applyOccupancyCommands_Step();
	void releaseDelayedInputsForStep(SimTick authorityTick, uint32_t numSteps);
	void stampLatencyAfterStep_Step(double stepStartSeconds);
	void stampLatencyStepEnd_Step();
	void publishTickOffset_Step(int64_t offset);
	void publishRenderSnapshot_Step(const TickOutcome& outcome, double stepDeadlineSeconds);
	void publishShadowSlot_Step(const TickOutcome& outcome);

	void startSteps_GameThread(float deltaSeconds);
	void endSteps_GameThread(const FGraphEventRef& completion);

private:
	struct StepBatch
	{
		uint64_t first = 0u;
		uint32_t count = 0u;
		std::array<double, kMaxStepsPerFrame> deadlines{};
	};

	void runBatch(const StepBatch& batch);

	ASimulationManagerUImpl*               m_owner       = nullptr;
	bool                                   m_isAuthority = false;
	bool                                   m_stepsInline = true;
	bool                                   m_loggedFirstStep = false;
	double                                 m_hostTimeSeconds = 0.0;
	std::optional<SimulationScheduler>     m_scheduler;
	SpscRing<OccupancyCommandUImpl, kOccupancyCommandCapacity> m_occupancyCommands;
	std::atomic<int64_t>                   m_tickOffset{0};
	FGraphEventRef                         m_pendingSteps;
	FGraphEventRef                         m_blockingSteps;
	FSimulationStartStepsTickFunctionUImpl m_startTick;
	FSimulationEndStepsTickFunctionUImpl   m_endTick;
};
