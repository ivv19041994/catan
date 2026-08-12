#include "CatanBoardActor.h"

#include "CatanGameSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWaveProcedural.h"

#include <cmath>

namespace
{
constexpr float TileRadius = 220.0f;
constexpr float RootThreeOverTwo = 0.86602540378f;

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
}

ACatanBoardActor::ACatanBoardActor()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
    HexMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexMesh"));
    HexMesh->SetupAttachment(SceneRoot);
    HexMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACatanBoardActor::BeginPlay()
{
    Super::BeginPlay();
    BasicMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial_Inst.BasicShapeMaterial_Inst"));
    BuildBoard();
    if (UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>())
    {
        Subsystem->OnGameStateChanged.AddDynamic(this, &ACatanBoardActor::RefreshPieces);
    }
    RefreshPieces();
    UE_LOG(LogTemp, Display, TEXT("Catan board ready. WASD/QE and mouse wheel control the camera."));
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

void ACatanBoardActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
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
    AddDecoration(TEXT("Sea"), Cylinder, FVector(0, 0, -17), FVector(28.0f, 28.0f, 0.18f),
        FLinearColor(0.015f, 0.22f, 0.42f));
    AddDecoration(TEXT("Coast"), Cylinder, FVector(0, 0, -8), FVector(21.5f, 21.5f, 0.12f),
        FLinearColor(0.72f, 0.52f, 0.26f));
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
        Label->SetText(FText::FromString(View.Hexes[Index].Dice > 0
            ? FString::Printf(TEXT("%d\n%d"), Index, View.Hexes[Index].Dice)
            : FString::Printf(TEXT("%d\n—"), Index)));
        Labels.Add(Label);
    }
}

void ACatanBoardActor::CreateHexSection(int32 Index, const FVector& Center, const FLinearColor& Color)
{
    TArray<FVector> Vertices{Center};
    TArray<FVector> Normals{FVector::UpVector};
    TArray<FVector2D> UVs{FVector2D(0.5f, 0.5f)};
    TArray<FLinearColor> Colors{Color};
    TArray<FProcMeshTangent> Tangents{FProcMeshTangent(1, 0, 0)};
    TArray<int32> Triangles;
    for (int32 Corner = 0; Corner < 6; ++Corner)
    {
        const float Angle = FMath::DegreesToRadians(30.0f + Corner * 60.0f);
        Vertices.Add(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TileRadius * 0.96f);
        Normals.Add(FVector::UpVector);
        UVs.Add(FVector2D((FMath::Cos(Angle)+1)*0.5f, (FMath::Sin(Angle)+1)*0.5f));
        Colors.Add(Color);
        Tangents.Add(FProcMeshTangent(1, 0, 0));
        Triangles.Add(0);
        Triangles.Add((Corner + 1) % 6 + 1);
        Triangles.Add(Corner + 1);
    }
    HexMesh->CreateMeshSection_LinearColor(Index, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
    HexMesh->SetMaterial(Index, ColoredMaterial(this, BasicMaterial, Color));
}

void ACatanBoardActor::BuildResourceDecorations()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const FName ResourceTag(TEXT("ResourceDecoration"));
    for (int32 Index = Decorations.Num() - 1; Index >= 0; --Index)
    {
        if (Decorations[Index] && Decorations[Index]->ComponentTags.Contains(ResourceTag))
        {
            Decorations[Index]->DestroyComponent();
            Decorations.RemoveAt(Index);
        }
    }
    TokenSlots.Reset();
    RobberPiece = nullptr;
    RobberTop = nullptr;
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
        auto Offset = [Angle](const FVector& Value) { return FRotator(0, Angle, 0).RotateVector(Value); };
        const auto Add = [this, Index, Center, &Offset, ResourceTag](const TCHAR* Kind, int32 Item, UStaticMesh* Mesh,
            const FVector& Local, const FVector& Scale, const FLinearColor& Color, const FRotator& Rotation = FRotator::ZeroRotator)
        {
            UStaticMeshComponent* Component = AddDecoration(FString::Printf(TEXT("G%dHex%d%s%d"), ResourceGeneration, Index, Kind, Item), Mesh,
                Center + Offset(Local), Scale, Color, Rotation);
            Component->ComponentTags.Add(ResourceTag);
            return Component;
        };

        switch (View.Hexes[Index].Resource)
        {
        case ECatanResource::Wood:
            for (int32 Item = 0; Item < 3; ++Item)
            {
                const FVector Local(-95.0f + Item * 62.0f, Item % 2 ? 82.0f : 65.0f, 22.0f);
                Add(TEXT("Trunk"), Item, Cylinder, Local, FVector(0.10f, 0.10f, 0.30f), FLinearColor(0.24f, 0.09f, 0.025f));
                Add(TEXT("Tree"), Item, Cone, Local + FVector(0, 0, 42), FVector(0.42f, 0.42f, 0.62f), FLinearColor(0.015f, 0.22f, 0.045f));
            }
            break;
        case ECatanResource::Clay:
            for (int32 Item = 0; Item < 3; ++Item)
                Add(TEXT("Brick"), Item, Cube, FVector(-82.0f + Item * 70.0f, 82.0f - Item * 10.0f, 18.0f),
                    FVector(0.42f, 0.25f, 0.16f), FLinearColor(0.68f, 0.12f, 0.035f), FRotator(0, Angle + Item * 18.0f, 0));
            break;
        case ECatanResource::Hay:
            for (int32 Item = 0; Item < 5; ++Item)
                Add(TEXT("Hay"), Item, Cylinder, FVector(-76.0f + Item * 38.0f, 82.0f + FMath::Abs(2 - Item) * 5.0f, 27.0f),
                    FVector(0.045f, 0.045f, 0.45f), FLinearColor(1.0f, 0.78f, 0.08f), FRotator(Item * 4.0f, Angle, Item * 5.0f));
            break;
        case ECatanResource::Sheep:
            for (int32 Item = 0; Item < 2; ++Item)
            {
                const FVector Local(-62.0f + Item * 120.0f, 85.0f, 26.0f);
                Add(TEXT("Sheep"), Item, Sphere, Local, FVector(0.38f, 0.28f, 0.28f), FLinearColor(0.94f, 0.95f, 0.89f));
                Add(TEXT("SheepHead"), Item, Sphere, Local + FVector(30, 0, -2), FVector(0.14f), FLinearColor(0.10f, 0.09f, 0.08f));
            }
            break;
        case ECatanResource::Stone:
            for (int32 Item = 0; Item < 3; ++Item)
                Add(TEXT("Rock"), Item, Sphere, FVector(-72.0f + Item * 68.0f, 83.0f, 22.0f + Item * 5.0f),
                    FVector(0.36f - Item * 0.04f, 0.28f, 0.24f + Item * 0.03f), FLinearColor(0.28f, 0.31f, 0.36f));
            break;
        case ECatanResource::Desert:
            for (int32 Item = 0; Item < 3; ++Item)
                Add(TEXT("Dune"), Item, Sphere, FVector(-78.0f + Item * 75.0f, 82.0f, 12.0f),
                    FVector(0.55f, 0.30f, 0.12f), FLinearColor(0.72f + Item * 0.035f, 0.51f, 0.24f));
            break;
        }

        UStaticMeshComponent* Token = AddDecoration(FString::Printf(TEXT("G%dToken%d"), ResourceGeneration, Index), Cylinder,
            Center + FVector(0, 0, 12), FVector(0.62f, 0.62f, 0.08f), FLinearColor(0.92f, 0.85f, 0.68f));
        Token->ComponentTags.Add(ResourceTag);
        TokenSlots.Add(Token);
    }

    RobberPiece = AddDecoration(FString::Printf(TEXT("G%dRobberBody"), ResourceGeneration), Cylinder, FVector::ZeroVector,
        FVector(0.28f, 0.28f, 0.62f), FLinearColor(0.035f, 0.04f, 0.05f));
    RobberTop = AddDecoration(FString::Printf(TEXT("G%dRobberTop"), ResourceGeneration), Sphere, FVector::ZeroVector,
        FVector(0.24f), FLinearColor(0.035f, 0.04f, 0.05f));
    RobberPiece->ComponentTags.Add(ResourceTag);
    RobberTop->ComponentTags.Add(ResourceTag);
    RenderedHexResources.Reset();
    for (const FCatanHexView& Hex : View.Hexes) RenderedHexResources.Add(static_cast<uint8>(Hex.Resource));
}

void ACatanBoardActor::BuildNodes()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UStaticMesh* Cone = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
    const TArray<FVector> Centers = NodeCenters();
    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        UStaticMeshComponent* Slot = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Node%d"), Index));
        Slot->SetupAttachment(SceneRoot);
        Slot->RegisterComponent();
        Slot->SetStaticMesh(Sphere);
        Slot->SetRelativeLocation(Centers[Index]);
        Slot->SetRelativeScale3D(FVector(0.13f));
        Slot->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Slot->ComponentTags.Add(*FString::Printf(TEXT("Node:%d"), Index));
        Slot->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.08f, 0.1f, 0.12f)));
        Slot->OnClicked.AddDynamic(this, &ACatanBoardActor::HandleSlotClicked);
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
        Slot->SetRelativeLocation(Placements[Index].Position);
        Slot->SetRelativeRotation(FRotator(0, Placements[Index].Angle, 0));
        Slot->SetRelativeScale3D(FVector(1.9f, 0.07f, 0.05f));
        Slot->SetCollisionProfileName(TEXT("BlockAllDynamic"));
        Slot->ComponentTags.Add(*FString::Printf(TEXT("Road:%d"), Index));
        Slot->SetMaterial(0, ColoredMaterial(this, BasicMaterial, FLinearColor(0.14f, 0.15f, 0.16f)));
        Slot->OnClicked.AddDynamic(this, &ACatanBoardActor::HandleSlotClicked);
        RoadSlots.Add(Slot);
    }
}

void ACatanBoardActor::BuildPorts()
{
    struct FPort
    {
        int32 FirstNode;
        int32 SecondNode;
        const TCHAR* Label;
    };
    constexpr FPort Ports[] = {
        {0, 1, TEXT("3:1")}, {3, 4, TEXT("2:1 SHEEP")}, {14, 15, TEXT("3:1")},
        {26, 37, TEXT("3:1")}, {45, 46, TEXT("2:1 CLAY")}, {47, 48, TEXT("3:1")},
        {50, 51, TEXT("2:1 WOOD")}, {28, 38, TEXT("2:1 HAY")}, {7, 17, TEXT("2:1 STONE")}
    };
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    const TArray<FVector> Nodes = NodeCenters();
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Ports); ++Index)
    {
        const FVector Shore = (Nodes[Ports[Index].FirstNode] + Nodes[Ports[Index].SecondNode]) * 0.5f;
        FVector Direction(Shore.X, Shore.Y, 0);
        Direction.Normalize();
        const FVector Position = Shore + Direction * 105.0f + FVector(0, 0, -2);
        const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Direction.Y, Direction.X));
        AddDecoration(FString::Printf(TEXT("PortDock%d"), Index), Cube, Position,
            FVector(0.72f, 0.20f, 0.07f), FLinearColor(0.30f, 0.13f, 0.035f), FRotator(0, Yaw, 0));

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("PortLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        Label->SetRelativeLocation(Position + Direction * 68.0f + FVector(0, 0, 15));
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
    for (int32 Index = 0; Index < 2; ++Index)
    {
        const FVector Position(-95.0f + Index * 190.0f, -1030.0f, 70.0f);
        UStaticMeshComponent* Die = AddDecoration(FString::Printf(TEXT("Die%d"), Index), Cube,
            Position, FVector(0.58f), FLinearColor(0.94f, 0.88f, 0.70f), FRotator(12, Index * 23.0f, 8));
        DicePieces.Add(Die);
        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("DieLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        Label->SetRelativeLocation(Position + FVector(0, 0, 66));
        Label->SetRelativeRotation(FRotator(90, 180, 0));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetWorldSize(48.0f);
        Label->SetTextRenderColor(FColor(55, 35, 22));
        Label->SetText(FText::FromString(TEXT("?")));
        DiceLabels.Add(Label);
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
    if (DiceAnimationRemaining > 0.0f)
    {
        DiceAnimationRemaining = FMath::Max(0.0f, DiceAnimationRemaining - DeltaSeconds);
        const float Alpha = DiceAnimationRemaining / 0.85f;
        for (int32 Index = 0; Index < DicePieces.Num(); ++Index)
        {
            const float Bounce = FMath::Abs(FMath::Sin((1.0f - Alpha) * PI * 3.0f)) * 120.0f * Alpha;
            DicePieces[Index]->SetRelativeLocation(FVector(-95.0f + Index * 190.0f, -1030.0f, 70.0f + Bounce));
            DicePieces[Index]->AddRelativeRotation(FRotator(420.0f * DeltaSeconds, 540.0f * DeltaSeconds, 310.0f * DeltaSeconds));
            DiceLabels[Index]->SetRelativeLocation(DicePieces[Index]->GetRelativeLocation() + FVector(0, 0, 66));
        }
    }
    if (RobberPiece && RobberTop)
    {
        const FVector BodyTarget = RobberTarget + FVector(0, 0, 46);
        RobberPiece->SetRelativeLocation(FMath::VInterpTo(RobberPiece->GetRelativeLocation(), BodyTarget, DeltaSeconds, 5.5f));
        RobberTop->SetRelativeLocation(RobberPiece->GetRelativeLocation() + FVector(0, 0, 36));
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
    }
}

void ACatanBoardActor::RefreshPieces()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    if ((View.FirstDie != PreviousFirstDie || View.SecondDie != PreviousSecondDie) && View.FirstDie > 0)
    {
        PreviousFirstDie = View.FirstDie;
        PreviousSecondDie = View.SecondDie;
        DiceAnimationRemaining = 0.85f;
        if (DiceLabels.Num() == 2)
        {
            DiceLabels[0]->SetText(FText::AsNumber(View.FirstDie));
            DiceLabels[1]->SetText(FText::AsNumber(View.SecondDie));
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
        const bool bValidTarget = View.ValidHexTargets.Contains(Index);
        Labels[Index]->SetText(FText::FromString(Hex.Dice > 0
            ? FString::Printf(TEXT("%d\n%d"), Index, Hex.Dice)
            : FString::Printf(TEXT("%d\n—"), Index)));
        Labels[Index]->SetTextRenderColor(bValidTarget ? FColor(255, 210, 35)
            : ((Hex.Dice == 6 || Hex.Dice == 8) ? FColor(190, 24, 18) : FColor(45, 30, 18)));
        if (Hex.bHasRobber && RobberPiece && RobberTop)
        {
            RobberTarget = HexCenters()[Index] + FVector(72, -68, 0);
            if (RobberPiece->GetRelativeLocation().IsNearlyZero())
            {
                RobberPiece->SetRelativeLocation(RobberTarget + FVector(0, 0, 46));
                RobberTop->SetRelativeLocation(RobberTarget + FVector(0, 0, 82));
            }
        }
    }
    for (int32 Index=0; Index<NodeSlots.Num() && Index<View.Nodes.Num(); ++Index)
    {
        const FCatanNodeView& Node = View.Nodes[Index];
        const bool bNewBuilding = PreviousNodeOwners[Index] != Node.OwnerId && Node.OwnerId != INDEX_NONE;
        if (bNewBuilding) PieceAnimationRemaining = 0.45f;
        PreviousNodeOwners[Index] = Node.OwnerId;
        const bool bValidTarget = View.ValidNodeTargets.Contains(Index);
        FLinearColor Color = Node.OwnerId != INDEX_NONE ? PlayerColor(Node.OwnerId) : FLinearColor(0.08f,0.1f,0.12f);
        if (bValidTarget) Color = FMath::Lerp(Color, FLinearColor(0.05f, 0.95f, 0.85f), 0.72f);
        Cast<UMaterialInstanceDynamic>(NodeSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        NodeSlots[Index]->SetHiddenInGame(Node.OwnerId != INDEX_NONE);
        NodeSlots[Index]->SetRelativeScale3D(bValidTarget ? FVector(0.23f) : FVector(0.13f));
        if (BuildingBodies.IsValidIndex(Index) && BuildingRoofs.IsValidIndex(Index))
        {
            const bool bOccupied = Node.OwnerId != INDEX_NONE;
            BuildingBodies[Index]->SetHiddenInGame(!bOccupied);
            BuildingRoofs[Index]->SetHiddenInGame(!bOccupied);
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
    for (int32 Index=0; Index<RoadSlots.Num() && Index<View.Roads.Num(); ++Index)
    {
        const FCatanRoadView& Road = View.Roads[Index];
        const bool bNewRoad = PreviousRoadOwners[Index] != Road.OwnerId && Road.OwnerId != INDEX_NONE;
        if (bNewRoad) PieceAnimationRemaining = 0.45f;
        PreviousRoadOwners[Index] = Road.OwnerId;
        const bool bValidTarget = View.ValidRoadTargets.Contains(Index);
        FLinearColor Color = Road.OwnerId != INDEX_NONE ? PlayerColor(Road.OwnerId) : FLinearColor(0.14f,0.15f,0.16f);
        if (bValidTarget) Color = FLinearColor(0.05f, 0.95f, 0.85f);
        Cast<UMaterialInstanceDynamic>(RoadSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        RoadScaleTargets[Index] = bValidTarget ? FVector(1.9f,0.15f,0.11f)
            : (Road.OwnerId != INDEX_NONE ? FVector(1.9f,0.12f,0.09f) : FVector(1.9f,0.07f,0.05f));
        RoadSlots[Index]->SetRelativeScale3D(bNewRoad ? RoadScaleTargets[Index] * 1.35f : RoadScaleTargets[Index]);
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

void ACatanBoardActor::ShowStatus(const FString& Message, const FColor& Color) const
{
    UE_LOG(LogTemp, Display, TEXT("%s"), *Message);
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, Color, Message);
}
