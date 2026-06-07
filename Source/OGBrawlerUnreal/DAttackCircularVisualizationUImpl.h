#pragma once

// SPDX-License-Identifier: BUSL-1.1

#include "glm/vec3.hpp"
#include "glm/mat3x3.hpp"
#include <vector>

struct DAttackRendererFunctorUImpl
{
	DAttackRendererFunctorUImpl(UWorld* world)
		: m_world(world)
	{}
	void drawLine(glm::vec3 start, glm::vec3 end, unsigned int colorId, float thickness) const;
	void drawCircleArc(glm::vec3 centerPosition, glm::vec3 direction, float radius, float angle, unsigned int colorId, float thickness) const;
	void drawTriangle(glm::vec3 one, glm::vec3 two, glm::vec3 three, unsigned int colorId) const;
	void drawPoint(glm::vec3 point) const;
	void drawSphere(glm::vec3 position, float radius, unsigned int colorId, float lifetime = -1.f) const;
	void drawSolidBox(glm::vec3 position, const glm::mat3& rotation, glm::vec3 extents, unsigned int colorId) const;
	void drawMesh(const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indicies, unsigned int colorId) const;

private:
	DAttackRendererFunctorUImpl() = default;

	UWorld* m_world;
};

