#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

struct FCatanHexMeshBuffers
{
    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UVs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;
};

namespace CatanHexMesh
{
CATAN_API void AppendTop(FCatanHexMeshBuffers& Mesh, const FVector& Center,
    float Radius, const FLinearColor& Color, float StartAngleDegrees = 30.0f);

CATAN_API void AppendPrism(FCatanHexMeshBuffers& Mesh, const FVector& Center,
    float Radius, float TopOffset, float BottomZ, const FLinearColor& TopColor,
    const FLinearColor& SideColor, float StartAngleDegrees = 30.0f);
}
