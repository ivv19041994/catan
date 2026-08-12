#pragma once

#include "CoreMinimal.h"

#include "CatanViewTypes.generated.h"

UENUM(BlueprintType)
enum class ECatanResource : uint8
{
    Wood,
    Clay,
    Hay,
    Sheep,
    Stone,
    Desert
};

UENUM(BlueprintType)
enum class ECatanGamePhase : uint8
{
    SetupSettlement,
    SetupRoad,
    RollDice,
    CommonPlay,
    DropCards,
    MoveRobber,
    RoadBuilding,
    Finished
};

UENUM(BlueprintType)
enum class ECatanBoardAction : uint8
{
    Automatic,
    BuildSettlement,
    BuildRoad,
    BuildCity,
    MoveRobber
};

USTRUCT(BlueprintType)
struct FCatanResourceView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Wood = 0;
    UPROPERTY(BlueprintReadOnly) int32 Clay = 0;
    UPROPERTY(BlueprintReadOnly) int32 Hay = 0;
    UPROPERTY(BlueprintReadOnly) int32 Sheep = 0;
    UPROPERTY(BlueprintReadOnly) int32 Stone = 0;
};

USTRUCT(BlueprintType)
struct FCatanPlayerView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) bool bIsCurrent = false;
    UPROPERTY(BlueprintReadOnly) int32 VictoryPoints = 0;
    UPROPERTY(BlueprintReadOnly) int32 DevelopmentCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeSettlements = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeCities = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeRoads = 0;
    UPROPERTY(BlueprintReadOnly) FCatanResourceView Resources;
};

USTRUCT(BlueprintType)
struct FCatanHexView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) ECatanResource Resource = ECatanResource::Desert;
    UPROPERTY(BlueprintReadOnly) int32 Dice = 0;
    UPROPERTY(BlueprintReadOnly) bool bHasRobber = false;
};

USTRUCT(BlueprintType)
struct FCatanNodeView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int32 OwnerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) bool bIsCity = false;
};

USTRUCT(BlueprintType)
struct FCatanRoadView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 Id = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) int32 OwnerId = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FCatanGameView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString CurrentPlayer;
    UPROPERTY(BlueprintReadOnly) FString Step;
    UPROPERTY(BlueprintReadOnly) ECatanGamePhase Phase = ECatanGamePhase::SetupSettlement;
    UPROPERTY(BlueprintReadOnly) ECatanBoardAction BoardAction = ECatanBoardAction::Automatic;
    UPROPERTY(BlueprintReadOnly) int32 FirstDie = 0;
    UPROPERTY(BlueprintReadOnly) int32 SecondDie = 0;
    UPROPERTY(BlueprintReadOnly) FString Winner;
    UPROPERTY(BlueprintReadOnly) FString StatusMessage;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanPlayerView> Players;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanHexView> Hexes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanNodeView> Nodes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanRoadView> Roads;
};
