#include "CatanGameSubsystem.h"

#include "game_controller.hpp"

#include <sstream>

namespace
{
ECatanResource ToViewResource(ivv::catan::Resurse Resource)
{
    using ivv::catan::Resurse;
    switch (Resource)
    {
    case Resurse::Wood: return ECatanResource::Wood;
    case Resurse::Clay: return ECatanResource::Clay;
    case Resurse::Hay: return ECatanResource::Hay;
    case Resurse::Sheep: return ECatanResource::Sheep;
    case Resurse::Stone: return ECatanResource::Stone;
    case Resurse::Not: return ECatanResource::Desert;
    }
    return ECatanResource::Desert;
}

ECatanGamePhase ToViewPhase(ivv::catan::GameController::GameStep Step)
{
    using GameStep = ivv::catan::GameController::GameStep;
    switch (Step)
    {
    case GameStep::ForwardBuildingSettlement:
    case GameStep::BackwardBuildingSettlement: return ECatanGamePhase::SetupSettlement;
    case GameStep::ForwardBuildingRoad:
    case GameStep::BackwardBuildingRoad: return ECatanGamePhase::SetupRoad;
    case GameStep::DiceDrop: return ECatanGamePhase::RollDice;
    case GameStep::CommonPlay: return ECatanGamePhase::CommonPlay;
    case GameStep::DropCards: return ECatanGamePhase::DropCards;
    case GameStep::BanditMove: return ECatanGamePhase::MoveRobber;
    case GameStep::RoadBuilding: return ECatanGamePhase::RoadBuilding;
    case GameStep::Finish: return ECatanGamePhase::Finished;
    }
    return ECatanGamePhase::Finished;
}

ivv::catan::Resurse ToCoreResource(ECatanResource Resource)
{
    using ivv::catan::Resurse;
    switch (Resource)
    {
    case ECatanResource::Wood: return Resurse::Wood;
    case ECatanResource::Clay: return Resurse::Clay;
    case ECatanResource::Hay: return Resurse::Hay;
    case ECatanResource::Sheep: return Resurse::Sheep;
    case ECatanResource::Stone: return Resurse::Stone;
    case ECatanResource::Desert: return Resurse::Not;
    }
    return Resurse::Not;
}

int32 CountDevelopmentCards(const ivv::catan::Player& Player)
{
    using ivv::catan::DevelopmentCard;
    constexpr DevelopmentCard Cards[] = {
        DevelopmentCard::Knights, DevelopmentCard::RoadBuilding,
        DevelopmentCard::YearOfPlenty, DevelopmentCard::Monopoly,
        DevelopmentCard::University, DevelopmentCard::Market,
        DevelopmentCard::GreatHall, DevelopmentCard::Chapel,
        DevelopmentCard::Library
    };
    size_t Count = 0;
    for (DevelopmentCard Card : Cards)
    {
        Count += Player.GetReadyForUseCardCount(Card);
        Count += Player.GetPurchasedCardCount(Card);
    }
    return static_cast<int32>(Count);
}

FCatanResourceView ToResourceView(const std::map<ivv::catan::Resurse, size_t>& Resources)
{
    FCatanResourceView View;
    auto Count = [&Resources](ivv::catan::Resurse Resource)
    {
        const auto It = Resources.find(Resource);
        return It == Resources.end() ? 0 : static_cast<int32>(It->second);
    };
    View.Wood = Count(ivv::catan::Resurse::Wood);
    View.Clay = Count(ivv::catan::Resurse::Clay);
    View.Hay = Count(ivv::catan::Resurse::Hay);
    View.Sheep = Count(ivv::catan::Resurse::Sheep);
    View.Stone = Count(ivv::catan::Resurse::Stone);
    return View;
}

std::map<ivv::catan::Resurse, size_t> ToResourceMap(const FCatanResourceView& Resources)
{
    std::map<ivv::catan::Resurse, size_t> Result;
    auto Add = [&Result](ivv::catan::Resurse Resource, int32 Count)
    {
        if (Count > 0) Result[Resource] = static_cast<size_t>(Count);
    };
    Add(ivv::catan::Resurse::Wood, Resources.Wood);
    Add(ivv::catan::Resurse::Clay, Resources.Clay);
    Add(ivv::catan::Resurse::Hay, Resources.Hay);
    Add(ivv::catan::Resurse::Sheep, Resources.Sheep);
    Add(ivv::catan::Resurse::Stone, Resources.Stone);
    return Result;
}
}

UCatanGameSubsystem::~UCatanGameSubsystem() = default;

void UCatanGameSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    StartLocalGame(TArray<FString>{TEXT("Player 1"), TEXT("Player 2")});
}

void UCatanGameSubsystem::Deinitialize()
{
    Game.reset();
    Super::Deinitialize();
}

void UCatanGameSubsystem::StartLocalGame(const TArray<FString>& Names)
{
    PlayerNames = Names.Num() >= 2 ? Names : TArray<FString>{TEXT("Player 1"), TEXT("Player 2")};
    std::vector<std::string> CoreNames;
    CoreNames.reserve(PlayerNames.Num());
    for (const FString& Name : PlayerNames)
    {
        CoreNames.emplace_back(TCHAR_TO_UTF8(*Name));
    }
    Game = std::make_unique<ivv::catan::GameController>(std::move(CoreNames));
    BoardAction = ECatanBoardAction::Automatic;
    PendingRobberHex = INDEX_NONE;
    RobberVictims.Reset();
    StatusMessage = TEXT("New local game started");
    EventLog.Reset();
    LastResources.Reset();
    LastLargestArmy.Reset();
    LastLongestRoad.Reset();
    for (const FString& PlayerName : PlayerNames)
    {
        const ivv::catan::Player& Player = Game->GetPlayer(TCHAR_TO_UTF8(*PlayerName));
        FCatanResourceView Resources;
        Resources.Wood = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Wood));
        Resources.Clay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Clay));
        Resources.Hay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Hay));
        Resources.Sheep = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Sheep));
        Resources.Stone = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Stone));
        LastResources.Add(PlayerName, Resources);
    }
    AppendEvent(StatusMessage);
    OnGameStateChanged.Broadcast();
}

FCatanGameView UCatanGameSubsystem::GetSnapshot() const
{
    FCatanGameView View;
    if (!Game) return View;

    View.CurrentPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
    View.Phase = ToViewPhase(Game->GetStep());
    View.BoardAction = BoardAction;
    View.StatusMessage = StatusMessage;
    View.PendingRobberHex = PendingRobberHex;
    View.RobberVictims = RobberVictims;
    View.EventLog = EventLog;
    const auto Dice = Game->GetLastDice();
    View.FirstDie = static_cast<int32>(Dice.first);
    View.SecondDie = static_cast<int32>(Dice.second);
    if (const std::optional<std::string> Winner = Game->GetWinner())
    {
        View.Winner = UTF8_TO_TCHAR(Winner->c_str());
    }
    if (const auto& Deal = Game->GetActivDeal())
    {
        View.ActiveDeal.bIsActive = true;
        View.ActiveDeal.OfferingPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
        View.ActiveDeal.Offered = ToResourceView(Deal->sell);
        View.ActiveDeal.Requested = ToResourceView(Deal->buy);
    }
    std::ostringstream Step;
    Game->PrintStep(Step);
    View.Step = UTF8_TO_TCHAR(Step.str().c_str());

    View.Players.Reserve(PlayerNames.Num());
    for (const FString& PlayerName : PlayerNames)
    {
        const ivv::catan::Player& Player = Game->GetPlayer(TCHAR_TO_UTF8(*PlayerName));
        FCatanPlayerView& PlayerView = View.Players.Emplace_GetRef();
        PlayerView.Id = static_cast<int32>(Player.getId());
        PlayerView.Name = PlayerName;
        PlayerView.bIsCurrent = PlayerName == View.CurrentPlayer;
        PlayerView.VictoryPoints = static_cast<int32>(Player.GetWinPoints());
        PlayerView.DevelopmentCards = CountDevelopmentCards(Player);
        PlayerView.Knights = static_cast<int32>(Player.GetReadyForUseCardCount(ivv::catan::DevelopmentCard::Knights));
        PlayerView.RoadBuildingCards = static_cast<int32>(Player.GetReadyForUseCardCount(ivv::catan::DevelopmentCard::RoadBuilding));
        PlayerView.YearOfPlentyCards = static_cast<int32>(Player.GetReadyForUseCardCount(ivv::catan::DevelopmentCard::YearOfPlenty));
        PlayerView.MonopolyCards = static_cast<int32>(Player.GetReadyForUseCardCount(ivv::catan::DevelopmentCard::Monopoly));
        PlayerView.bHasLargestArmy = Player.HasLargestArmy();
        PlayerView.bHasLongestRoad = Player.HasLongestRoad();
        PlayerView.FreeSettlements = static_cast<int32>(Player.getFreeSettlementCount());
        PlayerView.FreeCities = static_cast<int32>(Player.getFreeCastleCount());
        PlayerView.FreeRoads = static_cast<int32>(Player.getFreeRoadCount());
        PlayerView.Resources.Wood = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Wood));
        PlayerView.Resources.Clay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Clay));
        PlayerView.Resources.Hay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Hay));
        PlayerView.Resources.Sheep = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Sheep));
        PlayerView.Resources.Stone = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Stone));
        PlayerView.TradeRates.Wood = static_cast<int32>(Player.GetMarketPrice(ivv::catan::Resurse::Wood));
        PlayerView.TradeRates.Clay = static_cast<int32>(Player.GetMarketPrice(ivv::catan::Resurse::Clay));
        PlayerView.TradeRates.Hay = static_cast<int32>(Player.GetMarketPrice(ivv::catan::Resurse::Hay));
        PlayerView.TradeRates.Sheep = static_cast<int32>(Player.GetMarketPrice(ivv::catan::Resurse::Sheep));
        PlayerView.TradeRates.Stone = static_cast<int32>(Player.GetMarketPrice(ivv::catan::Resurse::Stone));
        if (PlayerView.bIsCurrent && View.Phase == ECatanGamePhase::DropCards)
        {
            View.RequiredDiscardCount = static_cast<int32>(Player.getCountResurses() / 2);
        }
    }

    const auto& Hexes = Game->GetMap().GetGexes();
    View.Hexes.Reserve(static_cast<int32>(Hexes.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Hexes.size()); ++Index)
    {
        FCatanHexView& Hex = View.Hexes.Emplace_GetRef();
        Hex.Id = Index;
        Hex.Resource = ToViewResource(Hexes[Index].getType());
        Hex.Dice = Hexes[Index].getDice();
        Hex.bHasRobber = Hexes[Index].isBandit();
    }

    const auto Nodes = Game->GetMap().GetNodes();
    View.Nodes.Reserve(static_cast<int32>(Nodes.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Nodes.size()); ++Index)
    {
        FCatanNodeView& Node = View.Nodes.Emplace_GetRef();
        Node.Id = Index;
        if (const ivv::catan::Building* Building = Nodes[Index].getBuilding())
        {
            Node.OwnerId = static_cast<int32>(Building->getPlayer()->getId());
            Node.bIsCity = !Building->canUpgrade();
        }
    }

    const auto Roads = Game->GetMap().GetFacets();
    View.Roads.Reserve(static_cast<int32>(Roads.size()));
    for (int32 Index = 0; Index < static_cast<int32>(Roads.size()); ++Index)
    {
        FCatanRoadView& Road = View.Roads.Emplace_GetRef();
        Road.Id = Index;
        if (const ivv::catan::Road* CoreRoad = Roads[Index].getRoad())
        {
            Road.OwnerId = static_cast<int32>(CoreRoad->getPlayer()->getId());
        }
    }

    const bool bSettlementTargets = View.Phase == ECatanGamePhase::SetupSettlement
        || (View.Phase == ECatanGamePhase::CommonPlay && BoardAction == ECatanBoardAction::BuildSettlement);
    const bool bCityTargets = View.Phase == ECatanGamePhase::CommonPlay
        && BoardAction == ECatanBoardAction::BuildCity;
    if (bSettlementTargets || bCityTargets)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Nodes.Num()); ++Index)
        {
            if ((bCityTargets && Game->CanBuildCastle(Index))
                || (bSettlementTargets && Game->CanBuildSettlement(Index)))
                View.ValidNodeTargets.Add(Index);
        }
    }
    const bool bRoadTargets = View.Phase == ECatanGamePhase::SetupRoad
        || View.Phase == ECatanGamePhase::RoadBuilding
        || (View.Phase == ECatanGamePhase::CommonPlay && BoardAction == ECatanBoardAction::BuildRoad);
    if (bRoadTargets)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Roads.Num()); ++Index)
            if (Game->CanBuildRoad(Index)) View.ValidRoadTargets.Add(Index);
    }
    if (View.Phase == ECatanGamePhase::MoveRobber && PendingRobberHex == INDEX_NONE)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Hexes.Num()); ++Index)
            if (Game->CanMoveBandit(Index)) View.ValidHexTargets.Add(Index);
    }
    return View;
}

bool UCatanGameSubsystem::TryBuildSettlement(int32 NodeId, FString& Error)
{
    if (!Game)
    {
        Error = TEXT("Game is not initialized");
        return false;
    }
    try
    {
        Game->BuildSettlement(Game->GetCurrentPlayer(), static_cast<size_t>(NodeId));
        return CompleteCommand(true, TEXT("Settlement built"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryBuildRoad(int32 RoadId, FString& Error)
{
    if (!Game)
    {
        Error = TEXT("Game is not initialized");
        return false;
    }
    try
    {
        Game->BuildRoad(Game->GetCurrentPlayer(), static_cast<size_t>(RoadId));
        return CompleteCommand(true, TEXT("Road built"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryBuildCity(int32 NodeId, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->BuildCastle(Game->GetCurrentPlayer(), static_cast<size_t>(NodeId));
        return CompleteCommand(true, TEXT("City built"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryMoveRobber(int32 HexId, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    if (HexId < 0 || HexId >= static_cast<int32>(Game->GetMap().GetGexes().size()))
    {
        return CompleteCommand(false, TEXT("Invalid robber hex"), Error);
    }
    if (Game->GetStep() != ivv::catan::GameController::GameStep::BanditMove)
    {
        return CompleteCommand(false, TEXT("The robber cannot move in this phase"), Error);
    }
    if (Game->GetMap().GetGexes()[HexId].isBandit())
    {
        return CompleteCommand(false, TEXT("The robber must move to another hex"), Error);
    }

    const FString CurrentPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
    TSet<FString> UniqueVictims;
    for (const ivv::catan::Node* Node : Game->GetMap().GetGexes()[HexId].GetNodes())
    {
        if (const ivv::catan::Building* Building = Node->getBuilding())
        {
            const FString Owner = UTF8_TO_TCHAR(Building->getPlayer()->getName().c_str());
            if (Owner != CurrentPlayer) UniqueVictims.Add(Owner);
        }
    }

    if (!UniqueVictims.IsEmpty())
    {
        PendingRobberHex = HexId;
        RobberVictims = UniqueVictims.Array();
        RobberVictims.Sort();
        StatusMessage = TEXT("Choose a player to steal from");
        Error.Reset();
        OnGameStateChanged.Broadcast();
        return true;
    }
    try
    {
        Game->BanditMove(Game->GetCurrentPlayer(), static_cast<size_t>(HexId));
        PendingRobberHex = INDEX_NONE;
        RobberVictims.Reset();
        return CompleteCommand(true, TEXT("Robber moved"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryChooseRobberVictim(const FString& Victim, FString& Error)
{
    if (!Game || PendingRobberHex == INDEX_NONE)
    {
        return CompleteCommand(false, TEXT("Choose a robber hex first"), Error);
    }
    if (!RobberVictims.Contains(Victim))
    {
        return CompleteCommand(false, TEXT("This player has no building on the selected hex"), Error);
    }
    try
    {
        Game->BanditMove(Game->GetCurrentPlayer(), static_cast<size_t>(PendingRobberHex), TCHAR_TO_UTF8(*Victim));
        PendingRobberHex = INDEX_NONE;
        RobberVictims.Reset();
        return CompleteCommand(true, TEXT("Robber moved and a resource was stolen"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryDropResources(const FCatanResourceView& Resources, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    std::map<ivv::catan::Resurse, size_t> Drop;
    auto Add = [&Drop](ivv::catan::Resurse Resource, int32 Count)
    {
        if (Count > 0) Drop[Resource] = static_cast<size_t>(Count);
    };
    Add(ivv::catan::Resurse::Wood, Resources.Wood);
    Add(ivv::catan::Resurse::Clay, Resources.Clay);
    Add(ivv::catan::Resurse::Hay, Resources.Hay);
    Add(ivv::catan::Resurse::Sheep, Resources.Sheep);
    Add(ivv::catan::Resurse::Stone, Resources.Stone);
    try
    {
        Game->DropCards(Game->GetCurrentPlayer(), Drop);
        return CompleteCommand(true, TEXT("Resources discarded"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryRollDice(FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->Dice(Game->GetCurrentPlayer());
        return CompleteCommand(true, TEXT("Dice rolled"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryBuyDevelopmentCard(FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->DevCard(Game->GetCurrentPlayer());
        return CompleteCommand(true, TEXT("Development card bought"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryPass(FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->Pass(Game->GetCurrentPlayer());
        return CompleteCommand(true, TEXT("Turn passed"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryUseDevelopmentCard(ECatanDevelopmentCard Card,
    ECatanResource FirstResource, ECatanResource SecondResource, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    using ivv::catan::DevelopmentCard;
    DevelopmentCard CoreCard = DevelopmentCard::Knights;
    ivv::catan::GameController::UseDevCardParam Param;
    switch (Card)
    {
    case ECatanDevelopmentCard::Knight:
        CoreCard = DevelopmentCard::Knights;
        break;
    case ECatanDevelopmentCard::RoadBuilding:
        CoreCard = DevelopmentCard::RoadBuilding;
        break;
    case ECatanDevelopmentCard::YearOfPlenty:
        CoreCard = DevelopmentCard::YearOfPlenty;
        Param = std::array<ivv::catan::Resurse, 2>{ToCoreResource(FirstResource), ToCoreResource(SecondResource)};
        break;
    case ECatanDevelopmentCard::Monopoly:
        CoreCard = DevelopmentCard::Monopoly;
        Param = ToCoreResource(FirstResource);
        break;
    }
    try
    {
        Game->UseDevCard(Game->GetCurrentPlayer(), CoreCard, Param);
        return CompleteCommand(true, TEXT("Development card played"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryBankTrade(ECatanResource From, ECatanResource To, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    if (From == To || From == ECatanResource::Desert || To == ECatanResource::Desert)
    {
        return CompleteCommand(false, TEXT("Choose two different resources"), Error);
    }
    try
    {
        Game->Market(Game->GetCurrentPlayer(), ToCoreResource(From), ToCoreResource(To));
        return CompleteCommand(true, TEXT("Bank trade completed"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryOfferTrade(const FCatanResourceView& Offered,
    const FCatanResourceView& Requested, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->SetDeal(Game->GetCurrentPlayer(), ToResourceMap(Offered), ToResourceMap(Requested));
        return CompleteCommand(true, TEXT("Trade offered — another player may accept or decline"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryAcceptTrade(const FString& Player, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    const auto& Deal = Game->GetActivDeal();
    if (!Deal) return CompleteCommand(false, TEXT("There is no active trade"), Error);
    try
    {
        Game->SetDeal(TCHAR_TO_UTF8(*Player), Deal->buy, Deal->sell);
        return CompleteCommand(true, FString::Printf(TEXT("%s accepted the trade"), *Player), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

bool UCatanGameSubsystem::TryCancelTrade(const FString& Player, FString& Error)
{
    if (!Game) return CompleteCommand(false, TEXT("Game is not initialized"), Error);
    try
    {
        Game->CancelDeal(TCHAR_TO_UTF8(*Player));
        return CompleteCommand(true, TEXT("Trade cancelled"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

void UCatanGameSubsystem::SelectBoardAction(ECatanBoardAction Action)
{
    BoardAction = Action;
    StatusMessage = TEXT("Select a target on the board");
    OnGameStateChanged.Broadcast();
}

bool UCatanGameSubsystem::CompleteCommand(bool bSucceeded, const FString& Message, FString& Error)
{
    StatusMessage = Message;
    if (bSucceeded)
    {
        Error.Reset();
        BoardAction = ECatanBoardAction::Automatic;
        AppendEvent(Message);
        CaptureResourceChanges();
        CaptureAwards();
    }
    else
    {
        Error = Message;
    }
    OnGameStateChanged.Broadcast();
    return bSucceeded;
}

void UCatanGameSubsystem::AppendEvent(const FString& Message)
{
    EventLog.Add(Message);
    constexpr int32 MaxEvents = 8;
    if (EventLog.Num() > MaxEvents) EventLog.RemoveAt(0, EventLog.Num() - MaxEvents);
}

void UCatanGameSubsystem::CaptureResourceChanges()
{
    if (!Game) return;
    constexpr const TCHAR* Names[] = {TEXT("wood"), TEXT("clay"), TEXT("hay"), TEXT("sheep"), TEXT("stone")};
    for (const FString& PlayerName : PlayerNames)
    {
        const ivv::catan::Player& Player = Game->GetPlayer(TCHAR_TO_UTF8(*PlayerName));
        FCatanResourceView Current;
        Current.Wood = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Wood));
        Current.Clay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Clay));
        Current.Hay = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Hay));
        Current.Sheep = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Sheep));
        Current.Stone = static_cast<int32>(Player.getCountResurses(ivv::catan::Resurse::Stone));
        const FCatanResourceView* Previous = LastResources.Find(PlayerName);
        if (Previous)
        {
            const int32 Before[] = {Previous->Wood, Previous->Clay, Previous->Hay, Previous->Sheep, Previous->Stone};
            const int32 After[] = {Current.Wood, Current.Clay, Current.Hay, Current.Sheep, Current.Stone};
            FString Changes;
            for (int32 Index = 0; Index < 5; ++Index)
            {
                const int32 Delta = After[Index] - Before[Index];
                if (Delta != 0) Changes += FString::Printf(TEXT("%s%+d %s"), Changes.IsEmpty() ? TEXT("") : TEXT(", "), Delta, Names[Index]);
            }
            if (!Changes.IsEmpty()) AppendEvent(FString::Printf(TEXT("%s: %s"), *PlayerName, *Changes));
        }
        LastResources.Add(PlayerName, Current);
    }
}

void UCatanGameSubsystem::CaptureAwards()
{
    if (!Game) return;
    FString LargestArmy;
    FString LongestRoad;
    for (const FString& PlayerName : PlayerNames)
    {
        const ivv::catan::Player& Player = Game->GetPlayer(TCHAR_TO_UTF8(*PlayerName));
        if (Player.HasLargestArmy()) LargestArmy = PlayerName;
        if (Player.HasLongestRoad()) LongestRoad = PlayerName;
    }
    if (LargestArmy != LastLargestArmy)
    {
        if (!LargestArmy.IsEmpty()) AppendEvent(FString::Printf(TEXT("★ %s claimed Largest Army"), *LargestArmy));
        else if (!LastLargestArmy.IsEmpty()) AppendEvent(FString::Printf(TEXT("★ %s lost Largest Army"), *LastLargestArmy));
        LastLargestArmy = LargestArmy;
    }
    if (LongestRoad != LastLongestRoad)
    {
        if (!LongestRoad.IsEmpty()) AppendEvent(FString::Printf(TEXT("★ %s claimed Longest Road"), *LongestRoad));
        else if (!LastLongestRoad.IsEmpty()) AppendEvent(FString::Printf(TEXT("★ %s lost Longest Road"), *LastLongestRoad));
        LastLongestRoad = LongestRoad;
    }
}
