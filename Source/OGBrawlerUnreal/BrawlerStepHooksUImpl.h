// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerStepHooksUImpl-rationale.md · docs/BrawlerStepHooksUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"

#include "OGSimulation/StepHooks.h"
#include "OGBrawlerUnreal/SimulationFrameHostUImpl.h"

#include <cstdint>

class BrawlerStepHooksUImpl
{
public:
	explicit BrawlerStepHooksUImpl(SimulationFrameHostUImpl& host) : m_host(host) {}

	BrawlerStepHooksUImpl(const BrawlerStepHooksUImpl&) = delete;
	BrawlerStepHooksUImpl& operator=(const BrawlerStepHooksUImpl&) = delete;

	void beforeTick(const UpcomingTick& upcoming)
	{
		m_tickStartSeconds = FPlatformTime::Seconds();
		checkInStep(TEXT("beforeTick"));
		m_host.applyOccupancyCommands_Step();
		if (!upcoming.authority)
			return;

		checkf(IsInGameThread(),
			TEXT("BrawlerStepHooksUImpl::beforeTick: an authority step ran off the game thread. The authority's input ")
			TEXT("release delivers into UObjects (USimmableUpdateComponent), so the authority always steps inline on the ")
			TEXT("game thread. Was design D2 (task 18)."));
		checkf(upcoming.authorityTick.has_value(),
			TEXT("BrawlerStepHooksUImpl::beforeTick: an authority UpcomingTick carries no authorityTick."));
		// ⛔G-01  docs/BrawlerStepHooksUImpl-guards.md
		m_host.releaseDelayedInputsForStep(*upcoming.authorityTick, 1u);
	}

	void beforeSimulate(const UpcomingTick&)
	{
		checkInStep(TEXT("beforeSimulate"));
		// ⛔G-03  docs/BrawlerStepHooksUImpl-guards.md
		m_stepStartSeconds = FPlatformTime::Seconds();
	}

	void beforePhysics(SimTick)
	{
		checkInStep(TEXT("beforePhysics"));
		m_host.stampLatencyAfterStep_Step(m_stepStartSeconds);
	}

	void afterTick(const TickOutcome& outcome)
	{
		const double tickEndSeconds = FPlatformTime::Seconds();
		checkInStep(TEXT("afterTick"));
		m_host.stampLatencyStepEnd_Step();
		m_host.publishRenderSnapshot_Step(outcome, currentStepDeadline);
		m_host.publishShadowSlot_Step(outcome);
		// ⛔G-02  docs/BrawlerStepHooksUImpl-guards.md
		m_host.publishTickOffset_Step(static_cast<int64_t>(outcome.tick) - static_cast<int64_t>(outcome.physicsStep));
		m_host.pushStepCost_Step(StepCostSampleUImpl{ m_stepStartSeconds - m_tickStartSeconds,
			tickEndSeconds - m_stepStartSeconds, outcome.replayedTicks, outcome.resimRefused });
	}

	double currentStepDeadline = 0.0;

private:
	void checkInStep(const TCHAR* hook) const
	{
		checkf(m_host.isStepRunningOnThisThread(),
			TEXT("BrawlerStepHooksUImpl::%s: called outside a step of this manager. The hooks reach the step driver, ")
			TEXT("the world and the step-owned mailboxes, which only the step (runJoltStep_Step, under the world mutex ")
			TEXT("and the step flag) may touch; from anywhere else they race the running step."),
			hook);
	}

	SimulationFrameHostUImpl& m_host;
	double                    m_tickStartSeconds = 0.0;
	double                    m_stepStartSeconds = 0.0;
};

static_assert(StepHooks<BrawlerStepHooksUImpl>,
	"BrawlerStepHooksUImpl must satisfy og-simulation's StepHooks concept: all four hooks, each returning void, "
	"afterTick reading a const outcome.");
