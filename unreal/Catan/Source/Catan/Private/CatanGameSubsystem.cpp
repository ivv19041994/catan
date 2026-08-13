#include "CatanGameSubsystem.h"

#include "CatanGameState.h"
#include "CatanPlayerController.h"
#include "CatanPlayerState.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
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
    BotTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCatanGameSubsystem::TickBotTicker));
}

void UCatanGameSubsystem::Deinitialize()
{
    if (BotTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(BotTickerHandle);
        BotTickerHandle.Reset();
    }
    Game.reset();
    Super::Deinitialize();
}

bool UCatanGameSubsystem::TickBotTicker(float DeltaSeconds)
{
    TickBots(DeltaSeconds);
    return true;
}

void UCatanGameSubsystem::StartLocalGame(const TArray<FString>& Names)
{
    BotPlayers.Reset();
    PlayerNames = Names.Num() >= 2 ? Names : TArray<FString>{TEXT("Player 1"), TEXT("Player 2")};
    LocalPlayerName = PlayerNames.IsEmpty() ? FString() : PlayerNames[0];
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

void UCatanGameSubsystem::StartBotGame(const FString& HumanName, int32 BotCount)
{
    const int32 SafeBotCount = FMath::Clamp(BotCount, 1, 3);
    TArray<FString> Names{HumanName.IsEmpty() ? TEXT("Player") : HumanName.Left(24)};
    for (int32 Index = 0; Index < SafeBotCount; ++Index)
        Names.Add(FString::Printf(TEXT("Bot %d"), Index + 1));
    StartLocalGame(Names);
    LocalPlayerName = Names[0];
    for (int32 Index = 1; Index < Names.Num(); ++Index) BotPlayers.Add(Names[Index]);
    bBotAutoplay = FParse::Param(FCommandLine::Get(), TEXT("CatanBotAutoplay"));
    if (bBotAutoplay) BotPlayers.Add(Names[0]);
    FParse::Value(FCommandLine::Get(), TEXT("CatanBotMaxActions="), BotE2EMaxActions);
    BotE2EMaxActions = FMath::Max(100, BotE2EMaxActions);
    BotE2EActions = 0;
    BotE2EUnchangedActions = 0;
    bBotE2EExitRequested = false;
    BotRandom.Initialize(FMath::Rand());
    BotActionDelay = 0.75f;
    StatusMessage = FString::Printf(TEXT("Single-player game started with %d bot%s"),
        SafeBotCount, SafeBotCount == 1 ? TEXT("") : TEXT("s"));
    AppendEvent(StatusMessage);
    OnGameStateChanged.Broadcast();
}

bool UCatanGameSubsystem::IsBotPlayer(const FString& Name) const
{
    return BotPlayers.Contains(Name);
}

void UCatanGameSubsystem::TickBots(float DeltaSeconds)
{
    if (!HasAuthoritativeGame() || BotPlayers.IsEmpty()) return;
    const FCatanGameView View = BuildAuthoritativeSnapshot();
    if (View.Phase == ECatanGamePhase::Finished)
    {
        if (bBotAutoplay && !bBotE2EExitRequested)
            FinishBotE2E(!View.Winner.IsEmpty(), FString::Printf(TEXT("winner=%s actions=%d"),
                View.Winner.IsEmpty() ? TEXT("none") : *View.Winner, BotE2EActions));
        return;
    }
    if (!IsBotPlayer(View.CurrentPlayer)) return;
    BotActionDelay -= DeltaSeconds;
    if (BotActionDelay > 0.0f) return;
    BotActionDelay = bBotAutoplay ? 0.001f : 0.48f;
    const FString Before = BotStateFingerprint(View);
    PerformBotAction();
    if (!bBotAutoplay) return;
    ++BotE2EActions;
    const FCatanGameView After = BuildAuthoritativeSnapshot();
    BotE2EUnchangedActions = BotStateFingerprint(After) == Before ? BotE2EUnchangedActions + 1 : 0;
    if (After.Phase == ECatanGamePhase::Finished)
        FinishBotE2E(!After.Winner.IsEmpty(), FString::Printf(TEXT("winner=%s actions=%d"),
            After.Winner.IsEmpty() ? TEXT("none") : *After.Winner, BotE2EActions));
    else if (BotE2EUnchangedActions >= 30)
        FinishBotE2E(false, FString::Printf(TEXT("stalled player=%s phase=%d actions=%d"),
            *After.CurrentPlayer, static_cast<int32>(After.Phase), BotE2EActions));
    else if (BotE2EActions >= BotE2EMaxActions)
        FinishBotE2E(false, FString::Printf(TEXT("action limit reached player=%s phase=%d actions=%d"),
            *After.CurrentPlayer, static_cast<int32>(After.Phase), BotE2EActions));
}

FString UCatanGameSubsystem::BotStateFingerprint(const FCatanGameView& View) const
{
    FString Result = FString::Printf(TEXT("%s|%d|%d|%d|%d|%d|%d"), *View.CurrentPlayer,
        static_cast<int32>(View.Phase), static_cast<int32>(View.BoardAction), View.FirstDie,
        View.SecondDie, View.PendingRobberHex, View.EventLog.Num());
    for (const FCatanPlayerView& Player : View.Players)
        Result += FString::Printf(TEXT("|%s:%d:%d:%d:%d:%d:%d:%d"), *Player.Name,
            Player.VictoryPoints, Player.ResourceCards, Player.DevelopmentCards,
            Player.FreeSettlements, Player.FreeCities, Player.FreeRoads, Player.Knights);
    return Result;
}

void UCatanGameSubsystem::FinishBotE2E(bool bSucceeded, const FString& Message)
{
    if (bBotE2EExitRequested) return;
    bBotE2EExitRequested = true;
    if (bSucceeded)
    {
        UE_LOG(LogTemp, Display, TEXT("CATAN_BOT_E2E PASS %s"), *Message);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CATAN_BOT_E2E FAIL %s"), *Message);
    }
    FPlatformMisc::RequestExitWithStatus(false, bSucceeded ? 0 : 2, TEXT("CatanBotE2E"));
}

FCatanGameView UCatanGameSubsystem::GetSnapshot() const
{
    const UWorld* World = GetWorld();
    const ACatanGameState* State = World ? World->GetGameState<ACatanGameState>() : nullptr;
    if (State && State->NetworkMode == ECatanNetworkMode::Playing
        && World && World->GetNetMode() != NM_Standalone)
    {
        FCatanGameView View = State->PublicView;
        if (const APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0))
            if (const ACatanPlayerState* PlayerState = Controller->GetPlayerState<ACatanPlayerState>())
                if (FCatanPlayerView* Player = View.Players.FindByPredicate([PlayerState](const FCatanPlayerView& Item)
                    { return Item.Name == PlayerState->GetPlayerName(); }))
                {
                    Player->bIsLocalPlayer = true;
                    Player->bResourcesVisible = true;
                    Player->Resources = PlayerState->PrivateView.Resources;
                    Player->Knights = PlayerState->PrivateView.Knights;
                    Player->RoadBuildingCards = PlayerState->PrivateView.RoadBuildingCards;
                    Player->YearOfPlentyCards = PlayerState->PrivateView.YearOfPlentyCards;
                    Player->MonopolyCards = PlayerState->PrivateView.MonopolyCards;
                }
        return View;
    }
    FCatanGameView View = BuildAuthoritativeSnapshot();
    for (FCatanPlayerView& Player : View.Players)
    {
        Player.bIsLocalPlayer = Player.Name == LocalPlayerName;
        Player.bResourcesVisible = Player.bIsLocalPlayer;
        if (Player.bResourcesVisible) continue;
        Player.Resources = {};
        Player.Knights = 0;
        Player.RoadBuildingCards = 0;
        Player.YearOfPlentyCards = 0;
        Player.MonopolyCards = 0;
    }
    return View;
}

bool UCatanGameSubsystem::HasAuthoritativeGame() const
{
    const UWorld* World = GetWorld();
    return Game != nullptr && (!World || World->GetNetMode() != NM_Client);
}

bool UCatanGameSubsystem::CanLocalPlayerAct(const FCatanGameView& View) const
{
    const UWorld* World = GetWorld();
    if (!World) return false;
    if (World->GetNetMode() == NM_Standalone) return !IsBotPlayer(View.CurrentPlayer);
    const APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
    const APlayerState* PlayerState = Controller ? Controller->PlayerState : nullptr;
    return PlayerState && !View.CurrentPlayer.IsEmpty()
        && View.CurrentPlayer == PlayerState->GetPlayerName();
}

void UCatanGameSubsystem::PerformBotAction()
{
    const FCatanGameView View = BuildAuthoritativeSnapshot();
    UE_LOG(LogTemp, Display, TEXT("CATAN_BOT turn=%s phase=%d nodes=%d roads=%d hexes=%d"),
        *View.CurrentPlayer, static_cast<int32>(View.Phase), View.ValidNodeTargets.Num(),
        View.ValidRoadTargets.Num(), View.ValidHexTargets.Num());
    FString Error;
    auto RandomTarget = [this](const TArray<int32>& Targets)
    {
        return Targets.IsEmpty() ? INDEX_NONE : Targets[BotRandom.RandRange(0, Targets.Num() - 1)];
    };

    if (View.PendingRobberHex != INDEX_NONE && !View.RobberVictims.IsEmpty())
    {
        TryChooseRobberVictim(View.RobberVictims[BotRandom.RandRange(0, View.RobberVictims.Num() - 1)], Error);
        return;
    }
    switch (View.Phase)
    {
    case ECatanGamePhase::SetupSettlement:
        if (const int32 Target = RandomTarget(View.ValidNodeTargets); Target != INDEX_NONE)
            TryBuildSettlement(Target, Error);
        return;
    case ECatanGamePhase::SetupRoad:
    case ECatanGamePhase::RoadBuilding:
        if (const int32 Target = RandomTarget(View.ValidRoadTargets); Target != INDEX_NONE)
            TryBuildRoad(Target, Error);
        else if (View.Phase == ECatanGamePhase::RoadBuilding)
            TryPass(Error);
        return;
    case ECatanGamePhase::RollDice:
        TryRollDice(Error);
        return;
    case ECatanGamePhase::DropCards:
        {
            const FCatanPlayerView* Player = View.Players.FindByPredicate(
                [&View](const FCatanPlayerView& Item) { return Item.Name == View.CurrentPlayer; });
            if (!Player) return;
            FCatanResourceView Drop;
            int32 Holdings[] = {Player->Resources.Wood, Player->Resources.Clay, Player->Resources.Hay,
                Player->Resources.Sheep, Player->Resources.Stone};
            int32* Counts[] = {&Drop.Wood, &Drop.Clay, &Drop.Hay, &Drop.Sheep, &Drop.Stone};
            for (int32 Remaining = View.RequiredDiscardCount; Remaining > 0;)
            {
                const int32 Resource = BotRandom.RandRange(0, 4);
                if (*Counts[Resource] < Holdings[Resource]) { ++*Counts[Resource]; --Remaining; }
            }
            TryDropResources(Drop, Error);
        }
        return;
    case ECatanGamePhase::MoveRobber:
        if (const int32 Target = RandomTarget(View.ValidHexTargets); Target != INDEX_NONE)
            TryMoveRobber(Target, Error);
        return;
    case ECatanGamePhase::CommonPlay:
        break;
    case ECatanGamePhase::Finished:
        return;
    }

    if (View.BoardAction == ECatanBoardAction::BuildCity)
    {
        if (const int32 Target = RandomTarget(View.ValidNodeTargets); Target != INDEX_NONE)
            TryBuildCity(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }
    if (View.BoardAction == ECatanBoardAction::BuildSettlement)
    {
        if (const int32 Target = RandomTarget(View.ValidNodeTargets); Target != INDEX_NONE)
            TryBuildSettlement(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }
    if (View.BoardAction == ECatanBoardAction::BuildRoad)
    {
        if (const int32 Target = RandomTarget(View.ValidRoadTargets); Target != INDEX_NONE)
            TryBuildRoad(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }

    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [&View](const FCatanPlayerView& Item) { return Item.Name == View.CurrentPlayer; });
    if (!Player) return;
    const FCatanResourceView& Have = Player->Resources;
    if (Player->FreeCities > 0 && Have.Hay >= 2 && Have.Stone >= 3 && BotRandom.FRand() < 0.82f)
    {
        SelectBoardAction(ECatanBoardAction::BuildCity);
        return;
    }
    if (Player->FreeSettlements > 0 && Have.Wood > 0 && Have.Clay > 0
        && Have.Hay > 0 && Have.Sheep > 0 && BotRandom.FRand() < 0.82f)
    {
        SelectBoardAction(ECatanBoardAction::BuildSettlement);
        return;
    }
    if (Player->FreeRoads > 0 && Have.Wood > 0 && Have.Clay > 0 && BotRandom.FRand() < 0.62f)
    {
        SelectBoardAction(ECatanBoardAction::BuildRoad);
        return;
    }
    if (Have.Hay > 0 && Have.Sheep > 0 && Have.Stone > 0 && BotRandom.FRand() < 0.58f)
    {
        if (TryBuyDevelopmentCard(Error)) return;
    }
    if (Player->Knights > 0 && BotRandom.FRand() < 0.3f)
    {
        if (TryUseDevelopmentCard(ECatanDevelopmentCard::Knight,
            ECatanResource::Wood, ECatanResource::Clay, Error)) return;
    }
    TryPass(Error);
}

FCatanGameView UCatanGameSubsystem::BuildAuthoritativeSnapshot() const
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
        PlayerView.bIsBot = IsBotPlayer(PlayerName);
        PlayerView.VictoryPoints = static_cast<int32>(Player.GetWinPoints());
        PlayerView.ResourceCards = static_cast<int32>(Player.getCountResurses());
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::BuildSettlement, NodeId, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::BuildRoad, RoadId, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::BuildCity, NodeId, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::MoveRobber, HexId, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::ChooseRobberVictim, 0, 0, Victim, {}, {}, Error);
    if (PendingRobberHex == INDEX_NONE)
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::DropResources, 0, 0, FString(), Resources, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::RollDice, 0, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::BuyDevelopmentCard, 0, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::Pass, 0, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::UseDevelopmentCard,
        static_cast<int32>(Card), static_cast<int32>(FirstResource),
        FString::FromInt(static_cast<int32>(SecondResource)), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::BankTrade,
        static_cast<int32>(From), static_cast<int32>(To), FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::OfferTrade, 0, 0, FString(), Offered, Requested, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::AcceptTrade, 0, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(ECatanServerCommand::CancelTrade, 0, 0, FString(), {}, {}, Error);
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
    if (!HasAuthoritativeGame())
    {
        FString Error;
        RouteRemoteCommand(ECatanServerCommand::SelectBoardAction, static_cast<int32>(Action), 0,
            FString(), {}, {}, Error);
        return;
    }
    BoardAction = Action;
    StatusMessage = TEXT("Select a target on the board");
    OnGameStateChanged.Broadcast();
    PublishAuthoritativeState();
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
    if (bSucceeded) PublishAuthoritativeState();
    return bSucceeded;
}

bool UCatanGameSubsystem::RouteRemoteCommand(ECatanServerCommand Command, int32 First, int32 Second,
    const FString& Text, const FCatanResourceView& FirstResources,
    const FCatanResourceView& SecondResources, FString& Error)
{
    if (UWorld* World = GetWorld())
        if (ACatanPlayerController* Controller = Cast<ACatanPlayerController>(UGameplayStatics::GetPlayerController(World, 0)))
        {
            Controller->ServerExecuteCatanCommand(Command, First, Second, Text, FirstResources, SecondResources);
            Error.Reset();
            return true;
        }
    Error = TEXT("Not connected to the game host");
    return false;
}

void UCatanGameSubsystem::NotifyNetworkStateChanged()
{
    OnGameStateChanged.Broadcast();
}

void UCatanGameSubsystem::PublishAuthoritativeState()
{
    if (!Game || !GetWorld() || GetWorld()->GetNetMode() == NM_Client) return;
    ACatanGameState* State = GetWorld()->GetGameState<ACatanGameState>();
    if (!State) return;
    FCatanGameView Public = BuildAuthoritativeSnapshot();
    for (FCatanPlayerView& Player : Public.Players)
    {
        FCatanPrivatePlayerView Private;
        Private.Resources = Player.Resources;
        Private.DevelopmentCards = Player.DevelopmentCards;
        Private.Knights = Player.Knights;
        Private.RoadBuildingCards = Player.RoadBuildingCards;
        Private.YearOfPlentyCards = Player.YearOfPlentyCards;
        Private.MonopolyCards = Player.MonopolyCards;
        Player.Resources = {};
        Player.Knights = Player.RoadBuildingCards = Player.YearOfPlentyCards = Player.MonopolyCards = 0;
        for (APlayerState* BaseState : State->PlayerArray)
            if (ACatanPlayerState* PlayerState = Cast<ACatanPlayerState>(BaseState))
                if (PlayerState->GetPlayerName() == Player.Name)
                {
                    PlayerState->PrivateView = Private;
                    PlayerState->ForceNetUpdate();
                    break;
                }
    }
    State->PublicView = MoveTemp(Public);
    State->ForceNetUpdate();
    State->NotifyLocalProxy();
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
            int32 TotalDelta = 0;
            for (int32 Index = 0; Index < 5; ++Index)
                TotalDelta += After[Index] - Before[Index];
            if (TotalDelta != 0) AppendEvent(FString::Printf(TEXT("%s: %+d resource card%s"),
                *PlayerName, TotalDelta, FMath::Abs(TotalDelta) == 1 ? TEXT("") : TEXT("s")));
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
