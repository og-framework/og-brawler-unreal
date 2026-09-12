// SPDX-License-Identifier: MPL-2.0

#include "ChaosPhysicsFactory.h"
#include "OGSimulationUnreal/ChaosPhysicsBodyAdapter.h"
#include "OGSimulationUnreal/ChaosSpatialQueryAdapter.h"

#include "Runtime/Engine/Classes/Components/CapsuleComponent.h"
#include "Runtime/Engine/Classes/Components/SphereComponent.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "Runtime/Engine/Classes/PhysicsEngine/BodyInstance.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"

DEFINE_LOG_CATEGORY(LogOGPhysicsFactory);

namespace
{
	// Number of category bits a CollisionCategories mask can carry. The masks are
	// a uint32_t bitfield of consumer-local category ids, so a generic walk over
	// every bit is the only mask-agnostic way to enumerate them — no mask value
	// (worldAndCharacter or otherwise) is hard-coded anywhere in this file.
	constexpr uint32_t kCollisionCategoryBits = 32;

	// [movement-sim T8] BodyResimPolicy -> Chaos::EResimType.
	//
	// Epic's own comments on the two implemented enumerators
	// (Chaos/GeometryParticles.h; USER-VERIFIED 2026-09-04, do not re-derive):
	//   FullResim        = "fully re-run simulation and keep results (any forces
	//                       must be applied again)"
	//   ResimAsFollower  = "use previous forces and snap to previous results
	//                       regardless of variation"
	//
	// ReplayRecordedHistory therefore maps to ResimAsFollower: the sim re-places
	// these bodies from its own owned state every tick, so re-solving them would
	// only discard the placement the sim is about to redo. This is the DEFAULT of
	// BodyDescriptor::resimPolicy, so every body that existed before this task
	// keeps precisely the behaviour the previous unconditional
	// SetResimType(ResimAsFollower) gave it.
	//
	// Resimulate maps to FullResim, for a body the engine should genuinely
	// integrate from the pushed anchor state. No descriptor selects it yet.
	Chaos::EResimType toEngineResimType(BodyResimPolicy policy)
	{
		switch (policy)
		{
		case BodyResimPolicy::ReplayRecordedHistory:
			return Chaos::EResimType::ResimAsFollower;
		case BodyResimPolicy::Resimulate:
			return Chaos::EResimType::FullResim;
		}

		checkf(false, TEXT("ChaosPhysicsFactory — unhandled BodyResimPolicy (%u)"),
			   static_cast<uint32>(policy));
		return Chaos::EResimType::ResimAsFollower;
	}
}

void ChaosPhysicsFactory::applyDescriptor(
	UPrimitiveComponent* component,
	const PhysicalObjectDescriptor& descriptor)
{
	const auto& primaryShape = descriptor.shapes[0];
	const BodyDescriptor& body = descriptor.body;

	// ── Settings that are valid before the physics body exists ─────────────────
	// Configure body flags from the descriptor — no hardcoded values.
	component->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	component->SetSimulatePhysics(body.simulatePhysics);
	component->SetEnableGravity(body.enableGravity);

	// The default relationship with every channel is OVERLAP; only the categories
	// the descriptor explicitly names produce contacts. ShapeDescriptor's
	// blockingCategories defaults to the EMPTY mask, so every descriptor written
	// before this task still ends up Overlap-on-all-channels, exactly as before.
	component->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Overlap);
	for (uint32_t cat = 0; cat < kCollisionCategoryBits; ++cat)
	{
		if (primaryShape.blockingCategories.contains(cat))
		{
			component->SetCollisionResponseToChannel(
				m_queryAdapter.toEngineChannel(cat), ECollisionResponse::ECR_Block);
		}
	}

	// Translate first set bit in shape's CollisionCategories -> ECollisionChannel
	// using the query adapter's explicit DAttack-to-Chaos mapping.
	for (uint32_t cat = 0; cat < kCollisionCategoryBits; ++cat)
	{
		if (primaryShape.categories.contains(cat))
		{
			component->SetCollisionObjectType(m_queryAdapter.toEngineChannel(cat));
			break;
		}
	}

	// Rotation locks. The three per-axis bits are plain data on the body instance
	// and are read when the DOF lock is (re)built below; they default to false on
	// FBodyInstance, so assigning a false lockRotation is a no-op.
	FBodyInstance* bodyInstance = component->GetBodyInstance();
	checkf(bodyInstance, TEXT("ChaosPhysicsFactory::applyDescriptor — component '%s' has no body instance"),
		   *component->GetName());
	bodyInstance->bLockXRotation = body.lockRotation;
	bodyInstance->bLockYRotation = body.lockRotation;
	bodyInstance->bLockZRotation = body.lockRotation;

	// ── Registration ──────────────────────────────────────────────────────────
	// A component this factory CREATED is not registered yet; registering it here,
	// between the flags above and the live-handle settings below, reproduces the
	// pre-T8 call order for every existing body exactly (flags -> RegisterComponent
	// -> SetResimType), which is what keeps this refactor behaviour-neutral —
	// notably, the auto-weld decision taken at InitBody still sees the same flags.
	// The ADOPT-ROOT path passes an ALREADY-registered component (a documented
	// constructor precondition, asserted at the call site), so adoption never
	// reaches this call and registers nothing.
	//
	// CAVEAT - that unreachability argument is BUILD-CONFIGURATION-DEPENDENT.
	// The call-site assertion is a checkf, and checkf compiles out when
	// DO_CHECK == 0 (Shipping, Test), as do the bodyId-identity and the
	// capsule-agreement checks on the same branch. There a violated
	// precondition is caught by nothing: an unregistered adopted component
	// would fall into the guard below and be registered SILENTLY. So the guard
	// is provably dead on the adopt path only in DO_CHECK builds; in
	// Shipping/Test it is a last-resort fallback, not a proof. Free of
	// consequence today - no descriptor sets isRoot - but when the first one
	// does, promote the precondition to enforcement that survives DO_CHECK == 0.
	if (!component->IsRegistered())
	{
		component->RegisterComponent();
	}

	// ── Settings that need a live physics body ────────────────────────────────
	// The per-axis bits above only take effect under a locked-axis mode, so the
	// lock has to be (re)built through the component's constraint mode. Applied
	// ONLY when the descriptor asks for it: calling this unconditionally would
	// build or tear down a DOF constraint on every pre-existing body, which is a
	// behaviour change rather than the pure refactor this branch is meant to be.
	if (body.lockRotation)
	{
		component->SetConstraintMode(EDOFMode::SixDOF);
	}

	// How this body behaves during a rollback REPLAY. Centralized here so callers
	// can't forget; the descriptor default reproduces the previous unconditional
	// ResimAsFollower for every existing body.
	component->GetBodyInstance()->GetPhysicsActorHandle()->GetGameThreadAPI()
		.SetResimType(toEngineResimType(body.resimPolicy));
}

ChaosPhysicsFactory::PhysicalObjectResult ChaosPhysicsFactory::createPhysicalObject(
	const PhysicalObjectDescriptor& descriptor,
	const char* name)
{
	const auto& primaryShape = descriptor.shapes[0];

	// ── ADOPT-ROOT ────────────────────────────────────────────────────────────
	// Branch FIRST: an isRoot descriptor never creates a component. See the
	// adopt-root block in the header for why creation would break hit routing and
	// for the single-root invariant this branch relies on.
	if (descriptor.body.isRoot)
	{
		checkf(m_attachParent,
			   TEXT("ChaosPhysicsFactory — isRoot descriptor '%hs' but the factory has no attachParent to adopt"),
			   name);
		// NOTE: the registered-root precondition is enforced by this checkf ALONE,
		// and checkf compiles out when DO_CHECK == 0. See the registration comment
		// in applyDescriptor for what that costs in Shipping/Test builds.
		checkf(m_attachParent->IsRegistered(),
			   TEXT("ChaosPhysicsFactory — isRoot descriptor '%hs': attachParent '%s' is not registered; ")
			   TEXT("adoption requires an already-registered root (constructor precondition)"),
			   name, *m_attachParent->GetName());

		UPrimitiveComponent* component = m_attachParent;

		// The adopted root is configured, never resized: verify the descriptor
		// AGREES with the authored capsule instead of overwriting it.
		if (const CapsuleGeometry* capsuleGeo = std::get_if<CapsuleGeometry>(&primaryShape.geometry))
		{
			if (const UCapsuleComponent* capsule = Cast<UCapsuleComponent>(component))
			{
				checkf(FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleRadius(), capsuleGeo->radius),
					   TEXT("ChaosPhysicsFactory — adopted root '%s' radius %f != descriptor '%hs' radius %f"),
					   *component->GetName(), capsule->GetUnscaledCapsuleRadius(), name, capsuleGeo->radius);
				checkf(FMath::IsNearlyEqual(capsule->GetUnscaledCapsuleHalfHeight(), capsuleGeo->halfHeight),
					   TEXT("ChaosPhysicsFactory — adopted root '%s' half-height %f != descriptor '%hs' half-height %f"),
					   *component->GetName(), capsule->GetUnscaledCapsuleHalfHeight(), name, capsuleGeo->halfHeight);
			}
		}

		applyDescriptor(component, descriptor);

		// The adopted body's id MUST be the parent id the constructor derived from
		// the very same component — that identity is what makes the hit-routing
		// cutover a change of provenance only (architecture §4.2).
		auto bodyHandle = component->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
		BodyId bodyId = m_bodyAdapter.getBodyId(bodyHandle);
		checkf(bodyId == m_parentBodyId,
			   TEXT("ChaosPhysicsFactory — adopted root '%hs' bodyId %u != m_parentBodyId %u"),
			   name, bodyId.value, m_parentBodyId.value);

		// parent = std::nullopt: the adopted body IS the root, so registerShape
		// stores NO m_shapeBodyToRootBody entry and overlap() falls back to
		// rootBodyId == bodyId for a direct hit on it.
		std::vector<ShapeId> shapeIds;
		for (unsigned int i = 0; i < static_cast<unsigned int>(descriptor.shapes.size()); ++i)
		{
			shapeIds.push_back(m_queryAdapter.registerShape(bodyHandle, i, std::nullopt));
		}

		UE_LOG(LogOGPhysicsFactory, Verbose,
			   TEXT("[PhysicsFactory.AdoptRoot] '%hs' adopted '%s': bodyId=%u, m_parentBodyId=%u (equal); ")
			   TEXT("%d shape(s) registered with parent=nullopt, so NO m_shapeBodyToRootBody entry exists ")
			   TEXT("for them; nothing created, nothing registered, capsule not resized."),
			   name, *component->GetName(), bodyId.value, m_parentBodyId.value,
			   static_cast<int32>(shapeIds.size()));

		return PhysicalObjectResult{component, bodyId, std::move(shapeIds)};
	}

	// ── CREATE ────────────────────────────────────────────────────────────────
	// Create the UE shape component from the descriptor's geometry.
	// Sphere and capsule are supported; extend when needed.
	UPrimitiveComponent* component = std::visit([&](const auto& geo) -> UPrimitiveComponent* {
		using Geometry = std::decay_t<decltype(geo)>;
		if constexpr (std::is_same_v<Geometry, SphereGeometry>)
		{
			auto* comp = NewObject<USphereComponent>(m_owner, FName(name));
			comp->SetSphereRadius(geo.radius);
			return comp;
		}
		else if constexpr (std::is_same_v<Geometry, CapsuleGeometry>)
		{
			auto* comp = NewObject<UCapsuleComponent>(m_owner, FName(name));
			comp->InitCapsuleSize(geo.radius, geo.halfHeight);
			return comp;
		}
		else
		{
			checkf(false, TEXT("ChaosPhysicsFactory::createPhysicalObject — BoxGeometry is not supported"));
			return nullptr;
		}
	}, primaryShape.geometry);

	if (!component)
	{
		return PhysicalObjectResult{};
	}

	// Attach, then apply every descriptor-driven setting (which also registers the
	// component we just created — see applyDescriptor).
	component->SetupAttachment(m_attachParent);
	applyDescriptor(component, descriptor);

	// Extract the BodyId from the newly-created body's Chaos handle. Note the
	// adapter is stateless — Chaos assigns the particle UniqueIdx at creation
	// (GetBodyInstanceAsyncPhysicsTickHandle materializes it); getBodyId
	// just wraps it in the engine-agnostic BodyId type.
	auto bodyHandle = component->GetBodyInstance()->GetBodyInstanceAsyncPhysicsTickHandle();
	BodyId bodyId = m_bodyAdapter.getBodyId(bodyHandle);

	// Register each shape with the SpatialQueryAdapter for enable/disable.
	// Thread the factory's parent body id so overlap() emits an actor-level rootBodyId
	// for hits on these shapes (every shape here belongs to m_parentBodyId's character).
	std::vector<ShapeId> shapeIds;
	for (unsigned int i = 0; i < static_cast<unsigned int>(descriptor.shapes.size()); ++i)
	{
		shapeIds.push_back(m_queryAdapter.registerShape(bodyHandle, i, m_parentBodyId));
	}

	return PhysicalObjectResult{component, bodyId, std::move(shapeIds)};
}
