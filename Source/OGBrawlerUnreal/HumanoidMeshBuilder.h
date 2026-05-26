#pragma once
// SPDX-License-Identifier: BUSL-1.1

#include "CoreMinimal.h"
#include "ProceduralMeshComponent/Public/ProceduralMeshComponent.h"
#include "OGBrawler/CharacterVisualizationData.h"

namespace HumanoidMeshBuilder
{
    struct SectionGeometry
    {
        TArray<FVector>          vertices;
        TArray<int32>            triangles;
        TArray<FVector>          normals;
        TArray<FVector2D>        uvs;
        TArray<FProcMeshTangent> tangents;
        TArray<FLinearColor>     vertexColors;
    };

    void buildEllipsoidSection(
        const EllipsoidPart& part,
        int32 latitudeSegments,
        int32 longitudeSegments,
        SectionGeometry& outGeometry);

    void buildConvexHullSection(
        const ConvexHullPart& part,
        SectionGeometry& outGeometry);

    void buildTaperedCylinderSection(
        const TaperedCylinderPart& part,
        SectionGeometry& outGeometry);
}
