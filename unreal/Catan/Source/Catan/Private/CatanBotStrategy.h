#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"

struct FCatanBotTopology
{
    TArray<TArray<int32>> NodeHexes;
    TArray<FIntPoint> RoadNodes;
};

struct FCatanBotBankTrade
{
    bool bValid = false;
    ECatanResource From = ECatanResource::Wood;
    ECatanResource To = ECatanResource::Clay;
};

struct FCatanBotPlayerTrade
{
    bool bValid = false;
    FCatanResourceView Offered;
    FCatanResourceView Requested;
};

enum class ECatanBotPlan : uint8
{
    Balanced,
    Expansion,
    CitiesAndArmy
};

class FCatanBotStrategy final
{
public:
    static int32 DiceWeight(int32 Dice);
    static ECatanBotPlan ChoosePlan(const FCatanGameView& View,
        const FCatanBotTopology& Topology, int32 PlayerId);
    static int32 ChooseSettlement(const FCatanGameView& View, const FCatanBotTopology& Topology,
        const TArray<int32>& Targets, int32 PlayerId);
    static int32 ChooseCity(const FCatanGameView& View, const FCatanBotTopology& Topology,
        const TArray<int32>& Targets, int32 PlayerId);
    static int32 ChooseRoad(const FCatanGameView& View, const FCatanBotTopology& Topology,
        const TArray<int32>& Targets, int32 PlayerId);
    static bool IsTacticalRoad(const FCatanGameView& View, const FCatanBotTopology& Topology,
        int32 RoadId, int32 PlayerId);
    static bool ShouldFundRoad(const FCatanPlayerView& Player, ECatanBotPlan Plan,
        bool bTacticalRoad);
    static bool ShouldBuyDevelopmentCard(const FCatanPlayerView& Player,
        const FCatanGameView& View, ECatanBotPlan Plan);
    static int32 ChooseRobberHex(const FCatanGameView& View, const FCatanBotTopology& Topology,
        const TArray<int32>& Targets, int32 PlayerId);
    static bool ShouldPlayKnightBeforeRoll(const FCatanGameView& View,
        const FCatanBotTopology& Topology, int32 PlayerId);
    static FString ChooseRobberVictim(const FCatanGameView& View);
    static FCatanResourceView ChooseDiscard(const FCatanPlayerView& Player, int32 Count,
        const FCatanGameView& View);
    static TPair<ECatanResource, ECatanResource> ChooseYearOfPlenty(
        const FCatanPlayerView& Player, const FCatanGameView& View);
    static ECatanResource ChooseMonopoly(const FCatanGameView& View, int32 PlayerId);
    static int32 MonopolyGain(const FCatanGameView& View, int32 PlayerId, ECatanResource Resource);
    static FCatanBotBankTrade ChooseBankTrade(const FCatanPlayerView& Player,
        const FCatanGameView& View);
    static FCatanBotPlayerTrade ChoosePlayerTrade(const FCatanPlayerView& Player,
        const FCatanGameView& View);
    static FString ChooseTradeTarget(const FCatanGameView& View,
        const FCatanBotTopology& Topology, int32 PlayerId,
        const FCatanBotPlayerTrade& Trade);
    static bool ShouldAcceptTrade(const FCatanPlayerView& Recipient,
        const FCatanPlayerView* Offerer, const FCatanResourceView& Offered,
        const FCatanResourceView& Requested, const FCatanGameView& View);
};
