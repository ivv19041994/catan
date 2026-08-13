#include "CatanResourceVisualBuilder.h"

#include "Components/StaticMeshComponent.h"

void CatanResourceVisuals::BuildCluster(ECatanResource Resource, int32 VisualId, float YawDegrees,
    UStaticMesh* Cube, UStaticMesh* Sphere, UStaticMesh* Cylinder, UStaticMesh* Cone,
    const FCatanResourceVisualCallbacks& C)
{
    const FRotator Yaw(0, YawDegrees, 0);
    auto Rotate = [&Yaw](const FVector& Value) { return Yaw.RotateVector(Value); };
    auto Add = [&](const TCHAR* Kind, int32 Item, UStaticMesh* Mesh, const FVector& Local,
        const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator)
    {
        return C.AddStatic(FString::Printf(TEXT("Hex%d%s%d"), VisualId, Kind, Item), Mesh,
            Rotate(Local), Scale, Color, Rotation);
    };
    auto Animate = [&C](UStaticMeshComponent* Component, ECatanResourceAnimation Kind, float Phase)
    {
        if (C.Animate) C.Animate(Component, Kind, Phase);
    };

    switch (Resource)
    {
    case ECatanResource::Wood:
        for (int32 Item = 0; Item < 5; ++Item)
        {
            const FVector Local(-112.0f + Item * 56.0f, 70.0f + (Item % 2) * 25.0f, 22.0f);
            Add(TEXT("Trunk"), Item, Cylinder, Local, FVector(0.085f, 0.085f, 0.27f + (Item % 3) * 0.035f),
                FLinearColor(0.24f, 0.09f, 0.025f));
            Animate(Add(TEXT("TreeLower"), Item, Cone, Local + FVector(0, 0, 38), FVector(0.39f, 0.39f, 0.46f),
                FLinearColor(0.018f, 0.20f + Item * 0.012f, 0.04f)), ECatanResourceAnimation::Sway, Item * 0.65f);
            Animate(Add(TEXT("TreeUpper"), Item, Cone, Local + FVector(0, 0, 66), FVector(0.29f, 0.29f, 0.39f),
                FLinearColor(0.025f, 0.28f + Item * 0.01f, 0.055f)), ECatanResourceAnimation::Sway, Item * 0.65f + 0.2f);
        }
        Add(TEXT("ForestStone"), 0, Sphere, FVector(112, 72, 13), FVector(0.19f, 0.14f, 0.10f),
            FLinearColor(0.25f, 0.27f, 0.22f));
        break;
    case ECatanResource::Clay:
        Add(TEXT("Quarry"), 0, Cylinder, FVector(0, 86, 7), FVector(1.28f, 0.82f, 0.06f),
            FLinearColor(0.28f, 0.055f, 0.018f));
        for (int32 Item = 0; Item < 7; ++Item)
        {
            const int32 Row = Item / 4;
            Add(TEXT("Brick"), Item, Cube,
                FVector(-105.0f + (Item % 4) * 66.0f + Row * 22.0f, 70.0f + Row * 38.0f, 17.0f + Row * 12.0f),
                FVector(0.30f, 0.19f, 0.11f), FLinearColor(0.62f + Item * 0.018f, 0.095f, 0.025f),
                FRotator(0, YawDegrees + (Item % 2) * 7.0f, 0));
        }
        for (int32 Item = 0; Item < 3; ++Item)
            Animate(Add(TEXT("ClayDust"), Item, Sphere, FVector(-70 + Item * 70, 126, 28 + Item * 5),
                FVector(0.10f + Item * 0.025f), FLinearColor(0.70f, 0.30f, 0.10f)),
                ECatanResourceAnimation::Drift, Item * 1.4f);
        break;
    case ECatanResource::Hay:
        for (int32 Item = 0; Item < 11; ++Item)
        {
            const int32 Row = Item / 6;
            const FVector Local(-120.0f + (Item % 6) * 47.0f,
                66.0f + Row * 46.0f + (Item % 2) * 7.0f, 27.0f);
            Animate(Add(TEXT("HayStalk"), Item, Cylinder, Local,
                FVector(0.028f, 0.028f, 0.43f + (Item % 3) * 0.035f),
                FLinearColor(0.94f, 0.66f + (Item % 3) * 0.045f, 0.035f),
                FRotator(Item % 2 ? 3.0f : -3.0f, YawDegrees, 0)), ECatanResourceAnimation::Sway, Item * 0.34f);
            Animate(Add(TEXT("HayHead"), Item, Sphere, Local + FVector(0, 0, 45 + (Item % 3) * 3),
                FVector(0.055f, 0.055f, 0.12f), FLinearColor(1.0f, 0.82f, 0.12f)),
                ECatanResourceAnimation::Sway, Item * 0.34f);
        }
        break;
    case ECatanResource::Sheep:
        for (int32 Item = 0; Item < 3; ++Item)
        {
            const FVector Local(-92.0f + Item * 92.0f, 74.0f + (Item % 2) * 42.0f, 30.0f);
            const float Phase = Item * 1.8f;
            Animate(Add(TEXT("SheepBody"), Item, Sphere, Local, FVector(0.34f, 0.25f, 0.25f),
                FLinearColor(0.94f, 0.95f, 0.89f)), ECatanResourceAnimation::Bob, Phase);
            Animate(Add(TEXT("SheepHead"), Item, Sphere, Local + FVector(29, 0, -3), FVector(0.135f),
                FLinearColor(0.10f, 0.09f, 0.08f)), ECatanResourceAnimation::Bob, Phase);
            for (int32 Leg = 0; Leg < 4; ++Leg)
                Animate(Add(TEXT("SheepLeg"), Item * 4 + Leg, Cylinder,
                    Local + FVector(Leg < 2 ? -16 : 16, Leg % 2 ? -12 : 12, -20),
                    FVector(0.035f, 0.035f, 0.17f), FLinearColor(0.12f, 0.10f, 0.08f)),
                    ECatanResourceAnimation::Bob, Phase);
        }
        break;
    case ECatanResource::Stone:
        if (C.AddHexPyramid)
        {
            const FVector LargeBase = Rotate(FVector(-42, 82, 1));
            const FVector SmallBase = Rotate(FVector(62, 92, 1));
            C.AddHexPyramid(FString::Printf(TEXT("Hex%dLargeMountain"), VisualId), LargeBase,
                75.6f, 142.8f, FLinearColor(0.30f, 0.34f, 0.41f));
            C.AddHexPyramid(FString::Printf(TEXT("Hex%dLargeSnow"), VisualId), LargeBase + FVector(0, 0, 91),
                28.0f, 51.8f, FLinearColor(0.90f, 0.95f, 1.0f));
            C.AddHexPyramid(FString::Printf(TEXT("Hex%dSmallMountain"), VisualId), SmallBase,
                54.6f, 100.8f, FLinearColor(0.36f, 0.39f, 0.46f));
            C.AddHexPyramid(FString::Printf(TEXT("Hex%dSmallSnow"), VisualId), SmallBase + FVector(0, 0, 64.4f),
                21.0f, 36.4f, FLinearColor(0.92f, 0.96f, 1.0f));
        }
        break;
    case ECatanResource::Desert:
        for (int32 Item = 0; Item < 5; ++Item)
            Animate(Add(TEXT("Dune"), Item, Sphere,
                FVector(-112.0f + Item * 56.0f, 72.0f + (Item % 2) * 36.0f, 11.0f),
                FVector(0.48f, 0.25f + (Item % 2) * 0.04f, 0.10f),
                FLinearColor(0.70f + Item * 0.028f, 0.48f, 0.20f)), ECatanResourceAnimation::Pulse, Item * 0.72f);
        Add(TEXT("CactusStem"), 0, Cylinder, FVector(103, 88, 29), FVector(0.075f, 0.075f, 0.36f),
            FLinearColor(0.08f, 0.34f, 0.12f));
        Animate(Add(TEXT("CactusTop"), 0, Sphere, FVector(103, 88, 66), FVector(0.085f, 0.085f, 0.13f),
            FLinearColor(0.10f, 0.43f, 0.15f)), ECatanResourceAnimation::Sway, 0.4f);
        break;
    }
}
