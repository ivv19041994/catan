#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"

class UStaticMesh;
class UStaticMeshComponent;

enum class ECatanResourceAnimation : uint8
{
    None,
    Sway,
    Bob,
    Pulse,
    Drift
};

struct FCatanResourceVisualCallbacks
{
    TFunction<UStaticMeshComponent*(const FString&, UStaticMesh*, const FVector&,
        const FVector&, const FLinearColor&, const FRotator&)> AddStatic;
    TFunction<void(const FString&, const FVector&, float, float, const FLinearColor&)> AddHexPyramid;
    TFunction<void(UStaticMeshComponent*, ECatanResourceAnimation, float)> Animate;
};

namespace CatanResourceVisuals
{
CATAN_API void BuildCluster(ECatanResource Resource, int32 VisualId, float YawDegrees,
    UStaticMesh* Cube, UStaticMesh* Sphere, UStaticMesh* Cylinder, UStaticMesh* Cone,
    const FCatanResourceVisualCallbacks& Callbacks);
}
