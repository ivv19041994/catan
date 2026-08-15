#include "CatanMenuBackdropActor.h"

#include "CatanHexMeshBuilder.h"
#include "CatanResourceVisualBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "ProceduralMeshComponent.h"

namespace
{
constexpr int32 FieldRadius = 7;
constexpr float HexRadius = 220.0f;
constexpr float RootThree = 1.73205080757f;

const FLinearColor TerrainColors[] = {
    FLinearColor(0.035f, 0.28f, 0.065f),
    FLinearColor(0.55f, 0.14f, 0.045f),
    FLinearColor(0.90f, 0.62f, 0.055f),
    FLinearColor(0.34f, 0.70f, 0.19f),
    FLinearColor(0.34f, 0.38f, 0.45f),
    FLinearColor(0.76f, 0.61f, 0.34f)
};

FVector AxialToWorld(int32 Q, int32 R, float Height)
{
    return FVector(HexRadius * 1.5f * Q,
        HexRadius * RootThree * (R + Q * 0.5f), Height);
}

UMaterialInstanceDynamic* TerrainMaterial(UObject* Owner, UMaterialInterface* Base, const FLinearColor& Color)
{
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Owner);
    Material->SetVectorParameterValue(TEXT("Color"), Color);
    Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    Material->SetScalarParameterValue(TEXT("Roughness"), 0.88f);
    return Material;
}

UMaterialInstanceDynamic* TiledSurfaceMaterial(UObject* Owner, UMaterialInterface* Base, float Tiling)
{
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Owner);
    Material->SetScalarParameterValue(TEXT("Tiling"), Tiling);
    Material->SetScalarParameterValue(TEXT("SamplingScale"), Tiling);
    return Material;
}

}

ACatanMenuBackdropActor::ACatanMenuBackdropActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false);
    HexField = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MenuHexField"));
    RootComponent = HexField;
    HexField->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HexField->SetCastShadow(true);
    Shore = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("MenuShore"));
    Shore->SetupAttachment(HexField);
    Shore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sea = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MenuSea"));
    Sea->SetupAttachment(HexField);
    Sea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Sea->SetCastShadow(false);
}

void ACatanMenuBackdropActor::BeginPlay()
{
    Super::BeginPlay();
    BasicMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Materials/M_CatanColor.M_CatanColor"));
    BuildEnvironment();
    if (HasAuthority())
    {
        LayoutSeed = FMath::RandRange(1, MAX_int32);
        ForceNetUpdate();
        BuildField();
    }
    else if (LayoutSeed != 0)
    {
        BuildField();
    }
}

void ACatanMenuBackdropActor::BuildEnvironment()
{
    if (!BasicMaterial) return;
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    Sea->SetStaticMesh(Plane);
    // Keep the surface just above the template level floor.  A flat water mesh
    // below Z=0 is completely occluded by that floor in the menu map.
    Sea->SetRelativeLocation(FVector(0, 0, 2.0f));
    Sea->SetRelativeScale3D(FVector(150.0f, 150.0f, 1.0f));
    UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Environment/Ocean/M_CatanOceanDepth.M_CatanOceanDepth"));
    Sea->SetMaterial(0, Water ? Water
        : TerrainMaterial(this, BasicMaterial, FLinearColor(0.012f, 0.18f, 0.38f)));

    struct FLayer { float RadiusScale; float Height; FLinearColor Color; };
    const FLayer Layers[] = {
        {1.28f, 5.0f, FLinearColor(0.28f, 0.20f, 0.12f)},
        {1.17f, 9.0f, FLinearColor(0.80f, 0.62f, 0.31f)},
        {1.07f, 12.0f, FLinearColor(0.68f, 0.48f, 0.23f)}
    };
    for (int32 Layer = 0; Layer < UE_ARRAY_COUNT(Layers); ++Layer)
    {
        FCatanHexMeshBuffers Mesh;
        for (int32 Q = -FieldRadius; Q <= FieldRadius; ++Q)
        {
            const int32 MinR = FMath::Max(-FieldRadius, -Q - FieldRadius);
            const int32 MaxR = FMath::Min(FieldRadius, -Q + FieldRadius);
            for (int32 R = MinR; R <= MaxR; ++R)
                CatanHexMesh::AppendTop(Mesh, AxialToWorld(Q, R, Layers[Layer].Height),
                    HexRadius * Layers[Layer].RadiusScale, Layers[Layer].Color, 0.0f);
        }
        Shore->CreateMeshSection_LinearColor(Layer, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
            Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
        const TCHAR* ShoreMaterialPath = Layer == 1
            ? TEXT("/Game/Fab/Megascans/Surfaces/Rocky_Sand_vd4pbdt/High/vd4pbdt_tier_1/Materials/MI_vd4pbdt.MI_vd4pbdt")
            : TEXT("/Game/Fab/Megascans/Surfaces/Rocky_Ground_vjdqcba/High/vjdqcba_tier_1/Materials/MI_vjdqcba.MI_vjdqcba");
        UMaterialInterface* ShoreMaterial = LoadObject<UMaterialInterface>(nullptr, ShoreMaterialPath);
        Shore->SetMaterial(Layer, ShoreMaterial ? TiledSurfaceMaterial(this, ShoreMaterial, 5.0f)
            : TerrainMaterial(this, BasicMaterial, Layers[Layer].Color));
    }
}

void ACatanMenuBackdropActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACatanMenuBackdropActor, LayoutSeed);
}

void ACatanMenuBackdropActor::OnRep_Seed()
{
    BuildField();
}

void ACatanMenuBackdropActor::BuildField()
{
    if (bBuilt || LayoutSeed == 0 || !BasicMaterial) return;
    bBuilt = true;
    int32 BuiltHexes = 0;

    for (int32 Terrain = 0; Terrain < UE_ARRAY_COUNT(TerrainColors); ++Terrain)
    {
        FCatanHexMeshBuffers Mesh;

        for (int32 Q = -FieldRadius; Q <= FieldRadius; ++Q)
        {
            const int32 MinR = FMath::Max(-FieldRadius, -Q - FieldRadius);
            const int32 MaxR = FMath::Min(FieldRadius, -Q + FieldRadius);
            for (int32 R = MinR; R <= MaxR; ++R)
            {
                const uint32 CellSeed = HashCombine(GetTypeHash(LayoutSeed),
                    HashCombine(GetTypeHash(Q), GetTypeHash(R)));
                FRandomStream CellRandom(static_cast<int32>(CellSeed));
                const int32 ChosenTerrain = CellRandom.RandRange(0, UE_ARRAY_COUNT(TerrainColors) - 1);
                const float Height = CellRandom.FRandRange(15.0f, 38.0f);
                if (ChosenTerrain != Terrain) continue;
                ++BuiltHexes;

                const FVector Center = AxialToWorld(Q, R, Height);
                CatanHexMesh::AppendPrism(Mesh, Center, HexRadius, 24.0f, 12.0f,
                    TerrainColors[Terrain], TerrainColors[Terrain] * 0.62f, 0.0f);
                BuildResourceCluster(BuiltHexes, Center + FVector(0, 0, 24),
                    static_cast<uint8>(Terrain));
            }
        }

        HexField->CreateMeshSection_LinearColor(Terrain, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
            Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
        if (UMaterialInterface* GroundMaterial = CatanResourceVisuals::GetGroundMaterial(
            static_cast<ECatanResource>(Terrain)))
        {
            HexField->SetMaterial(Terrain, GroundMaterial);
        }
        else HexField->SetMaterial(Terrain, TerrainMaterial(this, BasicMaterial, TerrainColors[Terrain]));
    }
    // Instances are added in large batches while their hierarchy rebuild is
    // disabled. Build each tree once after the complete layout is known.
    for (UHierarchicalInstancedStaticMeshComponent* Instances : ResourceInstancedParts)
    {
        if (!Instances) continue;
        Instances->bAutoRebuildTreeOnInstanceChanges = true;
        Instances->BuildTreeIfOutdated(true, true);
    }
    UE_LOG(LogTemp, Display, TEXT("CATAN_MENU backdrop ready radius=%d hexes=%d seed=%d"),
        FieldRadius, BuiltHexes, LayoutSeed);
}

void ACatanMenuBackdropActor::BuildResourceCluster(int32 VisualId, const FVector& SurfaceCenter, uint8 Terrain)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

    FCatanResourceVisualCallbacks Callbacks;
    Callbacks.AddStatic = [this, SurfaceCenter](const FString& Name, UStaticMesh* Mesh,
        const FVector& Local, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation)
    {
        UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Menu%s"), *Name));
        Part->SetupAttachment(HexField);
        Part->RegisterComponent();
        Part->SetStaticMesh(Mesh);
        Part->SetRelativeLocation(SurfaceCenter + Local);
        Part->SetRelativeScale3D(Scale);
        Part->SetRelativeRotation(Rotation);
        Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Part->SetMaterial(0, TerrainMaterial(this, BasicMaterial, Color));
        ResourceParts.Add(Part);
        return Part;
    };
    Callbacks.AddAuthoredStatic = [this, SurfaceCenter](const FString& Name, UStaticMesh* Mesh,
        const FVector& Local, const FVector& Scale, const FRotator& Rotation)
    {
        UHierarchicalInstancedStaticMeshComponent* Instances = nullptr;
        for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
        {
            if (Part && Part->GetStaticMesh() == Mesh)
            {
                Instances = Part;
                break;
            }
        }
        if (!Instances)
        {
            Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
                *FString::Printf(TEXT("Menu%sInstances"), *Name));
            Instances->SetupAttachment(HexField);
            Instances->SetStaticMesh(Mesh);
            Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Instances->SetGenerateOverlapEvents(false);
            Instances->SetCanEverAffectNavigation(false);
            Instances->SetAffectDistanceFieldLighting(false);
            Instances->SetCastShadow(false);
#if PLATFORM_ANDROID || PLATFORM_IOS
            Instances->SetCullDistances(2800, 4400);
#else
            Instances->SetCullDistances(3800, 6000);
#endif
            Instances->bAutoRebuildTreeOnInstanceChanges = false;
            Instances->RegisterComponent();
            ResourceInstancedParts.Add(Instances);
        }
        Instances->AddInstance(FTransform(Rotation, SurfaceCenter + Local, Scale));
    };
    // Keep AddAuthoredUniqueStatic unset in the menu. BuildCluster then falls
    // back to AddAuthoredStatic and batches repeated barns, bales, sheep,
    // mountains and brick props by mesh in HISM components. They do not need
    // independent gameplay state in this decorative backdrop.
    Callbacks.AddColoredInstance = [this, SurfaceCenter](const FString& Name, UStaticMesh* Mesh,
        const FVector& Local, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation)
    {
        UHierarchicalInstancedStaticMeshComponent* Instances = nullptr;
        for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
            if (Part && Part->GetName().Contains(Name)) { Instances = Part; break; }
        if (!Instances)
        {
            Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
                *FString::Printf(TEXT("Menu%sInstances"), *Name));
            Instances->SetupAttachment(HexField);
            Instances->SetStaticMesh(Mesh);
            Instances->SetMaterial(0, TerrainMaterial(this, BasicMaterial, Color));
            Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Instances->SetGenerateOverlapEvents(false);
            Instances->SetCanEverAffectNavigation(false);
            Instances->SetAffectDistanceFieldLighting(false);
            Instances->SetCastShadow(false);
#if PLATFORM_ANDROID || PLATFORM_IOS
            Instances->SetCullDistances(2800, 4400);
#else
            Instances->SetCullDistances(3800, 6000);
#endif
            Instances->bAutoRebuildTreeOnInstanceChanges = false;
            Instances->RegisterComponent();
            ResourceInstancedParts.Add(Instances);
        }
        Instances->AddInstance(FTransform(Rotation, SurfaceCenter + Local, Scale));
    };
    Callbacks.AddMaterialStatic = [this, SurfaceCenter](const FString& Name, UStaticMesh* Mesh,
        UMaterialInterface* Material, const FVector& Local, const FVector& Scale, const FRotator& Rotation)
    {
        UHierarchicalInstancedStaticMeshComponent* Instances = nullptr;
        for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
            if (Part && Part->GetName().Contains(Name)) { Instances = Part; break; }
        if (!Instances)
        {
            Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
                *FString::Printf(TEXT("Menu%sMaterialInstances"), *Name));
            Instances->SetupAttachment(HexField);
            Instances->SetStaticMesh(Mesh);
            Instances->SetMaterial(0, Material ? Material : TerrainMaterial(this, BasicMaterial,
                FLinearColor(0.18f, 0.24f, 0.10f)));
            Instances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Instances->SetGenerateOverlapEvents(false);
            Instances->SetCanEverAffectNavigation(false);
            Instances->SetAffectDistanceFieldLighting(false);
            Instances->SetCastShadow(false);
#if PLATFORM_ANDROID || PLATFORM_IOS
            Instances->SetCullDistances(2800, 4400);
#else
            Instances->SetCullDistances(3800, 6000);
#endif
            Instances->bAutoRebuildTreeOnInstanceChanges = false;
            Instances->RegisterComponent();
            ResourceInstancedParts.Add(Instances);
        }
        Instances->AddInstance(FTransform(Rotation, SurfaceCenter + Local, Scale));
    };
    Callbacks.AuthoredForestTreeBudget = 7;
#if PLATFORM_ANDROID || PLATFORM_IOS
    Callbacks.AuthoredForestTreeBudget = 4;
#endif
    Callbacks.AddHexPyramid = [this, SurfaceCenter](const FString& Name, const FVector& Local,
        float Radius, float Height, const FLinearColor& Color)
    {
        UProceduralMeshComponent* Pyramid = NewObject<UProceduralMeshComponent>(this,
            *FString::Printf(TEXT("Menu%s"), *Name));
        Pyramid->SetupAttachment(HexField);
        Pyramid->RegisterComponent();
        Pyramid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const FVector Position = SurfaceCenter + Local;
        TArray<FVector> Vertices{Position + FVector(0, 0, Height)};
        TArray<FVector> Normals{FVector::UpVector};
        TArray<FVector2D> UVs{FVector2D(0.5f, 0.0f)};
        TArray<FLinearColor> Colors{Color};
        TArray<FProcMeshTangent> Tangents{FProcMeshTangent(1, 0, 0)};
        TArray<int32> Triangles;
        for (int32 Side = 0; Side < 6; ++Side)
        {
            const float Angle = FMath::DegreesToRadians(30.0f + Side * 60.0f);
            Vertices.Add(Position + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D(static_cast<float>(Side) / 6.0f, 1.0f));
            Colors.Add(Color * (0.83f + (Side % 3) * 0.08f));
            Tangents.Add(FProcMeshTangent(1, 0, 0));
            Triangles.Append({0, (Side + 1) % 6 + 1, Side + 1});
        }
        Pyramid->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
        Pyramid->SetMaterial(0, TerrainMaterial(this, BasicMaterial, Color));
        ResourcePyramids.Add(Pyramid);
    };
    Callbacks.Animate = [](UStaticMeshComponent*, ECatanResourceAnimation, float) {};
    CatanResourceVisuals::BuildCluster(static_cast<ECatanResource>(Terrain), VisualId,
        static_cast<float>((VisualId * 47) % 360), Cube, Sphere, Cylinder, Cone, Callbacks);
}
