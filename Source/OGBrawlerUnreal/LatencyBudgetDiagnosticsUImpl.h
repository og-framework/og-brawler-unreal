// SPDX-License-Identifier: BUSL-1.1
// docs/LatencyBudgetDiagnosticsUImpl-rationale.md · docs/LatencyBudgetDiagnosticsUImpl-guards.md

#pragma once

#include "CoreMinimal.h"
#include "OGBrawler/SimulatableBrawlerTypes.h"
#include "OGSimulation/SimulationTimeContext.h"
#include "OGSimulation/Network/LocalInputCache.h"
#include "OGSimulation/LatencyBudgetProbe.h"

#include <functional>
#include <type_traits>

#ifndef OG_LATENCY_BUDGET_DIAGNOSTICS
#define OG_LATENCY_BUDGET_DIAGNOSTICS (!UE_BUILD_SHIPPING)
#endif

#if OG_LATENCY_BUDGET_DIAGNOSTICS
#include <memory>
#include <unordered_map>
#endif

class ASimulationManagerUImpl;
class USimmableUpdateComponent;
class UWorld;

#if OG_LATENCY_BUDGET_DIAGNOSTICS
DECLARE_LOG_CATEGORY_EXTERN(LogOGLatencyBudget, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogOGChaosDilation, Log, All);
#endif

class LatencyBudgetDiagnosticsUImpl
{
public:
	using InputProvider = std::function<simulatableBrawler::PlayerInput(
		const SimulationTimeStep&,
		const LocalInputCache<simulatableBrawler::PlayerInput>&)>;

	struct ClockCounts
	{
		uint32 skips       = 0u;
		uint32 stalls      = 0u;
		uint32 hardResyncs = 0u;
		bool operator==(const ClockCounts&) const = default;
	};

	LatencyBudgetDiagnosticsUImpl(const LatencyBudgetDiagnosticsUImpl&) = delete;
	LatencyBudgetDiagnosticsUImpl& operator=(const LatencyBudgetDiagnosticsUImpl&) = delete;

	InputProvider wrapInputProvider(unsigned int id, const USimmableUpdateComponent& owner, InputProvider inputProvider)
	{
		[[maybe_unused]] const bool present = static_cast<bool>(inputProvider);
		InputProvider wrapped = wrap(id, owner, std::move(inputProvider));
		checkf(static_cast<bool>(wrapped) == present,
			TEXT("LatencyBudgetDiagnosticsUImpl::wrapInputProvider: a present provider must stay present and an empty ")
			TEXT("one empty. Provider presence is the core's local-vs-remote identity test, so the latency wrapper may ")
			TEXT("never turn an empty provider into a callable or a callable into an empty one, in either state of ")
			TEXT("OG_LATENCY_BUDGET_DIAGNOSTICS. Was the checkf of wrapInputProviderWithLatencyStamp."));
		return wrapped;
	}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	explicit LatencyBudgetDiagnosticsUImpl(const ASimulationManagerUImpl& owner) : m_owner(owner) {}

	void begin(UWorld& world, bool isAuthority);
	void end(UWorld& world);
	void forgetCharacter(unsigned int id);

	void stamp(latencyBudget::Stream stream, unsigned int lane, uint32 tick, latencyBudget::Point point);
	void noteWireSample(double roundTripSeconds);
	void post_Internal(const latencyBudget::Event& event);
	void stampAfterStep_Internal(double stepStartSeconds);
	void stampStepEnd_Internal();
	void onPostTickFlush();

	template <typename BackendDiagnosticsT>
	void tick_GameThread(BackendDiagnosticsT backendDiagnostics);

	ClockCounts clockCountsInWindow() const;
	void restartClockWindow();

	template <typename FunctionT>
	static void withInstance(bool isAuthority, FunctionT&& function)
	{
		if (LatencyBudgetDiagnosticsUImpl* diagnostics = instanceFor(isAuthority))
			function(*diagnostics);
	}

private:
	using Mailbox      = latencyBudget::SpscMailbox<latencyBudget::Event, 4096>;
	using ClockMailbox = latencyBudget::SpscMailbox<ClockCounts, 64>;

	static LatencyBudgetDiagnosticsUImpl* instanceFor(bool isAuthority);

	InputProvider wrap(unsigned int id, const USimmableUpdateComponent& owner, InputProvider inputProvider);

	const ASimulationManagerUImpl& m_owner;

	static_assert(std::is_const_v<std::remove_reference_t<decltype(m_owner)>>,
		"LatencyBudgetDiagnosticsUImpl reads the manager's production state (m_manager, m_storage, m_inputResolution, "
		"m_delayedInputComponentsById and the client clock) through a CONST reference only. It is the manager's friend, "
		"so a mutable reference here would be a write door from a diagnostic into every private member of "
		"ASimulationManagerUImpl. Added by og-tickdriver task 18.");

	// ⛔G-01  docs/LatencyBudgetDiagnosticsUImpl-guards.md
	std::unique_ptr<latencyBudget::LatencyBudgetProbe> m_probe;
	std::unique_ptr<Mailbox>                           m_mailbox;
	std::unique_ptr<ClockMailbox>                      m_clockMailbox;
	ClockCounts                                        m_clockPosted_Physics;
	ClockCounts                                        m_clockLatest;
	ClockCounts                                        m_clockAtWindowStart;
	uint64                                             m_mailboxDropped = 0u;
	std::unordered_map<unsigned int, uint32>           m_lastHandedCorrectionTick;
	FDelegateHandle                                    m_postTickFlushHandle;
#else
	explicit LatencyBudgetDiagnosticsUImpl(const ASimulationManagerUImpl&) {}

	void begin(UWorld&, bool) {}
	void end(UWorld&) {}
	void forgetCharacter(unsigned int) {}

	void stamp(latencyBudget::Stream, unsigned int, uint32, latencyBudget::Point) {}
	void noteWireSample(double) {}
	void post_Internal(const latencyBudget::Event&) {}
	void stampAfterStep_Internal(double) {}
	void stampStepEnd_Internal() {}
	void onPostTickFlush() {}

	template <typename BackendDiagnosticsT>
	void tick_GameThread(BackendDiagnosticsT) {}

	ClockCounts clockCountsInWindow() const { return {}; }
	void restartClockWindow() {}

	template <typename FunctionT>
	static void withInstance(bool, FunctionT&&) {}

private:
	InputProvider wrap(unsigned int, const USimmableUpdateComponent&, InputProvider inputProvider)
	{
		return inputProvider;
	}
#endif
};
