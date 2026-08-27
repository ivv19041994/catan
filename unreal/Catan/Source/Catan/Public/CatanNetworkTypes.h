#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"

#include "CatanNetworkTypes.generated.h"

UENUM(BlueprintType)
enum class ECatanNetworkMode : uint8
{
    MainMenu,
    Lobby,
    Playing
};

UENUM()
enum class ECatanServerCommand : uint8
{
    BuildSettlement, BuildRoad, BuildCity, MoveRobber, ChooseRobberVictim,
    DropResources, RollDice, BuyDevelopmentCard, Pass, UseDevelopmentCard,
    BankTrade, OfferTrade, AcceptTrade, CancelTrade, SelectBoardAction
};

USTRUCT(BlueprintType)
struct FCatanDiscoveredLobby
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString Host;
    UPROPERTY(BlueprintReadOnly) FString Address;
    UPROPERTY(BlueprintReadOnly) int32 PingMs = 0;
    UPROPERTY(BlueprintReadOnly) int32 Players = 0;
    UPROPERTY(BlueprintReadOnly) int32 Capacity = 0;
};

USTRUCT(BlueprintType)
struct FCatanLobbyPlayerView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) int32 PlayerId = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) bool bReady = false;
    UPROPERTY(BlueprintReadOnly) bool bHost = false;
    UPROPERTY(BlueprintReadOnly) bool bConnected = true;
};

USTRUCT(BlueprintType)
struct FCatanPrivatePlayerView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FCatanResourceView Resources;
    UPROPERTY(BlueprintReadOnly) int32 VictoryPoints = 0;
    UPROPERTY(BlueprintReadOnly) int32 VictoryPointCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 DevelopmentCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 Knights = 0;
    UPROPERTY(BlueprintReadOnly) int32 RoadBuildingCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 YearOfPlentyCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 MonopolyCards = 0;
    UPROPERTY(BlueprintReadOnly) int32 PendingDevelopmentCards = 0;
};
