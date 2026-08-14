#include "CatanResourceVisualBuilder.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
struct FForestTreePlacement
{
    FVector Location;
    float DesiredHeight;
    float MaxDiameter;
    float Yaw;
    int32 MeshIndex;
};

TArray<UStaticMesh*> LoadForestMeshes()
{
    // Deliberately no palm assets here: the wood tile represents a temperate forest.
    static const TCHAR* Paths[] = {
        TEXT("/Game/DZ_Assets/DZ_Trees/Meshes/Pine/SM_Pine_1.SM_Pine_1")
    };
    TArray<UStaticMesh*> Result;
    for (const TCHAR* Path : Paths)
        if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, Path)) Result.Add(Mesh);
    return Result;
}

UStaticMesh* LoadStaticMesh(const TCHAR* Path)
{
    return LoadObject<UStaticMesh>(nullptr, Path);
}

void BuildAuthoredForest(int32 VisualId, float YawDegrees,
    const FCatanResourceVisualCallbacks& C, const TArray<UStaticMesh*>& ForestMeshes)
{
    if (!C.AddAuthoredStatic || ForestMeshes.IsEmpty() || C.AuthoredForestTreeBudget <= 0) return;

    const FForestTreePlacement Placements[] = {
        {FVector(-104, 83, 0), 310.0f, 145.0f,  17.0f, 0},
        {FVector( -35, 99, 0), 365.0f, 132.0f, 151.0f, 0},
        {FVector(  38, 73, 0), 335.0f, 142.0f, 248.0f, 0},
        {FVector( 101, 97, 0), 260.0f, 135.0f,  91.0f, 0},
        {FVector(  76, 31, 0), 285.0f, 125.0f, 312.0f, 0},
        {FVector( -78, 27, 0), 275.0f, 122.0f, 204.0f, 0},
        {FVector(   2, 31, 0), 245.0f, 116.0f,  54.0f, 0}
    };
    const FRotator ClusterYaw(0, YawDegrees, 0);
    const int32 Count = FMath::Min(C.AuthoredForestTreeBudget,
        static_cast<int32>(UE_ARRAY_COUNT(Placements)));
    for (int32 Item = 0; Item < Count; ++Item)
    {
        const FForestTreePlacement& Placement = Placements[Item];
        UStaticMesh* Mesh = ForestMeshes[Placement.MeshIndex % ForestMeshes.Num()];
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const float Height = FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f);
        const float Diameter = FMath::Max(1.0f,
            FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f);
        const float Scale = FMath::Min(Placement.DesiredHeight / Height,
            Placement.MaxDiameter / Diameter);
        FVector Local = ClusterYaw.RotateVector(Placement.Location);
        // Account for meshes whose pivot is above or below the base of the trunk.
        Local.Z = 4.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
        C.AddAuthoredStatic(FString::Printf(TEXT("Hex%dPhotoTree%d"), VisualId, Item), Mesh,
            Local, FVector(Scale), FRotator(0, YawDegrees + Placement.Yaw, 0));
    }
}
}

UMaterialInterface* CatanResourceVisuals::GetGroundMaterial(ECatanResource Resource)
{
    const TCHAR* Path = nullptr;
    switch (Resource)
    {
    case ECatanResource::Wood:
        Path = TEXT("/Game/Fab/Megascans/Surfaces/Forest_Floor_sfjmafua/High/sfjmafua_tier_1/Materials/MI_sfjmafua.MI_sfjmafua");
        break;
    case ECatanResource::Clay:
        Path = TEXT("/Game/Fab/Megascans/Surfaces/Soil_Mud_pjuph20/High/pjuph20_tier_1/Materials/MI_pjuph20.MI_pjuph20");
        break;
    case ECatanResource::Hay:
        Path = TEXT("/Game/Fab/Megascans/Surfaces/Dry_Trampled_Soil_wcivbfb/High/wcivbfb_tier_1/Materials/MI_wcivbfb.MI_wcivbfb");
        break;
    case ECatanResource::Sheep:
        Path = TEXT("/Game/Fab/Megascans/Surfaces/Uncut_Grass_oilpt20/High/oilpt20_tier_1/Materials/MI_oilpt20.MI_oilpt20");
        break;
    case ECatanResource::Stone:
        // Match the mountain mesh itself so its baked square skirt blends into the hex.
        Path = TEXT("/Game/Fab/Mountain__1/mountain_1/Materials/Material_002.Material_002");
        break;
    case ECatanResource::Desert:
        Path = TEXT("/Game/Fab/Megascans/Surfaces/Rocky_Sand_vd4pbdt/High/vd4pbdt_tier_1/Materials/MI_vd4pbdt.MI_vd4pbdt");
        break;
    }
    return Path ? LoadObject<UMaterialInterface>(nullptr, Path) : nullptr;
}

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
    {
        const TArray<UStaticMesh*> ForestMeshes = LoadForestMeshes();
        if (!ForestMeshes.IsEmpty() && C.AddAuthoredStatic)
        {
            BuildAuthoredForest(VisualId, YawDegrees, C, ForestMeshes);
            if (C.AddColoredInstance)
            {
                for (int32 Item = 0; Item < 18; ++Item)
                {
                    const float Ring = Item < 7 ? 88.0f : 148.0f;
                    const float Angle = YawDegrees + Item * 137.508f;
                    const FVector Local(FMath::Cos(FMath::DegreesToRadians(Angle)) * Ring,
                        FMath::Sin(FMath::DegreesToRadians(Angle)) * Ring, 12.0f);
                    const float Height = 0.14f + (Item % 4) * 0.018f;
                    C.AddColoredInstance(TEXT("ForestGrass"), Cone, Local,
                        FVector(0.035f + (Item % 3) * 0.006f, 0.018f, Height),
                        FLinearColor(0.075f, 0.20f + (Item % 4) * 0.018f, 0.035f),
                        FRotator((Item % 2) ? 5.0f : -4.0f, Angle, 0));
                }
            }
            break;
        }
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
    }
    case ECatanResource::Clay:
    {
        UStaticMesh* BrickWorkshop = LoadStaticMesh(
            TEXT("/Game/Fab/Historical_house_with_exposed_bricks/historical_house_with_exposed_bricks/StaticMeshes/historical_house_with_exposed_bricks.historical_house_with_exposed_bricks"));
        UStaticMesh* StackedBricks = LoadStaticMesh(
            TEXT("/Game/Fab/Megascans/3D/Stacked_Bricks_wjykcfnqx/Medium/wjykcfnqx_tier_2/StaticMeshes/wjykcfnqx_tier_2.wjykcfnqx_tier_2"));
        const auto& AddAuthored = C.AddAuthoredUniqueStatic
            ? C.AddAuthoredUniqueStatic : C.AddAuthoredStatic;
        if (BrickWorkshop && StackedBricks && AddAuthored)
        {
            const FBoxSphereBounds WorkshopBounds = BrickWorkshop->GetBounds();
            const float WorkshopScale = FMath::Min(
                125.0f / FMath::Max(1.0f, WorkshopBounds.BoxExtent.Z * 2.0f),
                145.0f / FMath::Max(1.0f,
                    FMath::Max(WorkshopBounds.BoxExtent.X, WorkshopBounds.BoxExtent.Y) * 2.0f));
            const FRotator WorkshopRotationA(0.0f, YawDegrees + 18.0f, 0.0f);
            const FVector WorkshopCenter = Rotate(FVector(-69.0f, 83.0f, 0.0f));
            const FVector WorkshopLengthOffset = WorkshopRotationA.RotateVector(FVector(1.5f, 0.0f, 0.0f));
            const FVector WorkshopWidthOffset = WorkshopRotationA.RotateVector(FVector(0.0f, 2.5f, 0.0f));
            FVector WorkshopLocationA = WorkshopCenter - WorkshopLengthOffset - WorkshopWidthOffset
                - WorkshopRotationA.RotateVector(
                    FVector(WorkshopBounds.Origin.X, WorkshopBounds.Origin.Y, 0.0f) * WorkshopScale);
            WorkshopLocationA.Z = 4.0f
                - (WorkshopBounds.Origin.Z - WorkshopBounds.BoxExtent.Z) * WorkshopScale;
            AddAuthored(FString::Printf(TEXT("Hex%dBrickWorkshopA"), VisualId), BrickWorkshop,
                WorkshopLocationA, FVector(WorkshopScale), WorkshopRotationA);

            // The authored house has two unfinished exterior sides. A second, reversed copy
            // overlaps it just enough for each copy's finished walls to hide the other's gaps.
            const FRotator WorkshopRotationB(0.0f, YawDegrees + 198.0f, 0.0f);
            FVector WorkshopLocationB = WorkshopCenter + WorkshopLengthOffset + WorkshopWidthOffset
                - WorkshopRotationB.RotateVector(
                    FVector(WorkshopBounds.Origin.X, WorkshopBounds.Origin.Y, 0.0f) * WorkshopScale);
            WorkshopLocationB.Z = 4.0f
                - (WorkshopBounds.Origin.Z - WorkshopBounds.BoxExtent.Z) * WorkshopScale;
            AddAuthored(FString::Printf(TEXT("Hex%dBrickWorkshopB"), VisualId), BrickWorkshop,
                WorkshopLocationB, FVector(WorkshopScale), WorkshopRotationB);

            const FBoxSphereBounds StackBounds = StackedBricks->GetBounds();
            const float StackScale = FMath::Min(
                28.0f / FMath::Max(1.0f, StackBounds.BoxExtent.Z * 2.0f),
                42.0f / FMath::Max(1.0f,
                    FMath::Max(StackBounds.BoxExtent.X, StackBounds.BoxExtent.Y) * 2.0f));
            const FVector StackLocations[] = {
                FVector(26, 53, 0), FVector(67, 56, 0), FVector(108, 60, 0), FVector(145, 67, 0),
                FVector(22, 91, 0), FVector(63, 96, 0), FVector(104, 100, 0), FVector(139, 106, 0)
            };
            for (int32 Item = 0; Item < UE_ARRAY_COUNT(StackLocations); ++Item)
            {
                const float StackYaw = YawDegrees + ((Item & 1) ? 8.0f : -5.0f);
                const FRotator StackRotation(0.0f, StackYaw, 0.0f);
                FVector StackLocation = Rotate(StackLocations[Item])
                    - StackRotation.RotateVector(
                        FVector(StackBounds.Origin.X, StackBounds.Origin.Y, 0.0f) * StackScale);
                StackLocation.Z = 4.0f
                    - (StackBounds.Origin.Z - StackBounds.BoxExtent.Z) * StackScale;
                AddAuthored(FString::Printf(TEXT("Hex%dBrickStack%d"), VisualId, Item), StackedBricks,
                    StackLocation, FVector(StackScale), StackRotation);
            }
            break;
        }
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
    }
    case ECatanResource::Hay:
    {
        UStaticMesh* Barn = LoadStaticMesh(
            TEXT("/Game/Fab/Old_Wooden_Barn__House_4_/ruined_house_4/StaticMeshes/ruined_house_4.ruined_house_4"));
        UStaticMesh* HayBale = LoadStaticMesh(
            TEXT("/Game/Fab/Megascans/3D/Round_Hay_Bale_rlCay/Medium/rlCay_tier_2/StaticMeshes/rlCay_tier_2.rlCay_tier_2"));
        const auto& AddAuthored = C.AddAuthoredUniqueStatic
            ? C.AddAuthoredUniqueStatic : C.AddAuthoredStatic;
        if (Barn && HayBale && AddAuthored)
        {
            const FBoxSphereBounds BarnBounds = Barn->GetBounds();
            const float BarnScale = FMath::Min(125.0f / FMath::Max(1.0f, BarnBounds.BoxExtent.Z * 2.0f),
                145.0f / FMath::Max(1.0f, FMath::Max(BarnBounds.BoxExtent.X, BarnBounds.BoxExtent.Y) * 2.0f));
            const FRotator BarnRotation(0, YawDegrees + 18.0f, 0);
            FVector BarnLocation = Rotate(FVector(-72, 82, 0))
                - BarnRotation.RotateVector(FVector(BarnBounds.Origin.X, BarnBounds.Origin.Y, 0) * BarnScale);
            BarnLocation.Z = 4.0f - (BarnBounds.Origin.Z - BarnBounds.BoxExtent.Z) * BarnScale;
            AddAuthored(FString::Printf(TEXT("Hex%dBarn"), VisualId), Barn, BarnLocation,
                FVector(BarnScale), BarnRotation);

            const FBoxSphereBounds BaleBounds = HayBale->GetBounds();
            const float BaleScale = FMath::Min(29.0f / FMath::Max(1.0f, BaleBounds.BoxExtent.Z * 2.0f),
                37.0f / FMath::Max(1.0f, FMath::Max(BaleBounds.BoxExtent.X, BaleBounds.BoxExtent.Y) * 2.0f));
            const FVector BaleLocations[] = {
                FVector(28, 55, 0), FVector(78, 55, 0), FVector(128, 55, 0),
                FVector(28, 105, 0), FVector(78, 105, 0), FVector(128, 105, 0)
            };
            for (int32 Item = 0; Item < UE_ARRAY_COUNT(BaleLocations); ++Item)
            {
                const FRotator BaleRotation(0, YawDegrees + 25.0f + Item * 67.0f, 0);
                FVector Location = Rotate(BaleLocations[Item])
                    - BaleRotation.RotateVector(FVector(BaleBounds.Origin.X, BaleBounds.Origin.Y, 0) * BaleScale);
                Location.Z = 4.0f - (BaleBounds.Origin.Z - BaleBounds.BoxExtent.Z) * BaleScale;
                AddAuthored(FString::Printf(TEXT("Hex%dHayBale%d"), VisualId, Item), HayBale,
                    Location, FVector(BaleScale), BaleRotation);
            }
            break;
        }
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
    }
    case ECatanResource::Sheep:
    {
        UStaticMesh* SheepMesh = LoadStaticMesh(
            TEXT("/Game/Fab/Suffolk_Sheep_Thick_Wool_Fleece_Standing_Pose_3D_Model/3d_765/StaticMeshes/3d_765.3d_765"));
        const auto& AddAuthored = C.AddAuthoredUniqueStatic
            ? C.AddAuthoredUniqueStatic : C.AddAuthoredStatic;
        if (SheepMesh && AddAuthored)
        {
            const FBoxSphereBounds Bounds = SheepMesh->GetBounds();
            const float SheepScale = FMath::Min(82.0f / FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f),
                105.0f / FMath::Max(1.0f, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f));
            const FVector Locations[] = {FVector(-96, 75, 0), FVector(15, 105, 0), FVector(105, 61, 0)};
            for (int32 Item = 0; Item < UE_ARRAY_COUNT(Locations); ++Item)
            {
                FVector Location = Rotate(Locations[Item]);
                Location.Z = 4.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * SheepScale;
                AddAuthored(FString::Printf(TEXT("Hex%dPhotoSheep%d"), VisualId, Item), SheepMesh,
                    Location, FVector(SheepScale * (0.92f + Item * 0.06f)),
                    FRotator(0, YawDegrees + 35.0f + Item * 103.0f, 0));
            }
            break;
        }
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
    }
    case ECatanResource::Stone:
    {
        UStaticMesh* Mountain = LoadStaticMesh(
            TEXT("/Game/Fab/Mountain__1/mountain_1/StaticMeshes/mountain_1.mountain_1"));
        const auto& AddAuthored = C.AddAuthoredUniqueStatic
            ? C.AddAuthoredUniqueStatic : C.AddAuthoredStatic;
        if (Mountain && AddAuthored)
        {
            const FBoxSphereBounds Bounds = Mountain->GetBounds();
            struct FMountainPlacement
            {
                FVector Center;
                float DesiredHeight;
                float MaxDiameter;
                float LocalYaw;
            };
            const FMountainPlacement Placements[] = {
                {FVector(-34, 55, 0), 205.0f, 156.0f,  17.0f},
                {FVector( 47, 68, 0), 142.0f, 112.0f, 211.0f}
            };
            for (int32 Item = 0; Item < UE_ARRAY_COUNT(Placements); ++Item)
            {
                const FMountainPlacement& Placement = Placements[Item];
                const float Scale = FMath::Min(
                    Placement.DesiredHeight / FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f),
                    Placement.MaxDiameter / FMath::Max(1.0f,
                        FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f));
                constexpr float WidthMultiplier = 1.30f;
                constexpr float HeightMultiplier = 3.5775f; // Previous 2.25 height * 1.59.
                const FRotator Rotation(0, YawDegrees + Placement.LocalYaw, 0);
                FVector Location = Rotate(Placement.Center)
                    - Rotation.RotateVector(FVector(Bounds.Origin.X * Scale * WidthMultiplier,
                        Bounds.Origin.Y * Scale * WidthMultiplier, 0));
                // Sink the mesh skirt under the matching ground surface; only the relief remains visible.
                Location.Z = -8.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale * HeightMultiplier;
                AddAuthored(FString::Printf(TEXT("Hex%dPhotoMountain%d"), VisualId, Item),
                    Mountain, Location,
                    FVector(Scale * WidthMultiplier, Scale * WidthMultiplier, Scale * HeightMultiplier),
                    Rotation);
            }
            break;
        }
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
    }
    case ECatanResource::Desert:
    {
        UStaticMesh* Coconut = LoadStaticMesh(
            TEXT("/Game/DZ_Assets/DZ_Trees/Meshes/Coconut_Tree/SM_Coconut_Tree_1.SM_Coconut_Tree_1"));
        UStaticMesh* FanPalm = LoadStaticMesh(
            TEXT("/Game/DZ_Assets/DZ_Trees/Meshes/Windmill_Palm/SM_Windmill_Palm_1.SM_Windmill_Palm_1"));
        if (C.AddMaterialStatic)
        {
            UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr,
                TEXT("/Game/Environment/Desert/M_OasisWater.M_OasisWater"));
            C.AddMaterialStatic(TEXT("OasisWetShore"), Sphere, nullptr, FVector(46, 80, 7),
                FVector(1.12f, 0.76f, 0.055f), FRotator::ZeroRotator);
            C.AddMaterialStatic(TEXT("OasisWater"), Sphere, Water, FVector(46, 80, 10),
                FVector(0.96f, 0.62f, 0.035f), FRotator::ZeroRotator);
        }
        auto AddPalm = [&](const TCHAR* Name, UStaticMesh* Mesh, const FVector& Local,
            float DesiredHeight, float MaxDiameter, float PalmYaw)
        {
            if (!Mesh || !C.AddAuthoredStatic) return;
            const FBoxSphereBounds Bounds = Mesh->GetBounds();
            const float Scale = FMath::Min(DesiredHeight / FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f),
                MaxDiameter / FMath::Max(1.0f, FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2.0f));
            FVector Position = Yaw.RotateVector(Local);
            Position.Z = 4.0f - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * Scale;
            C.AddAuthoredStatic(FString::Printf(TEXT("Hex%d%s"), VisualId, Name), Mesh,
                Position, FVector(Scale), FRotator(0, YawDegrees + PalmYaw, 0));
        };
        AddPalm(TEXT("CoconutPalm"), Coconut, FVector(-68, 85, 0), 330.0f, 150.0f, 28.0f);
        AddPalm(TEXT("FanPalm"), FanPalm, FVector(127, 75, 0), 270.0f, 135.0f, 211.0f);
        if (!Coconut && !FanPalm)
        {
            Add(TEXT("CactusStem"), 0, Cylinder, FVector(103, 88, 29), FVector(0.075f, 0.075f, 0.36f),
                FLinearColor(0.08f, 0.34f, 0.12f));
            Animate(Add(TEXT("CactusTop"), 0, Sphere, FVector(103, 88, 66), FVector(0.085f, 0.085f, 0.13f),
                FLinearColor(0.10f, 0.43f, 0.15f)), ECatanResourceAnimation::Sway, 0.4f);
        }
        break;
    }
    }
}
