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
		// ⛔G-03  docs/BrawlerStepHooksUImpl-guards.md
		m_stepStartSeconds = FPlatformTime::Seconds();
	}

	void beforePhysics(SimTick)
	{
		m_host.stampLatencyAfterStep_Step(m_stepStartSeconds);
	}

	void afterTick(const TickOutcome& outcome)
	{
		m_host.stampLatencyStepEnd_Step();
		m_host.publishRenderSnapshot_Step(outcome, currentStepDeadline);
		m_host.publishShadowSlot_Step(outcome);
		// ⛔G-02  docs/BrawlerStepHooksUImpl-guards.md
		m_host.publishTickOffset_Step(static_cast<int64_t>(outcome.tick) - static_cast<int64_t>(outcome.physicsStep));
	}

	double currentStepDeadline = 0.0;

private:
	SimulationFrameHostUImpl& m_host;
	double                    m_stepStartSeconds = 0.0;
};

static_assert(StepHooks<BrawlerStepHooksUImpl>,
	"BrawlerStepHooksUImpl must satisfy og-simulation's StepHooks concept: all four hooks, each returning void, "
	"afterTick reading a const outcome.");
