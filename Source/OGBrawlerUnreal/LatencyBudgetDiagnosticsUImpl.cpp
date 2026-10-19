// SPDX-License-Identifier: BUSL-1.1
// docs/LatencyBudgetDiagnosticsUImpl-rationale.md · docs/LatencyBudgetDiagnosticsUImpl-guards.md

#include "OGBrawlerUnreal/LatencyBudgetDiagnosticsUImpl.h"

#if OG_LATENCY_BUDGET_DIAGNOSTICS
#include "OGBrawlerUnreal/SimulationManagerUImpl.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGSimulation/CompilerControl.h"
#include "Engine/World.h"

#include <utility>

DEFINE_LOG_CATEGORY(LogOGLatencyBudget);
DEFINE_LOG_CATEGORY(LogOGChaosDilation);

OGSIM_OPTIMIZE_OFF

LatencyBudgetDiagnosticsUImpl* LatencyBudgetDiagnosticsUImpl::instanceFor(bool isAuthority)
{
	ASimulationManagerUImpl* manager = ASimulationManagerUImpl::instanceFor(isAuthority);
	return manager != nullptr ? &manager->editLatencyBudgetDiagnostics() : nullptr;
}

void LatencyBudgetDiagnosticsUImpl::begin(UWorld& world, bool isAuthority)
{
	m_probe = std::make_unique<latencyBudget::LatencyBudgetProbe>(
		isAuthority ? latencyBudget::kServerHops : latencyBudget::kClientHops);
	m_mailbox      = std::make_unique<Mailbox>();
	m_clockMailbox = std::make_unique<ClockMailbox>();
	m_postTickFlushHandle =
		world.OnPostTickFlush().AddWeakLambda(&m_owner, [this]() { onPostTickFlush(); });
}

void LatencyBudgetDiagnosticsUImpl::end(UWorld& world)
{
	if (m_postTickFlushHandle.IsValid())
	{
		world.OnPostTickFlush().Remove(m_postTickFlushHandle);
		m_postTickFlushHandle.Reset();
	}
}

void LatencyBudgetDiagnosticsUImpl::forgetCharacter(unsigned int id)
{
	if (m_probe)
		m_probe->forgetLane(id);
	m_lastHandedCorrectionTick.erase(id);
}

LatencyBudgetDiagnosticsUImpl::InputProvider LatencyBudgetDiagnosticsUImpl::wrap(
	unsigned int id, const USimmableUpdateComponent& owner, InputProvider inputProvider)
{
	if (!inputProvider)
		return inputProvider;

	const UOGBrawlerInputCollectionComponent* inputCollection = owner.getOwnerInputCollection();
	return
		[this, id, inputCollection, inner = std::move(inputProvider)](
			const SimulationTimeStep& step,
			const LocalInputCache<simulatableBrawler::PlayerInput>& localInputCache)
		{
			simulatableBrawler::PlayerInput input = inner(step, localInputCache);
			const double capturedSeconds = FPlatformTime::Seconds();
			const uint32 tick = step.getTick();
			const double sampledSeconds =
				inputCollection != nullptr ? inputCollection->getDiagnostics().inputSampledSeconds() : 0.0;
			if (sampledSeconds > 0.0)
			{
				post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
					id, tick, latencyBudget::Point::InputSampled, sampledSeconds));
			}
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
				id, tick, latencyBudget::Point::Captured, capturedSeconds));
			return input;
		};
}

void LatencyBudgetDiagnosticsUImpl::stamp(
	latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point)
{
	checkf(IsInGameThread(),
		TEXT("LatencyBudgetDiagnosticsUImpl::stamp is the GAME-THREAD door to the latency probe; ")
		TEXT("the physics thread posts through post_Internal's mailbox instead."));
	if (m_probe)
		m_probe->stamp(stream, lane, tick, point, FPlatformTime::Seconds());
}

void LatencyBudgetDiagnosticsUImpl::noteWireSample(double roundTripSeconds)
{
	checkf(IsInGameThread(),
		TEXT("LatencyBudgetDiagnosticsUImpl::noteWireSample is game-thread only: the probe has one owner."));
	if (m_probe)
		m_probe->addSample(latencyBudget::Hop::Wire, roundTripSeconds * 0.5);
}

void LatencyBudgetDiagnosticsUImpl::post_Internal(const latencyBudget::Event& event)
{
	if (m_mailbox)
		m_mailbox->tryPush(event);
}

void LatencyBudgetDiagnosticsUImpl::stampAfterStep_Internal(double stepStartSeconds)
{
	if (!m_mailbox || !m_owner.m_manager.has_value())
		return;

	const uint32 tick = m_owner.m_manager->currentIntegratedTick();

	if (!m_owner.m_manager->runsPrediction())
	{
		m_owner.m_storage.forEachSimulatable<SimulatableBrawler>(
			[this, stepStartSeconds](unsigned int id, const auto&)
			{
				const uint32 captureTick =
					m_owner.m_inputResolution.getLastUsedCaptureTick<SimulatableBrawler>(id);
				if (captureTick != kNoInputCaptureTick)
				{
					post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, captureTick, latencyBudget::Point::Consumed, stepStartSeconds));
				}
			});
		return;
	}

	post_Internal(latencyBudget::Event::stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::ClientReceived, latencyBudget::Point::Consumed, stepStartSeconds));

	m_owner.m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepStartSeconds](unsigned int id, const auto&)
		{
			const RelayedReadObservationRing* const ring =
				m_owner.m_inputResolution.getDiagnostics().relayedReadObservations<SimulatableBrawler>(id);
			if (ring == nullptr)
				return;
			for (std::size_t index = 0; index < ring->size(); ++index)
			{
				const RelayedReadObservation* const observation = ring->at(index);
				if (observation == nullptr || observation->simTick != tick)
					continue;
				if (observation->hasAppliedCaptureTick)
				{
					post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::Input,
						id, observation->appliedCaptureTick, latencyBudget::Point::Consumed,
						stepStartSeconds));
				}
				return;
			}
		});

	if (m_clockMailbox)
	{
		const ClientPredictionClock& clock = m_owner.m_manager->getClientClock();
		ClockCounts counts;
		counts.skips       = clock.getDiagnostics().skipCount();
		counts.stalls      = clock.getDiagnostics().stallCount();
		counts.hardResyncs = clock.getDiagnostics().hardResyncCount();
		if (!(counts == m_clockPosted_Physics) && m_clockMailbox->tryPush(counts))
			m_clockPosted_Physics = counts;
	}
}

void LatencyBudgetDiagnosticsUImpl::stampStepEnd_Internal()
{
	if (!m_mailbox || !m_owner.m_manager.has_value())
		return;

	const double stepEndSeconds = FPlatformTime::Seconds();
	const uint32 tick = m_owner.m_manager->currentIntegratedTick();
	m_owner.m_storage.forEachSimulatable<SimulatableBrawler>(
		[this, tick, stepEndSeconds](unsigned int id, const auto&)
		{
			post_Internal(latencyBudget::Event::stamp(latencyBudget::Stream::State,
				id, tick, latencyBudget::Point::StepEnd, stepEndSeconds));
		});
}

void LatencyBudgetDiagnosticsUImpl::onPostTickFlush()
{
	if (!m_probe)
		return;
	const double nowSeconds = FPlatformTime::Seconds();
	m_probe->stampPending(latencyBudget::Stream::Input,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
	m_probe->stampPending(latencyBudget::Stream::State,
		latencyBudget::Point::Handed, latencyBudget::Point::LeftProcess, nowSeconds);
}

template <typename BackendDiagnosticsT>
void LatencyBudgetDiagnosticsUImpl::tick_GameThread(BackendDiagnosticsT backendDiagnostics)
{
	if (!m_probe || !m_mailbox || !m_owner.m_manager.has_value())
		return;

	m_mailbox->drain([this](const latencyBudget::Event& event) { m_probe->apply(event); });
	m_mailboxDropped += m_mailbox->takeDroppedCount();
	if (m_clockMailbox)
	{
		m_clockMailbox->drain(
			[this](const ClockCounts& counts) { m_clockLatest = counts; });
		m_mailboxDropped += m_clockMailbox->takeDroppedCount();
	}

	const double nowSeconds = FPlatformTime::Seconds();
	const bool isClient = m_owner.m_manager->runsPrediction();

	if (!isClient)
	{
		for (const auto& entry : m_owner.m_delayedInputComponentsById)
		{
			const USimmableUpdateComponent* component = entry.second.Get();
			if (component == nullptr)
				continue;
			const uint32 correctionTick =
				component->getSyncedCorrectionStateBuffer().readFromBuffer<uint32>(
					correctionStateBuffer::kTickOffset);
			if (correctionTick == 0u)
				continue;
			uint32& lastHanded = m_lastHandedCorrectionTick[entry.first];
			if (lastHanded == correctionTick)
				continue;
			lastHanded = correctionTick;
			m_probe->stamp(latencyBudget::Stream::State, entry.first, correctionTick,
				latencyBudget::Point::Handed, nowSeconds);
		}
	}

	m_probe->stampNewestPending(latencyBudget::Stream::State,
		latencyBudget::Point::StepEnd, latencyBudget::Point::Rendered, nowSeconds);

	backendDiagnostics.sampleLatencyWindow_GameThread(isClient);

	latencyBudget::WindowReport report;
	if (!m_probe->closeWindowIfDue(nowSeconds, report))
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
		static_cast<unsigned long long>(m_mailboxDropped));
	m_mailboxDropped = 0u;

	backendDiagnostics.logLatencyWindow_GameThread(isClient);
}

template void LatencyBudgetDiagnosticsUImpl::tick_GameThread(
	decltype(std::declval<physicsBackendUImpl::Backend&>().editDiagnostics()));

LatencyBudgetDiagnosticsUImpl::ClockCounts LatencyBudgetDiagnosticsUImpl::clockCountsInWindow() const
{
	ClockCounts window;
	window.skips       = m_clockLatest.skips - m_clockAtWindowStart.skips;
	window.stalls      = m_clockLatest.stalls - m_clockAtWindowStart.stalls;
	window.hardResyncs = m_clockLatest.hardResyncs - m_clockAtWindowStart.hardResyncs;
	return window;
}

void LatencyBudgetDiagnosticsUImpl::restartClockWindow()
{
	m_clockAtWindowStart = m_clockLatest;
}

OGSIM_OPTIMIZE_ON

#endif
