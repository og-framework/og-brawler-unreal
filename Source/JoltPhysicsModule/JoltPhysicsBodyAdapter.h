#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulation/PhysicsBodyAdapter.h"
#include "OGSimulation/BodyId.h"
#include "JoltGLMConversion.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

class JoltPhysicsBodyAdapter
{
public:
	explicit JoltPhysicsBodyAdapter(JPH::BodyInterface& bodyInterface)
		: m_bodyInterface(bodyInterface)
	{
	}

	BodyId registerBody(JPH::BodyID joltBodyId) const
	{
		return BodyId{joltBodyId.GetIndexAndSequenceNumber()};
	}

	glm::mat4 getBodyTransform(BodyId bodyId) const
	{
		JPH::BodyID joltId(bodyId.value);

		JPH::RVec3 pos;
		JPH::Quat rot;
		m_bodyInterface.GetPositionAndRotation(joltId, pos, rot);

		const glm::vec3 glmPos = glm::vec3(float(pos.GetX()), float(pos.GetY()), float(pos.GetZ())) * 100.0f; // m → cm
		const glm::quat glmRot = toGlm(rot);
		return glm::translate(glm::mat4(1.0f), glmPos) * glm::mat4_cast(glmRot);
	}

	void setBodyTransform(BodyId bodyId, const glm::mat4& transform)
	{
		JPH::BodyID joltId(bodyId.value);

		const glm::vec3 pos = glm::vec3(transform[3]);
		const glm::quat rot = glm::quat_cast(transform);

		m_bodyInterface.SetPositionAndRotation(
			joltId,
			JPH::RVec3(toJolt(pos * 0.01f)), // cm → m
			toJolt(rot),
			JPH::EActivation::DontActivate);
	}

	void addBodyTorque(BodyId bodyId, const glm::vec3& torque)
	{
		JPH::BodyID joltId(bodyId.value);
		m_bodyInterface.AddTorque(joltId, toJolt(torque));
	}

	void setBodyAngularVelocity(BodyId bodyId, const glm::vec3& vel)
	{
		JPH::BodyID joltId(bodyId.value);
		m_bodyInterface.SetAngularVelocity(joltId, toJolt(vel));
	}

	void setBodyLinearVelocity(BodyId bodyId, const glm::vec3& vel)
	{
		JPH::BodyID joltId(bodyId.value);
		m_bodyInterface.SetLinearVelocity(joltId, toJolt(vel * 0.01f)); // cm/s → m/s
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
		s.position        = glm::vec3(float(pos.GetX()), float(pos.GetY()), float(pos.GetZ())) * 100.0f;  // m → cm
		s.rotation        = toGlm(rot);
		s.linearVelocity  = toGlm(linVel) * 100.0f;          // m/s → cm/s
		s.angularVelocity = toGlm(angVel);                    // rad/s — no scaling
		return s;
	}

private:
	JPH::BodyInterface& m_bodyInterface;
};

static_assert(PhysicsBodyAdapter<JoltPhysicsBodyAdapter>);
