// SPDX-License-Identifier: MPL-2.0
// docs/ChaosEngineTraitsUImpl-rationale.md · docs/ChaosEngineTraitsUImpl-guards.md

#pragma once

#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"

#include <cstdint>
#include <initializer_list>
#include <optional>

struct ChaosEngineTraitsUImpl
{
	using BodyAdapter   = ChaosPhysicsBodyAdapter;
	using ReaderAdapter = ChaosPhysicsBodyReaderAdapter;
	using QueryAdapter  = ChaosSpatialQueryAdapter;
	using Factory       = ChaosPhysicsFactory;
	using VizQuery      = ChaosSpatialQueryAdapter;
	using VizReader     = ChaosPhysicsBodyReaderAdapter;
	using CategoryTable = std::initializer_list<ChaosCategoryMapping>;

	static constexpr char kName[] = "chaos";
	static constexpr bool kHasWorldHost = false;

	static std::optional<uint64_t> computeDeterminismFingerprint(void (*)(const char*))
	{
		return std::nullopt;
	}
};

static_assert(!ChaosEngineTraitsUImpl::kHasWorldHost,
	"ChaosEngineTraitsUImpl must name no world host: Chaos schedules its own steps (its async physics tick and its "
	"rewind callback), so og-simulation's SimulationTickDriver must never step it. kHasWorldHost false is what makes "
	"a driver-scheduled host refuse these traits at compile time (og-tickdriver task 15b). Added by og-tickdriver "
	"task 15a.");
