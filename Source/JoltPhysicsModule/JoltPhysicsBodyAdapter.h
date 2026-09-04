#pragma once

// SPDX-License-Identifier: MPL-2.0

#include "OGSimulation/PhysicsBodyAdapter.h"
#include "OGSimulation/BodyId.h"
#include "JoltGLMConversion.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
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

	// Extract the OGSim BodyId from a Jolt native body id. Stateless / idempotent
	// / const — the "body id" IS the Jolt index-and-sequence-number that Jolt
	// itself assigns at body creation; this just wraps it in the engine-agnostic
	// BodyId type. Safe to call multiple times with the same joltBodyId; every
	// call returns the same value. Not part of the PhysicsBodyAdapter concept —
	// engine-specific convenience for code that has a Jolt BodyID and needs the
	// abstract BodyId to feed into other adapter methods.
	BodyId getBodyId(JPH::BodyID joltBodyId) const
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

	// Accumulate an acceleration for THIS step; engine applies force = a * mass.
	// Jolt is strict SI and mass-scaled: BodyInterface::AddForce takes newtons
	// (kg*m/s^2) applied at the centre of mass, so the seam's cm/s^2 is scaled
	// cm -> m exactly as setBodyLinearVelocity above scales cm/s -> m/s, and then
	// multiplied by the body's mass.
	// JPH::BodyInterface exposes no mass accessor, so the mass is read from the
	// shape's mass properties. That IS the body's mass under Jolt's default
	// EOverrideMassProperties::CalculateMassAndInertia; a body created with a mass
	// override would need its mass plumbed in instead. Nothing in this tree creates
	// Jolt bodies today, so the default is the only case that exists.
	void addBodyAcceleration(BodyId bodyId, const glm::vec3& acceleration)
	{
		JPH::BodyID joltId(bodyId.value);

		const JPH::RefConst<JPH::Shape> shape = m_bodyInterface.GetShape(joltId);
		if (shape.GetPtr() == nullptr)
		{
			return;
		}

		const float mass = shape->GetMassProperties().mMass;
		m_bodyInterface.AddForce(joltId, toJolt(acceleration * 0.01f) * mass); // cm/s^2 -> m/s^2, * kg -> N
	}

	// Instantaneous dv before this step's solve; engine applies impulse = dv * mass.
	// AddLinearVelocity is used rather than AddImpulse(dv * mass): Jolt defines
	// Body::AddImpulse as SetLinearVelocityClamped(v + impulse * invMass) and
	// BodyInterface::AddLinearVelocity as SetLinearVelocityClamped(v + dv), so for
	// impulse = dv * mass the two are the same operation -- but this form needs no
	// mass lookup and therefore cannot silently disagree with the Chaos adapter's
	// SetV(GetV() + dv).
	void addBodyVelocityChange(BodyId bodyId, const glm::vec3& velocityChange)
	{
		JPH::BodyID joltId(bodyId.value);
		m_bodyInterface.AddLinearVelocity(joltId, toJolt(velocityChange * 0.01f)); // cm/s -> m/s
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
