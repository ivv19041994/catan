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

UENUM(BlueprintType)
enum class ECatanDevelopmentCard : uint8
{
    Knight,
    RoadBuilding,
    YearOfPlenty,
    Monopoly
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
    UPROPERTY(BlueprintReadOnly) bool bIsBot = false;
    UPROPERTY(BlueprintReadOnly) bool bIsLocalPlayer = false;
    UPROPERTY(BlueprintReadOnly) bool bResourcesVisible = false;
    UPROPERTY(BlueprintReadOnly) int32 VictoryPoints = 0;
    UPROPERTY(BlueprintReadOnly) int32 VictoryPointCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 ResourceCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 DevelopmentCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeSettlements = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeCities = 0;
    UPROPERTY(BlueprintReadOnly) int32 FreeRoads = 0;
    UPROPERTY(BlueprintReadOnly) FCatanResourceView Resources;
    UPROPERTY(BlueprintReadOnly) FCatanResourceView TradeRates;
    UPROPERTY(BlueprintReadOnly) int32 Knights = 0;
    UPROPERTY(BlueprintReadOnly) int32 RoadBuildingCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 YearOfPlentyCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 MonopolyCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingDevelopmentCards = 0;
    UPROPERTY(BlueprintReadOnly) bool bHasLargestArmy = false;
    UPROPERTY(BlueprintReadOnly) bool bHasLongestRoad = false;
};

USTRUCT(BlueprintType)
struct FCatanDealView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool bIsActive = false;
    UPROPERTY(BlueprintReadOnly) FString OfferingPlayer;
    UPROPERTY(BlueprintReadOnly) FString TargetPlayer;
    UPROPERTY(BlueprintReadOnly) FCatanResourceView Offered;
    UPROPERTY(BlueprintReadOnly) FCatanResourceView Requested;
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
    UPROPERTY(BlueprintReadOnly) int32 RequiredDiscardCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingRobberHex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) TArray<FString> RobberVictims;
    UPROPERTY(BlueprintReadOnly) FCatanDealView ActiveDeal;
    // Public Core bank state adapted for presentation. The Core module owns
    // all supply rules; this struct only transports the resulting counts.
    UPROPERTY(BlueprintReadOnly) FCatanResourceView BankResources;
    UPROPERTY(BlueprintReadOnly) TArray<int32> ValidNodeTargets;
    UPROPERTY(BlueprintReadOnly) TArray<int32> ValidRoadTargets;
    UPROPERTY(BlueprintReadOnly) TArray<int32> ValidHexTargets;
    UPROPERTY(BlueprintReadOnly) bool bHasSettlementTarget = false;
    UPROPERTY(BlueprintReadOnly) bool bHasCityTarget = false;
    UPROPERTY(BlueprintReadOnly) bool bHasRoadTarget = false;
    UPROPERTY(BlueprintReadOnly) TArray<FString> EventLog;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanPlayerView> Players;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanHexView> Hexes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanNodeView> Nodes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanRoadView> Roads;
};
