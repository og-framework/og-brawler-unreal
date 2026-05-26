#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulation/PhysicsBodyReaderAdapter.h"
#include "OGSimulation/BodyId.h"
#include "JoltGLMConversion.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>

// Read-only Jolt adapter for game-thread visualization.
// Jolt's BodyInterface is internally locked — no GT/PT distinction needed.
// This adapter exists for type-level consistency with ChaosPhysicsBodyReaderAdapter so
// visualization code can be parameterized on PhysicsBodyReaderAdapter.
class JoltPhysicsBodyReaderAdapter
{
public:
	explicit JoltPhysicsBodyReaderAdapter(const JPH::BodyInterface& bodyInterface)
		: m_bodyInterface(bodyInterface)
	{}

	glm::mat4 getBodyTransform(BodyId bodyId) const
	{
		JPH::BodyID joltId(bodyId.value);
		JPH::RVec3 pos;
		JPH::Quat rot;
		m_bodyInterface.GetPositionAndRotation(joltId, pos, rot);
		const glm::vec3 glmPos = glm::vec3(float(pos.GetX()), float(pos.GetY()), float(pos.GetZ())) * 100.0f;
		return glm::translate(glm::mat4(1.0f), glmPos) * glm::mat4_cast(toGlm(rot));
	}

	glm::vec3 getBodyInertiaTensor(BodyId bodyId) const
	{
		JPH::BodyID joltId(bodyId.value);
		const JPH::Mat44 invInertia = m_bodyInterface.GetInverseInertia(joltId);
		const JPH::Mat44 inertia = invInertia.Inversed3x3();
		return glm::vec3(inertia(0, 0), inertia(1, 1), inertia(2, 2));
	}

	PhysicsBodyState captureBodyState(BodyId bodyId) const
	{
		JPH::BodyID joltId(bodyId.value);
		JPH::RVec3 pos;
		JPH::Quat rot;
		m_bodyInterface.GetPositionAndRotation(joltId, pos, rot);
		const JPH::Vec3 linVel = m_bodyInterface.GetLinearVelocity(joltId);
		const JPH::Vec3 angVel = m_bodyInterface.GetAngularVelocity(joltId);

		PhysicsBodyState s;
		s.position        = glm::vec3(float(pos.GetX()), float(pos.GetY()), float(pos.GetZ())) * 100.0f;
		s.rotation        = toGlm(rot);
		s.linearVelocity  = toGlm(linVel) * 100.0f;
		s.angularVelocity = toGlm(angVel);
		return s;
	}

private:
	const JPH::BodyInterface& m_bodyInterface;
};

static_assert(PhysicsBodyReaderAdapter<JoltPhysicsBodyReaderAdapter>);
