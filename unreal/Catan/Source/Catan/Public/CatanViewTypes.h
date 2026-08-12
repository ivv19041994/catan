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
    UPROPERTY(BlueprintReadOnly) TArray<FCatanHexView> Hexes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanNodeView> Nodes;
    UPROPERTY(BlueprintReadOnly) TArray<FCatanRoadView> Roads;
};
