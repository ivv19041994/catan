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
    View.Phase = ECatanGamePhase::SetupRoad;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotProbabilityAndPlanTest,
    "Catan.Bot.Strategy.ProbabilityAndPlan", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotProbabilityAndPlanTest::RunTest(const FString&)
{
    const int32 Expected[13] = {0, 0, 1, 2, 3, 4, 5, 0, 5, 4, 3, 2, 1};
    for (int32 Dice = 0; Dice <= 12; ++Dice)
        TestEqual(FString::Printf(TEXT("2d6 weight for %d"), Dice),
            FCatanBotStrategy::DiceWeight(Dice), Expected[Dice]);

    FCatanGameView View;
    FCatanPlayerView Bot = Player(0, TEXT("Bot"));
    View.Players = {Bot};
    View.Nodes.SetNum(1); View.Nodes[0].Id = 0; View.Nodes[0].OwnerId = 0;
    FCatanBotTopology Topology; Topology.NodeHexes = {{0, 1}};
    View.Hexes = {Hex(0, ECatanResource::Wood, 6), Hex(1, ECatanResource::Clay, 8)};
    TestEqual(TEXT("strong wood and clay production selects expansion"),
        FCatanBotStrategy::ChoosePlan(View, Topology, 0), ECatanBotPlan::Expansion);
    Topology.NodeHexes = {{0, 1, 2}};
    View.Hexes = {Hex(0, ECatanResource::Hay, 6), Hex(1, ECatanResource::Stone, 8),
        Hex(2, ECatanResource::Sheep, 5)};
    TestEqual(TEXT("ore wheat sheep production selects cities and army"),
        FCatanBotStrategy::ChoosePlan(View, Topology, 0), ECatanBotPlan::CitiesAndArmy);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBotPurposefulRoadTest,
    "Catan.Bot.Strategy.PurposefulRoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanBotPurposefulRoadTest::RunTest(const FString&)
{
    FCatanGameView View; View.Phase = ECatanGamePhase::CommonPlay;
    FCatanPlayerView Bot = Player(0, TEXT("Bot"));
    FCatanPlayerView Opponent = Player(1, TEXT("Opponent"));
    View.Players = {Bot, Opponent};
    View.Hexes = {Hex(0, ECatanResource::Hay, 6), Hex(1, ECatanResource::Wood, 12)};
    View.Nodes.SetNum(5);
    for (int32 Index = 0; Index < View.Nodes.Num(); ++Index) View.Nodes[Index].Id = Index;
    View.Nodes[0].OwnerId = 0;
    View.Nodes[4].OwnerId = 1;
    View.Roads.SetNum(4);
    for (int32 Index = 0; Index < View.Roads.Num(); ++Index) View.Roads[Index].Id = Index;
    View.Roads[0].OwnerId = 0;
    View.Roads[3].OwnerId = 1;
    FCatanBotTopology Topology;
    Topology.NodeHexes = {{}, {}, {0}, {1}, {}};
    Topology.RoadNodes = {FIntPoint(0, 1), FIntPoint(1, 2),
        FIntPoint(1, 3), FIntPoint(3, 4)};
    TestEqual(TEXT("road follows a concrete high-production settlement route"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {1, 2}, 0), 1);
    TestEqual(TEXT("road with no legal settlement route or award purpose is skipped"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {2}, 0), INDEX_NONE);

    FCatanPlayerView RoadBudget = Bot;
    RoadBudget.Resources.Wood = 1; RoadBudget.Resources.Clay = 1;
    TestFalse(TEXT("ordinary road is not funded from an otherwise empty hand"),
        FCatanBotStrategy::ShouldFundRoad(RoadBudget, ECatanBotPlan::Balanced, false));
    TestTrue(TEXT("an award road bypasses the expansion budget"),
        FCatanBotStrategy::ShouldFundRoad(RoadBudget, ECatanBotPlan::Balanced, true));
    RoadBudget.Resources.Hay = 1; RoadBudget.Resources.Sheep = 1;
    TestTrue(TEXT("expansion plan may invest once half a settlement hand is ready"),
        FCatanBotStrategy::ShouldFundRoad(RoadBudget, ECatanBotPlan::Expansion, false));
    TestFalse(TEXT("balanced plan preserves the same four cards for its primary goal"),
        FCatanBotStrategy::ShouldFundRoad(RoadBudget, ECatanBotPlan::Balanced, false));

    View.Nodes.SetNum(6);
    for (int32 Index = 0; Index < View.Nodes.Num(); ++Index)
    {
        View.Nodes[Index].Id = Index;
        View.Nodes[Index].OwnerId = INDEX_NONE;
    }
    View.Nodes[0].OwnerId = 0;
    Bot.FreeSettlements = 0; View.Players[0] = Bot;
    View.Roads.SetNum(5);
    for (int32 Index = 0; Index < View.Roads.Num(); ++Index)
    {
        View.Roads[Index].Id = Index;
        View.Roads[Index].OwnerId = Index < 4 ? 0 : INDEX_NONE;
    }
    Topology.NodeHexes.SetNum(6);
    Topology.RoadNodes = {FIntPoint(0, 1), FIntPoint(1, 2), FIntPoint(2, 3),
        FIntPoint(3, 4), FIntPoint(4, 5)};
    TestEqual(TEXT("fifth connected road is worthwhile for Longest Road"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {4}, 0), 4);
    TestTrue(TEXT("fifth connected road is classified as tactical"),
        FCatanBotStrategy::IsTacticalRoad(View, Topology, 4, 0));

    Bot.VictoryPoints = 8; Bot.bHasLongestRoad = false; Bot.FreeSettlements = 0;
    Opponent.VictoryPoints = 9; Opponent.bHasLongestRoad = true;
    View.Players = {Bot, Opponent};
    View.Nodes.SetNum(13);
    for (int32 Index = 0; Index < View.Nodes.Num(); ++Index)
    {
        View.Nodes[Index].Id = Index;
        View.Nodes[Index].OwnerId = INDEX_NONE;
    }
    View.Roads.SetNum(11);
    for (int32 Index = 0; Index < View.Roads.Num(); ++Index)
    {
        View.Roads[Index].Id = Index;
        View.Roads[Index].OwnerId = Index < 4 ? 0 : (Index >= 5 && Index <= 9 ? 1 : INDEX_NONE);
    }
    Topology.NodeHexes.SetNum(13);
    Topology.RoadNodes = {FIntPoint(0, 1), FIntPoint(1, 2), FIntPoint(2, 3),
        FIntPoint(3, 4), FIntPoint(4, 5), FIntPoint(6, 7), FIntPoint(7, 8),
        FIntPoint(8, 9), FIntPoint(9, 10), FIntPoint(10, 11), FIntPoint(5, 12)};
    TestEqual(TEXT("tying the incumbent does not steal Longest Road"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {4}, 0), INDEX_NONE);
    View.Roads[4].OwnerId = 0;
    TestEqual(TEXT("strict lead steals Longest Road, wins, and denies a match-point opponent"),
        FCatanBotStrategy::ChooseRoad(View, Topology, {10}, 0), 10);
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
    Self.Knights = 1; Self.bHasLargestArmy = true;
    Threat.VictoryPoints = 7; View.Players = {Self, Threat, Weak};
    TestFalse(TEXT("largest-army holder preserves a knight when no urgent move exists"),
        FCatanBotStrategy::ShouldPlayKnightBeforeRoll(View, Topology, Self.Id));
    View.Hexes[1].bHasRobber = true;
    TestTrue(TEXT("knight frees the bot's own high-probability city"),
        FCatanBotStrategy::ShouldPlayKnightBeforeRoll(View, Topology, Self.Id));
    View.Hexes[1].bHasRobber = false; View.Players[1].VictoryPoints = 8;
    TestTrue(TEXT("knight pressures an opponent close to victory"),
        FCatanBotStrategy::ShouldPlayKnightBeforeRoll(View, Topology, Self.Id));
    View.Players[0].bHasLargestArmy = false; View.Players[1].VictoryPoints = 4;
    TestTrue(TEXT("challenger continues building toward Largest Army"),
        FCatanBotStrategy::ShouldPlayKnightBeforeRoll(View, Topology, Self.Id));
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

    Bot.Resources = {}; Bot.Resources.Hay = 1; Bot.Resources.Sheep = 1; Bot.Resources.Stone = 2;
    TestFalse(TEXT("development card does not consume a hand two cards from a city"),
        FCatanBotStrategy::ShouldBuyDevelopmentCard(
            Bot, View, ECatanBotPlan::CitiesAndArmy));
    Bot.Resources.Wood = 1; Bot.Resources.Clay = 1; Bot.Resources.Stone = 1;
    TestTrue(TEXT("cities-and-army plan buys a card when the city remains distant"),
        FCatanBotStrategy::ShouldBuyDevelopmentCard(
            Bot, View, ECatanBotPlan::CitiesAndArmy));
    TestFalse(TEXT("balanced plan keeps a five-card hand for construction"),
        FCatanBotStrategy::ShouldBuyDevelopmentCard(Bot, View, ECatanBotPlan::Balanced));
    Bot.Resources.Wood = 4;
    TestTrue(TEXT("balanced plan spends only from a safe eight-card hand"),
        FCatanBotStrategy::ShouldBuyDevelopmentCard(Bot, View, ECatanBotPlan::Balanced));

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
    Offerer.VictoryPoints = 4; Offerer.ResourceCards = 4;
    Third.VictoryPoints = 4; Third.ResourceCards = 4;
    View.Players = {Bot, Offerer, Third};
    View.Hexes = {Hex(0, ECatanResource::Clay, 6), Hex(1, ECatanResource::Wood, 6)};
    View.Nodes.SetNum(3);
    for (int32 Index = 0; Index < 3; ++Index) View.Nodes[Index].Id = Index;
    View.Nodes[1].OwnerId = Offerer.Id; View.Nodes[2].OwnerId = Third.Id;
    FCatanBotTopology TradeTopology; TradeTopology.NodeHexes = {{}, {0}, {1}};
    TestEqual(TEXT("trade targets the opponent publicly producing the requested resource"),
        FCatanBotStrategy::ChooseTradeTarget(View, TradeTopology, Bot.Id, Proposal),
        Offerer.Name);
    View.Players[1].VictoryPoints = 9;
    TestEqual(TEXT("trade never helps an opponent already on match point"),
        FCatanBotStrategy::ChooseTradeTarget(View, TradeTopology, Bot.Id, Proposal),
        Third.Name);
    return true;
}

#endif
