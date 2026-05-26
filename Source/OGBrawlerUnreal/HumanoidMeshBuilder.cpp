// SPDX-License-Identifier: BUSL-1.1

#include "HumanoidMeshBuilder.h"
#include "OGSimulationUnreal/UGLMTypeConversion.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>
#include "glm/vec3.hpp"

namespace HumanoidMeshBuilder
{

void buildEllipsoidSection(
    const EllipsoidPart& part,
    int32 latitudeSegments,
    int32 longitudeSegments,
    SectionGeometry& outGeometry)
{
    outGeometry.vertices.Reset();
    outGeometry.triangles.Reset();
    outGeometry.normals.Reset();
    outGeometry.uvs.Reset();
    outGeometry.tangents.Reset();
    outGeometry.vertexColors.Reset();

    const glm::vec3 radii     = part.radii;
    const glm::quat rotation  = part.localRotation;
    const glm::vec3 offset    = part.localOffset;

    const int32 numRings   = latitudeSegments + 1;  // lat rings including poles
    const int32 numCols    = longitudeSegments + 1; // lon columns (first == last for seam)

    outGeometry.vertices.Reserve(numRings * numCols);
    outGeometry.normals.Reserve(numRings * numCols);
    outGeometry.uvs.Reserve(numRings * numCols);

    outGeometry.tangents.Reserve(numRings * numCols);

    for (int32 ring = 0; ring <= latitudeSegments; ++ring)
    {
        const float v      = static_cast<float>(ring) / static_cast<float>(latitudeSegments);
        const float lat    = v * glm::pi<float>();   // 0 (north pole) .. pi (south pole)
        const float sinLat = FMath::Sin(lat);
        const float cosLat = FMath::Cos(lat);

        for (int32 col = 0; col <= longitudeSegments; ++col)
        {
            const float u      = static_cast<float>(col) / static_cast<float>(longitudeSegments);
            const float lon    = u * 2.f * glm::pi<float>();
            const float sinLon = FMath::Sin(lon);
            const float cosLon = FMath::Cos(lon);

            // Unit sphere direction
            const glm::vec3 dir(sinLat * cosLon, sinLat * sinLon, cosLat);

            // Ellipsoid vertex in local space (before rotation + offset)
            const glm::vec3 localPos(dir.x * radii.x, dir.y * radii.y, dir.z * radii.z);

            // Analytical normal in local space: normalize(dir / radii) = normalize(p / radii^2) normalised
            const glm::vec3 localNormal = glm::normalize(glm::vec3(dir.x / radii.x, dir.y / radii.y, dir.z / radii.z));

            // Tangent = direction of increasing U (longitude). dpos/dlon scaled by radii.
            // At the poles (sinLat == 0) this vanishes; fall back to +X to avoid NaNs.
            glm::vec3 localTangent(-sinLon * radii.x, cosLon * radii.y, 0.f);
            const float tangentLen = glm::length(localTangent);
            localTangent = (tangentLen > 1e-6f) ? (localTangent / tangentLen) : glm::vec3(1.f, 0.f, 0.f);

            // Apply rotation then offset
            const glm::vec3 worldPos     = rotation * localPos + offset;
            const glm::vec3 worldNormal  = rotation * localNormal;
            const glm::vec3 worldTangent = rotation * localTangent;

            outGeometry.vertices.Add(uglm::toFVector(worldPos));
            outGeometry.normals.Add(uglm::toFVector(worldNormal));
            outGeometry.uvs.Add(FVector2D(u, v));
            // V increases with ring (lat π → south); cross(N, T) produces the +V direction
            // already, so no bitangent flip is needed.
            outGeometry.tangents.Add(FProcMeshTangent(uglm::toFVector(worldTangent), false));
        }
    }

    // Build triangles: latitudeSegments * longitudeSegments quads, 2 triangles each.
    // UE expects triangles wound so the right-hand cross of edges is OPPOSITE the assigned
    // vertex normal (front face = clockwise when viewed along the normal). With outward
    // vertex normals on a UV sphere, that means i0 → i1 → i2 / i1 → i3 → i2.
    outGeometry.triangles.Reserve(latitudeSegments * longitudeSegments * 6);

    for (int32 ring = 0; ring < latitudeSegments; ++ring)
    {
        for (int32 col = 0; col < longitudeSegments; ++col)
        {
            const int32 i0 = ring       * numCols + col;
            const int32 i1 = ring       * numCols + col + 1;
            const int32 i2 = (ring + 1) * numCols + col;
            const int32 i3 = (ring + 1) * numCols + col + 1;

            // Triangle 1
            outGeometry.triangles.Add(i0);
            outGeometry.triangles.Add(i1);
            outGeometry.triangles.Add(i2);

            // Triangle 2
            outGeometry.triangles.Add(i1);
            outGeometry.triangles.Add(i3);
            outGeometry.triangles.Add(i2);
        }
    }
}

void buildConvexHullSection(
    const ConvexHullPart& part,
    SectionGeometry& outGeometry)
{
    outGeometry.vertices.Reset();
    outGeometry.triangles.Reset();
    outGeometry.normals.Reset();
    outGeometry.uvs.Reset();
    outGeometry.tangents.Reset();
    outGeometry.vertexColors.Reset();

    const glm::quat rotation = part.localRotation;
    const glm::vec3 offset   = part.localOffset;

    const int32 numTriangles = static_cast<int32>(part.triangles.size() / 3);
    const int32 numVerts     = numTriangles * 3;

    outGeometry.vertices.Reserve(numVerts);
    outGeometry.normals.Reserve(numVerts);
    outGeometry.uvs.Reserve(numVerts);
    outGeometry.tangents.Reserve(numVerts);
    outGeometry.triangles.Reserve(numVerts);

    // Flat shading: emit each triangle's 3 vertices as new entries (no index reuse)
    // so each face gets its own normal.
    //
    // For each input triangle (i0,i1,i2), the assigned (outward) face normal is
    // chosen so the right-hand cross of (v1-v0) x (v2-v0) is OPPOSITE the assigned
    // normal — matching the ellipsoid builder convention. We negate the geometric
    // RH cross to obtain the assigned outward normal, and emit the triangle indices
    // in the same vertex order so the winding/normal relationship holds.
    for (int32 t = 0; t < numTriangles; ++t)
    {
        const unsigned int i0 = part.triangles[t * 3 + 0];
        const unsigned int i1 = part.triangles[t * 3 + 1];
        const unsigned int i2 = part.triangles[t * 3 + 2];

        const glm::vec3& c0 = part.corners[i0];
        const glm::vec3& c1 = part.corners[i1];
        const glm::vec3& c2 = part.corners[i2];

        // Geometric RH normal of the input winding.
        const glm::vec3 rhCross = glm::cross(c1 - c0, c2 - c0);
        const float     rhLen   = glm::length(rhCross);
        // Assigned outward normal = OPPOSITE of RH cross (UE convention).
        // Defaults to +Z if the triangle is degenerate.
        const glm::vec3 localNormal = (rhLen > 1e-6f)
            ? (-rhCross / rhLen)
            : glm::vec3(0.f, 0.f, 1.f);

        // Tangent: direction of the first edge in local space. Falls back to +X
        // if degenerate.
        const glm::vec3 edge0     = c1 - c0;
        const float     edgeLen   = glm::length(edge0);
        const glm::vec3 localTan  = (edgeLen > 1e-6f)
            ? (edge0 / edgeLen)
            : glm::vec3(1.f, 0.f, 0.f);

        const glm::vec3 worldNormal  = rotation * localNormal;
        const glm::vec3 worldTangent = rotation * localTan;

        const glm::vec3 worldPos0 = rotation * c0 + offset;
        const glm::vec3 worldPos1 = rotation * c1 + offset;
        const glm::vec3 worldPos2 = rotation * c2 + offset;

        const int32 baseIndex = outGeometry.vertices.Num();

        outGeometry.vertices.Add(uglm::toFVector(worldPos0));
        outGeometry.vertices.Add(uglm::toFVector(worldPos1));
        outGeometry.vertices.Add(uglm::toFVector(worldPos2));

        outGeometry.normals.Add(uglm::toFVector(worldNormal));
        outGeometry.normals.Add(uglm::toFVector(worldNormal));
        outGeometry.normals.Add(uglm::toFVector(worldNormal));

        // Per-triangle planar UVs.
        outGeometry.uvs.Add(FVector2D(0.f, 0.f));
        outGeometry.uvs.Add(FVector2D(1.f, 0.f));
        outGeometry.uvs.Add(FVector2D(0.f, 1.f));

        const FProcMeshTangent tan(uglm::toFVector(worldTangent), false);
        outGeometry.tangents.Add(tan);
        outGeometry.tangents.Add(tan);
        outGeometry.tangents.Add(tan);

        // Same vertex order as the input triangle. Because the assigned normal is
        // the OPPOSITE of the RH cross, the right-hand cross of the emitted
        // triangle is opposite the vertex normal (UE convention).
        outGeometry.triangles.Add(baseIndex + 0);
        outGeometry.triangles.Add(baseIndex + 1);
        outGeometry.triangles.Add(baseIndex + 2);
    }
}

void buildTaperedCylinderSection(
    const TaperedCylinderPart& part,
    SectionGeometry& outGeometry)
{
    outGeometry.vertices.Reset();
    outGeometry.triangles.Reset();
    outGeometry.normals.Reset();
    outGeometry.uvs.Reset();
    outGeometry.tangents.Reset();
    outGeometry.vertexColors.Reset();

    const glm::quat rotation = part.localRotation;
    const glm::vec3 offset   = part.localOffset;

    const int32 segments   = FMath::Max(part.segments, 3);
    const float topR       = part.topRadius;
    const float botR       = part.bottomRadius;
    const float halfH      = part.height * 0.5f;
    const float twoPi      = 2.f * glm::pi<float>();

    // ---- Side vertices: 2 rings of (segments + 1) vertices each (seam duplicated for UV) ----
    // Side outward normal at angle θ (cylinder narrows to +Z if topR < botR):
    //   n = normalize(cos θ * height, sin θ * height, (botR - topR))
    const int32 ringCount   = segments + 1;
    const int32 baseSideTop = outGeometry.vertices.Num(); // 0
    // Top ring
    for (int32 i = 0; i <= segments; ++i)
    {
        const float u   = static_cast<float>(i) / static_cast<float>(segments);
        const float th  = u * twoPi;
        const float c   = FMath::Cos(th);
        const float s   = FMath::Sin(th);

        const glm::vec3 localPos(topR * c, topR * s, +halfH);
        glm::vec3 localNormal(c * part.height, s * part.height, botR - topR);
        const float nLen = glm::length(localNormal);
        localNormal = (nLen > 1e-6f) ? (localNormal / nLen) : glm::vec3(c, s, 0.f);

        const glm::vec3 localTan(-s, c, 0.f);

        const glm::vec3 worldPos    = rotation * localPos + offset;
        const glm::vec3 worldNormal = rotation * localNormal;
        const glm::vec3 worldTan    = rotation * localTan;

        outGeometry.vertices.Add(uglm::toFVector(worldPos));
        outGeometry.normals.Add(uglm::toFVector(worldNormal));
        outGeometry.uvs.Add(FVector2D(u, 0.f));
        outGeometry.tangents.Add(FProcMeshTangent(uglm::toFVector(worldTan), false));
    }
    const int32 baseSideBot = outGeometry.vertices.Num(); // = ringCount
    // Bottom ring
    for (int32 i = 0; i <= segments; ++i)
    {
        const float u   = static_cast<float>(i) / static_cast<float>(segments);
        const float th  = u * twoPi;
        const float c   = FMath::Cos(th);
        const float s   = FMath::Sin(th);

        const glm::vec3 localPos(botR * c, botR * s, -halfH);
        glm::vec3 localNormal(c * part.height, s * part.height, botR - topR);
        const float nLen = glm::length(localNormal);
        localNormal = (nLen > 1e-6f) ? (localNormal / nLen) : glm::vec3(c, s, 0.f);

        const glm::vec3 localTan(-s, c, 0.f);

        const glm::vec3 worldPos    = rotation * localPos + offset;
        const glm::vec3 worldNormal = rotation * localNormal;
        const glm::vec3 worldTan    = rotation * localTan;

        outGeometry.vertices.Add(uglm::toFVector(worldPos));
        outGeometry.normals.Add(uglm::toFVector(worldNormal));
        outGeometry.uvs.Add(FVector2D(u, 1.f));
        outGeometry.tangents.Add(FProcMeshTangent(uglm::toFVector(worldTan), false));
    }

    // Side faces: quads between top ring (i, i+1) and bottom ring (i, i+1).
    // Vertex layout (top ring above bottom ring):
    //   t0 ---- t1
    //   |        |
    //   b0 ---- b1
    // Assigned outward normal points away from the central Z axis (+X-ish at θ=0).
    // For the order (t0, b1, b0), the RH cross of (b1-t0) x (b0-t0) at θ=0
    // points roughly in the -X direction (opposite the outward normal) — this
    // matches the UE convention (RH cross OPPOSITE assigned normal).
    // The second triangle (t0, t1, b1) has the same winding sense.
    for (int32 i = 0; i < segments; ++i)
    {
        const int32 t0 = baseSideTop + i;
        const int32 t1 = baseSideTop + i + 1;
        const int32 b0 = baseSideBot + i;
        const int32 b1 = baseSideBot + i + 1;

        outGeometry.triangles.Add(t0);
        outGeometry.triangles.Add(b1);
        outGeometry.triangles.Add(b0);

        outGeometry.triangles.Add(t0);
        outGeometry.triangles.Add(t1);
        outGeometry.triangles.Add(b1);
    }

    // ---- Top cap: triangle fan, normal +Z, separate vertices from side ----
    {
        const glm::vec3 localCapNormal(0.f, 0.f, 1.f);
        const glm::vec3 worldCapNormal = rotation * localCapNormal;
        const glm::vec3 localCapTan(1.f, 0.f, 0.f);
        const glm::vec3 worldCapTan    = rotation * localCapTan;
        const FProcMeshTangent capTan(uglm::toFVector(worldCapTan), false);

        // Center vertex
        const int32 centerIdx = outGeometry.vertices.Num();
        const glm::vec3 localCenter(0.f, 0.f, +halfH);
        outGeometry.vertices.Add(uglm::toFVector(rotation * localCenter + offset));
        outGeometry.normals.Add(uglm::toFVector(worldCapNormal));
        outGeometry.uvs.Add(FVector2D(0.5f, 0.5f));
        outGeometry.tangents.Add(capTan);

        // Rim vertices (segments + 1, seam duplicated)
        const int32 rimBase = outGeometry.vertices.Num();
        for (int32 i = 0; i <= segments; ++i)
        {
            const float u  = static_cast<float>(i) / static_cast<float>(segments);
            const float th = u * twoPi;
            const float c  = FMath::Cos(th);
            const float s  = FMath::Sin(th);
            const glm::vec3 localPos(topR * c, topR * s, +halfH);
            outGeometry.vertices.Add(uglm::toFVector(rotation * localPos + offset));
            outGeometry.normals.Add(uglm::toFVector(worldCapNormal));
            outGeometry.uvs.Add(FVector2D(0.5f + 0.5f * c, 0.5f + 0.5f * s));
            outGeometry.tangents.Add(capTan);
        }

        // Triangle fan. For the top cap the outward normal is +Z.
        // Choose winding so RH cross is OPPOSITE the assigned normal (i.e. -Z).
        // (center, rim[i+1], rim[i]) gives RH cross pointing downward (−Z) for a
        // CCW-when-viewed-from-above rim. The rim is parameterized CCW in θ, so
        // this is the correct ordering.
        for (int32 i = 0; i < segments; ++i)
        {
            outGeometry.triangles.Add(centerIdx);
            outGeometry.triangles.Add(rimBase + i + 1);
            outGeometry.triangles.Add(rimBase + i);
        }
    }

    // ---- Bottom cap: triangle fan, normal -Z, separate vertices from side ----
    {
        const glm::vec3 localCapNormal(0.f, 0.f, -1.f);
        const glm::vec3 worldCapNormal = rotation * localCapNormal;
        const glm::vec3 localCapTan(1.f, 0.f, 0.f);
        const glm::vec3 worldCapTan    = rotation * localCapTan;
        const FProcMeshTangent capTan(uglm::toFVector(worldCapTan), false);

        const int32 centerIdx = outGeometry.vertices.Num();
        const glm::vec3 localCenter(0.f, 0.f, -halfH);
        outGeometry.vertices.Add(uglm::toFVector(rotation * localCenter + offset));
        outGeometry.normals.Add(uglm::toFVector(worldCapNormal));
        outGeometry.uvs.Add(FVector2D(0.5f, 0.5f));
        outGeometry.tangents.Add(capTan);

        const int32 rimBase = outGeometry.vertices.Num();
        for (int32 i = 0; i <= segments; ++i)
        {
            const float u  = static_cast<float>(i) / static_cast<float>(segments);
            const float th = u * twoPi;
            const float c  = FMath::Cos(th);
            const float s  = FMath::Sin(th);
            const glm::vec3 localPos(botR * c, botR * s, -halfH);
            outGeometry.vertices.Add(uglm::toFVector(rotation * localPos + offset));
            outGeometry.normals.Add(uglm::toFVector(worldCapNormal));
            outGeometry.uvs.Add(FVector2D(0.5f + 0.5f * c, 0.5f + 0.5f * s));
            outGeometry.tangents.Add(capTan);
        }

        // Bottom cap outward normal is -Z. RH cross OPPOSITE that means +Z.
        // (center, rim[i], rim[i+1]) gives RH cross pointing upward (+Z) for a
        // CCW-when-viewed-from-above rim — correct.
        for (int32 i = 0; i < segments; ++i)
        {
            outGeometry.triangles.Add(centerIdx);
            outGeometry.triangles.Add(rimBase + i);
            outGeometry.triangles.Add(rimBase + i + 1);
        }
    }
}

} // namespace HumanoidMeshBuilder
