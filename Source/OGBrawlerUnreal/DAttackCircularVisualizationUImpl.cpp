// SPDX-License-Identifier: BUSL-1.1

#include "DAttackCircularVisualizationUImpl.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"
#include "DrawDebugHelpers.h"

namespace
{
	FColor idToColor(unsigned int id)
	{
		switch (id)
		{
		case 0:
			return FColor::Red;
		case 1:
			return FColor::Green;
		case 2:
			return FColor::Blue;
		case 3:
			return FColor::Yellow;
		case 4:
			return FColor::Cyan;
		case 5:
			return FColor::Magenta;
		case 6:
			return FColor::Orange;
		case 7:
			return FColor::Purple;
		case 8:
			return FColor::Turquoise;
		case 9:
			return FColor::Emerald;
		case 10:
			return FColor::Silver;
		case 11:
			return FColor::Silver;
		case 12:
			return FColor::White;
		case 13:
			return FColor::Black;
		default:
			return FColor::Black;
		}
	}
}

void DAttackRendererFunctorUImpl::drawLine(glm::vec3 start, glm::vec3 end, unsigned int colorId, float thickness)  const
{
	DrawDebugLine(m_world, uglm::toFVector(start), uglm::toFVector(end), idToColor(colorId), false, -1.f, 0, thickness);
}

void DAttackRendererFunctorUImpl::drawCircleArc(glm::vec3 centerPosition, glm::vec3 direction, float radius, float angle, unsigned int colorId, float thickness) const
{
	DrawDebugCircleArc(m_world, uglm::toFVector(centerPosition), radius, uglm::toFVector(direction), angle, 10, idToColor(colorId), false, -1.f, 5, thickness);
}

void DAttackRendererFunctorUImpl::drawTriangle(glm::vec3 one, glm::vec3 two, glm::vec3 three, unsigned int colorId) const
{
	std::vector<glm::vec3> verts;
	verts.push_back(one);
	verts.push_back(two);
	verts.push_back(three);
	std::vector<unsigned int> indices;
	indices.resize(3);
	indices[0] = 0; indices[1] = 2; indices[2] = 1;

	TArray<FVector> uVerts;
	for (const glm::vec3& vert : verts)
		uVerts.Add(FVector(vert.x, vert.y, vert.z));

	TArray<int32> fIndices;
	for (unsigned int index : indices)
		fIndices.Add(index);

	DrawDebugMesh(m_world, uVerts, fIndices, idToColor(colorId), false, -1.f, 5.f);
}

void DAttackRendererFunctorUImpl::drawPoint(glm::vec3 point) const
{
	DrawDebugPoint(m_world, uglm::toFVector(point), 50.f, FColor::Red, false, 0.5f, 0);
}

void DAttackRendererFunctorUImpl::drawSolidBox(glm::vec3 position, const glm::mat3& rotation, glm::vec3 extents, unsigned int colorId) const
{
	FTransform uTransform = uglm::toFtransform(glm::mat4(rotation));
	FQuat uRotation = uTransform.GetRotation();
	DrawDebugSolidBox(m_world, uglm::toFVector(position), uglm::toFVector(extents), uRotation, idToColor(colorId), false, -1.f, 0);
}

void DAttackRendererFunctorUImpl::drawMesh(const std::vector<glm::vec3>& vertices, const std::vector<unsigned int>& indicies, unsigned int colorId) const
{
	TArray<FVector> uVertices;
	TArray<int32> uIndices;

	for (const glm::vec3& vert : vertices)
		uVertices.Add(uglm::toFVector(vert));

	for (unsigned int index : indicies)
		uIndices.Add(index);

	FColor color = idToColor(colorId);
	color.A = 150;
	DrawDebugMesh(m_world, uVertices, uIndices, color, false, -1.f, 4.f);
}


#pragma optimize( "", on )
