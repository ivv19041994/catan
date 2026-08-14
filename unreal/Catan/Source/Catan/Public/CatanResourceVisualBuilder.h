#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;

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
    // Authored meshes keep their original material slots (bark, leaves, wind, etc.).
    TFunction<void(const FString&, UStaticMesh*, const FVector&, const FVector&,
        const FRotator&)> AddAuthoredStatic;
    // Assets whose authored materials do not support instancing use ordinary mesh components.
    TFunction<void(const FString&, UStaticMesh*, const FVector&, const FVector&,
        const FRotator&)> AddAuthoredUniqueStatic;
    TFunction<void(const FString&, UStaticMesh*, const FVector&, const FVector&,
        const FLinearColor&, const FRotator&)> AddColoredInstance;
    TFunction<void(const FString&, UStaticMesh*, UMaterialInterface*, const FVector&,
        const FVector&, const FRotator&)> AddMaterialStatic;
    TFunction<void(const FString&, const FVector&, float, float, const FLinearColor&)> AddHexPyramid;
    TFunction<void(UStaticMeshComponent*, ECatanResourceAnimation, float)> Animate;
    int32 AuthoredForestTreeBudget = 7;
};

namespace CatanResourceVisuals
{
CATAN_API UMaterialInterface* GetGroundMaterial(ECatanResource Resource);
CATAN_API void BuildCluster(ECatanResource Resource, int32 VisualId, float YawDegrees,
    UStaticMesh* Cube, UStaticMesh* Sphere, UStaticMesh* Cylinder, UStaticMesh* Cone,
    const FCatanResourceVisualCallbacks& Callbacks);
}
