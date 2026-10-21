// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionGameUImpl-rationale.md · docs/BrawlerSessionGameUImpl-guards.md

#pragma once

#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGSimulation/CompilerControl.h"
#include "Engine/EngineBaseTypes.h"

#include <optional>
#include <string_view>

template <typename TraitsT> class UESimulationSession;
class BrawlerRosterUImpl;
class UWorld;

class BrawlerSessionGameUImpl
{
public:
	explicit BrawlerSessionGameUImpl(BrawlerRosterUImpl& roster) : m_roster(roster) {}

	static BrawlerSessionTraitsUImpl::StaticData makeStaticData();

	void beforeWorldBuild(UWorld& world, ENetMode netMode, bool compositionExists,
		BrawlerSessionTraitsUImpl::StaticData& staticData);

	static void emplaceSystems(std::optional<BrawlerCompositionUImpl::SystemsExec>& systemsExec,
		EngineTraits::BodyAdapter& bodyAdapter, EngineTraits::QueryAdapter& queryAdapter);

	static BrawlerSessionTraitsUImpl::PlayerInput neutralInput();

	static void installGameLoggers(bool isAuthority);

	void afterSessionConfigured(ENetMode netMode);

	static std::string_view backendFingerprint();

	static void afterVisualization(const UESimulationSession<BrawlerSessionTraitsUImpl>& session);

	static BrawlerSessionTraitsUImpl::InputSampleSource inputSampleSource(
		const BrawlerSessionTraitsUImpl::Component& owner);

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	static double inputSampledSeconds(BrawlerSessionTraitsUImpl::InputSampleSource source);
#endif

private:
	using BrawlerHitDetectionSystem = BrawlerCompositionUImpl::HitDetectionSystem;

	BrawlerRosterUImpl& m_roster;
};

OGSIM_OPTIMIZE_OFF

inline BrawlerSessionTraitsUImpl::PlayerInput BrawlerSessionGameUImpl::neutralInput()
{
	// ⛔G-01  docs/BrawlerSessionGameUImpl-guards.md
	return simulatableBrawler::getZeroPlayerInput();
}

inline BrawlerSessionTraitsUImpl::InputSampleSource BrawlerSessionGameUImpl::inputSampleSource(
	const BrawlerSessionTraitsUImpl::Component& owner)
{
	return owner.getOwnerInputCollection();
}

#if OG_LATENCY_BUDGET_DIAGNOSTICS
inline double BrawlerSessionGameUImpl::inputSampledSeconds(BrawlerSessionTraitsUImpl::InputSampleSource source)
{
	return source != nullptr ? source->getDiagnostics().inputSampledSeconds() : 0.0;
}
#endif

OGSIM_OPTIMIZE_ON
