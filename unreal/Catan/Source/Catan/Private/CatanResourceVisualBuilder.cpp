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
        {
            const FVector StackCenters[] = {
                FVector(-90, 54, 0), FVector(84, 47, 0),
                FVector(-76, 130, 0), FVector(92, 120, 0)
            };
            const float StackYaws[] = {-18.0f, 11.0f, 27.0f, -8.0f};
            const FVector LayerJitters[] = {
                FVector::ZeroVector, FVector(2.5f, -1.5f, 0), FVector(-1.5f, 2.0f, 0)
            };
            int32 BrickId = 0;
            for (int32 Stack = 0; Stack < UE_ARRAY_COUNT(StackCenters); ++Stack)
            {
                for (int32 Layer = 0; Layer < 3; ++Layer)
                {
                    const bool bAcross = Layer == 1;
                    const float LayerYaw = StackYaws[Stack] + (bAcross ? 90.0f : 0.0f)
                        + (Layer - 1) * 2.0f;
                    const FVector SideDirection = FRotator(0, LayerYaw, 0).RotateVector(FVector::YAxisVector);
                    for (int32 Brick = 0; Brick < 2; ++Brick)
                    {
                        const float Side = Brick == 0 ? -7.0f : 7.0f;
                        Add(TEXT("Brick"), BrickId++, Cube,
                            StackCenters[Stack] + LayerJitters[Layer] + SideDirection * Side
                                + FVector(0, 0, 5.0f + Layer * 7.2f),
                            FVector(0.24f, 0.11f, 0.07f),
                            FLinearColor(0.61f + Layer * 0.035f, 0.085f, 0.020f),
                            FRotator(0, YawDegrees + LayerYaw, 0));
                    }
                }
            }
        }
        break;
    case ECatanResource::Hay:
        {
            const FLinearColor BaleColor(0.62f, 0.39f, 0.035f);
            const FVector Locations[] = {
                FVector(-82, 70, 16), FVector(-48, 72, 18),
                FVector(38, 108, 16), FVector(72, 106, 18),
                FVector(112, 52, 18)
            };
            const bool Standing[] = {true, false, true, false, false};
            for (int32 Item = 0; Item < UE_ARRAY_COUNT(Locations); ++Item)
            {
                const FRotator Rotation = Standing[Item]
                    ? FRotator(0, YawDegrees, 0)
                    : FRotator(90.0f, YawDegrees + Item * 31.0f, 0);
                FVector Location = Locations[Item];
                Location.Z = Standing[Item] ? 28.0f : 18.0f;
                Add(TEXT("HayBale"), Item, Cylinder, Location,
                    FVector(0.36f, 0.36f, 0.56f),
                    BaleColor * (0.92f + Item * 0.018f), Rotation);
            }
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
                    Local + FVector(Leg < 2 ? -14 : 14, Leg % 2 ? -10 : 10, -20),
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
        {
            const FLinearColor CactusColor(0.055f, 0.31f, 0.095f);
            auto AddCactus = [&](int32 Cactus, const FVector& Base, float Size)
            {
                const float JointZ = 52.0f * Size;
                const float MainTopZ = 80.0f * Size;
                const float ElbowX = -34.0f * Size;
                const float BranchTopZ = 69.0f * Size;
                const FVector ArmDirection = Yaw.RotateVector(-FVector::XAxisVector);
                const FRotator ArmRotation = FQuat::FindBetweenNormals(
                    FVector::UpVector, ArmDirection).Rotator();

                // A taller, slightly wider base supports the thinner main stem.
                Add(TEXT("CactusBase"), Cactus, Cylinder,
                    Base + FVector(0, 0, 22.0f * Size),
                    FVector(0.15f, 0.15f, 0.44f) * Size, CactusColor);
                Add(TEXT("CactusStem"), Cactus, Cylinder,
                    Base + FVector(0, 0, 62.0f * Size),
                    FVector(0.11f, 0.11f, 0.36f) * Size, CactusColor * 1.05f);
                Add(TEXT("CactusTop"), Cactus, Sphere,
                    Base + FVector(0, 0, MainTopZ), FVector(0.12f) * Size,
                    CactusColor * 1.08f);

                Add(TEXT("CactusJoint"), Cactus, Sphere,
                    Base + FVector(0, 0, JointZ), FVector(0.115f) * Size,
                    CactusColor * 1.04f);
                Add(TEXT("CactusArm"), Cactus, Cylinder,
                    Base + FVector(ElbowX * 0.5f, 0, JointZ),
                    FVector(0.10f, 0.10f, 0.34f) * Size,
                    CactusColor * 1.03f, ArmRotation);
                Add(TEXT("CactusElbow"), Cactus, Sphere,
                    Base + FVector(ElbowX, 0, JointZ), FVector(0.11f) * Size,
                    CactusColor * 1.06f);
                Add(TEXT("CactusBranch"), Cactus, Cylinder,
                    Base + FVector(ElbowX, 0, (JointZ + BranchTopZ) * 0.5f),
                    FVector(0.10f, 0.10f, 0.17f) * Size, CactusColor * 1.03f);
                Add(TEXT("CactusBranchTop"), Cactus, Sphere,
                    Base + FVector(ElbowX, 0, BranchTopZ), FVector(0.11f) * Size,
                    CactusColor * 1.08f);
            };
            AddCactus(0, FVector(-63, 79, 0), 1.0f);
            AddCactus(1, FVector(91, 112, 0), 0.72f);
        }
        break;
    }
}
