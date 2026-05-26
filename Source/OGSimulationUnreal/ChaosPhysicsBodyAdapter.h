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
#include "PBDRigidsSolver.h"

class ChaosPhysicsBodyAdapter
{
public:
	explicit ChaosPhysicsBodyAdapter(Chaos::FPBDRigidsSolver& solver)
		: m_solver(solver)
	{}

	BodyId registerBody(FBodyInstanceAsyncPhysicsTickHandle handle) const
	{
		const uint32_t nativeId = static_cast<uint32_t>(handle.Proxy->GetParticle_LowLevel()->UniqueIdx().Idx);
		return BodyId{nativeId};
	}

	bool isBodyResolvable(BodyId bodyId) const { return resolveProxy(bodyId) != nullptr; }

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

	glm::vec3 getBodyInertiaTensor(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			const Chaos::FMatrix33 inertiaTensor = Chaos::FParticleUtilitiesXR::GetWorldInertia(proxy->GetPhysicsThreadAPI());
			return glm::vec3(inertiaTensor.M[0][0], inertiaTensor.M[1][1], inertiaTensor.M[2][2]);
		}
		return glm::vec3(0.f);
	}

	PhysicsBodyState captureBodyState(BodyId bodyId) const
	{
		if (auto* proxy = resolveProxy(bodyId))
		{
			auto* ptApi = proxy->GetPhysicsThreadAPI();
			PhysicsBodyState s;
			s.position = uglm::toGLMVec3(ptApi->GetX());
			s.rotation = uglm::toGLMQuat(FQuat(ptApi->GetR()));
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
