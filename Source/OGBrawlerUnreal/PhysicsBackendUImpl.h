// SPDX-License-Identifier: BUSL-1.1
// docs/SimulationManagerUImpl-rationale.md

#pragma once

#ifndef OG_PHYSICS_BACKEND_CHAOS
#error "OG_PHYSICS_BACKEND_CHAOS is undefined: it is a PublicDefinitions entry of OGBrawlerUnreal.Build.cs. Was the D21 backend-switch rule (SimulationManagerUImpl-rationale.md section 18)."
#endif

#if OG_PHYSICS_BACKEND_CHAOS
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"
#include "OGSimulationUnreal/ChaosPhysicsFactory.h"
#else
#include "OGSimulationJolt/JoltPhysicsBodyAdapter.h"
#include "OGSimulationJolt/JoltPhysicsBodyReaderAdapter.h"
#include "OGSimulationJolt/JoltSpatialQueryAdapter.h"
#include "OGSimulationJolt/JoltPhysicsFactory.h"
#endif

namespace physicsBackendUImpl
{

#if OG_PHYSICS_BACKEND_CHAOS
using BodyAdapter   = ChaosPhysicsBodyAdapter;
using ReaderAdapter = ChaosPhysicsBodyReaderAdapter;
using QueryAdapter  = ChaosSpatialQueryAdapter;
using Factory       = ChaosPhysicsFactory;
using VizQuery      = ChaosSpatialQueryAdapter;
using VizReader     = ChaosPhysicsBodyReaderAdapter;

inline constexpr bool kChaosBackend = true;
inline constexpr char kBackendToken[] = "chaos";
#else
using BodyAdapter   = JoltPhysicsBodyAdapter;
using ReaderAdapter = JoltPhysicsBodyReaderAdapter;
using QueryAdapter  = JoltSpatialQueryAdapter;
using Factory       = JoltPhysicsFactory;
using VizQuery      = JoltSpatialQueryAdapter;
using VizReader     = JoltPhysicsBodyReaderAdapter;

inline constexpr bool kChaosBackend = false;
inline constexpr char kBackendToken[] = "jolt";
#endif

} // namespace physicsBackendUImpl
