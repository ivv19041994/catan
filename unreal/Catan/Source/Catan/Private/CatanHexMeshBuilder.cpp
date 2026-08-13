#include "CatanHexMeshBuilder.h"

namespace
{
void AddVertex(FCatanHexMeshBuffers& Mesh, const FVector& Position, const FVector& Normal,
    const FVector2D& UV, const FLinearColor& Color)
{
    Mesh.Vertices.Add(Position);
    Mesh.Normals.Add(Normal);
    Mesh.UVs.Add(UV);
    Mesh.Colors.Add(Color);
    Mesh.Tangents.Add(FProcMeshTangent(1, 0, 0));
}
}

void CatanHexMesh::AppendTop(FCatanHexMeshBuffers& Mesh, const FVector& Center,
    float Radius, const FLinearColor& Color, float StartAngleDegrees)
{
    const int32 CenterIndex = Mesh.Vertices.Num();
    AddVertex(Mesh, Center, FVector::UpVector, FVector2D(0.5f, 0.5f), Color);
    for (int32 Corner = 0; Corner < 6; ++Corner)
    {
        const float Angle = FMath::DegreesToRadians(StartAngleDegrees + Corner * 60.0f);
        AddVertex(Mesh,
            Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * Radius,
            FVector::UpVector,
            FVector2D(0.5f + FMath::Cos(Angle) * 0.5f, 0.5f + FMath::Sin(Angle) * 0.5f),
            Color);
        // This is the same clockwise winding used by the visible gameplay board.
        Mesh.Triangles.Append({CenterIndex,
            CenterIndex + (Corner + 1) % 6 + 1,
            CenterIndex + Corner + 1});
    }
}

void CatanHexMesh::AppendPrism(FCatanHexMeshBuffers& Mesh, const FVector& Center,
    float Radius, float TopOffset, float BottomZ, const FLinearColor& TopColor,
    const FLinearColor& SideColor, float StartAngleDegrees)
{
    const FVector TopCenter = Center + FVector(0, 0, TopOffset);
    AppendTop(Mesh, TopCenter, Radius, TopColor, StartAngleDegrees);
    for (int32 Corner = 0; Corner < 6; ++Corner)
    {
        const float AngleA = FMath::DegreesToRadians(StartAngleDegrees + Corner * 60.0f);
        const float AngleB = FMath::DegreesToRadians(StartAngleDegrees + (Corner + 1) * 60.0f);
        const FVector TopA = TopCenter + FVector(FMath::Cos(AngleA), FMath::Sin(AngleA), 0) * Radius;
        const FVector TopB = TopCenter + FVector(FMath::Cos(AngleB), FMath::Sin(AngleB), 0) * Radius;
        const FVector BottomA(TopA.X, TopA.Y, BottomZ);
        const FVector BottomB(TopB.X, TopB.Y, BottomZ);
        const FVector Normal = ((TopA + TopB) * 0.5f - FVector(0, 0, TopA.Z)).GetSafeNormal();
        const int32 Side = Mesh.Vertices.Num();
        AddVertex(Mesh, TopA, Normal, FVector2D(0, 0), SideColor);
        AddVertex(Mesh, TopB, Normal, FVector2D(1, 0), SideColor);
        AddVertex(Mesh, BottomB, Normal, FVector2D(1, 1), SideColor);
        AddVertex(Mesh, BottomA, Normal, FVector2D(0, 1), SideColor);
        Mesh.Triangles.Append({Side, Side + 2, Side + 1, Side, Side + 3, Side + 2});
    }
}
