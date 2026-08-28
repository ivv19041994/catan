#include "CatanBotStrategy.h"

namespace
{
constexpr int32 ResourceCount = 5;

int32 ResourceIndex(ECatanResource Resource)
{
    return Resource == ECatanResource::Desert ? INDEX_NONE : static_cast<int32>(Resource);
}

ECatanResource ResourceAt(int32 Index)
{
    return static_cast<ECatanResource>(FMath::Clamp(Index, 0, ResourceCount - 1));
}

int32 GetResource(const FCatanResourceView& Resources, int32 Index)
{
    switch (Index)
    {
    case 0: return Resources.Wood;
    case 1: return Resources.Clay;
    case 2: return Resources.Hay;
    case 3: return Resources.Sheep;
    case 4: return Resources.Stone;
    default: return 0;
    }
}

void SetResource(FCatanResourceView& Resources, int32 Index, int32 Value)
{
    switch (Index)
    {
    case 0: Resources.Wood = Value; break;
    case 1: Resources.Clay = Value; break;
    case 2: Resources.Hay = Value; break;
    case 3: Resources.Sheep = Value; break;
    case 4: Resources.Stone = Value; break;
    default: break;
    }
}

FCatanResourceView Cost(int32 Wood, int32 Clay, int32 Hay, int32 Sheep, int32 Stone)
{
    FCatanResourceView Result;
    Result.Wood = Wood; Result.Clay = Clay; Result.Hay = Hay;
    Result.Sheep = Sheep; Result.Stone = Stone;
    return Result;
}

int32 DicePips(int32 Dice)
{
    return FCatanBotStrategy::DiceWeight(Dice);
}

float BaseResourceValue(int32 Index)
{
    static constexpr float Values[ResourceCount] = {1.08f, 1.08f, 1.30f, 0.92f, 1.24f};
    return Values[Index];
}

float MissingCost(const FCatanResourceView& Have, const FCatanResourceView& Goal)
{
    float Result = 0.0f;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
        Result += FMath::Max(0, GetResource(Goal, Index) - GetResource(Have, Index))
            * BaseResourceValue(Index);
    return Result;
}

FCatanResourceView BestGoal(const FCatanPlayerView& Player, const FCatanGameView& View)
{
    FCatanResourceView Best = Cost(0, 0, 1, 1, 1);
    float BestScore = TNumericLimits<float>::Max();
    auto Consider = [&Player, &Best, &BestScore](const FCatanResourceView& Goal, float Bias)
    {
        const float Score = MissingCost(Player.Resources, Goal) - Bias;
        if (Score < BestScore) { BestScore = Score; Best = Goal; }
    };
    if (Player.FreeCities > 0 && View.bHasCityTarget)
        Consider(Cost(0, 0, 2, 0, 3), 2.3f + Player.VictoryPoints * 0.08f);
    if (Player.FreeSettlements > 0 && View.bHasSettlementTarget)
        Consider(Cost(1, 1, 1, 1, 0), 1.25f + Player.VictoryPoints * 0.04f);
    if (Player.FreeRoads > 0 && View.bHasRoadTarget)
        Consider(Cost(1, 1, 0, 0, 0), 0.25f);
    Consider(Cost(0, 0, 1, 1, 1), 0.55f);
    return Best;
}

float ResourceUtility(const FCatanPlayerView& Player, const FCatanGameView& View,
    int32 Resource, int32 CurrentCount)
{
    const FCatanResourceView Goal = BestGoal(Player, View);
    const int32 Need = GetResource(Goal, Resource);
    const float DeficitBonus = CurrentCount < Need ? 2.6f : (CurrentCount == Need ? 0.8f : 0.0f);
    return BaseResourceValue(Resource) + DeficitBonus;
}

int32 BuildingMultiplier(const FCatanNodeView& Node) { return Node.bIsCity ? 2 : 1; }

float PortBonus(int32 NodeId)
{
    static const TSet<int32> Common{0, 1, 14, 15, 26, 37, 47, 48};
    static const TSet<int32> Specialized{3, 4, 7, 17, 28, 38, 45, 46, 50, 51};
    if (Common.Contains(NodeId)) return 1.15f;
    if (Specialized.Contains(NodeId)) return 0.9f;
    return 0.0f;
}

float ScarcityMultiplier(const FCatanGameView& View, int32 Resource)
{
    float Total[ResourceCount]{};
    for (const FCatanHexView& Hex : View.Hexes)
    {
        const int32 Index = ResourceIndex(Hex.Resource);
        if (Index != INDEX_NONE) Total[Index] += DicePips(Hex.Dice);
    }
    float Mean = 0.0f;
    for (float Value : Total) Mean += Value;
    Mean /= ResourceCount;
    if (Mean <= 0.0f) return 1.0f;
    return 1.0f + FMath::Clamp((Mean - Total[Resource]) / Mean, -0.12f, 0.25f);
}

float NodeProductionScore(const FCatanGameView& View, const FCatanBotTopology& Topology,
    int32 NodeId, int32 PlayerId, bool bExpansion)
{
    if (!Topology.NodeHexes.IsValidIndex(NodeId)) return -100000.0f;
    float Score = 0.0f;
    bool Seen[ResourceCount]{};
    bool Existing[ResourceCount]{};
    TSet<int32> ExistingNumbers;
    for (const FCatanNodeView& Node : View.Nodes)
    {
        if (Node.OwnerId != PlayerId || !Topology.NodeHexes.IsValidIndex(Node.Id)) continue;
        for (int32 HexId : Topology.NodeHexes[Node.Id])
            if (View.Hexes.IsValidIndex(HexId))
            {
                const int32 Resource = ResourceIndex(View.Hexes[HexId].Resource);
                if (Resource != INDEX_NONE) Existing[Resource] = true;
                if (View.Hexes[HexId].Dice > 0) ExistingNumbers.Add(View.Hexes[HexId].Dice);
            }
    }
    for (int32 HexId : Topology.NodeHexes[NodeId])
    {
        if (!View.Hexes.IsValidIndex(HexId)) continue;
        const FCatanHexView& Hex = View.Hexes[HexId];
        const int32 Resource = ResourceIndex(Hex.Resource);
        if (Resource == INDEX_NONE) { Score -= 1.5f; continue; }
        const float Pips = static_cast<float>(DicePips(Hex.Dice));
        Score += Pips * BaseResourceValue(Resource) * ScarcityMultiplier(View, Resource) * 1.45f;
        if (Hex.bHasRobber) Score -= Pips * 0.7f;
        if (!Existing[Resource]) Score += 1.1f;
        if (!Seen[Resource]) { Seen[Resource] = true; Score += 0.35f; }
        if (!ExistingNumbers.Contains(Hex.Dice)) Score += 0.2f;
    }
    if (bExpansion)
    {
        int32 OpenRoads = 0;
        for (int32 RoadId = 0; RoadId < Topology.RoadNodes.Num(); ++RoadId)
        {
            const FIntPoint Ends = Topology.RoadNodes[RoadId];
            if ((Ends.X == NodeId || Ends.Y == NodeId) && View.Roads.IsValidIndex(RoadId)
                && View.Roads[RoadId].OwnerId == INDEX_NONE) ++OpenRoads;
        }
        Score += FMath::Min(OpenRoads, 3) * 0.35f;
    }
    Score += PortBonus(NodeId);
    return Score;
}

TArray<int32> IncidentRoads(const FCatanBotTopology& Topology, int32 NodeId)
{
    TArray<int32> Result;
    for (int32 RoadId = 0; RoadId < Topology.RoadNodes.Num(); ++RoadId)
    {
        const FIntPoint Ends = Topology.RoadNodes[RoadId];
        if (Ends.X == NodeId || Ends.Y == NodeId) Result.Add(RoadId);
    }
    return Result;
}

int32 OtherEnd(const FCatanBotTopology& Topology, int32 RoadId, int32 NodeId)
{
    if (!Topology.RoadNodes.IsValidIndex(RoadId)) return INDEX_NONE;
    const FIntPoint Ends = Topology.RoadNodes[RoadId];
    return Ends.X == NodeId ? Ends.Y : Ends.X;
}

bool IsSettlementCandidate(const FCatanGameView& View, const FCatanBotTopology& Topology, int32 NodeId)
{
    if (!View.Nodes.IsValidIndex(NodeId) || View.Nodes[NodeId].OwnerId != INDEX_NONE) return false;
    for (int32 RoadId : IncidentRoads(Topology, NodeId))
    {
        const int32 Neighbor = OtherEnd(Topology, RoadId, NodeId);
        if (View.Nodes.IsValidIndex(Neighbor) && View.Nodes[Neighbor].OwnerId != INDEX_NONE) return false;
    }
    return true;
}

bool ConnectedBeforeCandidate(const FCatanGameView& View, const FCatanBotTopology& Topology,
    int32 NodeId, int32 CandidateRoad, int32 PlayerId)
{
    if (View.Nodes.IsValidIndex(NodeId) && View.Nodes[NodeId].OwnerId == PlayerId) return true;
    for (int32 RoadId : IncidentRoads(Topology, NodeId))
        if (RoadId != CandidateRoad && View.Roads.IsValidIndex(RoadId)
            && View.Roads[RoadId].OwnerId == PlayerId) return true;
    return false;
}

int32 VirtualRoadOwner(const FCatanGameView& View, int32 RoadId,
    int32 CandidateRoad, int32 PlayerId)
{
    if (RoadId == CandidateRoad) return PlayerId;
    return View.Roads.IsValidIndex(RoadId) ? View.Roads[RoadId].OwnerId : INDEX_NONE;
}

int32 LongestFromNode(const FCatanGameView& View, const FCatanBotTopology& Topology,
    int32 NodeId, int32 CandidateRoad, int32 PlayerId, TSet<int32>& Used)
{
    if (View.Nodes.IsValidIndex(NodeId) && View.Nodes[NodeId].OwnerId != INDEX_NONE
        && View.Nodes[NodeId].OwnerId != PlayerId && !Used.IsEmpty()) return 0;
    int32 Best = 0;
    for (int32 RoadId : IncidentRoads(Topology, NodeId))
    {
        if (Used.Contains(RoadId)
            || VirtualRoadOwner(View, RoadId, CandidateRoad, PlayerId) != PlayerId) continue;
        Used.Add(RoadId);
        Best = FMath::Max(Best, 1 + LongestFromNode(View, Topology,
            OtherEnd(Topology, RoadId, NodeId), CandidateRoad, PlayerId, Used));
        Used.Remove(RoadId);
    }
    return Best;
}

int32 LongestRoadLength(const FCatanGameView& View, const FCatanBotTopology& Topology,
    int32 CandidateRoad, int32 PlayerId)
{
    int32 Best = 0;
    for (int32 NodeId = 0; NodeId < View.Nodes.Num(); ++NodeId)
    {
        TSet<int32> Used;
        Best = FMath::Max(Best,
            LongestFromNode(View, Topology, NodeId, CandidateRoad, PlayerId, Used));
    }
    return Best;
}

float BestExpansionFromRoad(const FCatanGameView& View, const FCatanBotTopology& Topology,
    int32 CandidateRoad, int32 PlayerId)
{
    if (!Topology.RoadNodes.IsValidIndex(CandidateRoad)) return -100000.0f;
    const FIntPoint Ends = Topology.RoadNodes[CandidateRoad];
    TArray<int32> Frontier;
    for (int32 NodeId : {Ends.X, Ends.Y})
        if (!ConnectedBeforeCandidate(View, Topology, NodeId, CandidateRoad, PlayerId))
            Frontier.AddUnique(NodeId);
    if (Frontier.IsEmpty()) return -100000.0f;

    struct FVisit { int32 Node; int32 ExtraRoads; };
    TArray<FVisit> Queue;
    TMap<int32, int32> BestDistance;
    for (int32 NodeId : Frontier) { Queue.Add({NodeId, 0}); BestDistance.Add(NodeId, 0); }
    float Best = -100000.0f;
    for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
    {
        const FVisit Current = Queue[Cursor];
        if (IsSettlementCandidate(View, Topology, Current.Node))
        {
            float Utility = NodeProductionScore(View, Topology, Current.Node, PlayerId, true)
                - Current.ExtraRoads * 5.0f;
            bool OpponentApproach = false;
            for (int32 RoadId : IncidentRoads(Topology, Current.Node))
                if (View.Roads.IsValidIndex(RoadId) && View.Roads[RoadId].OwnerId != INDEX_NONE
                    && View.Roads[RoadId].OwnerId != PlayerId) OpponentApproach = true;
            if (OpponentApproach) Utility += 2.0f;
            Best = FMath::Max(Best, Utility);
        }
        if (Current.ExtraRoads >= 2) continue;
        if (View.Nodes.IsValidIndex(Current.Node) && View.Nodes[Current.Node].OwnerId != INDEX_NONE
            && View.Nodes[Current.Node].OwnerId != PlayerId) continue;
        for (int32 RoadId : IncidentRoads(Topology, Current.Node))
        {
            if (RoadId == CandidateRoad || !View.Roads.IsValidIndex(RoadId)) continue;
            const int32 Owner = View.Roads[RoadId].OwnerId;
            if (Owner != INDEX_NONE && Owner != PlayerId) continue;
            const int32 Distance = Current.ExtraRoads + (Owner == PlayerId ? 0 : 1);
            const int32 Next = OtherEnd(Topology, RoadId, Current.Node);
            const int32* Previous = BestDistance.Find(Next);
            if (Distance <= 2 && (!Previous || Distance < *Previous))
            {
                BestDistance.Add(Next, Distance);
                Queue.Add({Next, Distance});
            }
        }
    }
    return Best;
}

FCatanResourceView AddResources(const FCatanResourceView& Left, const FCatanResourceView& Right,
    int32 RightSign = 1)
{
    FCatanResourceView Result;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
        SetResource(Result, Index, GetResource(Left, Index) + RightSign * GetResource(Right, Index));
    return Result;
}

float WeightedTotal(const FCatanPlayerView& Player, const FCatanGameView& View,
    const FCatanResourceView& Resources)
{
    float Result = 0.0f;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
        for (int32 Count = 0; Count < GetResource(Resources, Index); ++Count)
            Result += ResourceUtility(Player, View, Index, GetResource(Player.Resources, Index) + Count);
    return Result;
}
}

int32 FCatanBotStrategy::DiceWeight(int32 Dice)
{
    return Dice >= 2 && Dice <= 12 && Dice != 7 ? 6 - FMath::Abs(7 - Dice) : 0;
}

ECatanBotPlan FCatanBotStrategy::ChoosePlan(const FCatanGameView& View,
    const FCatanBotTopology& Topology, int32 PlayerId)
{
    float Production[ResourceCount]{};
    for (const FCatanNodeView& Node : View.Nodes)
    {
        if (Node.OwnerId != PlayerId || !Topology.NodeHexes.IsValidIndex(Node.Id)) continue;
        const float Multiplier = static_cast<float>(BuildingMultiplier(Node));
        for (int32 HexId : Topology.NodeHexes[Node.Id])
        {
            if (!View.Hexes.IsValidIndex(HexId)) continue;
            const int32 Resource = ResourceIndex(View.Hexes[HexId].Resource);
            if (Resource != INDEX_NONE)
                Production[Resource] += DiceWeight(View.Hexes[HexId].Dice) * Multiplier;
        }
    }
    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [PlayerId](const FCatanPlayerView& Item) { return Item.Id == PlayerId; });
    if (Player && Player->FreeSettlements <= 0) return ECatanBotPlan::CitiesAndArmy;
    const float Expansion = Production[0] + Production[1]
        + Production[2] * 0.35f + Production[3] * 0.35f;
    const float Cities = Production[2] * 1.15f + Production[4] * 1.35f + Production[3] * 0.55f;
    if (Expansion > Cities * 1.15f) return ECatanBotPlan::Expansion;
    if (Cities > Expansion * 1.15f) return ECatanBotPlan::CitiesAndArmy;
    return ECatanBotPlan::Balanced;
}

int32 FCatanBotStrategy::ChooseSettlement(const FCatanGameView& View,
    const FCatanBotTopology& Topology, const TArray<int32>& Targets, int32 PlayerId)
{
    int32 Best = INDEX_NONE; float BestScore = -TNumericLimits<float>::Max();
    for (int32 Target : Targets)
    {
        const float Score = NodeProductionScore(View, Topology, Target, PlayerId, true);
        if (Score > BestScore || (Score == BestScore && Target < Best)) { BestScore = Score; Best = Target; }
    }
    return Best;
}

int32 FCatanBotStrategy::ChooseCity(const FCatanGameView& View, const FCatanBotTopology& Topology,
    const TArray<int32>& Targets, int32 PlayerId)
{
    int32 Best = INDEX_NONE; float BestScore = -TNumericLimits<float>::Max();
    for (int32 Target : Targets)
    {
        const float Score = NodeProductionScore(View, Topology, Target, PlayerId, false);
        if (Score > BestScore || (Score == BestScore && Target < Best)) { BestScore = Score; Best = Target; }
    }
    return Best;
}

int32 FCatanBotStrategy::ChooseRoad(const FCatanGameView& View, const FCatanBotTopology& Topology,
    const TArray<int32>& Targets, int32 PlayerId)
{
    int32 Best = INDEX_NONE; float BestScore = -TNumericLimits<float>::Max();
    const int32 BeforeLongest = LongestRoadLength(View, Topology, INDEX_NONE, PlayerId);
    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [PlayerId](const FCatanPlayerView& Item) { return Item.Id == PlayerId; });
    int32 MaximumOpponentLongest = 0;
    const FCatanPlayerView* OpponentHolder = nullptr;
    for (const FCatanPlayerView& Opponent : View.Players)
    {
        if (Opponent.Id == PlayerId) continue;
        MaximumOpponentLongest = FMath::Max(MaximumOpponentLongest,
            LongestRoadLength(View, Topology, INDEX_NONE, Opponent.Id));
        if (Opponent.bHasLongestRoad) OpponentHolder = &Opponent;
    }
    for (int32 Target : Targets)
    {
        if (!Topology.RoadNodes.IsValidIndex(Target)) continue;
        float Score = Player && Player->FreeSettlements > 0
            ? BestExpansionFromRoad(View, Topology, Target, PlayerId) : -100000.0f;
        const int32 AfterLongest = LongestRoadLength(View, Topology, Target, PlayerId);
        if (AfterLongest > BeforeLongest)
            Score = FMath::Max(Score, 0.0f) + (AfterLongest - BeforeLongest) * 1.1f;
        const bool bClaimsLongestRoad = Player && !Player->bHasLongestRoad
            && AfterLongest >= 5 && AfterLongest > MaximumOpponentLongest;
        if (bClaimsLongestRoad)
        {
            Score = FMath::Max(Score, 0.0f) + 12.0f;
            if (OpponentHolder) Score += 5.0f;
            if (Player->VictoryPoints >= 8) Score += 100.0f;
            if (OpponentHolder && OpponentHolder->VictoryPoints >= 8) Score += 25.0f;
        }
        else if (Player && Player->bHasLongestRoad && AfterLongest > BeforeLongest
            && MaximumOpponentLongest + 1 >= BeforeLongest)
        {
            Score += 5.0f;
        }
        if (Score > BestScore || (Score == BestScore && Target < Best)) { BestScore = Score; Best = Target; }
    }
    const bool bMandatoryPlacement = View.Phase == ECatanGamePhase::SetupRoad
        || View.Phase == ECatanGamePhase::RoadBuilding;
    if (!bMandatoryPlacement && BestScore < 4.0f) return INDEX_NONE;
    return Best;
}

bool FCatanBotStrategy::IsTacticalRoad(const FCatanGameView& View,
    const FCatanBotTopology& Topology, int32 RoadId, int32 PlayerId)
{
    if (!Topology.RoadNodes.IsValidIndex(RoadId)) return false;
    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [PlayerId](const FCatanPlayerView& Item) { return Item.Id == PlayerId; });
    if (!Player) return false;
    const int32 Before = LongestRoadLength(View, Topology, INDEX_NONE, PlayerId);
    const int32 After = LongestRoadLength(View, Topology, RoadId, PlayerId);
    if (After <= Before) return false;
    int32 MaximumOpponent = 0;
    for (const FCatanPlayerView& Opponent : View.Players)
        if (Opponent.Id != PlayerId)
            MaximumOpponent = FMath::Max(MaximumOpponent,
                LongestRoadLength(View, Topology, INDEX_NONE, Opponent.Id));
    const bool bClaimsAward = !Player->bHasLongestRoad && After >= 5 && After > MaximumOpponent;
    const bool bDefendsAward = Player->bHasLongestRoad && MaximumOpponent + 1 >= Before;
    return bClaimsAward || bDefendsAward;
}

bool FCatanBotStrategy::ShouldFundRoad(const FCatanPlayerView& Player, ECatanBotPlan Plan,
    bool bTacticalRoad)
{
    if (bTacticalRoad) return true;
    const FCatanResourceView& Have = Player.Resources;
    const int32 Total = Have.Wood + Have.Clay + Have.Hay + Have.Sheep + Have.Stone;
    return Total >= 7 || (Plan == ECatanBotPlan::Expansion && Total >= 4);
}

bool FCatanBotStrategy::ShouldBuyDevelopmentCard(const FCatanPlayerView& Player,
    const FCatanGameView& View, ECatanBotPlan Plan)
{
    const FCatanResourceView& Have = Player.Resources;
    if (Have.Hay <= 0 || Have.Sheep <= 0 || Have.Stone <= 0) return false;
    const int32 Total = Have.Wood + Have.Clay + Have.Hay + Have.Sheep + Have.Stone;
    const int32 CityDeficit = FMath::Max(0, 2 - Have.Hay) + FMath::Max(0, 3 - Have.Stone);
    const int32 SettlementDeficit = FMath::Max(0, 1 - Have.Wood)
        + FMath::Max(0, 1 - Have.Clay) + FMath::Max(0, 1 - Have.Hay)
        + FMath::Max(0, 1 - Have.Sheep);
    if (View.bHasCityTarget && Player.FreeCities > 0 && CityDeficit <= 2) return false;
    if (View.bHasSettlementTarget && Player.FreeSettlements > 0
        && SettlementDeficit <= 1) return false;
    if (Player.VictoryPoints >= 8) return Total >= 5;
    if (Plan == ECatanBotPlan::CitiesAndArmy) return Total >= 5;
    return Total >= 8;
}

int32 FCatanBotStrategy::ChooseRobberHex(const FCatanGameView& View,
    const FCatanBotTopology& Topology, const TArray<int32>& Targets, int32 PlayerId)
{
    int32 Best = INDEX_NONE; float BestScore = -TNumericLimits<float>::Max();
    for (int32 Target : Targets)
    {
        if (!View.Hexes.IsValidIndex(Target)) continue;
        const float Pips = static_cast<float>(DicePips(View.Hexes[Target].Dice));
        float Score = View.Hexes[Target].Resource == ECatanResource::Desert ? -6.0f : 0.0f;
        for (const FCatanNodeView& Node : View.Nodes)
        {
            if (Node.OwnerId == INDEX_NONE || !Topology.NodeHexes.IsValidIndex(Node.Id)
                || !Topology.NodeHexes[Node.Id].Contains(Target)) continue;
            const FCatanPlayerView* Owner = View.Players.FindByPredicate(
                [&Node](const FCatanPlayerView& Player) { return Player.Id == Node.OwnerId; });
            const float Threat = Owner ? Owner->VictoryPoints * 0.45f + Owner->ResourceCards * 0.08f : 0.0f;
            const float Impact = Pips * BuildingMultiplier(Node) + Threat;
            Score += Node.OwnerId == PlayerId ? -Impact * 2.8f : Impact;
        }
        if (Score > BestScore || (Score == BestScore && Target < Best)) { BestScore = Score; Best = Target; }
    }
    return Best;
}

bool FCatanBotStrategy::ShouldPlayKnightBeforeRoll(const FCatanGameView& View,
    const FCatanBotTopology& Topology, int32 PlayerId)
{
    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [PlayerId](const FCatanPlayerView& Item) { return Item.Id == PlayerId; });
    if (!Player || Player->Knights <= 0) return false;
    if (!Player->bHasLargestArmy) return true;
    for (const FCatanPlayerView& Opponent : View.Players)
        if (Opponent.Id != PlayerId && Opponent.VictoryPoints >= 8) return true;
    for (const FCatanNodeView& Node : View.Nodes)
    {
        if (Node.OwnerId != PlayerId || !Topology.NodeHexes.IsValidIndex(Node.Id)) continue;
        for (int32 HexId : Topology.NodeHexes[Node.Id])
            if (View.Hexes.IsValidIndex(HexId) && View.Hexes[HexId].bHasRobber
                && DicePips(View.Hexes[HexId].Dice) * BuildingMultiplier(Node) >= 4)
                return true;
    }
    return false;
}

FString FCatanBotStrategy::ChooseRobberVictim(const FCatanGameView& View)
{
    FString Best; float BestScore = -1.0f;
    for (const FString& Candidate : View.RobberVictims)
        if (const FCatanPlayerView* Player = View.Players.FindByPredicate(
            [&Candidate](const FCatanPlayerView& Item) { return Item.Name == Candidate; }))
        {
            const float Score = Player->VictoryPoints * 8.0f + Player->ResourceCards
                + Player->DevelopmentCards * 0.4f;
            if (Score > BestScore) { BestScore = Score; Best = Candidate; }
        }
    return Best.IsEmpty() && !View.RobberVictims.IsEmpty() ? View.RobberVictims[0] : Best;
}

FCatanResourceView FCatanBotStrategy::ChooseDiscard(const FCatanPlayerView& Player, int32 Count,
    const FCatanGameView& View)
{
    FCatanResourceView Remaining = Player.Resources;
    FCatanResourceView Drop;
    for (int32 Number = 0; Number < Count; ++Number)
    {
        int32 Choice = INDEX_NONE; float Lowest = TNumericLimits<float>::Max();
        for (int32 Index = 0; Index < ResourceCount; ++Index)
        {
            const int32 Held = GetResource(Remaining, Index);
            if (Held <= 0) continue;
            const float Utility = ResourceUtility(Player, View, Index, Held - 1)
                + FMath::Max(0, 2 - Held) * 0.3f;
            if (Utility < Lowest) { Lowest = Utility; Choice = Index; }
        }
        if (Choice == INDEX_NONE) break;
        SetResource(Remaining, Choice, GetResource(Remaining, Choice) - 1);
        SetResource(Drop, Choice, GetResource(Drop, Choice) + 1);
    }
    return Drop;
}

TPair<ECatanResource, ECatanResource> FCatanBotStrategy::ChooseYearOfPlenty(
    const FCatanPlayerView& Player, const FCatanGameView& View)
{
    FCatanResourceView Simulated = Player.Resources;
    int32 Choices[2]{};
    for (int32 Pick = 0; Pick < 2; ++Pick)
    {
        int32 Best = 0; float BestValue = -1.0f;
        for (int32 Index = 0; Index < ResourceCount; ++Index)
        {
            FCatanPlayerView Copy = Player; Copy.Resources = Simulated;
            const float Value = ResourceUtility(Copy, View, Index, GetResource(Simulated, Index));
            if (Value > BestValue) { BestValue = Value; Best = Index; }
        }
        Choices[Pick] = Best;
        SetResource(Simulated, Best, GetResource(Simulated, Best) + 1);
    }
    return {ResourceAt(Choices[0]), ResourceAt(Choices[1])};
}

ECatanResource FCatanBotStrategy::ChooseMonopoly(const FCatanGameView& View, int32 PlayerId)
{
    int32 Best = 0; float BestScore = -1.0f;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
    {
        int32 Gain = 0;
        for (const FCatanPlayerView& Player : View.Players)
            if (Player.Id != PlayerId) Gain += GetResource(Player.Resources, Index);
        const float Score = Gain * BaseResourceValue(Index);
        if (Score > BestScore) { BestScore = Score; Best = Index; }
    }
    return ResourceAt(Best);
}

int32 FCatanBotStrategy::MonopolyGain(const FCatanGameView& View, int32 PlayerId,
    ECatanResource Resource)
{
    const int32 Index = ResourceIndex(Resource);
    int32 Result = 0;
    if (Index == INDEX_NONE) return 0;
    for (const FCatanPlayerView& Player : View.Players)
        if (Player.Id != PlayerId) Result += GetResource(Player.Resources, Index);
    return Result;
}

FCatanBotBankTrade FCatanBotStrategy::ChooseBankTrade(const FCatanPlayerView& Player,
    const FCatanGameView& View)
{
    FCatanBotBankTrade Best;
    const FCatanResourceView Goal = BestGoal(Player, View);
    const float Before = MissingCost(Player.Resources, Goal);
    float BestImprovement = 0.01f;
    for (int32 From = 0; From < ResourceCount; ++From)
    {
        const int32 Rate = FMath::Max(2, GetResource(Player.TradeRates, From));
        if (GetResource(Player.Resources, From) < Rate) continue;
        for (int32 To = 0; To < ResourceCount; ++To)
        {
            if (From == To) continue;
            FCatanResourceView After = Player.Resources;
            SetResource(After, From, GetResource(After, From) - Rate);
            SetResource(After, To, GetResource(After, To) + 1);
            const float Improvement = Before - MissingCost(After, Goal);
            const int32 Surplus = GetResource(Player.Resources, From) - GetResource(Goal, From);
            const float Score = Improvement + FMath::Max(0, Surplus - Rate + 1) * 0.05f;
            if (Score > BestImprovement)
            {
                BestImprovement = Score; Best.bValid = true;
                Best.From = ResourceAt(From); Best.To = ResourceAt(To);
            }
        }
    }
    return Best;
}

FCatanBotPlayerTrade FCatanBotStrategy::ChoosePlayerTrade(const FCatanPlayerView& Player,
    const FCatanGameView& View)
{
    FCatanBotPlayerTrade Result;
    const FCatanResourceView Goal = BestGoal(Player, View);
    int32 Wanted = INDEX_NONE; float WantedValue = -1.0f;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
    {
        const int32 Deficit = GetResource(Goal, Index) - GetResource(Player.Resources, Index);
        const float Value = Deficit * BaseResourceValue(Index);
        if (Deficit > 0 && Value > WantedValue) { WantedValue = Value; Wanted = Index; }
    }
    if (Wanted == INDEX_NONE) return Result;
    int32 Offered = INDEX_NONE; float LowestCost = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < ResourceCount; ++Index)
    {
        if (Index == Wanted) continue;
        const int32 Surplus = GetResource(Player.Resources, Index) - GetResource(Goal, Index);
        if (Surplus <= 0) continue;
        const float CostValue = BaseResourceValue(Index) - Surplus * 0.12f;
        if (CostValue < LowestCost) { LowestCost = CostValue; Offered = Index; }
    }
    if (Offered == INDEX_NONE) return Result;
    Result.bValid = true;
    SetResource(Result.Offered, Offered, BaseResourceValue(Offered) + 0.15f
        < BaseResourceValue(Wanted) ? 2 : 1);
    if (GetResource(Result.Offered, Offered) > GetResource(Player.Resources, Offered)
        - GetResource(Goal, Offered))
        SetResource(Result.Offered, Offered, 1);
    SetResource(Result.Requested, Wanted, 1);
    return Result;
}

FString FCatanBotStrategy::ChooseTradeTarget(const FCatanGameView& View,
    const FCatanBotTopology& Topology, int32 PlayerId, const FCatanBotPlayerTrade& Trade)
{
    if (!Trade.bValid) return FString();
    int32 OfferedResource = INDEX_NONE;
    int32 RequestedResource = INDEX_NONE;
    for (int32 Index = 0; Index < ResourceCount; ++Index)
    {
        if (GetResource(Trade.Offered, Index) > 0) OfferedResource = Index;
        if (GetResource(Trade.Requested, Index) > 0) RequestedResource = Index;
    }
    if (OfferedResource == INDEX_NONE || RequestedResource == INDEX_NONE) return FString();
    FString Best;
    float BestScore = -TNumericLimits<float>::Max();
    for (const FCatanPlayerView& Candidate : View.Players)
    {
        if (Candidate.Id == PlayerId || Candidate.VictoryPoints >= 9
            || Candidate.ResourceCards <= 0) continue;
        float Production[ResourceCount]{};
        for (const FCatanNodeView& Node : View.Nodes)
        {
            if (Node.OwnerId != Candidate.Id || !Topology.NodeHexes.IsValidIndex(Node.Id)) continue;
            for (int32 HexId : Topology.NodeHexes[Node.Id])
            {
                if (!View.Hexes.IsValidIndex(HexId)) continue;
                const int32 Resource = ResourceIndex(View.Hexes[HexId].Resource);
                if (Resource != INDEX_NONE)
                    Production[Resource] += DicePips(View.Hexes[HexId].Dice)
                        * BuildingMultiplier(Node);
            }
        }
        const float PublicSupply = Production[RequestedResource];
        const float PublicNeed = FMath::Max(0.0f,
            Production[RequestedResource] - Production[OfferedResource]);
        const float Score = PublicSupply * 1.4f + PublicNeed * 0.45f
            + Candidate.ResourceCards * 0.12f - Candidate.VictoryPoints * 0.75f;
        if (Score > BestScore || (FMath::IsNearlyEqual(Score, BestScore) && Candidate.Name < Best))
        {
            BestScore = Score;
            Best = Candidate.Name;
        }
    }
    return Best;
}

bool FCatanBotStrategy::ShouldAcceptTrade(const FCatanPlayerView& Recipient,
    const FCatanPlayerView* Offerer, const FCatanResourceView& Offered,
    const FCatanResourceView& Requested, const FCatanGameView& View)
{
    for (int32 Index = 0; Index < ResourceCount; ++Index)
        if (GetResource(Requested, Index) > GetResource(Recipient.Resources, Index)) return false;
    const FCatanResourceView Goal = BestGoal(Recipient, View);
    const float BeforeDistance = MissingCost(Recipient.Resources, Goal);
    const FCatanResourceView After = AddResources(AddResources(Recipient.Resources, Requested, -1), Offered);
    const float AfterDistance = MissingCost(After, Goal);
    const float Gain = WeightedTotal(Recipient, View, Offered);
    const float CostValue = WeightedTotal(Recipient, View, Requested);
    const float ThreatPremium = Offerer && Offerer->VictoryPoints >= 8 ? 1.35f : 1.08f;
    return AfterDistance + 0.01f < BeforeDistance
        || (AfterDistance <= BeforeDistance + 0.01f && Gain >= CostValue * ThreatPremium);
}
