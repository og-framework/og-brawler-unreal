#pragma once

// SPDX-License-Identifier: MPL-2.0

// glm ↔ Jolt type conversion utilities.
// No axis swizzle — both Jolt (with gravity (0,0,-9.81)) and the game use Z-up.

#include <Jolt/Jolt.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Mat44.h>
#include <Jolt/Math/Float4.h>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------
inline JPH::Vec3 toJolt(const glm::vec3& v)
{
	return JPH::Vec3(v.x, v.y, v.z);
}

inline glm::vec3 toGlm(const JPH::Vec3& v)
{
	return glm::vec3(v.GetX(), v.GetY(), v.GetZ());
}

// ---------------------------------------------------------------------------
// Quat   (glm stores as w,x,y,z — Jolt constructor is x,y,z,w)
// ---------------------------------------------------------------------------
inline JPH::Quat toJolt(const glm::quat& q)
{
	return JPH::Quat(q.x, q.y, q.z, q.w);
}

inline glm::quat toGlm(const JPH::Quat& q)
{
	return glm::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}

// ---------------------------------------------------------------------------
// Mat44  (both glm and Jolt store column-major)
// ---------------------------------------------------------------------------
inline JPH::Mat44 toJolt(const glm::mat4& m)
{
	JPH::Float4 cols[4];
	for (int c = 0; c < 4; ++c)
		cols[c] = JPH::Float4(m[c][0], m[c][1], m[c][2], m[c][3]);
	return JPH::Mat44::sLoadFloat4x4(cols);
}

inline glm::mat4 toGlm(const JPH::Mat44& m)
{
	JPH::Float4 cols[4];
	m.StoreFloat4x4(cols);
	glm::mat4 result;
	for (int c = 0; c < 4; ++c)
		result[c] = glm::vec4(cols[c].x, cols[c].y, cols[c].z, cols[c].w);
	return result;
}
