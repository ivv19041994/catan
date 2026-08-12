#include "CatanBoardActor.h"

#include "CatanGameSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

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
    return Material;
}
}

ACatanBoardActor::ACatanBoardActor()
{
    PrimaryActorTick.bCanEverTick = false;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;
    HexMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HexMesh"));
    HexMesh->SetupAttachment(SceneRoot);
    HexMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ACatanBoardActor::BeginPlay()
{
    Super::BeginPlay();
    BasicMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    BuildBoard();
    RefreshPieces();
    ShowStatus(TEXT("Catan: click a node, then an adjacent road. WASD/QE + mouse wheel control the camera."), FColor::Cyan);
}

void ACatanBoardActor::BuildBoard()
{
    BuildHexes();
    BuildNodes();
    BuildRoads();
}

void ACatanBoardActor::BuildHexes()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    const TArray<FVector> Centers = HexCenters();

    for (int32 Index = 0; Index < Centers.Num(); ++Index)
    {
        TArray<FVector> Vertices{Centers[Index]};
        TArray<FVector> Normals{FVector::UpVector};
        TArray<FVector2D> UVs{FVector2D(0.5f, 0.5f)};
        TArray<FLinearColor> Colors{FLinearColor::White};
        TArray<FProcMeshTangent> Tangents{FProcMeshTangent(1, 0, 0)};
        TArray<int32> Triangles;
        for (int32 Corner = 0; Corner < 6; ++Corner)
        {
            const float Angle = FMath::DegreesToRadians(30.0f + Corner * 60.0f);
            Vertices.Add(Centers[Index] + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * TileRadius * 0.96f);
            Normals.Add(FVector::UpVector);
            UVs.Add(FVector2D((FMath::Cos(Angle)+1)*0.5f, (FMath::Sin(Angle)+1)*0.5f));
            Colors.Add(FLinearColor::White);
            Tangents.Add(FProcMeshTangent(1, 0, 0));
            Triangles.Add(0);
            Triangles.Add(Corner + 1);
            Triangles.Add((Corner + 1) % 6 + 1);
        }
        HexMesh->CreateMeshSection_LinearColor(Index, Vertices, Triangles, Normals, UVs, Colors, Tangents, false);
        HexMesh->SetMaterial(Index, ColoredMaterial(this, BasicMaterial, ResourceColor(View.Hexes[Index].Resource)));

        UTextRenderComponent* Label = NewObject<UTextRenderComponent>(this, *FString::Printf(TEXT("HexLabel%d"), Index));
        Label->SetupAttachment(SceneRoot);
        Label->RegisterComponent();
        Label->SetRelativeLocation(Centers[Index] + FVector(0, 0, 12));
        Label->SetRelativeRotation(FRotator(90, 0, 0));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetVerticalAlignment(EVRTA_TextCenter);
        Label->SetWorldSize(58.0f);
        Label->SetText(FText::FromString(View.Hexes[Index].bHasRobber
            ? FString::Printf(TEXT("%d\nB"), Index)
            : FString::Printf(TEXT("%d\n%d"), Index, View.Hexes[Index].Dice)));
        Labels.Add(Label);
    }
}

void ACatanBoardActor::BuildNodes()
{
    UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
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

void ACatanBoardActor::RefreshPieces()
{
    UCatanGameSubsystem* Subsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!Subsystem) return;
    const FCatanGameView View = Subsystem->GetSnapshot();
    for (int32 Index=0; Index<NodeSlots.Num(); ++Index)
    {
        const FCatanNodeView& Node = View.Nodes[Index];
        const FLinearColor Color = Node.OwnerId != INDEX_NONE ? PlayerColor(Node.OwnerId) : FLinearColor(0.08f,0.1f,0.12f);
        Cast<UMaterialInstanceDynamic>(NodeSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        NodeSlots[Index]->SetRelativeScale3D(Node.OwnerId != INDEX_NONE ? FVector(0.19f) : FVector(0.13f));
    }
    for (int32 Index=0; Index<RoadSlots.Num(); ++Index)
    {
        const FCatanRoadView& Road = View.Roads[Index];
        const FLinearColor Color = Road.OwnerId != INDEX_NONE ? PlayerColor(Road.OwnerId) : FLinearColor(0.14f,0.15f,0.16f);
        Cast<UMaterialInstanceDynamic>(RoadSlots[Index]->GetMaterial(0))->SetVectorParameterValue(TEXT("Color"), Color);
        RoadSlots[Index]->SetRelativeScale3D(Road.OwnerId != INDEX_NONE ? FVector(1.9f,0.12f,0.09f) : FVector(1.9f,0.07f,0.05f));
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
    const bool bSucceeded = Kind == TEXT("Node")
        ? Subsystem->TryBuildSettlement(FCString::Atoi(*IdText), Error)
        : Kind == TEXT("Road") && Subsystem->TryBuildRoad(FCString::Atoi(*IdText), Error);
    if (bSucceeded)
    {
        RefreshPieces();
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
