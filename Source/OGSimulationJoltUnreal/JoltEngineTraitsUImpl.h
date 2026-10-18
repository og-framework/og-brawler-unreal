// SPDX-License-Identifier: MPL-2.0
// docs/JoltEngineTraitsUImpl-rationale.md · docs/JoltEngineTraitsUImpl-guards.md

#pragma once

#include "OGSimulationJolt/JoltDeterminismFingerprint.h"
#include "OGSimulationJolt/JoltPhysicsBodyAdapter.h"
#include "OGSimulationJolt/JoltPhysicsBodyReaderAdapter.h"
#include "OGSimulationJolt/JoltSpatialQueryAdapter.h"
#include "OGSimulationJolt/JoltPhysicsFactory.h"
#include "OGSimulationJolt/JoltTickWorldHost.h"
#include "OGSimulationUnreal/UEFMutexLockableUImpl.h"
#include "OGSimulationUnreal/UEStaticGeometryImporter.h"
#include "OGSimulationUnreal/UETickDriverHost.h"

#include <cstdint>
#include <optional>
#include <vector>

struct JoltEngineTraitsUImpl
{
	using BodyAdapter   = JoltPhysicsBodyAdapter;
	using ReaderAdapter = JoltPhysicsBodyReaderAdapter;
	using QueryAdapter  = JoltSpatialQueryAdapter;
	using Factory       = JoltPhysicsFactory;
	using VizQuery      = JoltSpatialQueryAdapter;
	using VizReader     = JoltPhysicsBodyReaderAdapter;
	using CategoryTable = const std::vector<UEStaticCategoryChannel>&;
	using Statics       = JoltTickStatics;

	static constexpr char kName[] = "jolt";
	static constexpr bool kHasWorldHost = true;

	template <typename SimulatableT>
	using WorldHost = JoltTickWorldHost<SimulatableT, UEFMutexLockableUImpl, SimulationEngineUImpl>;

	static std::optional<uint64_t> computeDeterminismFingerprint(void (*logger)(const char*))
	{
		JoltRuntime& runtime = JoltRuntime::acquire(logger);
		const uint64_t value = determinismFingerprint(runtime).value();
		JoltRuntime::release();
		return value;
	}

	static Statics splitStatics(const StaticWorldDescription& description, const std::vector<uint8_t>& physicsOnly)
	{
		return joltTickWorldHost::splitStatics(description, physicsOnly);
	}

	static OGSIMULATIONJOLTUNREAL_API void logEngineMessage(const char* message);
};
