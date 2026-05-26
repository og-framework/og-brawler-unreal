#pragma once

// SPDX-License-Identifier: MPL-2.0

// FVector / FQuat / FTransform ↔ Jolt type conversion utilities.
// Unit conversion: UE centimetres ↔ Jolt metres.

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Mat44.h>

#include "Math/Vector.h"
#include "Math/Quat.h"
#include "Math/Transform.h"

// ---------------------------------------------------------------------------
// Vec3 — with cm ↔ m conversion
// ---------------------------------------------------------------------------
inline JPH::Vec3 toJolt(const FVector& v)
{
	return JPH::Vec3(
		static_cast<float>(v.X * 0.01),
		static_cast<float>(v.Y * 0.01),
		static_cast<float>(v.Z * 0.01)
	);
}

inline FVector toUE(const JPH::Vec3& v)
{
	return FVector(
		v.GetX() * 100.0,
		v.GetY() * 100.0,
		v.GetZ() * 100.0
	);
}

// ---------------------------------------------------------------------------
// Vec3 — no unit conversion (for directions, normals, etc.)
// ---------------------------------------------------------------------------
inline JPH::Vec3 toJoltDir(const FVector& v)
{
	return JPH::Vec3(
		static_cast<float>(v.X),
		static_cast<float>(v.Y),
		static_cast<float>(v.Z)
	);
}

inline FVector toUEDir(const JPH::Vec3& v)
{
	return FVector(v.GetX(), v.GetY(), v.GetZ());
}

// ---------------------------------------------------------------------------
// Quat — no unit conversion needed
// ---------------------------------------------------------------------------
inline JPH::Quat toJolt(const FQuat& q)
{
	return JPH::Quat(
		static_cast<float>(q.X),
		static_cast<float>(q.Y),
		static_cast<float>(q.Z),
		static_cast<float>(q.W)
	);
}

inline FQuat toUE(const JPH::Quat& q)
{
	return FQuat(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

// ---------------------------------------------------------------------------
// FTransform ↔ Jolt Mat44
// ---------------------------------------------------------------------------
inline JPH::Mat44 toJolt(const FTransform& t)
{
	JPH::Vec3 translation = toJolt(t.GetTranslation());
	JPH::Quat rotation    = toJolt(t.GetRotation());
	return JPH::Mat44::sRotationTranslation(rotation, translation);
}

inline FTransform toUE(const JPH::Mat44& m)
{
	FVector   translation = toUE(m.GetTranslation());
	FQuat     rotation    = toUE(m.GetQuaternion());
	return FTransform(rotation, translation);
}
