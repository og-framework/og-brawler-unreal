#pragma once

// SPDX-License-Identifier: MPL-2.0

#include <cstdint>
#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsBodyState.h"
#include "OGSimulation/PhysicsBodyAdapter.h"
#include "OGSimulation/PhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "Runtime/Experimental/Chaos/Public/PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "Chaos/ParticleHandle.h"
#include "PBDRigidsSolver.h"

class ChaosPhysicsBodyAdapter
{
public:
	explicit ChaosPhysicsBodyAdapter(Chaos::FPBDRigidsSolver& solver)
		: m_solver(solver)
	{}

	// Extract the OGSim BodyId from a Chaos particle handle. Stateless / idempotent
	// / const — the "body id" IS the Chaos particle UniqueIdx that Chaos itself
	// assigns at particle creation; this just wraps it in the engine-agnostic
	// BodyId type. Safe to call multiple times with the same handle; every call
	// returns the same value. Not part of the PhysicsBodyAdapter concept —
	// engine-specific convenience for code that has a Chaos handle and needs the
	// abstract BodyId to feed into other adapter methods.
	BodyId getBodyId(FBodyInstanceAsyncPhysicsTickHandle handle) const
	{
		const uint32_t nativeId = static_cast<uint32_t>(handle.Proxy->GetParticle_LowLevel()->UniqueIdx().Idx);
		return BodyId{nativeId};
	}

	// NOTE: this method is called from the GAME THREAD by
	// SimulationManagerUImpl::tryRegister during the register-with-Chaos
	// polling loop (SimulationManagerUImpl.cpp ~lines 630/635). All other
	// methods on this adapter are called from the physics thread by the sim
	// and can use the private resolveProxy() (PT-side proxy registry) safely.
	//
	// For isBodyResolvable specifically, use the GT-owned UniqueIdxToGTParticles
	// registry instead — GetParticleProxy_PT would race with PT-side
	// registration writes to SingleParticlePhysicsProxies_PT during
	// ProcessSinglePushedData_Internal, same class of bug that motivated
	// the ChaosPhysicsBodyReaderAdapter GT-safe rework (2026-07-17). A torn
	// read of the sparse array could return a non-null proxy pointer whose
	// underlying Handle field is still null, tricking tryRegister into
	// declaring the body ready one push-cycle too early.
	//
	// Semantic note: this returns true as soon as GT registration completes,
	// which precedes the PT-side proxy Handle assignment by one push cycle.
	// Since tryRegister runs on GT and the sim runs on PT, by the time the
	// sim first touches the body on PT, ProcessSinglePushedData_Internal
	// will have run (it drains queued pushes at the top of every PT tick
	// before the sim step) and SetHandle will have populated the PT side.
	bool isBodyResolvable(BodyId bodyId) const
	{
		return m_solver.UniqueIdxToGTParticle_External(
			Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)}) != nullptr;
	}

	glm::mat4 getBodyTransform(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			auto* ptApi = proxy->GetPhysicsThreadAPI();
			FTransform uWorldTransform(ptApi->GetR(), ptApi->GetX());
			return uglm::toGLMMat4(uWorldTransform);
		}
		ensureAlwaysMsgf(false, TEXT("ChaosPhysicsBodyAdapter::getBodyTransform — unresolved BodyId %u"), bodyId.value);
		return glm::mat4(1.f);
	}

	void setBodyTransform(BodyId bodyId, const glm::mat4& transform)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::setBodyTransform — unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		FTransform uWorldTransform;
		uglm::toFtransform(transform, uWorldTransform);
		auto* ptApi = proxy->GetPhysicsThreadAPI();
		ptApi->SetX(uWorldTransform.GetTranslation());
		ptApi->SetR(uWorldTransform.GetRotation());
	}

	void addBodyTorque(BodyId bodyId, const glm::vec3& torque)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::addBodyTorque — unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		constexpr bool bInvalidate = false;
		proxy->GetPhysicsThreadAPI()->AddTorque(uglm::toFVector(torque), bInvalidate);
	}

	void setBodyAngularVelocity(BodyId bodyId, const glm::vec3& vel)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::setBodyAngularVelocity — unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		proxy->GetPhysicsThreadAPI()->SetW(uglm::toFVector(vel));
	}

	void setBodyLinearVelocity(BodyId bodyId, const glm::vec3& vel)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::setBodyLinearVelocity — unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		proxy->GetPhysicsThreadAPI()->SetV(uglm::toFVector(vel));
	}

	// Accumulate an acceleration for THIS step; engine applies force = a * mass.
	// Mass-agnostic seam: the sim expresses intent as an acceleration (cm/s^2) and
	// each adapter converts using its own body mass, so a contested step is resolved
	// by Chaos with mass symmetry instead of one side overwriting V.
	// Shape mirrors addBodyTorque above: resolve the proxy, then one call on the
	// physics-thread API with bInvalidate = false, because we are already on the
	// physics thread inside the sim step -- the same assumption setBodyLinearVelocity
	// and captureBodyState make.
	void addBodyAcceleration(BodyId bodyId, const glm::vec3& acceleration)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::addBodyAcceleration - unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		constexpr bool bInvalidate = false;
		auto* ptApi = proxy->GetPhysicsThreadAPI();
		ptApi->AddForce(uglm::toFVector(acceleration) * ptApi->M(), bInvalidate);
	}

	// Instantaneous dv before this step's solve; engine applies impulse = dv * mass.
	// Written as a read-modify-write of the physics-thread velocity through the same
	// GetV()/SetV() pair captureBodyState and setBodyLinearVelocity already use. That
	// form is mass-agnostic by construction, so no M() term appears here.
	void addBodyVelocityChange(BodyId bodyId, const glm::vec3& velocityChange)
	{
		auto* proxy = resolveProxy(bodyId);
		ensureAlwaysMsgf(proxy != nullptr, TEXT("ChaosPhysicsBodyAdapter::addBodyVelocityChange - unresolved BodyId %u"), bodyId.value);
		if (!proxy)
		{
			return;
		}

		auto* ptApi = proxy->GetPhysicsThreadAPI();
		ptApi->SetV(ptApi->GetV() + uglm::toFVector(velocityChange));
	}

	glm::vec3 getBodyInertiaTensor(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			const Chaos::FMatrix33 inertiaTensor = Chaos::FParticleUtilitiesXR::GetWorldInertia(proxy->GetPhysicsThreadAPI());
			return glm::vec3(inertiaTensor.M[0][0], inertiaTensor.M[1][1], inertiaTensor.M[2][2]);
		}
		return glm::vec3(0.f);
	}

	// [task 46] READS THE SOLVED POSE -- `GetP()`/`GetQ()`, NEVER `GetX()`/`GetR()`.
	//
	// This runs from Chaos's `PostSolveCallback`
	// (`FSimulationManagerAsyncCallback::OnPostSolve_Internal` ->
	// `SimulationManager::onPostGameSimulation` -> `captureBodyStatesAll`). In UE 5.6
	// `FPBDRigidsEvolutionGBF::AdvanceOneTimeStepImpl` the order inside ONE step is:
	//
	//     Integrate               writes P/Q and V; leaves X/R UNTOUCHED
	//     ... constraint solve ...  refines P/Q and V
	//     PostSolveCallback       <- WE ARE HERE
	//     ParticleUpdatePosition  SetX(GetP()); SetR(GetQ())  -- the commit, 43 lines later
	//
	// So at this point X/R still hold the START-of-step pose, which for a sub-sim that
	// wrote the body pre-solve is its own command echoed back. Measured by the task-9
	// spike, run 6A (2026-09-05, both peers): capX.Z = 10.000000 (commanded) vs
	// capP.Z = 7.812868 (solved) -- 2.187133 cm of solve discarded every step.
	//
	// V/W are DELIBERATELY UNCHANGED: Integrate and the solve both write them before this
	// callback, so `GetV()`/`GetW()` are already end-of-step values. Reading P/Q here is
	// what makes the captured tuple internally consistent -- pose and velocity from the
	// same instant -- and what makes slot[T] mean "the state AFTER tick T", which is what
	// the rollback push consumes (`SimulationManagerUImpl.cpp` `FirstPreResimStep_Internal`
	// writes it as the PostPushData pose of the frame that replays tick T+1).
	//
	// WHY NOT `ptApi->GetP()`: `FRigidBodyHandle_Internal` HAS NO `GetP()`/`GetQ()`
	// (`PhysicsProxy/SingleParticlePhysicsProxy.h` @5.6 -- the class is PreV/PreW/SetX/
	// SetR/SetV/SetW/SetObjectState, and its base exposes only X/R/V/W). P/Q live on the
	// rigid particle handle. `FConstGenericParticleHandle` is the engine's own
	// type-agnostic reader for exactly this case: its `GetP()`/`GetQ()` return the rigid
	// handle's P/Q for a DYNAMIC (or sleeping) particle and fall back to X/R otherwise
	// (`Chaos/ParticleHandle.h` @5.6), which is the right answer for a static or kinematic
	// body whose P is never integrated. Do NOT hand-roll that with `CastToRigidParticle()`:
	// a rigid-typed particle parked in the Kinematic object state casts fine and would hand
	// back a stale P.
	//
	// SAME FRAME, no conversion: X/R and P/Q are both particle-frame -- `XCom`/`PCom` are
	// symmetric derivations from them (`Chaos/PBDRigidParticles.h` @5.6), and every body
	// this project creates has CoM = 0 anyway. Established by task 47.
	//
	// BOTH PEERS MUST SHIP THIS TOGETHER. What this returns is what
	// `StateCorrectionCache::tryInsertingCorrectState` feeds to `isSimilarTo` against the
	// authority's copy, at a 0.0001 epsilon. Peers on opposite sides of this change would
	// compare pose(T) against pose(T-1) and mispredict every tick.
	PhysicsBodyState captureBodyState(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			auto* ptApi = proxy->GetPhysicsThreadAPI();
			const Chaos::FConstGenericParticleHandle particle(proxy->GetHandle_LowLevel());
			PhysicsBodyState s;
			s.position = uglm::toGLMVec3(particle->GetP());
			s.rotation = uglm::toGLMQuat(FQuat(particle->GetQ()));
			s.linearVelocity = uglm::toGLMVec3(ptApi->GetV());
			s.angularVelocity = uglm::toGLMVec3(ptApi->GetW());
			return s;
		}
		return PhysicsBodyState{};
	}

private:
	Chaos::FSingleParticlePhysicsProxy* resolveProxy(BodyId bodyId) const
	{
		return m_solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
	}

	Chaos::FPBDRigidsSolver& m_solver;
};

static_assert(PhysicsBodyAdapter<ChaosPhysicsBodyAdapter>);
static_assert(PhysicsBodyReaderAdapter<ChaosPhysicsBodyAdapter>);
