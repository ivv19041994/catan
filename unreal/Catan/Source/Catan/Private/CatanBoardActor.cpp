#include "CatanBoardActor.h"

#include "CatanGameSubsystem.h"
#include "CatanHexMeshBuilder.h"
#include "CatanResourceVisualBuilder.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"
#include "Algo/Reverse.h"

#include <cmath>

namespace
{
constexpr float TileRadius = 220.0f;
constexpr float RootThreeOverTwo = 0.86602540378f;
constexpr float DiceAnimationDuration = 1.15f;
constexpr float DiceSettleDuration = 0.34f;
constexpr float DiceTableY = -1370.0f;
enum : uint8 { ResourceSway = 1, ResourceBob, ResourcePulse, ResourceDrift };

FVector RobberPlaceholderOffset(ECatanResource Resource, float YawDegrees)
{
    // Resource clusters are authored primarily on the positive-Y half of a tile.
    // Keep a separate composition point for every terrain type so future art
    // changes only need to adjust the corresponding placeholder.
    FVector Local;
    switch (Resource)
    {
    case ECatanResource::Wood:   Local = FVector(70.0f, -82.0f, 0.0f); break;
    case ECatanResource::Clay:   Local = FVector(-64.0f, -86.0f, 0.0f); break;
    case ECatanResource::Hay:    Local = FVector(-42.0f, -92.0f, 0.0f); break;
    case ECatanResource::Sheep:  Local = FVector(18.0f, -92.0f, 0.0f); break;
    case ECatanResource::Stone:  Local = FVector(92.0f, -88.0f, 0.0f); break;
    case ECatanResource::Desert: Local = FVector(-18.0f, -94.0f, 0.0f); break;
    default:                     Local = FVector(0.0f, -88.0f, 0.0f); break;
    }
    return FRotator(0.0f, YawDegrees, 0.0f).RotateVector(Local);
}

FVector ToWorld(float X, float Y, float Z = 0.0f)
{
    return FVector((X - 4.330127f) * TileRadius, (3.5f - Y) * TileRadius, Z);
}

TArray<FVector> HexCenters()
{
    TArray<FVector> Result;
    const float HorizontalOffset = 1.5f;
    const float Width = RootThreeOverTwo * 2.0f;
    for (int32 I = 0; I < 3; ++I) Result.Add(ToWorld(RootThreeOverTwo * 3 + Width * I, 1.0f));
    for (int32 I = 0; I < 4; ++I) Result.Add(ToWorld(RootThreeOverTwo * 2 + Width * I, 1.0f + HorizontalOffset));
    for (int32 I = 0; I < 5; ++I) Result.Add(ToWorld(RootThreeOverTwo + Width * I, 1.0f + HorizontalOffset * 2));
    for (int32 I = 0; I < 4; ++I) Result.Add(ToWorld(RootThreeOverTwo * 2 + Width * I, 1.0f + HorizontalOffset * 3));
    for (int32 I = 0; I < 3; ++I) Result.Add(ToWorld(RootThreeOverTwo * 3 + Width * I, 1.0f + HorizontalOffset * 4));
    return Result;
}

TArray<FVector> BoardBoundary()
{
    struct FBoundaryEdge { FVector A; FVector B; };
    TArray<FBoundaryEdge> Edges;
    for (const FVector& Center : HexCenters())
    {
        FVector Corners[6];
        for (int32 Corner = 0; Corner < 6; ++Corner)
        {
            const float Angle = FMath::DegreesToRadians(30.0f + Corner * 60.0f);
            Corners[Corner] = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TileRadius;
        }
        for (int32 Corner = 0; Corner < 6; ++Corner)
        {
            const FBoundaryEdge Candidate{Corners[Corner], Corners[(Corner + 1) % 6]};
            const int32 Shared = Edges.IndexOfByPredicate([&Candidate](const FBoundaryEdge& Edge)
            {
                return (Edge.A.Equals(Candidate.A, 0.5f) && Edge.B.Equals(Candidate.B, 0.5f))
                    || (Edge.A.Equals(Candidate.B, 0.5f) && Edge.B.Equals(Candidate.A, 0.5f));
            });
            if (Shared == INDEX_NONE) Edges.Add(Candidate);
            else Edges.RemoveAtSwap(Shared);
        }
    }

    TArray<FVector> Boundary;
    if (Edges.IsEmpty()) return Boundary;
    FBoundaryEdge Current = Edges.Pop();
    Boundary.Add(Current.A);
    Boundary.Add(Current.B);
    while (!Edges.IsEmpty())
    {
        const FVector Tail = Boundary.Last();
        const int32 Next = Edges.IndexOfByPredicate([&Tail](const FBoundaryEdge& Edge)
        {
            return Edge.A.Equals(Tail, 0.5f) || Edge.B.Equals(Tail, 0.5f);
        });
        if (Next == INDEX_NONE) break;
        Current = Edges[Next];
        Edges.RemoveAtSwap(Next);
        Boundary.Add(Current.A.Equals(Tail, 0.5f) ? Current.B : Current.A);
        if (Boundary.Last().Equals(Boundary[0], 0.5f))
        {
            Boundary.Pop();
            break;
        }
    }
    return Boundary;
}

TArray<FVector> NodeCenters()
{
    TArray<FVector> Result;
    constexpr float Sin30 = 0.5f;
    for (int32 I = 0; I < 7; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * (2 + I), (I % 2) ? 0.0f : Sin30, 30.0f));
    for (int32 I = 0; I < 9; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * (1 + I), Sin30 + 1.0f + ((I % 2) ? 0.0f : Sin30), 30.0f));
    for (int32 I = 0; I < 11; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * I, 2.0f * (Sin30 + 1.0f) + ((I % 2) ? 0.0f : Sin30), 30.0f));
    for (int32 I = 0; I < 11; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * I, 3.0f * (Sin30 + 1.0f) + ((I % 2 == 0) ? 0.0f : Sin30), 30.0f));
    for (int32 I = 0; I < 9; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * (1 + I), 4.0f * (Sin30 + 1.0f) + ((I % 2 == 0) ? 0.0f : Sin30), 30.0f));
    for (int32 I = 0; I < 7; ++I)
        Result.Add(ToWorld(RootThreeOverTwo * (2 + I), 5.0f * (Sin30 + 1.0f) + ((I % 2 == 0) ? 0.0f : Sin30), 30.0f));
    return Result;
}

struct FRoadPlacement
{
    FVector Position;
    float Angle;
};

TArray<FRoadPlacement> RoadCenters()
{
    TArray<FRoadPlacement> Result;
    constexpr float Sin30 = 0.5f;
    constexpr float Width = RootThreeOverTwo * 2.0f;
    auto Add = [&Result](float X, float Y, float Angle) { Result.Add({ToWorld(X, Y, 40.0f), Angle}); };
    for (int32 I=0; I<6; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*(2+I), Sin30/2, (I%2)?150.0f:30.0f);
    for (int32 I=0; I<4; ++I) Add(RootThreeOverTwo*2 + I*Width, Sin30+0.5f, 90.0f);
    for (int32 I=0; I<8; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*(1+I), Sin30/2 + 1.5f, (I%2)?150.0f:30.0f);
    for (int32 I=0; I<5; ++I) Add(RootThreeOverTwo + I*Width, Sin30+0.5f+1.5f, 90.0f);
    for (int32 I=0; I<10; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*I, Sin30/2+3.0f, (I%2)?150.0f:30.0f);
    for (int32 I=0; I<6; ++I) Add(I*Width, Sin30+0.5f+3.0f, 90.0f);
    for (int32 I=0; I<10; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*I, Sin30/2+4.5f, (I%2)?30.0f:150.0f);
    for (int32 I=0; I<5; ++I) Add(RootThreeOverTwo + I*Width, Sin30+0.5f+4.5f, 90.0f);
    for (int32 I=0; I<8; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*(1+I), Sin30/2+6.0f, (I%2)?30.0f:150.0f);
    for (int32 I=0; I<4; ++I) Add(RootThreeOverTwo*2 + I*Width, Sin30+0.5f+6.0f, 90.0f);
    for (int32 I=0; I<6; ++I) Add(RootThreeOverTwo/2 + RootThreeOverTwo*(2+I), Sin30/2+7.5f, (I%2)?30.0f:150.0f);
    return Result;
}

FLinearColor ResourceColor(ECatanResource Resource)
{
    switch (Resource)
    {
    case ECatanResource::Wood: return FLinearColor(0.04f, 0.34f, 0.08f);
    case ECatanResource::Clay: return FLinearColor(0.55f, 0.16f, 0.06f);
    case ECatanResource::Hay: return FLinearColor(0.92f, 0.68f, 0.08f);
    case ECatanResource::Sheep: return FLinearColor(0.38f, 0.78f, 0.24f);
    case ECatanResource::Stone: return FLinearColor(0.42f, 0.46f, 0.52f);
    case ECatanResource::Desert: return FLinearColor(0.83f, 0.70f, 0.43f);
    }
    return FLinearColor::Black;
}

FLinearColor PlayerColor(size_t Id)
{
    constexpr FLinearColor Colors[] = {
        FLinearColor(0.85f, 0.08f, 0.05f), FLinearColor(0.05f, 0.35f, 0.9f),
        FLinearColor(0.95f, 0.72f, 0.04f), FLinearColor(0.1f, 0.7f, 0.25f)
    };
    return Colors[Id % UE_ARRAY_COUNT(Colors)];
}

UMaterialInstanceDynamic* ColoredMaterial(UObject* Owner, UMaterialInterface* Base, FLinearColor Color)
{
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Owner);
    Material->SetVectorParameterValue(TEXT("Color"), Color);
    Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    return Material;
}

UMaterialInstanceDynamic* TiledSurfaceMaterial(UObject* Owner, UMaterialInterface* Base, float Tiling)
{
    UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(Base, Owner);
    Material->SetScalarParameterValue(TEXT("Tiling"), Tiling);
    Material->SetScalarParameterValue(TEXT("SamplingScale"), Tiling);
    return Material;
}

TArray<FVector2D> PipOffsets(int32 Value)
{
    constexpr float Offset = 20.0f;
    switch (Value)
    {
    case 1: return {{0, 0}};
    case 2: return {{-Offset, Offset}, {Offset, -Offset}};
    case 3: return {{-Offset, Offset}, {0, 0}, {Offset, -Offset}};
    case 4: return {{-Offset, Offset}, {Offset, Offset}, {-Offset, -Offset}, {Offset, -Offset}};
    case 5: return {{-Offset, Offset}, {Offset, Offset}, {0, 0}, {-Offset, -Offset}, {Offset, -Offset}};
    case 6: return {{-Offset, Offset}, {-Offset, 0}, {-Offset, -Offset},
        {Offset, Offset}, {Offset, 0}, {Offset, -Offset}};
    default: return {};
    }
}

FVector DieFaceNormal(int32 Value)
{
    switch (Value)
    {
    case 1: return FVector::UpVector;
    case 2: return FVector::YAxisVector;
    case 3: return FVector::XAxisVector;
    case 4: return -FVector::XAxisVector;
    case 5: return -FVector::YAxisVector;
    case 6: return -FVector::UpVector;
    default: return FVector::UpVector;
    }
}

FQuat DieResultRotation(int32 Value, int32 DieIndex)
{
    const FQuat FaceUp = FQuat::FindBetweenNormals(DieFaceNormal(Value), FVector::UpVector);
    const float Yaw = FMath::DegreesToRadians(17.0f + Value * 29.0f + DieIndex * 41.0f);
    return FQuat(FVector::UpVector, Yaw) * FaceUp;
}

void ConfigureVisualPrimitive(UPrimitiveComponent* Component, bool bCastShadow = true)
{
    if (!Component) return;
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetGenerateOverlapEvents(false);
    Component->SetCanEverAffectNavigation(false);
    Component->SetSimulatePhysics(false);
    Component->SetEnableGravity(false);
    Component->SetAffectDistanceFieldLighting(false);
    Component->SetCastShadow(bCastShadow);
}

bool ShouldCastResourceShadow(const FString& Name)
{
#if PLATFORM_ANDROID || PLATFORM_IOS
    // Keep a small set of large silhouette casters. Tiny repeated props still
    // receive the board shadow, but omitting them from the depth atlas avoids
    // drawing their geometry a second time every frame on tile GPUs.
    return Name.Contains(TEXT("PhotoTree"))
        || Name.Contains(TEXT("Workshop"))
        || Name.Contains(TEXT("Barn"))
        || Name.Contains(TEXT("PhotoMountain"))
        || Name.Contains(TEXT("Palm"));
#else
    return true;
#endif
}

void ConfigureBoardLighting(UWorld* World)
{
    if (!World) return;
    ADirectionalLight* BoardLightActor = nullptr;
    for (TActorIterator<ADirectionalLight> It(World); It; ++It)
    {
        if (It->ActorHasTag(TEXT("CatanBoardLight")))
        {
            BoardLightActor = *It;
            break;
        }
        // Do not depend on the lighting metadata of the engine template map.
        // Keep its skylight/atmosphere setup, but replace its sun with a light
        // owned and configured explicitly by the game.
        if (ULightComponent* TemplateLight = It->GetLightComponent())
            TemplateLight->SetIntensity(0.0f);
    }
    if (!BoardLightActor)
    {
        BoardLightActor = World->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(), FVector::ZeroVector, FRotator(-52.0f, -28.0f, 0.0f));
        if (BoardLightActor) BoardLightActor->Tags.Add(TEXT("CatanBoardLight"));
    }
    if (BoardLightActor)
    {
        UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(BoardLightActor->GetLightComponent());
        if (!Light) return;
        Light->SetMobility(EComponentMobility::Movable);
        Light->SetIntensity(6.0f);
        Light->SetLightColor(FLinearColor(1.0f, 0.94f, 0.84f));
        Light->SetCastShadows(true);
        Light->CastDynamicShadows = true;
#if PLATFORM_ANDROID || PLATFORM_IOS
        // The board fits comfortably inside this range. The template map used
        // 20,000 units and four cascades, which spends mobile GPU time on empty
        // ocean and distant geometry.
        Light->SetDynamicShadowDistanceMovableLight(4800.0f);
        Light->SetDynamicShadowCascades(1);
        Light->SetCascadeDistributionExponent(1.0f);
        Light->SetCascadeTransitionFraction(0.06f);
#endif
        Light->bCastVolumetricShadow = false;
        Light->bCastDeepShadow = false;
        Light->MarkRenderStateDirty();
        UE_LOG(LogTemp, Display, TEXT("CATAN_PERF directional_light=%s mobility=%d "
            "cast=%d dynamic_cast=%d shadow_distance=%.0f cascades=%d"),
            *BoardLightActor->GetName(), static_cast<int32>(Light->Mobility), static_cast<int32>(Light->CastShadows),
            static_cast<int32>(Light->CastDynamicShadows), Light->DynamicShadowDistanceMovableLight,
            Light->DynamicShadowCascades);
    }
}

void SetVisualActive(UPrimitiveComponent* Component, bool bActive)
{
    if (!Component) return;
    Component->SetHiddenInGame(!bActive);
    if (bActive && !Component->IsRegistered()) Component->RegisterComponent();
    else if (!bActive && Component->IsRegistered()) Component->UnregisterComponent();
}
}

ACatanBoardActor::ACatanBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SceneRoot->SetMobility(EComponentMobility::Static);
    RootComponent = SceneRoot;
    HexMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexMesh"));
    HexMesh->SetupAttachment(SceneRoot);
    ConfigureVisualPrimitive(HexMesh, false);
    EnvironmentMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentMesh"));
    EnvironmentMesh->SetupAttachment(SceneRoot);
    ConfigureVisualPrimitive(EnvironmentMesh, false);
}

void ACatanBoardActor::BeginPlay()
{
    Super::BeginPlay();
    ConfigureBoardLighting(GetWorld());
    BasicMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Materials/M_CatanColor.M_CatanColor"));
    if (UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>())
    {
        Subsystem->OnGameStateChanged.AddDynamic(this, &ACatanBoardActor::RefreshPieces);
    }
    RefreshPieces();
}

void ACatanBoardActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>())
    {
        Subsystem->OnGameStateChanged.RemoveDynamic(this, &ACatanBoardActor::RefreshPieces);
    }
    Super::EndPlay(EndPlayReason);
}

void ACatanBoardActor::BuildBoard()
{
    BuildEnvironment();
    BuildHexes();
    BuildResourceDecorations();
    BuildHexHitTargets();
    BuildNodes();
    BuildRoads();
    BuildPorts();
    BuildDice();
    int32 RegisteredBuildingParts = 0;
    int32 RegisteredRoadParts = 0;
    for (const UStaticMeshComponent* Part : BuildingParts)
        if (Part && Part->IsRegistered()) ++RegisteredBuildingParts;
    for (const UStaticMeshComponent* Part : RoadPavingParts)
        if (Part && Part->IsRegistered()) ++RegisteredRoadParts;
    UE_LOG(LogTemp, Display, TEXT("CATAN_PERF board resource_hism=%d animated_resources=%d "
        "building_variants=%d registered_building_parts=%d road_variants=%d registered_road_parts=%d"),
        ResourceInstancedParts.Num(), AnimatedResourceParts.Num(), BuildingParts.Num(),
        RegisteredBuildingParts, RoadPavingParts.Num(), RegisteredRoadParts);
}

bool ACatanBoardActor::TryBuildBoard()
{
    if (bBoardBuilt) return true;
    const UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return false;
    const FCatanGameView View = Subsystem->GetSnapshot();
    constexpr int32 ExpectedHexes = 19;
    constexpr int32 ExpectedNodes = 54;
    constexpr int32 ExpectedRoads = 72;
    if (View.Hexes.Num() != ExpectedHexes || View.Nodes.Num() != ExpectedNodes
        || View.Roads.Num() != ExpectedRoads)
    {
        return false;
    }
    BuildBoard();
    bBoardBuilt = true;
    UE_LOG(LogTemp, Display, TEXT("CATAN_SMOKE client board ready. WASD/QE and mouse wheel control the camera."));
    return true;
}

void ACatanBoardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bBoardBuilt) TryBuildBoard();
#if PLATFORM_ANDROID
    MobileAnimationAccumulator += DeltaSeconds;
    // Transform animation does not need to run at the render frame rate on a
    // touch device. Twenty updates per second remain smooth after interpolation
    // and avoid walking hundreds of component entries every frame.
    constexpr float MobileAnimationStep = 1.0f / 20.0f;
    if (MobileAnimationAccumulator < MobileAnimationStep) return;
    DeltaSeconds = MobileAnimationAccumulator;
    MobileAnimationAccumulator = 0.0f;
#endif
    AnimateFeedback(DeltaSeconds);
}

UStaticMeshComponent* ACatanBoardActor::AddDecoration(const FString& Name, UStaticMesh* Mesh,
    const FVector& Location, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation)
{
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, *Name);
    Component->SetupAttachment(SceneRoot);
    Component->RegisterComponent();
    Component->SetStaticMesh(Mesh);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Scale);
    ConfigureVisualPrimitive(Component);
    Component->SetMaterial(0, ColoredMaterial(this, BasicMaterial, Color));
    Decorations.Add(Component);
    return Component;
}

UStaticMeshComponent* ACatanBoardActor::AddAuthoredDecoration(const FString& Name, UStaticMesh* Mesh,
    const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
    if (!Mesh) return nullptr;
    UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, *Name);
    Component->SetupAttachment(SceneRoot);
    Component->SetStaticMesh(Mesh);
    Component->SetRelativeLocation(Location);
    Component->SetRelativeRotation(Rotation);
    Component->SetRelativeScale3D(Scale);
    ConfigureVisualPrimitive(Component);
    Component->RegisterComponent();
    Decorations.Add(Component);
    return Component;
}

void ACatanBoardActor::BuildEnvironment()
{
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    UStaticMeshComponent* Sea = AddDecoration(TEXT("Sea"), Plane, FVector(0, 0, -20),
        FVector(150.0f, 150.0f, 1.0f), FLinearColor(0.015f, 0.22f, 0.42f));
    if (UMaterialInterface* Water = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Environment/Ocean/M_CatanOcean.M_CatanOcean")))
    {
        Sea->SetMaterial(0, Water);
    }
    UStaticMeshComponent* SeaDepth = AddDecoration(TEXT("SeaDepth"), Plane, FVector(0, 0, -240),
        FVector(150.0f, 150.0f, 1.0f), FLinearColor(0.003f, 0.028f, 0.065f));
    if (UMaterialInterface* DeepWater = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Environment/Ocean/M_CatanOceanDepth.M_CatanOceanDepth")))
    {
        SeaDepth->SetMaterial(0, DeepWater);
    }

    BuildShore();
}

void ACatanBoardActor::BuildShore()
{
    TArray<FVector> Boundary = BoardBoundary();
    if (Boundary.Num() < 3) return;
    const UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const TArray<FVector> Centers = HexCenters();
    if (View.Hexes.Num() != Centers.Num()) return;

    float SignedArea = 0.0f;
    for (int32 Index = 0; Index < Boundary.Num(); ++Index)
    {
        const FVector& A = Boundary[Index];
        const FVector& B = Boundary[(Index + 1) % Boundary.Num()];
        SignedArea += A.X * B.Y - B.X * A.Y;
    }
    if (SignedArea < 0.0f) Algo::Reverse(Boundary);
    FVector Center = FVector::ZeroVector;
    for (const FVector& Point : Boundary) Center += Point;
    Center /= Boundary.Num();

    // Continue each outer hex's own surface into the sea. The progressively
    // wider and lower rings form a beach/shelf which vanishes below the water
    // instead of ending in a visible vertical foundation.
    struct FShoreRing { float Distance; float Height; };
    constexpr float VisibleDryShoreWidth = TileRadius * 0.125f;
    constexpr float CoastalRoadClearance = 25.0f;
    constexpr float DryShelfOuterEdge = VisibleDryShoreWidth + CoastalRoadClearance;
    constexpr FShoreRing Rings[] = {
        // Coastal roads are centred on the original hex edge. Keep a compact
        // level shelf under their outward half before beginning the descent;
        // all horizontal dimensions use the requested half-length profile.
        {0.0f, 0.0f}, {DryShelfOuterEdge, 0.0f},
        {DryShelfOuterEdge + 20.0f, -6.0f}, {DryShelfOuterEdge + 37.5f, -18.0f},
        {DryShelfOuterEdge + 57.5f, -42.0f}, {DryShelfOuterEdge + 82.5f, -92.0f},
        {DryShelfOuterEdge + 115.0f, -190.0f}
    };
    EnvironmentMesh->ClearAllMeshSections();
    for (int32 Edge = 0; Edge < Boundary.Num(); ++Edge)
    {
        const FVector A = Boundary[Edge];
        const FVector B = Boundary[(Edge + 1) % Boundary.Num()];
        const FVector Midpoint = (A + B) * 0.5f;
        int32 OwnerHex = INDEX_NONE;
        double BestDistanceSquared = TNumericLimits<double>::Max();
        for (int32 Hex = 0; Hex < Centers.Num(); ++Hex)
        {
            const double DistanceSquared = FVector::DistSquared2D(Midpoint, Centers[Hex]);
            if (DistanceSquared < BestDistanceSquared)
            {
                BestDistanceSquared = DistanceSquared;
                OwnerHex = Hex;
            }
        }
        if (!View.Hexes.IsValidIndex(OwnerHex)) continue;

        const ECatanResource Resource = View.Hexes[OwnerHex].Resource;
        const FLinearColor Color = ResourceColor(Resource);
        const FVector DirectionA = (A - Center).GetSafeNormal2D();
        const FVector DirectionB = (B - Center).GetSafeNormal2D();
        TArray<FVector> Vertices;
        TArray<int32> Triangles;
        TArray<FVector> Normals;
        TArray<FVector2D> UVs;
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;

        for (int32 Ring = 0; Ring + 1 < UE_ARRAY_COUNT(Rings); ++Ring)
        {
            const FVector InnerA = A + DirectionA * Rings[Ring].Distance
                + FVector(0, 0, Rings[Ring].Height);
            const FVector InnerB = B + DirectionB * Rings[Ring].Distance
                + FVector(0, 0, Rings[Ring].Height);
            const FVector OuterA = A + DirectionA * Rings[Ring + 1].Distance
                + FVector(0, 0, Rings[Ring + 1].Height);
            const FVector OuterB = B + DirectionB * Rings[Ring + 1].Distance
                + FVector(0, 0, Rings[Ring + 1].Height);
            const FVector Normal = FVector::CrossProduct(OuterB - InnerA, InnerB - InnerA)
                .GetSafeNormal();
            const int32 Base = Vertices.Num();
            const FVector Quad[] = {InnerA, InnerB, OuterB, OuterA};
            for (const FVector& Position : Quad)
            {
                Vertices.Add(Position);
                Normals.Add(Normal.Z >= 0.0f ? Normal : -Normal);
                UVs.Add(FVector2D(
                    0.5f + (Position.X - Centers[OwnerHex].X) / (TileRadius * 2.0f),
                    0.5f + (Position.Y - Centers[OwnerHex].Y) / (TileRadius * 2.0f)));
                Colors.Add(Color);
                Tangents.Add(FProcMeshTangent(1, 0, 0));
            }
            Triangles.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
            // The stepped outline contains both convex and concave turns. At
            // those turns a radial expansion can invert an individual strip,
            // while the imported ground materials are one-sided. Add the
            // reverse faces as well so every dry shelf and underwater slope is
            // visible from above along the complete coastline.
            Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
        }
        EnvironmentMesh->CreateMeshSection_LinearColor(Edge, Vertices, Triangles,
            Normals, UVs, Colors, Tangents, false);
        if (UMaterialInterface* GroundMaterial = CatanResourceVisuals::GetGroundMaterial(Resource))
            EnvironmentMesh->SetMaterial(Edge, GroundMaterial);
        else
            EnvironmentMesh->SetMaterial(Edge, ColoredMaterial(this, BasicMaterial, Color));
    }
}

void ACatanBoardActor::BuildHexHitTargets()
{
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    const TArray<FVector> Centers = HexCenters();
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("HexHit%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Cylinder);
        ConfigureVisualPrimitive(Slot, false);
        Slot->SetRelativeLocation(Centers[Index] + FVector(0, 0, 8));
        Slot->SetRelativeScale3D(FVector(4.0f, 4.0f, 0.05f));
        Slot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        Slot->SetCollisionResponseToAllChannels(ECR_Block);
        Slot->SetHiddenInGame(true);
        Slot->ComponentTags.Add(*FString::Printf(TEXT("Hex:%d"), Index));
        Slot->OnClicked.AddDynamic(this, &ACatanBoardActor::HandleSlotClicked);
        Slot->OnInputTouchBegin.AddDynamic(this, &ACatanBoardActor::HandleSlotTouched);
        HexSlots.Add(Slot);
    }
}

void ACatanBoardActor::BuildHexes()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const TArray<FVector> Centers = HexCenters();

    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        CreateHexSection(Index, Centers[Index], View.Hexes[Index].Resource,
            ResourceColor(View.Hexes[Index].Resource));

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("HexLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        ConfigureVisualPrimitive(Label, false);
        Label->SetRelativeRotation(FRotator(90, 180, 0));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetRelativeLocation(Centers[Index] + FVector(0, 0, 23));
        Label->SetWorldSize(48.0f);
        Label->SetText(View.Hexes[Index].Dice > 0
            ? FText::AsNumber(View.Hexes[Index].Dice)
            : FText::FromString(TEXT("—")));
        Labels.Add(Label);
        HexLabelSizeTargets.Add(48.0f);
    }
}

void ACatanBoardActor::CreateHexSection(int32 Index, const FVector& Center, ECatanResource Resource,
    const FLinearColor& Color)
{
    FCatanHexMeshBuffers Mesh;
    // Hex centers are exactly one full hex apart; full radius removes seams to the foundation.
    CatanHexMesh::AppendTop(Mesh, Center, TileRadius, Color);
    HexMesh->CreateMeshSection_LinearColor(Index, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
        Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
    if (UMaterialInterface* GroundMaterial = CatanResourceVisuals::GetGroundMaterial(Resource))
    {
        HexMesh->SetMaterial(Index, GroundMaterial);
        return;
    }
    HexMesh->SetMaterial(Index, ColoredMaterial(this, BasicMaterial, Color));
}

void ACatanBoardActor::BuildResourceDecorations()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const FName ResourceTag(TEXT("ResourceDecoration"));
    AnimatedResourceParts.Reset();
    ResourceAnimationKinds.Reset();
    ResourceAnimationPhases.Reset();
    ResourceAnimationLocations.Reset();
    ResourceAnimationScales.Reset();
    ResourceAnimationRotations.Reset();
    for (UProceduralMeshComponent* Part : ResourceProceduralParts)
        if (Part) Part->DestroyComponent();
    ResourceProceduralParts.Reset();
    for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
        if (Part) Part->DestroyComponent();
    ResourceInstancedParts.Reset();
    for (int32 Index = Decorations.Num() - 1; Index >= 0; --Index)
    {
        if (Decorations[Index] && Decorations[Index]->ComponentTags.Contains(ResourceTag))
        {
            Decorations[Index]->DestroyComponent();
            Decorations.RemoveAt(Index);
        }
    }
    TokenSlots.Reset();
    HexTokenScaleTargets.Reset();
    for (USkeletalMeshComponent* Figure : RobberFigures)
        if (Figure) Figure->DestroyComponent();
    RobberFigures.Reset();
    for (USceneComponent* Placeholder : RobberPlaceholders)
        if (Placeholder) Placeholder->DestroyComponent();
    RobberPlaceholders.Reset();
    RobberFigureOffsets.Reset();
    ++ResourceGeneration;
    const TArray<FVector> Centers = HexCenters();
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

    for (int32 Index = 0; Index < Centers.Num() && Index < View.Hexes.Num(); ++Index)
    {
        const FVector Center = Centers[Index];
        const float Angle = static_cast<float>((Index * 47) % 360);
        FCatanResourceVisualCallbacks Callbacks;
        Callbacks.AddStatic = [this, Index, Center, ResourceTag](const FString& Name, UStaticMesh* Mesh,
            const FVector& Local, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation)
        {
            UStaticMeshComponent* Component = AddDecoration(
                FString::Printf(TEXT("G%dHex%d%s"), ResourceGeneration, Index, *Name), Mesh,
                Center + Local, Scale, Color, Rotation);
            Component->ComponentTags.Add(ResourceTag);
            return Component;
        };
        Callbacks.AddAuthoredStatic = [this, Center, ResourceTag](const FString& Name, UStaticMesh* Mesh,
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
                    *FString::Printf(TEXT("G%d%sInstances"), ResourceGeneration, *Name));
                Instances->SetupAttachment(SceneRoot);
                Instances->SetStaticMesh(Mesh);
                Instances->SetMobility(EComponentMobility::Static);
                ConfigureVisualPrimitive(Instances, ShouldCastResourceShadow(Name));
#if PLATFORM_ANDROID || PLATFORM_IOS
                Instances->SetCullDistances(3000, 4600);
#else
                Instances->SetCullDistances(4200, 6500);
#endif
                Instances->bAutoRebuildTreeOnInstanceChanges = false;
                Instances->ComponentTags.Add(ResourceTag);
                ResourceInstancedParts.Add(Instances);
            }
            Instances->AddInstance(FTransform(Rotation, Center + Local, Scale));
        };
        // Static resource props have no per-object gameplay state. Leaving the
        // unique callback unset lets BuildCluster batch them through the HISM
        // callback above while robber/token interaction remains independent.
        Callbacks.AddColoredInstance = [this, Center, ResourceTag](const FString& Name, UStaticMesh* Mesh,
            const FVector& Local, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation)
        {
            UHierarchicalInstancedStaticMeshComponent* Instances = nullptr;
            for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
                if (Part && Part->GetName().Contains(Name)) { Instances = Part; break; }
            if (!Instances)
            {
                Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
                    *FString::Printf(TEXT("G%d%sInstances"), ResourceGeneration, *Name));
                Instances->SetupAttachment(SceneRoot);
                Instances->SetStaticMesh(Mesh);
                Instances->SetMaterial(0, ColoredMaterial(this, BasicMaterial, Color));
                Instances->SetMobility(EComponentMobility::Static);
                ConfigureVisualPrimitive(Instances, ShouldCastResourceShadow(Name));
#if PLATFORM_ANDROID || PLATFORM_IOS
                Instances->SetCullDistances(3000, 4600);
#else
                Instances->SetCullDistances(4200, 6500);
#endif
                Instances->bAutoRebuildTreeOnInstanceChanges = false;
                Instances->ComponentTags.Add(ResourceTag);
                ResourceInstancedParts.Add(Instances);
            }
            Instances->AddInstance(FTransform(Rotation, Center + Local, Scale));
        };
        Callbacks.AddMaterialStatic = [this, Center, ResourceTag](const FString& Name, UStaticMesh* Mesh,
            UMaterialInterface* Material, const FVector& Local, const FVector& Scale, const FRotator& Rotation)
        {
            UHierarchicalInstancedStaticMeshComponent* Instances = nullptr;
            for (UHierarchicalInstancedStaticMeshComponent* Part : ResourceInstancedParts)
                if (Part && Part->GetName().Contains(Name)) { Instances = Part; break; }
            if (!Instances)
            {
                Instances = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,
                    *FString::Printf(TEXT("G%d%sMaterialInstances"), ResourceGeneration, *Name));
                Instances->SetupAttachment(SceneRoot);
                Instances->SetStaticMesh(Mesh);
                Instances->SetMaterial(0, Material ? Material : ColoredMaterial(this, BasicMaterial,
                    FLinearColor(0.18f, 0.24f, 0.10f)));
                Instances->SetMobility(EComponentMobility::Static);
                ConfigureVisualPrimitive(Instances, false);
#if PLATFORM_ANDROID || PLATFORM_IOS
                Instances->SetCullDistances(3000, 4600);
#else
                Instances->SetCullDistances(4200, 6500);
#endif
                Instances->bAutoRebuildTreeOnInstanceChanges = false;
                Instances->ComponentTags.Add(ResourceTag);
                ResourceInstancedParts.Add(Instances);
            }
            Instances->AddInstance(FTransform(Rotation, Center + Local, Scale));
        };
#if PLATFORM_ANDROID || PLATFORM_IOS
        Callbacks.AuthoredForestTreeBudget = 3;
#endif
        Callbacks.Animate = [this, Index](UStaticMeshComponent* Component,
            ECatanResourceAnimation Kind, float Phase)
        {
            AnimatedResourceParts.Add(Component);
            switch (Kind)
            {
            case ECatanResourceAnimation::Sway: ResourceAnimationKinds.Add(ResourceSway); break;
            case ECatanResourceAnimation::Bob: ResourceAnimationKinds.Add(ResourceBob); break;
            case ECatanResourceAnimation::Pulse: ResourceAnimationKinds.Add(ResourcePulse); break;
            case ECatanResourceAnimation::Drift: ResourceAnimationKinds.Add(ResourceDrift); break;
            default: ResourceAnimationKinds.Add(0); break;
            }
            ResourceAnimationPhases.Add(Phase + Index * 0.37f);
            ResourceAnimationLocations.Add(Component->GetRelativeLocation());
            ResourceAnimationScales.Add(Component->GetRelativeScale3D());
            ResourceAnimationRotations.Add(Component->GetRelativeRotation());
        };
        Callbacks.AddHexPyramid = [this, Center, ResourceTag](const FString& Name, const FVector& Local,
            float Radius, float Height, const FLinearColor& Color)
        {
            UProceduralMeshComponent* Pyramid = NewObject<UProceduralMeshComponent>(this,
                *FString::Printf(TEXT("G%d%s"), ResourceGeneration, *Name));
            Pyramid->SetupAttachment(SceneRoot);
            Pyramid->RegisterComponent();
            ConfigureVisualPrimitive(Pyramid);
            Pyramid->ComponentTags.Add(ResourceTag);
            const FVector Position = Center + Local;
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
                Triangles.Add(0);
                Triangles.Add((Side + 1) % 6 + 1);
                Triangles.Add(Side + 1);
            }
            Pyramid->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
            Pyramid->SetMaterial(0, ColoredMaterial(this, BasicMaterial, Color));
            ResourceProceduralParts.Add(Pyramid);
        };
        CatanResourceVisuals::BuildCluster(View.Hexes[Index].Resource, Index, Angle,
            Cube, Sphere, Cylinder, Cone, Callbacks);

        USceneComponent* RobberPlaceholder = NewObject<USceneComponent>(this,
            *FString::Printf(TEXT("G%dHex%dRobberPlaceholder"), ResourceGeneration, Index));
        RobberPlaceholder->SetupAttachment(SceneRoot);
        RobberPlaceholder->SetRelativeLocation(
            Center + RobberPlaceholderOffset(View.Hexes[Index].Resource, Angle));
        RobberPlaceholder->SetVisibility(false, true);
        RobberPlaceholder->ComponentTags.Add(ResourceTag);
        RobberPlaceholder->RegisterComponent();
        RobberPlaceholders.Add(RobberPlaceholder);

        UStaticMeshComponent* Token = AddDecoration(FString::Printf(TEXT("G%dToken%d"), ResourceGeneration, Index), Cylinder,
            Center + FVector(0, 0, 12), FVector(0.62f, 0.62f, 0.08f), FLinearColor(0.92f, 0.85f, 0.68f));
        Token->ComponentTags.Add(ResourceTag);
        TokenSlots.Add(Token);
        HexTokenScaleTargets.Add(FVector(0.62f, 0.62f, 0.08f));
    }

    for (UHierarchicalInstancedStaticMeshComponent* Instances : ResourceInstancedParts)
    {
        if (!Instances) continue;
        Instances->bAutoRebuildTreeOnInstanceChanges = true;
        if (!Instances->IsRegistered()) Instances->RegisterComponent();
        Instances->BuildTreeIfOutdated(true, true);
    }

    USkeletalMesh* PirateMeshes[] = {
        LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_01.SKM_Pirate_Full_01")),
        LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_02.SKM_Pirate_Full_02")),
        LoadObject<USkeletalMesh>(nullptr,
            TEXT("/Game/Pirate/Mesh_UE5/Full/SKM_Pirate_Full_03.SKM_Pirate_Full_03"))
    };
    UAnimSequence* PirateIdle = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/Pirate/Demoscene_UE5/Animations/MM_Idle.MM_Idle"));
    const FVector GroupOffsets[] = {
        FVector(-70.0f, -16.0f, 0.0f), FVector(5.0f, -13.0f, 0.0f), FVector(-32.0f, 34.0f, 0.0f)
    };
    const float DesiredHeights[] = {111.26f, 128.0f, 144.74f};
    const float FigureYaws[] = {-14.0f, 11.0f, 2.0f};
    for (int32 Figure = 0; Figure < 3; ++Figure)
    {
        USkeletalMesh* PirateMesh = PirateMeshes[Figure];
        if (!PirateMesh) continue;
        const FBoxSphereBounds Bounds = PirateMesh->GetBounds();
        const float MeshHeight = FMath::Max(1.0f, Bounds.BoxExtent.Z * 2.0f);
        const float UniformScale = DesiredHeights[Figure] / MeshHeight;
        const float MeshBottom = Bounds.Origin.Z - Bounds.BoxExtent.Z;
        const FVector GroundedOffset = GroupOffsets[Figure]
            + FVector(0.0f, 0.0f, 5.0f - MeshBottom * UniformScale);

        USkeletalMeshComponent* Pirate = NewObject<USkeletalMeshComponent>(this,
            *FString::Printf(TEXT("G%dRobberPirate%d"), ResourceGeneration, Figure));
        Pirate->SetupAttachment(SceneRoot);
        Pirate->SetSkeletalMesh(PirateMesh);
        Pirate->SetRelativeScale3D(FVector(UniformScale));
        Pirate->SetRelativeRotation(FRotator(0.0f, FigureYaws[Figure], 0.0f));
        ConfigureVisualPrimitive(Pirate);
        Pirate->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
        Pirate->ComponentTags.Add(ResourceTag);
        Pirate->RegisterComponent();
        if (PirateIdle)
        {
            Pirate->PlayAnimation(PirateIdle, true);
            Pirate->SetPlayRate(0.82f + Figure * 0.08f);
            Pirate->SetPosition(Figure * 0.37f, false);
        }
        RobberFigures.Add(Pirate);
        RobberFigureOffsets.Add(GroundedOffset);
    }
    RenderedHexResources.Reset();
    for (const FCatanHexView& Hex : View.Hexes) RenderedHexResources.Add(static_cast<uint8>(Hex.Resource));
}

void ACatanBoardActor::BuildNodes()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* VillageMeshes[] = {
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Catan/RuntimeAssets/Viking/SM_Barn_1_2.SM_Barn_1_2")),
        // SM_Barn_1 is the open/missing-roof variant. Reuse the complete house;
        // a different yaw and scale below keep the cluster from looking cloned.
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Catan/RuntimeAssets/Viking/SM_Barn_1_2.SM_Barn_1_2")),
        LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Catan/RuntimeAssets/Viking/SM_Barn_2.SM_Barn_2"))
    };
    UStaticMesh* CastleTowerMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Catan/Buildings/catan_castle_tower_only/StaticMeshes/SM_CastleTower.SM_CastleTower"));
    UMaterialInterface* CastleStoneMaterial = LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/Fab/Medieval_Castle_Tower_4/castle_4/Materials/wall_1_c_tower.wall_1_c_tower"));

    auto AddBuildingPart = [this](UStaticMeshComponent* Part, int32 NodeId, uint8 Mode,
        bool bUsesPlayerColor, float Shade)
    {
        if (!Part) return;
        SetVisualActive(Part, false);
        BuildingParts.Add(Part);
        BuildingPartNodeIds.Add(NodeId);
        BuildingPartModes.Add(Mode);
        BuildingPartUsesPlayerColor.Add(bUsesPlayerColor);
        BuildingPartShades.Add(Shade);
        BuildingPartScaleTargets.Add(Part->GetRelativeScale3D());
    };

    auto AddGroundedBuilding = [this](const FString& Name, UStaticMesh* Mesh,
        const FVector& Center, double BaseZ, double Scale, const FRotator& Rotation)
    {
        if (!Mesh) return static_cast<UStaticMeshComponent*>(nullptr);
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FBox LocalBox(Bounds.Origin - Bounds.BoxExtent, Bounds.Origin + Bounds.BoxExtent);
        const FBox RotatedBox = LocalBox.TransformBy(FTransform(
            Rotation, FVector::ZeroVector, FVector(Scale)));
        FVector Location = Center - FVector(RotatedBox.GetCenter().X, RotatedBox.GetCenter().Y, 0.0f);
        Location.Z = BaseZ - RotatedBox.Min.Z;
        return AddAuthoredDecoration(Name, Mesh, Location, FVector(Scale), Rotation);
    };
    const TArray<FVector> Centers = NodeCenters();
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        const FVector GroundCenter(Centers[Index].X, Centers[Index].Y, 0.0f);
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Node%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Sphere);
        ConfigureVisualPrimitive(Slot, false);
        Slot->SetRelativeLocation(Centers[Index]);
        Slot->SetRelativeScale3D(FVector(0.13f));
        Slot->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Slot->SetHiddenInGame(true);
        Slot->ComponentTags.Add(*FString::Printf(TEXT("Node:%d"), Index));
        Slot->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.08f, 0.1f, 0.12f)));
        Slot->OnClicked.AddDynamic(this, &ACatanBoardActor::HandleSlotClicked);
        Slot->OnInputTouchBegin.AddDynamic(this, &ACatanBoardActor::HandleSlotTouched);
        NodeSlots.Add(Slot);

        const FVector HouseOffsets[] = {
            FVector(-30, -22, 0), FVector(29, -21, 0), FVector(0, 27, 0)
        };
        const float HouseYaws[] = {18.0f, -24.0f, 92.0f};
        const float HouseWidths[] = {50.0f, 50.0f, 54.0f};
        for (int32 House = 0; House < UE_ARRAY_COUNT(HouseOffsets); ++House)
        {
            if (!VillageMeshes[House]) continue;
            const FBoxSphereBounds Bounds = VillageMeshes[House]->GetBounds();
            const double Scale = HouseWidths[House] / FMath::Max(Bounds.BoxExtent.X * 2.0, 1.0);
            AddBuildingPart(AddGroundedBuilding(
                FString::Printf(TEXT("Village%dHouse%d"), Index, House), VillageMeshes[House],
                GroundCenter + HouseOffsets[House], 3.5, Scale, FRotator(0, HouseYaws[House], 0)),
                Index, 0, false, 1.0f);

            const double HouseTop = 3.5 + Bounds.BoxExtent.Z * 2.0 * Scale;
            UStaticMeshComponent* FlagPole = AddDecoration(
                FString::Printf(TEXT("Village%dFlagPole%d"), Index, House), Cylinder,
                GroundCenter + HouseOffsets[House] + FVector(0, 0, HouseTop + 7.0),
                FVector(0.018f, 0.018f, 0.15f), FLinearColor::White);
            const FRotator FlagRotation(0, HouseYaws[House], 0);
            const FVector FlagAnchor = GroundCenter + HouseOffsets[House]
                + FVector(0, 0, HouseTop + 15.0);
            UStaticMeshComponent* Flag = AddDecoration(
                FString::Printf(TEXT("Village%dFlag%d"), Index, House), Cube,
                FlagAnchor + FlagRotation.RotateVector(FVector(14, 0, 0)),
                FVector(0.28f, 0.018f, 0.13f), FLinearColor::White, FlagRotation);
            AddBuildingPart(FlagPole, Index, 0, true, 0.72f);
            AddBuildingPart(Flag, Index, 0, true, 1.0f);
            FlagCloths.Add(Flag);
            FlagAnchors.Add(FlagAnchor);
            FlagBaseYaws.Add(HouseYaws[House]);
            FlagHalfLengths.Add(14.0f);
            FlagAnimationPhases.Add(FlagAnimationPhases.Num() * 0.73f);
        }

        const FVector TowerOffsets[] = {
            FVector(-31, -31, 0), FVector(31, -31, 0), FVector(-31, 31, 0),
            FVector(31, 31, 0), FVector(0, 0, 0)
        };
        const float TowerYaws[] = {15.0f, -18.0f, 32.0f, -30.0f, 8.0f};
        for (int32 Tower = 0; Tower < UE_ARRAY_COUNT(TowerOffsets); ++Tower)
        {
            if (!CastleTowerMesh) continue;
            const bool bCenterTower = Tower == 4;
            const float TargetHeight = bCenterTower ? 98.0f : 70.0f;
            const FBoxSphereBounds TowerBounds = CastleTowerMesh->GetBounds();
            const double TowerScale = TargetHeight
                / FMath::Max(TowerBounds.BoxExtent.Z * 2.0, 1.0);
            const FRotator TowerRotation(0, TowerYaws[Tower], 0.0f);
            AddBuildingPart(AddGroundedBuilding(
                FString::Printf(TEXT("City%dTower%d"), Index, Tower), CastleTowerMesh,
                GroundCenter + TowerOffsets[Tower], 3.5, TowerScale, TowerRotation),
                Index, 1, false, 1.0f);

            const FVector FlagBase = GroundCenter + TowerOffsets[Tower]
                + FVector(0, 0, 3.5f + TargetHeight + (bCenterTower ? 8.0f : 7.0f));
            UStaticMeshComponent* FlagPole = AddDecoration(
                FString::Printf(TEXT("City%dFlagPole%d"), Index, Tower), Cylinder,
                FlagBase, bCenterTower ? FVector(0.018f, 0.018f, 0.16f)
                    : FVector(0.015f, 0.015f, 0.14f), FLinearColor::White);
            const float FlagHalfLength = bCenterTower ? 16.0f : 13.0f;
            const float FlagHeightOffset = bCenterTower ? 5.0f : 4.0f;
            const FRotator FlagRotation(0, TowerYaws[Tower], 0);
            const FVector FlagAnchor = FlagBase + FVector(0, 0, FlagHeightOffset);
            UStaticMeshComponent* Flag = AddDecoration(
                FString::Printf(TEXT("City%dFlag%d"), Index, Tower), Cube,
                FlagAnchor + FlagRotation.RotateVector(FVector(FlagHalfLength, 0, 0)),
                bCenterTower ? FVector(0.32f, 0.018f, 0.14f)
                    : FVector(0.26f, 0.015f, 0.12f), FLinearColor::White, FlagRotation);
            AddBuildingPart(FlagPole, Index, 1, true, 0.72f);
            AddBuildingPart(Flag, Index, 1, true, 1.0f);
            FlagCloths.Add(Flag);
            FlagAnchors.Add(FlagAnchor);
            FlagBaseYaws.Add(TowerYaws[Tower]);
            FlagHalfLengths.Add(FlagHalfLength);
            FlagAnimationPhases.Add(FlagAnimationPhases.Num() * 0.73f);
        }

        struct FWallSpec { FVector Offset; FVector Scale; };
        const FWallSpec Walls[] = {
            {FVector(0, -32, 20), FVector(0.40f, 0.075f, 0.32f)},
            {FVector(0, 32, 20), FVector(0.40f, 0.075f, 0.32f)},
            {FVector(-32, 0, 20), FVector(0.075f, 0.40f, 0.32f)},
            {FVector(32, 0, 20), FVector(0.075f, 0.40f, 0.32f)}
        };
        for (int32 Wall = 0; Wall < UE_ARRAY_COUNT(Walls); ++Wall)
        {
            UStaticMeshComponent* WallPart = AddDecoration(
                FString::Printf(TEXT("City%dWall%d"), Index, Wall), Cube,
                GroundCenter + Walls[Wall].Offset, Walls[Wall].Scale, FLinearColor::White);
            if (CastleStoneMaterial) WallPart->SetMaterial(0, CastleStoneMaterial);
            AddBuildingPart(WallPart, Index, 1, false, 1.0f);
        }
    }
}

void ACatanBoardActor::BuildRoads()
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cobblestones = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Cobblestones_Scan/cobblestones_scan/StaticMeshes/cobblestones_scan.cobblestones_scan"));
    const TArray<FRoadPlacement> Placements = RoadCenters();
    for (int32 Index = 0; Index < Placements.Num(); ++Index)
    {
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Road%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Cube);
        ConfigureVisualPrimitive(Slot, false);
        Slot->SetRelativeLocation(FVector(Placements[Index].Position.X, Placements[Index].Position.Y, 5.0f));
        Slot->SetRelativeRotation(FRotator(0, Placements[Index].Angle, 0));
        Slot->SetRelativeScale3D(FVector(1.9f, 0.07f, 0.05f));
        Slot->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Slot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Slot->SetHiddenInGame(true);
        Slot->ComponentTags.Add(*FString::Printf(TEXT("Road:%d"), Index));
        Slot->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.14f, 0.15f, 0.16f)));
        Slot->OnClicked.AddDynamic(this, &ACatanBoardActor::HandleSlotClicked);
        Slot->OnInputTouchBegin.AddDynamic(this, &ACatanBoardActor::HandleSlotTouched);
        RoadSlots.Add(Slot);

        const FRotator RoadRotation(0, Placements[Index].Angle, 0);
        const FVector RoadDirection = RoadRotation.RotateVector(FVector::XAxisVector);
        const FVector RoadSide = RoadRotation.RotateVector(FVector::YAxisVector);

        // Six small pennants identify the owner without tinting or framing the
        // photorealistic cobblestones: a pair at each end and one in the middle.
        constexpr float FlagStations[] = {-58.0f, 0.0f, 58.0f};
        for (int32 Station = 0; Station < UE_ARRAY_COUNT(FlagStations); ++Station)
        {
            for (int32 Side = -1; Side <= 1; Side += 2)
            {
                const FVector PoleCenter = FVector(
                    Placements[Index].Position.X, Placements[Index].Position.Y, 11.0f)
                    + RoadDirection * FlagStations[Station] + RoadSide * (19.0f * Side);
                UStaticMeshComponent* Pole = AddDecoration(
                    FString::Printf(TEXT("Road%dFlagPole%d_%d"), Index, Station, Side), Cylinder,
                    PoleCenter, FVector(0.012f, 0.012f, 0.16f),
                    FLinearColor(0.16f, 0.075f, 0.025f));
                const FVector FlagAnchor = PoleCenter + FVector(0, 0, 5.0f);
                UStaticMeshComponent* Flag = AddDecoration(
                    FString::Printf(TEXT("Road%dFlag%d_%d"), Index, Station, Side), Cube,
                    FlagAnchor + RoadDirection * 10.0f,
                    FVector(0.20f, 0.012f, 0.11f), FLinearColor::White, RoadRotation);
                SetVisualActive(Pole, false);
                SetVisualActive(Flag, false);
                RoadPavingParts.Add(Pole);
                RoadPavingRoadIds.Add(Index);
                RoadPavingPartUsesPlayerColor.Add(false);
                RoadPavingScaleTargets.Add(Pole->GetRelativeScale3D());
                RoadPavingParts.Add(Flag);
                RoadPavingRoadIds.Add(Index);
                RoadPavingPartUsesPlayerColor.Add(true);
                RoadPavingScaleTargets.Add(Flag->GetRelativeScale3D());
                FlagCloths.Add(Flag);
                FlagAnchors.Add(FlagAnchor);
                FlagBaseYaws.Add(Placements[Index].Angle);
                FlagHalfLengths.Add(10.0f);
                FlagAnimationPhases.Add(FlagAnimationPhases.Num() * 0.73f);
            }
        }

        if (Cobblestones)
        {
            const FBoxSphereBounds Bounds = Cobblestones->GetBounds();
            constexpr int32 SegmentCount = 7;
            constexpr float NeighborTurns[] = {86.0f, 93.0f, 81.0f, 98.0f, 89.0f, 95.0f};
            const double CobbleScale = 29.0 / FMath::Max(Bounds.BoxExtent.Y * 2.0, 1.0);

            float LocalYaws[SegmentCount]{};
            float AlongHalfExtents[SegmentCount]{};
            float AlongPositions[SegmentCount]{};
            const float ScaledHalfX = Bounds.BoxExtent.X * CobbleScale;
            const float ScaledHalfY = Bounds.BoxExtent.Y * CobbleScale;
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                if (Segment > 0)
                    LocalYaws[Segment] = LocalYaws[Segment - 1] + NeighborTurns[Segment - 1];
                const float Radians = FMath::DegreesToRadians(LocalYaws[Segment]);
                AlongHalfExtents[Segment] = FMath::Abs(FMath::Cos(Radians)) * ScaledHalfX
                    + FMath::Abs(FMath::Sin(Radians)) * ScaledHalfY;
                if (Segment > 0)
                {
                    // Move the next patch back by 25% of the smaller projected
                    // length, producing a real overlap even after rotation.
                    const float Overlap = 0.5f * FMath::Min(
                        AlongHalfExtents[Segment - 1], AlongHalfExtents[Segment]);
                    AlongPositions[Segment] = AlongPositions[Segment - 1]
                        + AlongHalfExtents[Segment - 1] + AlongHalfExtents[Segment] - Overlap;
                }
            }
            const float PositionCenter = 0.5f * AlongPositions[SegmentCount - 1];
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const float Along = AlongPositions[Segment] - PositionCenter;
                const FVector DesiredCenter = FVector(
                    Placements[Index].Position.X, Placements[Index].Position.Y, 0.0f)
                    + RoadDirection * Along;
                const FRotator SegmentRotation(
                    0, Placements[Index].Angle + LocalYaws[Segment], 0);
                const FVector RotatedOrigin = SegmentRotation.RotateVector(Bounds.Origin * CobbleScale);
                FVector Location = DesiredCenter - FVector(RotatedOrigin.X, RotatedOrigin.Y, 0.0f);
                Location.Z = 2.0 - (Bounds.Origin.Z - Bounds.BoxExtent.Z) * CobbleScale;
                UStaticMeshComponent* SegmentMesh = AddAuthoredDecoration(
                    FString::Printf(TEXT("Road%dCobble%d"), Index, Segment), Cobblestones,
                    Location, FVector(CobbleScale), SegmentRotation);
                SetVisualActive(SegmentMesh, false);
                RoadPavingParts.Add(SegmentMesh);
                RoadPavingRoadIds.Add(Index);
                RoadPavingPartUsesPlayerColor.Add(false);
                RoadPavingScaleTargets.Add(SegmentMesh->GetRelativeScale3D());
            }
        }
    }
}

void ACatanBoardActor::BuildPorts()
{
    struct FPort
    {
        int32 FirstNode;
        int32 SecondNode;
        const TCHAR* Label;
        bool bGeneric;
        ECatanResource Cargo;
    };
    constexpr FPort Ports[] = {
        {0, 1, TEXT("3:1"), true, ECatanResource::Desert},
        {3, 4, TEXT("2:1 SHEEP"), false, ECatanResource::Sheep},
        {14, 15, TEXT("3:1"), true, ECatanResource::Desert},
        {26, 37, TEXT("3:1"), true, ECatanResource::Desert},
        {45, 46, TEXT("2:1 CLAY"), false, ECatanResource::Clay},
        {47, 48, TEXT("3:1"), true, ECatanResource::Desert},
        {50, 51, TEXT("2:1 WOOD"), false, ECatanResource::Wood},
        {28, 38, TEXT("2:1 HAY"), false, ECatanResource::Hay},
        {7, 17, TEXT("2:1 STONE"), false, ECatanResource::Stone}
    };
    UStaticMesh* ShipMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Gislinge_Viking_Boat/gislinge_viking_boat/StaticMeshes/gislinge_viking_boat.gislinge_viking_boat"));
    UStaticMesh* BridgeMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Old_Dry_Bamboo_Bridge/olddry_bamboo_bridge/StaticMeshes/olddry_bamboo_bridge.olddry_bamboo_bridge"));
    UStaticMesh* WoodCargoMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Cut_ashtree_logs_stack/cut_ashtree_logs_stack/StaticMeshes/cut_ashtree_logs_stack.cut_ashtree_logs_stack"));
    UStaticMesh* ClayCargoMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Megascans/3D/Stacked_Bricks_wjykcfnqx/Medium/wjykcfnqx_tier_2/StaticMeshes/wjykcfnqx_tier_2.wjykcfnqx_tier_2"));
    UStaticMesh* HayCargoMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Megascans/3D/Round_Hay_Bale_rlCay/Medium/rlCay_tier_2/StaticMeshes/rlCay_tier_2.rlCay_tier_2"));
    UStaticMesh* SheepCargoMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Fab/Suffolk_Sheep_Thick_Wool_Fleece_Standing_Pose_3D_Model/3d_765/StaticMeshes/3d_765.3d_765"));
    UStaticMesh* StoneCargoMesh = LoadObject<UStaticMesh>(nullptr,
        TEXT("/Game/Catan/RuntimeAssets/Viking/SM_Stone_1.SM_Stone_1"));
    if (!ShipMesh || !BridgeMesh)
    {
        UE_LOG(LogTemp, Error, TEXT("Catan ports: authored ship or bridge mesh is missing"));
        return;
    }

    auto AddGroundedAuthored = [this](const FString& Name, UStaticMesh* Mesh,
        const FVector& BoundsCenter, float BaseZ, const FVector& Scale, const FRotator& Rotation)
    {
        if (!Mesh) return static_cast<UStaticMeshComponent*>(nullptr);
        const FBoxSphereBounds Bounds = Mesh->GetBounds();
        const FVector ScaledOrigin(Bounds.Origin.X * Scale.X, Bounds.Origin.Y * Scale.Y,
            Bounds.Origin.Z * Scale.Z);
        const FVector RotatedOrigin = Rotation.RotateVector(ScaledOrigin);
        const FVector ScaledExtent(Bounds.BoxExtent.X * Scale.X, Bounds.BoxExtent.Y * Scale.Y,
            Bounds.BoxExtent.Z * Scale.Z);
        FVector RotatedExtent = FVector::ZeroVector;
        for (int32 X = -1; X <= 1; X += 2)
            for (int32 Y = -1; Y <= 1; Y += 2)
                for (int32 Z = -1; Z <= 1; Z += 2)
                {
                    const FVector Corner = Rotation.RotateVector(
                        FVector(ScaledExtent.X * X, ScaledExtent.Y * Y, ScaledExtent.Z * Z));
                    RotatedExtent.X = FMath::Max(RotatedExtent.X, FMath::Abs(Corner.X));
                    RotatedExtent.Y = FMath::Max(RotatedExtent.Y, FMath::Abs(Corner.Y));
                    RotatedExtent.Z = FMath::Max(RotatedExtent.Z, FMath::Abs(Corner.Z));
                }
        FVector PivotLocation = BoundsCenter - FVector(RotatedOrigin.X, RotatedOrigin.Y, 0.0f);
        PivotLocation.Z = BaseZ - (RotatedOrigin.Z - RotatedExtent.Z);
        return AddAuthoredDecoration(Name, Mesh, PivotLocation, Scale, Rotation);
    };

    auto FitCargoScale = [](UStaticMesh* Mesh, const FVector& MaximumSize) -> double
    {
        if (!Mesh) return 0.0;
        const FVector Size = Mesh->GetBounds().BoxExtent * 2.0f;
        return FMath::Min3(MaximumSize.X / FMath::Max(Size.X, 0.01f),
            MaximumSize.Y / FMath::Max(Size.Y, 0.01f),
            MaximumSize.Z / FMath::Max(Size.Z, 0.01f));
    };

    constexpr float ShipLength = 300.0f;
    const FBoxSphereBounds ShipBounds = ShipMesh->GetBounds();
    const float ShipScale = ShipLength / (ShipBounds.BoxExtent.Y * 2.0f);
    const float ShipWidth = ShipBounds.BoxExtent.X * 2.0f * ShipScale;
    const TArray<FVector> Nodes = NodeCenters();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ports); ++Index)
    {
        const FVector Shore = (Nodes[Ports[Index].FirstNode] + Nodes[Ports[Index].SecondNode]) * 0.5f;
        FVector Direction(Shore.X, Shore.Y, 0);
        Direction.Normalize();
        const FVector Along(-Direction.Y, Direction.X, 0);
        const FVector ShipPosition = Shore + Direction * (175.0f + ShipWidth * 0.5f);
        const float ShipYaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
        const FRotator ShipRotation(0, ShipYaw, 0);

        auto AddBridge = [this, BridgeMesh, AddGroundedAuthored, Index](int32 BridgeIndex,
            const FVector& Start, const FVector& End)
        {
            const FVector Delta = End - Start;
            const float Length = Delta.Size2D();
            const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
            const FVector Midpoint = (Start + End) * 0.5f;
            const FBoxSphereBounds Bounds = BridgeMesh->GetBounds();
            const FVector Scale(Length / (Bounds.BoxExtent.X * 2.0f), 0.082f, 0.072f);
            AddGroundedAuthored(FString::Printf(TEXT("Port%dBridge%d"), Index, BridgeIndex),
                BridgeMesh, Midpoint, -3.0f, Scale, FRotator(0, Yaw, 0));
        };
        const FVector DockCenter = ShipPosition - Direction * (ShipWidth * 0.46f);
        const FVector ShipEnds[] = {
            DockCenter - Along * 60.0f,
            DockCenter + Along * 60.0f
        };
        const FVector PortNodes[] = {
            FVector(Nodes[Ports[Index].FirstNode].X, Nodes[Ports[Index].FirstNode].Y, 2.0f),
            FVector(Nodes[Ports[Index].SecondNode].X, Nodes[Ports[Index].SecondNode].Y, 2.0f)
        };
        const float DirectDistance = FVector::DistSquared2D(ShipEnds[0], PortNodes[0])
            + FVector::DistSquared2D(ShipEnds[1], PortNodes[1]);
        const float CrossDistance = FVector::DistSquared2D(ShipEnds[0], PortNodes[1])
            + FVector::DistSquared2D(ShipEnds[1], PortNodes[0]);
        if (DirectDistance <= CrossDistance)
        {
            AddBridge(0, ShipEnds[0], PortNodes[0]);
            AddBridge(1, ShipEnds[1], PortNodes[1]);
        }
        else
        {
            AddBridge(0, ShipEnds[0], PortNodes[1]);
            AddBridge(1, ShipEnds[1], PortNodes[0]);
        }

        AddGroundedAuthored(FString::Printf(TEXT("Port%dShip"), Index), ShipMesh,
            ShipPosition, -7.0f, FVector(ShipScale), ShipRotation);

        UStaticMesh* CargoMeshes[] = {
            WoodCargoMesh, ClayCargoMesh, HayCargoMesh, SheepCargoMesh, StoneCargoMesh
        };
        const FVector CargoMaximumSizes[] = {
            FVector(28, 34, 24), FVector(28, 32, 24), FVector(28, 32, 23),
            FVector(27, 32, 28), FVector(28, 32, 23)
        };
        const FRotator CargoRotations[] = {
            FRotator(0, ShipYaw, 90.0f), ShipRotation, ShipRotation, ShipRotation, ShipRotation
        };
        // Keep the cargo on the narrow centerline of the real deck. The hull's bounding box is
        // much wider than its usable interior, particularly near the bow and stern.
        const FVector CargoSlots[] = {
            ShipPosition - Along * 65.0f - Direction * 3.0f,
            ShipPosition - Along * 40.0f + Direction * 3.0f,
            ShipPosition - Along * 15.0f - Direction * 3.0f,
            ShipPosition + Along * 25.0f - Direction * 3.0f,
            ShipPosition + Along * 50.0f + Direction * 3.0f
        };
        if (Ports[Index].bGeneric)
        {
            // A 3:1 ship carries one compact sample of every resource instead of looking empty.
            for (int32 CargoIndex = 0; CargoIndex < UE_ARRAY_COUNT(CargoMeshes); ++CargoIndex)
            {
                const double CargoScale = FitCargoScale(CargoMeshes[CargoIndex], CargoMaximumSizes[CargoIndex]);
                AddGroundedAuthored(FString::Printf(TEXT("Port%dCargo%d"), Index, CargoIndex),
                    CargoMeshes[CargoIndex], CargoSlots[CargoIndex], 6.0f,
                    FVector(CargoScale), CargoRotations[CargoIndex]);
            }
        }
        else
        {
            const int32 ResourceIndex = static_cast<int32>(Ports[Index].Cargo);
            if (ResourceIndex >= 0 && ResourceIndex < UE_ARRAY_COUNT(CargoMeshes))
            {
                const double CargoScale = FitCargoScale(
                    CargoMeshes[ResourceIndex], CargoMaximumSizes[ResourceIndex]);
                // Three pieces behind the sail and two in front, all inside the safe deck footprint.
                for (int32 CargoIndex = 0; CargoIndex < UE_ARRAY_COUNT(CargoSlots); ++CargoIndex)
                    AddGroundedAuthored(FString::Printf(TEXT("Port%dCargo%d"), Index, CargoIndex),
                        CargoMeshes[ResourceIndex], CargoSlots[CargoIndex], 6.0f,
                        FVector(CargoScale), CargoRotations[ResourceIndex]);
            }
        }

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("PortLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        ConfigureVisualPrimitive(Label, false);
        Label->SetRelativeLocation(ShipPosition + Direction * (ShipWidth * 0.5f + 26.0f) + FVector(0, 0, 8));
        Label->SetRelativeRotation(FRotator(90, 180, 0));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetWorldSize(24.0f);
        Label->SetTextRenderColor(FColor(255, 224, 150));
        Label->SetText(FText::FromString(Ports[Index].Label));
        PortLabels.Add(Label);
    }
}

void ACatanBoardActor::BuildDice()
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    DiceTargetRotations.SetNum(2);
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FVector Position(-95.0f + Index * 190.0f, DiceTableY, 70.0f);
        UStaticMeshComponent* Die = AddDecoration(FString::Printf(TEXT("Die%d"), Index), Cube,
            Position, FVector(0.58f), FLinearColor(0.94f, 0.88f, 0.70f), FRotator(12, Index * 23.0f, 8));
        DicePieces.Add(Die);
        DiceTargetRotations[Index] = Die->GetRelativeRotation().Quaternion();

        auto AddFace = [this, Sphere, Die, Index](int32 Value, const FVector& Normal,
            const FVector& Horizontal, const FVector& Vertical)
        {
            const TArray<FVector2D> Offsets = PipOffsets(Value);
            for (int32 PipIndex = 0; PipIndex < Offsets.Num(); ++PipIndex)
            {
                UStaticMeshComponent* Pip = NewObject<UStaticMeshComponent>(this,
                    *FString::Printf(TEXT("Die%dFace%dPip%d"), Index, Value, PipIndex));
                Pip->SetupAttachment(Die);
                Pip->RegisterComponent();
                Pip->SetStaticMesh(Sphere);
                ConfigureVisualPrimitive(Pip, false);
                Pip->SetRelativeLocation(Normal * 51.2f
                    + Horizontal * Offsets[PipIndex].X + Vertical * Offsets[PipIndex].Y);
                FVector PipScale(0.105f);
                if (!FMath::IsNearlyZero(Normal.X)) PipScale.X = 0.025f;
                if (!FMath::IsNearlyZero(Normal.Y)) PipScale.Y = 0.025f;
                if (!FMath::IsNearlyZero(Normal.Z)) PipScale.Z = 0.025f;
                Pip->SetRelativeScale3D(PipScale);
                Pip->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.025f, 0.018f, 0.012f)));
                Decorations.Add(Pip);
            }
        };

        AddFace(1, FVector::UpVector, FVector::XAxisVector, FVector::YAxisVector);
        AddFace(6, -FVector::UpVector, FVector::XAxisVector, -FVector::YAxisVector);
        AddFace(3, FVector::XAxisVector, FVector::YAxisVector, FVector::UpVector);
        AddFace(4, -FVector::XAxisVector, -FVector::YAxisVector, FVector::UpVector);
        AddFace(2, FVector::YAxisVector, -FVector::XAxisVector, FVector::UpVector);
        AddFace(5, -FVector::YAxisVector, FVector::XAxisVector, FVector::UpVector);
    }
}

void ACatanBoardActor::PlayFeedbackTone(float Frequency, float Duration, float Volume)
{
    constexpr int32 SampleRate = 24000;
    const int32 SampleCount = FMath::Max(1, FMath::RoundToInt(SampleRate * Duration));
    TArray<int16> Samples;
    Samples.SetNumUninitialized(SampleCount);
    for (int32 Index = 0; Index < SampleCount; ++Index)
    {
        const float Time = static_cast<float>(Index) / SampleRate;
        const float Envelope = FMath::Square(1.0f - static_cast<float>(Index) / SampleCount);
        const float Fundamental = FMath::Sin(2.0f * PI * Frequency * Time);
        const float Harmonic = 0.25f * FMath::Sin(2.0f * PI * Frequency * 2.0f * Time);
        Samples[Index] = static_cast<int16>(FMath::Clamp((Fundamental + Harmonic) * Envelope * Volume, -1.0f, 1.0f) * 32767.0f);
    }
    FeedbackSound = NewObject<USoundWaveProcedural>(this);
    FeedbackSound->SetSampleRate(SampleRate);
    FeedbackSound->NumChannels = 1;
    FeedbackSound->Duration = Duration;
    FeedbackSound->SoundGroup = SOUNDGROUP_UI;
    FeedbackSound->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
    UGameplayStatics::PlaySound2D(this, FeedbackSound);
}

void ACatanBoardActor::AnimateFeedback(float DeltaSeconds)
{
    ResourceAnimationClock += DeltaSeconds;
    for (int32 Index = 0; Index < Labels.Num() && Index < HexLabelSizeTargets.Num(); ++Index)
        Labels[Index]->SetWorldSize(FMath::FInterpTo(
            Labels[Index]->WorldSize, HexLabelSizeTargets[Index], DeltaSeconds, 10.0f));
    for (int32 Index = 0; Index < TokenSlots.Num() && Index < HexTokenScaleTargets.Num(); ++Index)
        TokenSlots[Index]->SetRelativeScale3D(FMath::VInterpTo(
            TokenSlots[Index]->GetRelativeScale3D(), HexTokenScaleTargets[Index], DeltaSeconds, 10.0f));
    for (int32 Index = 0; Index < AnimatedResourceParts.Num(); ++Index)
    {
        UStaticMeshComponent* Part = AnimatedResourceParts[Index];
        if (!Part || !ResourceAnimationKinds.IsValidIndex(Index)
            || !ResourceAnimationLocations.IsValidIndex(Index)
            || !ResourceAnimationScales.IsValidIndex(Index)
            || !ResourceAnimationRotations.IsValidIndex(Index)) continue;
        const float Time = ResourceAnimationClock + ResourceAnimationPhases[Index];
        const FVector BaseLocation = ResourceAnimationLocations[Index];
        const FVector BaseScale = ResourceAnimationScales[Index];
        const FRotator BaseRotation = ResourceAnimationRotations[Index];
        switch (ResourceAnimationKinds[Index])
        {
        case ResourceSway:
            Part->SetRelativeLocation(BaseLocation + FVector(FMath::Sin(Time * 1.25f) * 1.8f, FMath::Cos(Time) * 1.2f, 0));
            Part->SetRelativeRotation(BaseRotation + FRotator(FMath::Sin(Time) * 2.6f, 0, FMath::Cos(Time * 0.8f) * 3.2f));
            break;
        case ResourceBob:
            Part->SetRelativeLocation(BaseLocation + FVector(0, 0, FMath::Sin(Time * 2.1f) * 3.2f));
            Part->SetRelativeRotation(BaseRotation + FRotator(0, FMath::Sin(Time * 0.65f) * 2.0f, 0));
            break;
        case ResourcePulse:
            Part->SetRelativeScale3D(BaseScale * (1.0f + FMath::Sin(Time * 1.35f) * 0.025f));
            break;
        case ResourceDrift:
            Part->SetRelativeLocation(BaseLocation + FVector(FMath::Cos(Time * 0.8f) * 5.0f,
                FMath::Sin(Time * 0.7f) * 3.0f, (1.0f + FMath::Sin(Time * 1.1f)) * 4.0f));
            Part->SetRelativeScale3D(BaseScale * (0.88f + (1.0f + FMath::Sin(Time)) * 0.08f));
            break;
        default:
            break;
        }
    }
    for (int32 Index = 0; Index < FlagCloths.Num(); ++Index)
    {
        if (!FlagCloths[Index] || !FlagAnchors.IsValidIndex(Index)
            || !FlagBaseYaws.IsValidIndex(Index) || !FlagHalfLengths.IsValidIndex(Index)
            || !FlagAnimationPhases.IsValidIndex(Index)
            || FlagCloths[Index]->bHiddenInGame) continue;
        const float Sway = FMath::Sin(
            ResourceAnimationClock * 1.35f + FlagAnimationPhases[Index]) * 11.0f;
        const float Yaw = FlagBaseYaws[Index] + Sway;
        const FRotator Rotation(0, Yaw, 0);
        FlagCloths[Index]->SetRelativeRotation(Rotation);
        FlagCloths[Index]->SetRelativeLocation(FlagAnchors[Index]
            + Rotation.RotateVector(FVector(FlagHalfLengths[Index], 0, 0)));
    }
    if (DiceAnimationRemaining > 0.0f)
    {
        DiceAnimationRemaining = FMath::Max(0.0f, DiceAnimationRemaining - DeltaSeconds);
        const float Alpha = DiceAnimationRemaining / DiceAnimationDuration;
        for (int32 Index = 0; Index < DicePieces.Num(); ++Index)
        {
            const float Bounce = FMath::Abs(FMath::Sin((1.0f - Alpha) * PI * 3.0f)) * 120.0f * Alpha;
            DicePieces[Index]->SetRelativeLocation(FVector(-95.0f + Index * 190.0f, DiceTableY, 70.0f + Bounce));
            if (DiceAnimationRemaining > DiceSettleDuration)
            {
                const float Direction = Index == 0 ? 1.0f : -1.0f;
                DicePieces[Index]->AddRelativeRotation(FRotator(
                    610.0f * DeltaSeconds * Direction,
                    790.0f * DeltaSeconds,
                    530.0f * DeltaSeconds * Direction));
            }
            else if (DiceTargetRotations.IsValidIndex(Index))
            {
                const FQuat Current = DicePieces[Index]->GetRelativeRotation().Quaternion();
                const float SettleAlpha = FMath::Clamp(DeltaSeconds * 13.0f, 0.0f, 1.0f);
                DicePieces[Index]->SetRelativeRotation(FQuat::Slerp(Current, DiceTargetRotations[Index], SettleAlpha));
                if (DiceAnimationRemaining <= 0.0f)
                {
                    DicePieces[Index]->SetRelativeRotation(DiceTargetRotations[Index]);
                    DicePieces[Index]->SetRelativeLocation(FVector(-95.0f + Index * 190.0f, DiceTableY, 70.0f));
                }
            }
        }
    }
    for (int32 Figure = 0; Figure < RobberFigures.Num(); ++Figure)
    {
        if (!RobberFigures[Figure] || !RobberFigureOffsets.IsValidIndex(Figure)) continue;
        RobberFigures[Figure]->SetRelativeLocation(FMath::VInterpTo(
            RobberFigures[Figure]->GetRelativeLocation(), RobberTarget + RobberFigureOffsets[Figure],
            DeltaSeconds, 5.5f));
    }
    if (PieceAnimationRemaining > 0.0f)
    {
        PieceAnimationRemaining = FMath::Max(0.0f, PieceAnimationRemaining - DeltaSeconds);
        for (int32 Index = 0; Index < BuildingBodies.Num() && Index < BuildingBodyTargets.Num(); ++Index)
        {
            if (!BuildingBodies[Index]->bHiddenInGame)
                BuildingBodies[Index]->SetRelativeScale3D(FMath::VInterpTo(BuildingBodies[Index]->GetRelativeScale3D(), BuildingBodyTargets[Index], DeltaSeconds, 12.0f));
            if (!BuildingRoofs[Index]->bHiddenInGame)
                BuildingRoofs[Index]->SetRelativeScale3D(FMath::VInterpTo(BuildingRoofs[Index]->GetRelativeScale3D(), BuildingRoofTargets[Index], DeltaSeconds, 12.0f));
        }
        for (int32 Index = 0; Index < RoadSlots.Num() && Index < RoadScaleTargets.Num(); ++Index)
            RoadSlots[Index]->SetRelativeScale3D(FMath::VInterpTo(RoadSlots[Index]->GetRelativeScale3D(), RoadScaleTargets[Index], DeltaSeconds, 12.0f));
        for (int32 Index = 0; Index < BuildingParts.Num() && Index < BuildingPartScaleTargets.Num(); ++Index)
            if (!BuildingParts[Index]->bHiddenInGame)
                BuildingParts[Index]->SetRelativeScale3D(FMath::VInterpTo(
                    BuildingParts[Index]->GetRelativeScale3D(), BuildingPartScaleTargets[Index], DeltaSeconds, 12.0f));
        for (int32 Index = 0; Index < RoadPavingParts.Num() && Index < RoadPavingScaleTargets.Num(); ++Index)
            if (!RoadPavingParts[Index]->bHiddenInGame)
                RoadPavingParts[Index]->SetRelativeScale3D(FMath::VInterpTo(
                    RoadPavingParts[Index]->GetRelativeScale3D(), RoadPavingScaleTargets[Index], DeltaSeconds, 12.0f));
    }
}

void ACatanBoardActor::RefreshPieces()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    if (!TryBuildBoard()) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const bool bCanLocalPlayerAct = Subsystem->CanLocalPlayerAct(View);
    const bool bPieceGallery = FParse::Param(FCommandLine::Get(), TEXT("CatanPieceGallery"));
    const bool bShoreCheck = FParse::Param(FCommandLine::Get(), TEXT("CatanShoreCheck"));
    if ((View.FirstDie != PreviousFirstDie || View.SecondDie != PreviousSecondDie) && View.FirstDie > 0)
    {
        PreviousFirstDie = View.FirstDie;
        PreviousSecondDie = View.SecondDie;
        DiceAnimationRemaining = DiceAnimationDuration;
        if (DiceTargetRotations.Num() == 2)
        {
            DiceTargetRotations[0] = DieResultRotation(View.FirstDie, 0);
            DiceTargetRotations[1] = DieResultRotation(View.SecondDie, 1);
        }
        PlayFeedbackTone(135.0f, 0.16f, 0.22f);
    }
    if (!PreviousStatus.IsEmpty() && PreviousStatus != View.StatusMessage)
    {
        if (View.StatusMessage.Contains(TEXT("built"), ESearchCase::IgnoreCase)) PlayFeedbackTone(420.0f, 0.18f);
        else if (View.StatusMessage.Contains(TEXT("trade"), ESearchCase::IgnoreCase)) PlayFeedbackTone(620.0f, 0.12f);
        else if (View.StatusMessage.Contains(TEXT("Robber"), ESearchCase::IgnoreCase)) PlayFeedbackTone(92.0f, 0.28f, 0.24f);
        else if (View.StatusMessage.Contains(TEXT("card"), ESearchCase::IgnoreCase)) PlayFeedbackTone(760.0f, 0.14f);
    }
    PreviousStatus = View.StatusMessage;
    if (PreviousNodeOwners.Num() != View.Nodes.Num()) PreviousNodeOwners.Init(INDEX_NONE, View.Nodes.Num());
    if (PreviousRoadOwners.Num() != View.Roads.Num()) PreviousRoadOwners.Init(INDEX_NONE, View.Roads.Num());
    BuildingBodyTargets.SetNum(View.Nodes.Num());
    BuildingRoofTargets.SetNum(View.Nodes.Num());
    RoadScaleTargets.SetNum(View.Roads.Num());
    bool bLayoutChanged = RenderedHexResources.Num() != View.Hexes.Num();
    for (int32 Index = 0; !bLayoutChanged && Index < View.Hexes.Num(); ++Index)
        bLayoutChanged = RenderedHexResources[Index] != static_cast<uint8>(View.Hexes[Index].Resource);
    if (bLayoutChanged)
    {
        const TArray<FVector> Centers = HexCenters();
        for (int32 Index = 0; Index < Centers.Num() && Index < View.Hexes.Num(); ++Index)
            CreateHexSection(Index, Centers[Index], View.Hexes[Index].Resource,
                ResourceColor(View.Hexes[Index].Resource));
        BuildShore();
        BuildResourceDecorations();
    }
    for (int32 Index=0; Index<Labels.Num() && Index<View.Hexes.Num(); ++Index)
    {
        const FCatanHexView& Hex = View.Hexes[Index];
        const int32 RolledTotal = View.FirstDie + View.SecondDie;
        if (HexLabelSizeTargets.IsValidIndex(Index))
            HexLabelSizeTargets[Index] = View.FirstDie > 0 && Hex.Dice == RolledTotal ? 96.0f : 48.0f;
        if (HexTokenScaleTargets.IsValidIndex(Index))
            HexTokenScaleTargets[Index] = View.FirstDie > 0 && Hex.Dice == RolledTotal
                ? FVector(1.24f, 1.24f, 0.16f)
                : FVector(0.62f, 0.62f, 0.08f);
        const bool bValidTarget = bCanLocalPlayerAct && View.ValidHexTargets.Contains(Index);
        if (HexSlots.IsValidIndex(Index))
            HexSlots[Index]->SetCollisionEnabled(bValidTarget
                ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        Labels[Index]->SetText(Hex.Dice > 0
            ? FText::AsNumber(Hex.Dice)
            : FText::FromString(TEXT("—")));
        Labels[Index]->SetTextRenderColor(bValidTarget ? FColor(255, 210, 35)
            : ((Hex.Dice == 6 || Hex.Dice == 8) ? FColor(190, 24, 18) : FColor(45, 30, 18)));
        if (Hex.bHasRobber && !RobberFigures.IsEmpty() && RobberPlaceholders.IsValidIndex(Index)
            && RobberPlaceholders[Index])
        {
            RobberTarget = RobberPlaceholders[Index]->GetRelativeLocation();
            if (RobberFigures[0]->GetRelativeLocation().IsNearlyZero())
            {
                for (int32 Figure = 0; Figure < RobberFigures.Num(); ++Figure)
                    if (RobberFigures[Figure] && RobberFigureOffsets.IsValidIndex(Figure))
                        RobberFigures[Figure]->SetRelativeLocation(RobberTarget + RobberFigureOffsets[Figure]);
            }
        }
    }
    for (int32 Index=0; Index<NodeSlots.Num() && Index<View.Nodes.Num(); ++Index)
    {
        const FCatanNodeView& Node = View.Nodes[Index];
        int32 GalleryOwner = INDEX_NONE;
        bool bGalleryCity = false;
        if (bPieceGallery)
        {
            if (Index == 20) GalleryOwner = 0;
            else if (Index == 22) GalleryOwner = 1;
            else if (Index == 31) { GalleryOwner = 2; bGalleryCity = true; }
            else if (Index == 33) { GalleryOwner = 3; bGalleryCity = true; }
        }
        const int32 EffectiveOwner = GalleryOwner != INDEX_NONE ? GalleryOwner : Node.OwnerId;
        const bool bEffectiveCity = GalleryOwner != INDEX_NONE ? bGalleryCity : Node.bIsCity;
        const bool bNewBuilding = PreviousNodeOwners[Index] != EffectiveOwner && EffectiveOwner != INDEX_NONE;
        if (bNewBuilding) PieceAnimationRemaining = 0.45f;
        PreviousNodeOwners[Index] = EffectiveOwner;
        const bool bValidTarget = !bPieceGallery && bCanLocalPlayerAct && View.ValidNodeTargets.Contains(Index);
        FLinearColor Color = EffectiveOwner != INDEX_NONE ? PlayerColor(EffectiveOwner) : FLinearColor(0.08f,0.1f,0.12f);
        if (bValidTarget) Color = FMath::Lerp(Color, FLinearColor(0.05f, 0.95f, 0.85f), 0.72f);
        Cast<UMaterialInstanceDynamic>(NodeSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        NodeSlots[Index]->SetHiddenInGame(!bValidTarget);
        NodeSlots[Index]->SetCollisionEnabled(bValidTarget
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        NodeSlots[Index]->SetRelativeScale3D(bValidTarget ? FVector(0.92f) : FVector(0.13f));
        if (BuildingBodies.IsValidIndex(Index) && BuildingRoofs.IsValidIndex(Index))
        {
            const bool bOccupied = EffectiveOwner != INDEX_NONE;
            BuildingBodies[Index]->SetHiddenInGame(true);
            BuildingRoofs[Index]->SetHiddenInGame(true);
            if (bOccupied)
            {
                const FLinearColor BuildingColor = bValidTarget
                    ? FMath::Lerp(PlayerColor(EffectiveOwner), FLinearColor(0.05f, 0.95f, 0.85f), 0.55f)
                    : PlayerColor(EffectiveOwner);
                Cast<UMaterialInstanceDynamic>(BuildingBodies[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), BuildingColor);
                Cast<UMaterialInstanceDynamic>(BuildingRoofs[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), BuildingColor * 0.62f);
                BuildingBodyTargets[Index] = bEffectiveCity ? FVector(0.42f, 0.34f, 0.48f) : FVector(0.25f, 0.25f, 0.32f);
                BuildingRoofTargets[Index] = bEffectiveCity ? FVector(0.47f, 0.39f, 0.25f) : FVector(0.31f, 0.31f, 0.22f);
                BuildingBodies[Index]->SetRelativeScale3D(bNewBuilding ? BuildingBodyTargets[Index] * 1.35f : BuildingBodyTargets[Index]);
                BuildingRoofs[Index]->SetRelativeScale3D(bNewBuilding ? BuildingRoofTargets[Index] * 1.35f : BuildingRoofTargets[Index]);
            }
        }
    }
    for (int32 Index = 0; Index < BuildingParts.Num(); ++Index)
    {
        if (!BuildingPartNodeIds.IsValidIndex(Index) || !BuildingPartModes.IsValidIndex(Index)
            || !BuildingPartUsesPlayerColor.IsValidIndex(Index)
            || !BuildingPartShades.IsValidIndex(Index) || !BuildingPartScaleTargets.IsValidIndex(Index)) continue;
        const int32 NodeId = BuildingPartNodeIds[Index];
        if (!View.Nodes.IsValidIndex(NodeId)) continue;
        const FCatanNodeView& Node = View.Nodes[NodeId];
        int32 GalleryOwner = INDEX_NONE;
        bool bGalleryCity = false;
        if (bPieceGallery)
        {
            if (NodeId == 20) GalleryOwner = 0;
            else if (NodeId == 22) GalleryOwner = 1;
            else if (NodeId == 31) { GalleryOwner = 2; bGalleryCity = true; }
            else if (NodeId == 33) { GalleryOwner = 3; bGalleryCity = true; }
        }
        const int32 EffectiveOwner = GalleryOwner != INDEX_NONE ? GalleryOwner : Node.OwnerId;
        const bool bEffectiveCity = GalleryOwner != INDEX_NONE ? bGalleryCity : Node.bIsCity;
        const bool bShow = EffectiveOwner != INDEX_NONE
            && (bEffectiveCity ? BuildingPartModes[Index] == 1 : BuildingPartModes[Index] == 0);
        SetVisualActive(BuildingParts[Index], bShow);
        if (bShow && BuildingPartUsesPlayerColor[Index])
        {
            const FLinearColor Color = PlayerColor(EffectiveOwner) * BuildingPartShades[Index];
            if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(BuildingParts[Index]->GetMaterial(0)))
                Material->SetVectorParameterValue(TEXT("Color"), Color);
        }
    }
    for (int32 Index=0; Index<RoadSlots.Num() && Index<View.Roads.Num(); ++Index)
    {
        const FCatanRoadView& Road = View.Roads[Index];
        const int32 GalleryOwner = bShoreCheck && Index == 0 ? 0
            : (bPieceGallery && Index >= 24 && Index < 28 ? Index - 24 : INDEX_NONE);
        const int32 EffectiveOwner = GalleryOwner != INDEX_NONE ? GalleryOwner : Road.OwnerId;
        const bool bNewRoad = PreviousRoadOwners[Index] != EffectiveOwner && EffectiveOwner != INDEX_NONE;
        if (bNewRoad) PieceAnimationRemaining = 0.45f;
        PreviousRoadOwners[Index] = EffectiveOwner;
        const bool bValidTarget = !bPieceGallery && bCanLocalPlayerAct && View.ValidRoadTargets.Contains(Index);
        FLinearColor Color = EffectiveOwner != INDEX_NONE ? PlayerColor(EffectiveOwner) : FLinearColor(0.14f,0.15f,0.16f);
        if (bValidTarget) Color = FLinearColor(0.05f, 0.95f, 0.85f);
        Cast<UMaterialInstanceDynamic>(RoadSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        RoadSlots[Index]->SetHiddenInGame(!bValidTarget);
        RoadSlots[Index]->SetCollisionEnabled(bValidTarget
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        RoadScaleTargets[Index] = bValidTarget ? FVector(1.9f,0.30f,0.11f)
            : (EffectiveOwner != INDEX_NONE ? FVector(1.9f,0.12f,0.09f) : FVector(1.9f,0.07f,0.05f));
        RoadSlots[Index]->SetRelativeScale3D(bNewRoad ? RoadScaleTargets[Index] * 1.35f : RoadScaleTargets[Index]);
    }
    for (int32 Index = 0; Index < RoadPavingParts.Num(); ++Index)
    {
        if (!RoadPavingRoadIds.IsValidIndex(Index) || !RoadPavingPartUsesPlayerColor.IsValidIndex(Index)
            || !RoadPavingScaleTargets.IsValidIndex(Index)) continue;
        const int32 RoadId = RoadPavingRoadIds[Index];
        if (!View.Roads.IsValidIndex(RoadId)) continue;
        const FCatanRoadView& Road = View.Roads[RoadId];
        const int32 GalleryOwner = bShoreCheck && RoadId == 0 ? 0
            : (bPieceGallery && RoadId >= 24 && RoadId < 28 ? RoadId - 24 : INDEX_NONE);
        const int32 EffectiveOwner = GalleryOwner != INDEX_NONE ? GalleryOwner : Road.OwnerId;
        const bool bShow = EffectiveOwner != INDEX_NONE;
        SetVisualActive(RoadPavingParts[Index], bShow);
        if (bShow && RoadPavingPartUsesPlayerColor[Index])
        {
            if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(RoadPavingParts[Index]->GetMaterial(0)))
                Material->SetVectorParameterValue(TEXT("Color"), PlayerColor(EffectiveOwner));
        }
    }
}

void ACatanBoardActor::HandleSlotClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed)
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem || TouchedComponent->ComponentTags.IsEmpty()) return;
    const FString Tag = TouchedComponent->ComponentTags[0].ToString();
    FString Kind, IdText;
    if (!Tag.Split(TEXT(":"), &Kind, &IdText)) return;
    FString Error;
    const FCatanGameView View = Subsystem->GetSnapshot();
    if (!Subsystem->CanLocalPlayerAct(View))
    {
        ShowStatus(FString::Printf(TEXT("Waiting for %s"), *View.CurrentPlayer), FColor::Yellow);
        return;
    }
    bool bSucceeded = false;
    if (Kind == TEXT("Node"))
    {
        bSucceeded = View.BoardAction == ECatanBoardAction::BuildCity
            ? Subsystem->TryBuildCity(FCString::Atoi(*IdText), Error)
            : Subsystem->TryBuildSettlement(FCString::Atoi(*IdText), Error);
    }
    else if (Kind == TEXT("Road"))
    {
        bSucceeded = Subsystem->TryBuildRoad(FCString::Atoi(*IdText), Error);
    }
    else if (Kind == TEXT("Hex"))
    {
        bSucceeded = Subsystem->TryMoveRobber(FCString::Atoi(*IdText), Error);
    }
    if (bSucceeded)
    {
        ShowStatus(Subsystem->GetSnapshot().Step, FColor::Green);
    }
    else
    {
        ShowStatus(Error.IsEmpty() ? TEXT("Unknown board slot") : Error, FColor::Red);
    }
}

void ACatanBoardActor::HandleSlotTouched(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent)
{
    HandleSlotClicked(TouchedComponent, EKeys::LeftMouseButton);
}

void ACatanBoardActor::ShowStatus(const FString& Message, const FColor& Color) const
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, Color, Message);
}
