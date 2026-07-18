#pragma once

// SPDX-License-Identifier: MPL-2.0

#include <cstdint>
#include "OGSimulation/BodyId.h"
#include "OGSimulation/PhysicsBodyState.h"
#include "OGSimulation/PhysicsBodyReaderAdapter.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "PhysicsProxy/SingleParticlePhysicsProxy.h"
#include "PBDRigidsSolver.h"

// Read-only body adapter for the Chaos game thread.
// Reads GT-interpolated state via Proxy->GetGameThreadAPI().
// Safe to call from TickComponent, visualization, UI, etc.
//
// Stateless: resolves proxies via the solver's particle registry.
// Shares BodyId namespace with ChaosPhysicsBodyAdapter (same native UniqueIdx).
class ChaosPhysicsBodyReaderAdapter
{
public:
	explicit ChaosPhysicsBodyReaderAdapter(Chaos::FPBDRigidsSolver& solver)
		: m_solver(solver)
	{}

	glm::mat4 getBodyTransform(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			const auto& gtApi = proxy->GetGameThreadAPI();
			FTransform uWorldTransform(gtApi.GetR(), gtApi.GetX());
			return uglm::toGLMMat4(uWorldTransform);
		}
		return glm::mat4(1.f);
	}

	glm::vec3 getBodyInertiaTensor(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			const auto& gtApi = proxy->GetGameThreadAPI();
			if (gtApi.CanTreatAsRigid())
			{
				const Chaos::FMatrix33 inertiaTensor =
					Chaos::FParticleUtilitiesXR::GetWorldInertia(&gtApi);
				return glm::vec3(inertiaTensor.M[0][0], inertiaTensor.M[1][1], inertiaTensor.M[2][2]);
			}
		}
		return glm::vec3(0.f);
	}

	bool isBodyResolvable(BodyId bodyId) const { return resolveProxy(bodyId) != nullptr; }

	PhysicsBodyState captureBodyState(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			const auto& gtApi = proxy->GetGameThreadAPI();
			PhysicsBodyState s;
			s.position        = uglm::toGLMVec3(gtApi.GetX());
			s.rotation        = uglm::toGLMQuat(FQuat(gtApi.GetR()));
			s.linearVelocity  = uglm::toGLMVec3(gtApi.GetV());
			s.angularVelocity = uglm::toGLMVec3(gtApi.GetW());
			return s;
		}
		return PhysicsBodyState{};
	}

private:
	// GT-safe proxy lookup: go through the game-thread-owned UniqueIdxToGTParticles
	// registry (written by GT-side RegisterObject / UnregisterObject), then follow
	// the particle's GetProxy() pointer to arrive at the same FSingleParticlePhysicsProxy
	// the PT-side SingleParticlePhysicsProxies_PT array holds.
	//
	// The earlier implementation used FPBDRigidsSolver::GetParticleProxy_PT directly,
	// which is a physics-thread accessor by both naming convention (`_PT` suffix)
	// and implementation — its backing sparse array is written on the PT during
	// ProcessSinglePushedData_Internal. Reading from the game thread was a data
	// race, most dangerous during a character spawn: the PT could be mid-emplace
	// on the sparse array's bitmap / element storage, causing torn reads with
	// null Handle fields (matching the ~0x48-offset null-deref crash seen 2026-07-17).
	//
	// The current path (UniqueIdxToGTParticle_External → GetProxy) reads a GT-owned
	// structure, so no synchronization with the physics thread is required.
	// Downstream (proxy->GetGameThreadAPI().GetR/GetX/GetV/GetW) is unchanged.
	Chaos::FSingleParticlePhysicsProxy* resolveProxy(BodyId bodyId) const
	{
		Chaos::FGeometryParticle* particle =
			m_solver.UniqueIdxToGTParticle_External(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
		return particle ? static_cast<Chaos::FSingleParticlePhysicsProxy*>(particle->GetProxy()) : nullptr;
	}

	Chaos::FPBDRigidsSolver& m_solver;
};

static_assert(PhysicsBodyReaderAdapter<ChaosPhysicsBodyReaderAdapter>);
