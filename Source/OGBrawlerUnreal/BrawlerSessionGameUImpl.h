// SPDX-License-Identifier: BUSL-1.1
// docs/BrawlerSessionGameUImpl-rationale.md · docs/BrawlerSessionGameUImpl-guards.md

#pragma once

#include "OGBrawlerUnreal/BrawlerSessionTraitsUImpl.h"
#include "OGBrawlerUnreal/SimmableUpdateComponent.h"
#include "OGBrawlerUnreal/OGBrawlerInputCollectionComponent.h"
#include "OGSimulation/CompilerControl.h"

class BrawlerSessionGameUImpl
{
public:
	static BrawlerSessionTraitsUImpl::InputSampleSource inputSampleSource(
		const BrawlerSessionTraitsUImpl::Component& owner);

#if OG_LATENCY_BUDGET_DIAGNOSTICS
	static double inputSampledSeconds(BrawlerSessionTraitsUImpl::InputSampleSource source);
#endif
};

OGSIM_OPTIMIZE_OFF

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
