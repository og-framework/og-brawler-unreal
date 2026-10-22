// SPDX-License-Identifier: BUSL-1.1
// docs/PhysicsBackendUImpl-rationale.md

#pragma once

#ifndef OG_PHYSICS_BACKEND_CHAOS
#error "OG_PHYSICS_BACKEND_CHAOS is undefined: it is a PublicDefinitions entry of OGBrawlerUnreal.Build.cs. Was the D21 backend-switch rule (OGBrawlerSimulationManager-rationale.md section 18)."
#endif

#include "OGSimulation/PhysicsEngineTraits.h"

#include <concepts>
#include <cstddef>
#include <optional>

class AOGBrawlerUECharacter;

using CapsuleContractCheck = void (*)(const AOGBrawlerUECharacter&, unsigned int);

template <typename BindingT, typename EngineTraitsT>
concept BrawlerEngineBinding = PhysicsEngineBinding<BindingT, EngineTraitsT> && (!EngineTraitsT::kHasWorldHost || requires
{
	{ &BindingT::checkCapsuleContract } -> std::convertible_to<CapsuleContractCheck>;
});

namespace physicsEngineSeamSelfCheck
{
struct BindingWithoutParkSnap : SelfScheduledBinding
{
	static void checkCapsuleContract(const AOGBrawlerUECharacter&, unsigned int);
};

struct Binding : BindingWithoutParkSnap
{
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithoutCapsuleContract : SelfScheduledBinding
{
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithNullCapsuleContract : SelfScheduledBinding
{
	static constexpr std::nullptr_t checkCapsuleContract = nullptr;
	static constexpr float kParkSnap = 0.f;
};

struct BindingWithNoneParkSnap : BindingWithoutParkSnap
{
	static constexpr std::nullopt_t kParkSnap = std::nullopt;
};

static_assert(BrawlerEngineBinding<Binding, DriverScheduledTraits> && BrawlerEngineBinding<Binding, SelfScheduledTraits>
		&& BrawlerEngineBinding<SelfScheduledBinding, SelfScheduledTraits>,
	"BrawlerEngineBinding self-check: the reference binding must model the concept over both reference traits, and a "
	"binding with only a simulatable and a category table must model it over a self-scheduled engine. Added by "
	"og-tickdriver task 15a; the self-scheduled binding by og-tickdriver task 15b.");
static_assert(!BrawlerEngineBinding<BindingWithoutParkSnap, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a binding without kParkSnap must NOT model the concept. Added by og-tickdriver "
	"task 15a.");
static_assert(!BrawlerEngineBinding<BindingWithoutCapsuleContract, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a binding of a driver-scheduled engine without checkCapsuleContract must NOT "
	"model the concept. Added by og-tickdriver task 15b, when the capsule check and the park snap became required "
	"only of driver-scheduled engines.");
static_assert(!BrawlerEngineBinding<BindingWithNullCapsuleContract, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a driver-scheduled engine's capsule check must be a function, never a typed "
	"none: a std::nullptr_t member converts to a function pointer, so a host that stored it would compile and make a "
	"null call. Was the Chaos binding's typed-none capsule check (og-tickdriver task 15a); og-tickdriver task 15b.");
static_assert(!BrawlerEngineBinding<BindingWithNoneParkSnap, DriverScheduledTraits>,
	"BrawlerEngineBinding self-check: a driver-scheduled engine's park snap must be a float, never a typed none "
	"(std::nullopt_t converts to std::optional<float>). Was the Chaos binding's typed-none park snap (og-tickdriver "
	"task 15a); og-tickdriver task 15b.");
} // namespace physicsEngineSeamSelfCheck

#if OG_PHYSICS_BACKEND_CHAOS
#include "OGSimulationUnreal/ChaosEngineTraitsUImpl.h"
#include "OGBrawlerUnreal/BrawlerChaosBindingUImpl.h"
using EngineTraits  = ChaosEngineTraitsUImpl;
using EngineBinding = BrawlerChaosBindingUImpl;
#else
#include "OGSimulationJoltUnreal/JoltEngineTraitsUImpl.h"
#include "OGBrawlerUnreal/BrawlerJoltBindingUImpl.h"
using EngineTraits  = JoltEngineTraitsUImpl;
using EngineBinding = BrawlerJoltBindingUImpl;
#endif

static_assert(PhysicsEngineTraits<EngineTraits>,
	"EngineTraits must model PhysicsEngineTraits: the session, the hosts and the build identity read the engine's "
	"adapter types, name, fingerprint and log sink through it. Was the per-configuration alias block of this header "
	"(og-simulationscheduler-withjolt task 83); og-tickdriver task 15a.");
static_assert(BrawlerEngineBinding<EngineBinding, EngineTraits>,
	"EngineBinding must model BrawlerEngineBinding over EngineTraits: the hosts read the brawler's category table, "
	"capsule contract check, park snap and simulatable type through it. Was brawler glue inside the two host .cpp files "
	"(the Jolt host's and the Chaos host's, before og-tickdriver task 15b renamed them); og-tickdriver task 15a.");
