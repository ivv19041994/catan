#if WITH_DEV_AUTOMATION_TESTS

#include "CatanBotStrategy.h"
#include "Misc/AutomationTest.h"

namespace
{
FCatanHexView Hex(int32 Id, ECatanResource Resource, int32 Dice)
{
    FCatanHexView Result; Result.Id = Id; Result.Resource = Resource; Result.Dice = Dice; return Result;
}

FCatanPlayerView Player(int32 Id, const TCHAR* Name)
{
    FCatanPlayerView Result; Result.Id = Id; Result.Name = Name;
    Result.FreeSettlements = 5; Result.FreeCities = 4; Result.FreeRoads = 15;
    Result.TradeRates.Wood = Result.TradeRates.Clay = Result.TradeRates.Hay
        = Result.TradeRates.Sheep = Result.TradeRates.Stone = 4;
    return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotPlacementTest,
    "Catan.Bot.Strategy.Placement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotPlacementTest::RunTest(const FString&)
{
    FCatanGameView View;
    View.Hexes = {Hex(0, ECatanResource::Wood, 6), Hex(1, ECatanResource::Clay, 3)};
    View.Nodes.SetNum(4); for (int32 Index = 0; Index < View.Nodes.Num(); ++Index) View.Nodes[Index].Id = Index;
    View.Roads.SetNum(2); for (int32 Index = 0; Index < View.Roads.Num(); ++Index) View.Roads[Index].Id = Index;
    FCatanBotTopology Topology;
    Topology.NodeHexes = {{0}, {1}, {0}, {1}};
    Topology.RoadNodes = {FIntPoint(2, 3), FIntPoint(1, 3)};
    TestEqual(TEXT("settlement prefers the substantially stronger production number"),
        FCatanBotStrategy::ChooseSettlement(View, Topology, {0, 1}, 0), 0);
    TestEqual(TEXT("road aims toward the stronger future settlement"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {0, 1}, 0), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotRobberTest,
    "Catan.Bot.Strategy.Robber", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotRobberTest::RunTest(const FString&)
{
    FCatanGameView View;
    View.Hexes = {Hex(0, ECatanResource::Stone, 6), Hex(1, ECatanResource::Hay, 6)};
    View.Nodes.SetNum(2); View.Nodes[0].Id = 0; View.Nodes[0].OwnerId = 1; View.Nodes[0].bIsCity = true;
    View.Nodes[1].Id = 1; View.Nodes[1].OwnerId = 0; View.Nodes[1].bIsCity = true;
    FCatanPlayerView Self = Player(0, TEXT("Bot")); Self.VictoryPoints = 5;
    FCatanPlayerView Threat = Player(1, TEXT("Threat")); Threat.VictoryPoints = 8; Threat.ResourceCards = 7;
    FCatanPlayerView Weak = Player(2, TEXT("Weak")); Weak.VictoryPoints = 3; Weak.ResourceCards = 2;
    View.Players = {Self, Threat, Weak}; View.RobberVictims = {TEXT("Weak"), TEXT("Threat")};
    FCatanBotTopology Topology; Topology.NodeHexes = {{0}, {1}};
    TestEqual(TEXT("robber blocks an opponent rather than the bot's own city"),
        FCatanBotStrategy::ChooseRobberHex(View, Topology, {0, 1}, 0), 0);
    TestEqual(TEXT("robber steals from the leading opponent"),
        FCatanBotStrategy::ChooseRobberVictim(View), FString(TEXT("Threat")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotResourcePlanningTest,
    "Catan.Bot.Strategy.ResourcePlanning", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotResourcePlanningTest::RunTest(const FString&)
{
    FCatanGameView View; View.bHasCityTarget = true;
    FCatanPlayerView Bot = Player(0, TEXT("Bot"));
    Bot.Resources.Wood = 5; Bot.Resources.Clay = 1; Bot.Resources.Hay = 2;
    Bot.Resources.Sheep = 1; Bot.Resources.Stone = 3;
    const FCatanResourceView Drop = FCatanBotStrategy::ChooseDiscard(Bot, 3, View);
    TestEqual(TEXT("discard preserves hay required by an immediately available city"), Drop.Hay, 0);
    TestEqual(TEXT("discard preserves ore required by an immediately available city"), Drop.Stone, 0);
    TestEqual(TEXT("discard removes expendable wood first"), Drop.Wood, 3);

    Bot.Resources = {}; Bot.Resources.Hay = 1; Bot.Resources.Stone = 2;
    const auto Plenty = FCatanBotStrategy::ChooseYearOfPlenty(Bot, View);
    TestTrue(TEXT("Year of Plenty completes the city with hay and ore"),
        (Plenty.Key == ECatanResource::Hay && Plenty.Value == ECatanResource::Stone)
        || (Plenty.Key == ECatanResource::Stone && Plenty.Value == ECatanResource::Hay));

    View.bHasCityTarget = false; View.bHasSettlementTarget = true;
    Bot.Resources = {}; Bot.Resources.Wood = 5; Bot.Resources.Hay = 1; Bot.Resources.Sheep = 1;
    const FCatanBotBankTrade Trade = FCatanBotStrategy::ChooseBankTrade(Bot, View);
    TestTrue(TEXT("bank trade uses a surplus to complete the nearest build"), Trade.bValid);
    TestEqual(TEXT("bank trade sells wood surplus"), Trade.From, ECatanResource::Wood);
    TestEqual(TEXT("bank trade buys missing clay"), Trade.To, ECatanResource::Clay);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotCardsAndTradeTest,
    "Catan.Bot.Strategy.CardsAndTrade", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotCardsAndTradeTest::RunTest(const FString&)
{
    FCatanGameView View; View.bHasSettlementTarget = true;
    FCatanPlayerView Bot = Player(0, TEXT("Bot")); Bot.Resources.Wood = 2;
    Bot.Resources.Hay = 1; Bot.Resources.Sheep = 1;
    FCatanPlayerView Offerer = Player(1, TEXT("Player"));
    FCatanPlayerView Third = Player(2, TEXT("Third"));
    Offerer.Resources.Stone = 4; Third.Resources.Stone = 2; Third.Resources.Clay = 3;
    View.Players = {Bot, Offerer, Third};
    TestEqual(TEXT("Monopoly targets the largest weighted opponent holding"),
        FCatanBotStrategy::ChooseMonopoly(View, Bot.Id), ECatanResource::Stone);
    TestEqual(TEXT("Monopoly gain counts all opponents"),
        FCatanBotStrategy::MonopolyGain(View, Bot.Id, ECatanResource::Stone), 6);

    FCatanResourceView Offered; Offered.Clay = 1;
    FCatanResourceView Requested; Requested.Wood = 1;
    TestTrue(TEXT("bot accepts a trade that completes its settlement from surplus"),
        FCatanBotStrategy::ShouldAcceptTrade(Bot, &Offerer, Offered, Requested, View));
    Offered = {}; Offered.Sheep = 1; Requested = {}; Requested.Wood = 3;
    TestFalse(TEXT("bot rejects a trade it cannot pay"),
        FCatanBotStrategy::ShouldAcceptTrade(Bot, &Offerer, Offered, Requested, View));
    Offerer.VictoryPoints = 9; Offered = {}; Offered.Sheep = 1; Requested = {}; Requested.Hay = 1;
    TestFalse(TEXT("bot does not make a marginal trade with an opponent on match point"),
        FCatanBotStrategy::ShouldAcceptTrade(Bot, &Offerer, Offered, Requested, View));

    Bot.Resources = {}; Bot.Resources.Wood = 2; Bot.Resources.Hay = 1; Bot.Resources.Sheep = 1;
    const FCatanBotPlayerTrade Proposal = FCatanBotStrategy::ChoosePlayerTrade(Bot, View);
    TestTrue(TEXT("bot proposes a player trade when a surplus can complete its goal"), Proposal.bValid);
    TestEqual(TEXT("proposal offers the surplus resource"), Proposal.Offered.Wood, 1);
    TestEqual(TEXT("proposal requests the missing resource"), Proposal.Requested.Clay, 1);
    return true;
}

#endif
