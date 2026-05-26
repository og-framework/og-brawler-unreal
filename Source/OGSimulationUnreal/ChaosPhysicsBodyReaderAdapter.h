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
	Chaos::FSingleParticlePhysicsProxy* resolveProxy(BodyId bodyId) const
	{
		return m_solver.GetParticleProxy_PT(Chaos::FUniqueIdx{static_cast<int32>(bodyId.value)});
	}

	Chaos::FPBDRigidsSolver& m_solver;
};

static_assert(PhysicsBodyReaderAdapter<ChaosPhysicsBodyReaderAdapter>);
