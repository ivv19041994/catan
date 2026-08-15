#include "CatanBoardActor.h"

#include "CatanGameSubsystem.h"
#include "CatanHexMeshBuilder.h"
#include "CatanResourceVisualBuilder.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
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
constexpr float DiceScale = 0.58f * 4.0f;
constexpr float DiceSpacing = 340.0f;
constexpr float DiceRestZ = 157.0f;

FVector DiceRestLocation(int32 Index, float HeightOffset = 0.0f)
{
    return FVector((static_cast<float>(Index) - 0.5f) * DiceSpacing,
        DiceTableY, DiceRestZ + HeightOffset);
}
enum : uint8 { ResourceSway = 1, ResourceBob, ResourcePulse, ResourceDrift };

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
}

ACatanBoardActor::ACatanBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
    HexMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexMesh"));
    HexMesh->SetupAttachment(SceneRoot);
    HexMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    EnvironmentMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("EnvironmentMesh"));
    EnvironmentMesh->SetupAttachment(SceneRoot);
    EnvironmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACatanBoardActor::BeginPlay()
{
    Super::BeginPlay();
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
    constexpr float MobileAnimationStep = 1.0f / 30.0f;
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
    Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Component->SetMaterial(0, ColoredMaterial(this, BasicMaterial, Color));
    Decorations.Add(Component);
    return Component;
}

void ACatanBoardActor::BuildEnvironment()
{
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    AddDecoration(TEXT("Sea"), Cylinder, FVector(0, 0, -25), FVector(140.0f, 140.0f, 0.12f),
        FLinearColor(0.015f, 0.22f, 0.42f));

    TArray<FVector> Boundary = BoardBoundary();
    if (Boundary.Num() < 3) return;
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

    TArray<FVector> Vertices{Center + FVector(0, 0, -8)};
    TArray<FVector> Normals{FVector::UpVector};
    TArray<FVector2D> UVs{FVector2D(0.5f, 0.5f)};
    TArray<FLinearColor> Colors{FLinearColor(0.72f, 0.52f, 0.26f)};
    TArray<FProcMeshTangent> Tangents{FProcMeshTangent(1, 0, 0)};
    TArray<int32> Triangles;
    constexpr float ShoreExpansion = 1.105f;
    for (const FVector& Point : Boundary)
    {
        const FVector Expanded = Center + (Point - Center) * ShoreExpansion + FVector(0, 0, -8);
        Vertices.Add(Expanded);
        Normals.Add(FVector::UpVector);
        UVs.Add(FVector2D(0.5f + Expanded.X / 2400.0f, 0.5f + Expanded.Y / 2400.0f));
        Colors.Add(FLinearColor(0.72f, 0.52f, 0.26f));
        Tangents.Add(FProcMeshTangent(1, 0, 0));
    }
    for (int32 Index = 0; Index < Boundary.Num(); ++Index)
    {
        Triangles.Add(0);
        Triangles.Add((Index + 1) % Boundary.Num() + 1);
        Triangles.Add(Index + 1);
    }
    EnvironmentMesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    EnvironmentMesh->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.72f, 0.52f, 0.26f)));
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
        CreateHexSection(Index, Centers[Index], ResourceColor(View.Hexes[Index].Resource));

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("HexLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
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

void ACatanBoardActor::CreateHexSection(int32 Index, const FVector& Center, const FLinearColor& Color)
{
    FCatanHexMeshBuffers Mesh;
    CatanHexMesh::AppendTop(Mesh, Center, TileRadius * 0.96f, Color);
    HexMesh->CreateMeshSection_LinearColor(Index, Mesh.Vertices, Mesh.Triangles, Mesh.Normals,
        Mesh.UVs, Mesh.Colors, Mesh.Tangents, false);
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
    RobberBodies.Reset();
    RobberHeads.Reset();
    RobberBodyOffsets.Reset();
    RobberHeadOffsets.Reset();
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
            Pyramid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

        UStaticMeshComponent* Token = AddDecoration(FString::Printf(TEXT("G%dToken%d"), ResourceGeneration, Index), Cylinder,
            Center + FVector(0, 0, 12), FVector(0.62f, 0.62f, 0.08f), FLinearColor(0.92f, 0.85f, 0.68f));
        Token->ComponentTags.Add(ResourceTag);
        TokenSlots.Add(Token);
        HexTokenScaleTargets.Add(FVector(0.62f, 0.62f, 0.08f));
    }

    const FVector GroupOffsets[] = {FVector(-28, -13, 0), FVector(27, -10, 0), FVector(0, 28, 0)};
    const float HeightFactors[] = {0.82f, 1.0f, 1.18f};
    for (int32 Figure = 0; Figure < 3; ++Figure)
    {
        const float BodyScaleZ = 0.93f * HeightFactors[Figure];
        const float BodyCenterZ = 5.0f + BodyScaleZ * 50.0f;
        const float HeadCenterZ = 5.0f + BodyScaleZ * 100.0f + 17.0f;
        UStaticMeshComponent* Body = AddDecoration(
            FString::Printf(TEXT("G%dRobberBody%d"), ResourceGeneration, Figure), Cylinder, FVector::ZeroVector,
            FVector(0.42f, 0.42f, BodyScaleZ), FLinearColor(0.025f + Figure * 0.012f, 0.03f, 0.04f));
        UStaticMeshComponent* Head = AddDecoration(
            FString::Printf(TEXT("G%dRobberHead%d"), ResourceGeneration, Figure), Sphere, FVector::ZeroVector,
            FVector(0.36f), FLinearColor(0.025f + Figure * 0.012f, 0.03f, 0.04f));
        Body->ComponentTags.Add(ResourceTag);
        Head->ComponentTags.Add(ResourceTag);
        RobberBodies.Add(Body);
        RobberHeads.Add(Head);
        RobberBodyOffsets.Add(GroupOffsets[Figure] + FVector(0, 0, BodyCenterZ));
        RobberHeadOffsets.Add(GroupOffsets[Figure] + FVector(0, 0, HeadCenterZ));
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
    const TArray<FVector> Centers = NodeCenters();
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        const FVector GroundCenter(Centers[Index].X, Centers[Index].Y, 0.0f);
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Node%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Sphere);
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

        UStaticMeshComponent* Body = AddDecoration(FString::Printf(TEXT("BuildingBody%d"), Index), Cube,
            Centers[Index] + FVector(0, 0, 16), FVector(0.25f, 0.25f, 0.32f), FLinearColor::White);
        UStaticMeshComponent* Roof = AddDecoration(FString::Printf(TEXT("BuildingRoof%d"), Index), Cone,
            Centers[Index] + FVector(0, 0, 43), FVector(0.31f, 0.31f, 0.22f), FLinearColor::White,
            FRotator(0, 45.0f, 0));
        Body->SetHiddenInGame(true);
        Roof->SetHiddenInGame(true);
        BuildingBodies.Add(Body);
        BuildingRoofs.Add(Roof);

        const FVector HouseOffsets[] = {
            FVector(-37.5f, -30.0f, 0), FVector(37.5f, -30.0f, 0), FVector(0, 33.0f, 0)
        };
        for (int32 House = 0; House < UE_ARRAY_COUNT(HouseOffsets); ++House)
        {
            UStaticMeshComponent* HouseBody = AddDecoration(
                FString::Printf(TEXT("Settlement%dHouse%d"), Index, House), Cube,
                GroundCenter + HouseOffsets[House] + FVector(0, 0, 16.5f),
                FVector(0.285f, 0.24f, 0.33f), FLinearColor::White);
            UStaticMeshComponent* HouseRoof = AddDecoration(
                FString::Printf(TEXT("Settlement%dRoof%d"), Index, House), Cone,
                GroundCenter + HouseOffsets[House] + FVector(0, 0, 43.0f),
                FVector(0.345f, 0.30f, 0.225f), FLinearColor::White, FRotator(0, 45, 0));
            for (UStaticMeshComponent* Part : {HouseBody, HouseRoof})
            {
                Part->SetHiddenInGame(true);
                BuildingParts.Add(Part);
                BuildingPartNodeIds.Add(Index);
                BuildingPartModes.Add(0);
                BuildingPartShades.Add(Part == HouseRoof ? 0.62f : 1.0f);
                BuildingPartScaleTargets.Add(Part->GetRelativeScale3D());
            }
        }

        const FVector TowerOffsets[] = {
            FVector(-30, -30, 0), FVector(30, -30, 0), FVector(-30, 30, 0),
            FVector(30, 30, 0), FVector(0, 0, 0)
        };
        for (int32 Tower = 0; Tower < UE_ARRAY_COUNT(TowerOffsets); ++Tower)
        {
            const bool bCenterTower = Tower == 4;
            UStaticMeshComponent* TowerBody = AddDecoration(
                FString::Printf(TEXT("City%dTower%d"), Index, Tower), Cylinder,
                GroundCenter + TowerOffsets[Tower] + FVector(0, 0, bCenterTower ? 27 : 21),
                bCenterTower ? FVector(0.24f, 0.24f, 0.50f) : FVector(0.18f, 0.18f, 0.39f), FLinearColor::White);
            UStaticMeshComponent* TowerTop = AddDecoration(
                FString::Printf(TEXT("City%dTowerTop%d"), Index, Tower), Cone,
                GroundCenter + TowerOffsets[Tower] + FVector(0, 0, bCenterTower ? 69 : 54),
                bCenterTower ? FVector(0.28f, 0.28f, 0.25f) : FVector(0.22f, 0.22f, 0.20f),
                FLinearColor::White);
            for (UStaticMeshComponent* Part : {TowerBody, TowerTop})
            {
                Part->SetHiddenInGame(true);
                BuildingParts.Add(Part);
                BuildingPartNodeIds.Add(Index);
                BuildingPartModes.Add(1);
                BuildingPartShades.Add(Part == TowerTop ? 0.58f : (bCenterTower ? 0.86f : 1.0f));
                BuildingPartScaleTargets.Add(Part->GetRelativeScale3D());
            }
        }
        struct FWallSpec { FVector Offset; FVector Scale; };
        const FWallSpec Walls[] = {
            {FVector(0, -30, 18), FVector(0.39f, 0.075f, 0.27f)},
            {FVector(0, 30, 18), FVector(0.39f, 0.075f, 0.27f)},
            {FVector(-30, 0, 18), FVector(0.075f, 0.39f, 0.27f)},
            {FVector(30, 0, 18), FVector(0.075f, 0.39f, 0.27f)}
        };
        for (int32 Wall = 0; Wall < UE_ARRAY_COUNT(Walls); ++Wall)
        {
            UStaticMeshComponent* WallPart = AddDecoration(
                FString::Printf(TEXT("City%dWall%d"), Index, Wall), Cube,
                GroundCenter + Walls[Wall].Offset, Walls[Wall].Scale, FLinearColor::White);
            WallPart->SetHiddenInGame(true);
            BuildingParts.Add(WallPart);
            BuildingPartNodeIds.Add(Index);
            BuildingPartModes.Add(1);
            BuildingPartShades.Add(0.78f);
            BuildingPartScaleTargets.Add(WallPart->GetRelativeScale3D());
        }
    }
}

void ACatanBoardActor::BuildRoads()
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const TArray<FRoadPlacement> Placements = RoadCenters();
    for (int32 Index = 0; Index < Placements.Num(); ++Index)
    {
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Road%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Cube);
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

        const FVector RoadDirection = FRotator(0, Placements[Index].Angle, 0).RotateVector(FVector::XAxisVector);
        const FVector AcrossDirection = FRotator(0, Placements[Index].Angle, 0).RotateVector(FVector::YAxisVector);
        constexpr int32 SegmentCount = 6;
        constexpr float SegmentSpacing = 30.0f;
        constexpr float LaneSpacing = 13.0f;
        constexpr float Stagger = SegmentSpacing / 3.0f;
        for (int32 Across = -1; Across <= 1; ++Across)
        {
            for (int32 Segment = 0; Segment < SegmentCount; ++Segment)
            {
                const float Along = (Segment - (SegmentCount - 1) * 0.5f) * SegmentSpacing + Across * Stagger;
                FVector Position = FVector(Placements[Index].Position.X, Placements[Index].Position.Y, 3.0f)
                    + AcrossDirection * (Across * LaneSpacing) + RoadDirection * Along;
                UStaticMeshComponent* Slab = AddDecoration(
                    FString::Printf(TEXT("Road%dLane%dSlab%d"), Index, Across + 1, Segment), Cube, Position,
                    FVector(0.28f, 0.105f, 0.06f), FLinearColor::White,
                    FRotator(0, Placements[Index].Angle, 0));
                Slab->SetHiddenInGame(true);
                RoadPavingParts.Add(Slab);
                RoadPavingRoadIds.Add(Index);
                RoadPavingScaleTargets.Add(Slab->GetRelativeScale3D());
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
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cylinder = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    const TArray<FVector> Nodes = NodeCenters();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ports); ++Index)
    {
        const FVector Shore = (Nodes[Ports[Index].FirstNode] + Nodes[Ports[Index].SecondNode]) * 0.5f;
        FVector Direction(Shore.X, Shore.Y, 0);
        Direction.Normalize();
        const FVector Along(-Direction.Y, Direction.X, 0);
        const FVector BridgeAnchor = Shore + Direction * 175.0f + FVector(0, 0, -14);
        const FVector ShipPosition = BridgeAnchor + Direction * 22.0f + FVector(0, 0, 5.0f);
        const float ShipYaw = FMath::RadiansToDegrees(FMath::Atan2(Along.Y, Along.X));

        auto AddBridge = [this, Cube, Index](int32 BridgeIndex, const FVector& Start, const FVector& End)
        {
            const FVector Delta = End - Start;
            const float Length = Delta.Size();
            const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
            const FVector Midpoint = (Start + End) * 0.5f;
            AddDecoration(FString::Printf(TEXT("Port%dBridge%d"), Index, BridgeIndex), Cube,
                Midpoint, FVector(Length / 100.0f, 0.105f, 0.055f),
                FLinearColor(0.32f, 0.14f, 0.035f), Delta.Rotation());
            for (int32 Plank = -2; Plank <= 2; ++Plank)
                AddDecoration(FString::Printf(TEXT("Port%dBridge%dPlank%d"), Index, BridgeIndex, Plank + 2), Cube,
                    Midpoint + Delta.GetSafeNormal() * (Plank * Length / 5.5f) + FVector(0, 0, 3),
                    FVector(0.035f, 0.14f, 0.025f), FLinearColor(0.47f, 0.24f, 0.07f), FRotator(0, Yaw, 0));
        };
        const FVector ShipEnds[] = {
            BridgeAnchor - Along * 28.0f + FVector(0, 0, 12),
            BridgeAnchor + Along * 28.0f + FVector(0, 0, 12)
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

        AddDecoration(FString::Printf(TEXT("Port%dHull"), Index), Cube, ShipPosition,
            FVector(1.10f, 0.42f, 0.20f), FLinearColor(0.23f, 0.075f, 0.018f), FRotator(0, ShipYaw, 0));
        AddDecoration(FString::Printf(TEXT("Port%dDeck"), Index), Cube, ShipPosition + FVector(0, 0, 13),
            FVector(0.88f, 0.36f, 0.08f), FLinearColor(0.48f, 0.25f, 0.075f), FRotator(0, ShipYaw, 0));
        const FRotator BowRotation = FQuat::FindBetweenNormals(FVector::UpVector, Along).Rotator();
        AddDecoration(FString::Printf(TEXT("Port%dBow"), Index), Cone, ShipPosition + Along * 76.0f + FVector(0, 0, 1),
            FVector(0.38f, 0.38f, 0.44f), FLinearColor(0.30f, 0.10f, 0.025f), BowRotation);
        AddDecoration(FString::Printf(TEXT("Port%dMast"), Index), Cylinder, ShipPosition + FVector(0, 0, 52),
            FVector(0.07f, 0.07f, 0.78f), FLinearColor(0.25f, 0.095f, 0.02f));
        AddDecoration(FString::Printf(TEXT("Port%dSail"), Index), Cube,
            ShipPosition + Along * 6.0f + FVector(0, 0, 66), FVector(0.05f, 0.56f, 0.46f),
            FLinearColor(0.88f, 0.80f, 0.61f), FRotator(0, ShipYaw, 0));

        if (!Ports[Index].bGeneric)
        {
            for (int32 CargoSide = -1; CargoSide <= 1; CargoSide += 2)
            {
                const FVector CargoCenter = ShipPosition + Along * (CargoSide * 20.0f) + FVector(0, 0, 22.0f);
                switch (Ports[Index].Cargo)
                {
                case ECatanResource::Wood:
                {
                    const FRotator LogRotation = FQuat::FindBetweenNormals(FVector::UpVector, Along).Rotator();
                    for (int32 Log = 0; Log < 6; ++Log)
                    {
                        const int32 Row = Log < 3 ? 0 : (Log < 5 ? 1 : 2);
                        const int32 Column = Row == 0 ? Log : (Row == 1 ? Log - 3 : 0);
                        const int32 RowCount = 3 - Row;
                        AddDecoration(FString::Printf(TEXT("Port%dWood%d_%d"), Index, CargoSide + 1, Log), Cylinder,
                            CargoCenter + Direction * ((Column - (RowCount - 1) * 0.5f) * 6.5f) + FVector(0, 0, Row * 6.5f),
                            FVector(0.033f, 0.033f, 0.11f), FLinearColor(0.22f, 0.075f, 0.018f), LogRotation);
                    }
                    break;
                }
                case ECatanResource::Clay:
                    for (int32 Brick = 0; Brick < 4; ++Brick)
                    {
                        const FVector BrickCenter = CargoCenter
                            + Along * ((Brick % 2) * 10.0f - 5.0f)
                            + Direction * ((Brick / 2) * 8.0f - 4.0f);
                        for (int32 Sub = 0; Sub < 8; ++Sub)
                            AddDecoration(FString::Printf(TEXT("Port%dClay%d_%d_%d"), Index, CargoSide + 1, Brick, Sub), Cube,
                                BrickCenter + Along * ((Sub & 1) ? 2.8f : -2.8f)
                                    + Direction * ((Sub & 2) ? 2.2f : -2.2f)
                                    + FVector(0, 0, (Sub & 4) ? 0.25f : -3.25f),
                                FVector(0.052f, 0.042f, 0.035f), FLinearColor(0.70f, 0.11f, 0.025f), FRotator(0, ShipYaw, 0));
                    }
                    break;
                case ECatanResource::Hay:
                    for (int32 Item = 0; Item < 6; ++Item)
                    {
                        const int32 Row = Item / 3;
                        const int32 Column = Item % 3;
                        AddDecoration(FString::Printf(TEXT("Port%dHay%d_%d"), Index, CargoSide + 1, Item), Cylinder,
                            CargoCenter + Direction * ((Column - 1) * 5.0f) + Along * ((Row * 2 - 1) * 4.0f),
                            FVector(0.02f, 0.02f, 0.18f), FLinearColor(1.0f, 0.76f, 0.05f));
                    }
                    break;
                case ECatanResource::Sheep:
                    AddDecoration(FString::Printf(TEXT("Port%dSheepBody%d"), Index, CargoSide + 1), Sphere, CargoCenter,
                        FVector(0.18f, 0.13f, 0.13f), FLinearColor(0.94f, 0.95f, 0.89f));
                    AddDecoration(FString::Printf(TEXT("Port%dSheepHead%d"), Index, CargoSide + 1), Sphere,
                        CargoCenter + Direction * 15.0f, FVector(0.075f), FLinearColor(0.10f, 0.09f, 0.08f));
                    break;
                case ECatanResource::Stone:
                    for (int32 Item = 0; Item < 4; ++Item)
                        AddDecoration(FString::Printf(TEXT("Port%dStone%d_%d"), Index, CargoSide + 1, Item), Sphere,
                            CargoCenter + Direction * ((Item % 2) * 12.0f - 6.0f)
                                + Along * ((Item / 2) * 10.0f - 5.0f) + FVector(0, 0, (Item / 2) * 4.0f),
                            FVector(0.09f + Item * 0.008f), FLinearColor(0.31f, 0.35f, 0.41f));
                    break;
                default:
                    break;
                }
            }
        }

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("PortLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        Label->SetRelativeLocation(ShipPosition + Direction * 78.0f + FVector(0, 0, 14));
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
        const FVector Position = DiceRestLocation(Index);
        UStaticMeshComponent* Die = AddDecoration(FString::Printf(TEXT("Die%d"), Index), Cube,
            Position, FVector(DiceScale), FLinearColor(0.94f, 0.88f, 0.70f), FRotator(12, Index * 23.0f, 8));
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
                Pip->SetRelativeLocation(Normal * 51.2f
                    + Horizontal * Offsets[PipIndex].X + Vertical * Offsets[PipIndex].Y);
                FVector PipScale(0.105f);
                if (!FMath::IsNearlyZero(Normal.X)) PipScale.X = 0.025f;
                if (!FMath::IsNearlyZero(Normal.Y)) PipScale.Y = 0.025f;
                if (!FMath::IsNearlyZero(Normal.Z)) PipScale.Z = 0.025f;
                Pip->SetRelativeScale3D(PipScale);
                Pip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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
    if (DiceAnimationRemaining > 0.0f)
    {
        DiceAnimationRemaining = FMath::Max(0.0f, DiceAnimationRemaining - DeltaSeconds);
        const float Alpha = DiceAnimationRemaining / DiceAnimationDuration;
        for (int32 Index = 0; Index < DicePieces.Num(); ++Index)
        {
            const float Bounce = FMath::Abs(FMath::Sin((1.0f - Alpha) * PI * 3.0f)) * 120.0f * Alpha;
            DicePieces[Index]->SetRelativeLocation(DiceRestLocation(Index, Bounce));
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
                    DicePieces[Index]->SetRelativeLocation(DiceRestLocation(Index));
                }
            }
        }
    }
    for (int32 Figure = 0; Figure < RobberBodies.Num() && Figure < RobberHeads.Num(); ++Figure)
    {
        if (!RobberBodyOffsets.IsValidIndex(Figure) || !RobberHeadOffsets.IsValidIndex(Figure)) continue;
        RobberBodies[Figure]->SetRelativeLocation(FMath::VInterpTo(
            RobberBodies[Figure]->GetRelativeLocation(), RobberTarget + RobberBodyOffsets[Figure], DeltaSeconds, 5.5f));
        RobberHeads[Figure]->SetRelativeLocation(FMath::VInterpTo(
            RobberHeads[Figure]->GetRelativeLocation(), RobberTarget + RobberHeadOffsets[Figure], DeltaSeconds, 5.5f));
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
            CreateHexSection(Index, Centers[Index], ResourceColor(View.Hexes[Index].Resource));
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
        if (Hex.bHasRobber && !RobberBodies.IsEmpty() && RobberBodies.Num() == RobberHeads.Num())
        {
            RobberTarget = HexCenters()[Index] + FVector(72, -68, 0);
            if (RobberBodies[0]->GetRelativeLocation().IsNearlyZero())
            {
                for (int32 Figure = 0; Figure < RobberBodies.Num(); ++Figure)
                {
                    RobberBodies[Figure]->SetRelativeLocation(RobberTarget + RobberBodyOffsets[Figure]);
                    RobberHeads[Figure]->SetRelativeLocation(RobberTarget + RobberHeadOffsets[Figure]);
                }
            }
        }
    }
    for (int32 Index=0; Index<NodeSlots.Num() && Index<View.Nodes.Num(); ++Index)
    {
        const FCatanNodeView& Node = View.Nodes[Index];
        const bool bNewBuilding = PreviousNodeOwners[Index] != Node.OwnerId && Node.OwnerId != INDEX_NONE;
        if (bNewBuilding) PieceAnimationRemaining = 0.45f;
        PreviousNodeOwners[Index] = Node.OwnerId;
        const bool bValidTarget = bCanLocalPlayerAct && View.ValidNodeTargets.Contains(Index);
        FLinearColor Color = Node.OwnerId != INDEX_NONE ? PlayerColor(Node.OwnerId) : FLinearColor(0.08f,0.1f,0.12f);
        if (bValidTarget) Color = FMath::Lerp(Color, FLinearColor(0.05f, 0.95f, 0.85f), 0.72f);
        Cast<UMaterialInstanceDynamic>(NodeSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        NodeSlots[Index]->SetHiddenInGame(!bValidTarget);
        NodeSlots[Index]->SetCollisionEnabled(bValidTarget
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        NodeSlots[Index]->SetRelativeScale3D(bValidTarget ? FVector(0.92f) : FVector(0.13f));
        if (BuildingBodies.IsValidIndex(Index) && BuildingRoofs.IsValidIndex(Index))
        {
            const bool bOccupied = Node.OwnerId != INDEX_NONE;
            BuildingBodies[Index]->SetHiddenInGame(true);
            BuildingRoofs[Index]->SetHiddenInGame(true);
            if (bOccupied)
            {
                const FLinearColor BuildingColor = bValidTarget
                    ? FMath::Lerp(PlayerColor(Node.OwnerId), FLinearColor(0.05f, 0.95f, 0.85f), 0.55f)
                    : PlayerColor(Node.OwnerId);
                Cast<UMaterialInstanceDynamic>(BuildingBodies[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), BuildingColor);
                Cast<UMaterialInstanceDynamic>(BuildingRoofs[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), BuildingColor * 0.62f);
                BuildingBodyTargets[Index] = Node.bIsCity ? FVector(0.42f, 0.34f, 0.48f) : FVector(0.25f, 0.25f, 0.32f);
                BuildingRoofTargets[Index] = Node.bIsCity ? FVector(0.47f, 0.39f, 0.25f) : FVector(0.31f, 0.31f, 0.22f);
                BuildingBodies[Index]->SetRelativeScale3D(bNewBuilding ? BuildingBodyTargets[Index] * 1.35f : BuildingBodyTargets[Index]);
                BuildingRoofs[Index]->SetRelativeScale3D(bNewBuilding ? BuildingRoofTargets[Index] * 1.35f : BuildingRoofTargets[Index]);
            }
        }
    }
    for (int32 Index = 0; Index < BuildingParts.Num(); ++Index)
    {
        if (!BuildingPartNodeIds.IsValidIndex(Index) || !BuildingPartModes.IsValidIndex(Index)
            || !BuildingPartShades.IsValidIndex(Index) || !BuildingPartScaleTargets.IsValidIndex(Index)) continue;
        const int32 NodeId = BuildingPartNodeIds[Index];
        if (!View.Nodes.IsValidIndex(NodeId)) continue;
        const FCatanNodeView& Node = View.Nodes[NodeId];
        const bool bShow = Node.OwnerId != INDEX_NONE && (Node.bIsCity ? BuildingPartModes[Index] == 1 : BuildingPartModes[Index] == 0);
        BuildingParts[Index]->SetHiddenInGame(!bShow);
        if (bShow)
        {
            const FLinearColor Color = PlayerColor(Node.OwnerId) * BuildingPartShades[Index];
            Cast<UMaterialInstanceDynamic>(BuildingParts[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        }
    }
    for (int32 Index=0; Index<RoadSlots.Num() && Index<View.Roads.Num(); ++Index)
    {
        const FCatanRoadView& Road = View.Roads[Index];
        const bool bNewRoad = PreviousRoadOwners[Index] != Road.OwnerId && Road.OwnerId != INDEX_NONE;
        if (bNewRoad) PieceAnimationRemaining = 0.45f;
        PreviousRoadOwners[Index] = Road.OwnerId;
        const bool bValidTarget = bCanLocalPlayerAct && View.ValidRoadTargets.Contains(Index);
        FLinearColor Color = Road.OwnerId != INDEX_NONE ? PlayerColor(Road.OwnerId) : FLinearColor(0.14f,0.15f,0.16f);
        if (bValidTarget) Color = FLinearColor(0.05f, 0.95f, 0.85f);
        Cast<UMaterialInstanceDynamic>(RoadSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        RoadSlots[Index]->SetHiddenInGame(!bValidTarget);
        RoadSlots[Index]->SetCollisionEnabled(bValidTarget
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
        RoadScaleTargets[Index] = bValidTarget ? FVector(1.9f,0.30f,0.11f)
            : (Road.OwnerId != INDEX_NONE ? FVector(1.9f,0.12f,0.09f) : FVector(1.9f,0.07f,0.05f));
        RoadSlots[Index]->SetRelativeScale3D(bNewRoad ? RoadScaleTargets[Index] * 1.35f : RoadScaleTargets[Index]);
    }
    for (int32 Index = 0; Index < RoadPavingParts.Num(); ++Index)
    {
        if (!RoadPavingRoadIds.IsValidIndex(Index) || !RoadPavingScaleTargets.IsValidIndex(Index)) continue;
        const int32 RoadId = RoadPavingRoadIds[Index];
        if (!View.Roads.IsValidIndex(RoadId)) continue;
        const FCatanRoadView& Road = View.Roads[RoadId];
        const bool bShow = Road.OwnerId != INDEX_NONE;
        RoadPavingParts[Index]->SetHiddenInGame(!bShow);
        if (bShow)
        {
            const float Shade = 0.78f + (Index % 3) * 0.10f;
            Cast<UMaterialInstanceDynamic>(RoadPavingParts[Index]->GetMaterial(0))->SetVectorParameterValue(
                TEXT("Color"), PlayerColor(Road.OwnerId) * Shade);
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
