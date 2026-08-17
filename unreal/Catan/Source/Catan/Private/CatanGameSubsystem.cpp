#include "CatanGameSubsystem.h"
#include "CatanBotStrategy.h"
#include "CatanTradePolicy.h"
#include "CatanInteractionPolicy.h"

#include "CatanGameState.h"
#include "CatanPlayerController.h"
#include "CatanPlayerState.h"
#include "CatanNetworkSubsystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformMisc.h"
#include "game_controller.hpp"

#include <sstream>

namespace
{
constexpr uint32 CatanLanSaveMagic = 0x43544e53; // CTNS
constexpr uint32 CatanLanSaveVersion = 1;

struct FCatanLanSaveEnvelope
{
    TArray<FString> PlayerNames;
    FString ActiveTradeTarget;
    FString StatusMessage;
    int32 PendingRobberHex = INDEX_NONE;
    TArray<FString> RobberVictims;
    TArray<FString> EventLog;
    TArray<uint8> CoreState;
};

bool ReadLanSaveEnvelope(const FString& Path, FCatanLanSaveEnvelope& Out, FString& Error)
{
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *Path))
    {
        Error = TEXT("No saved LAN game was found on this host");
        return false;
    }
    if (Bytes.Num() > 32 * 1024 * 1024)
    {
        Error = TEXT("Saved LAN game is too large");
        return false;
    }
    FMemoryReader Reader(Bytes, true);
    uint32 Magic = 0;
    uint32 Version = 0;
    Reader << Magic << Version;
    if (Magic != CatanLanSaveMagic || Version != CatanLanSaveVersion)
    {
        Error = TEXT("Saved LAN game has an unsupported format");
        return false;
    }
    Reader << Out.PlayerNames << Out.ActiveTradeTarget << Out.StatusMessage
        << Out.PendingRobberHex << Out.RobberVictims << Out.EventLog << Out.CoreState;
    if (Reader.IsError() || !Reader.AtEnd() || Out.PlayerNames.Num() < 2
        || Out.PlayerNames.Num() > 4 || Out.CoreState.IsEmpty())
    {
        Error = TEXT("Saved LAN game is damaged");
        return false;
    }
    TSet<FString> UniqueNames;
    for (const FString& Name : Out.PlayerNames)
    {
        const FString Key = Name.ToLower();
        if (Name.TrimStartAndEnd().IsEmpty() || UniqueNames.Contains(Key))
        {
            Error = TEXT("Saved LAN game has invalid player names");
            return false;
        }
        UniqueNames.Add(Key);
    }
    return true;
}

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

int32 CountPendingDevelopmentCards(const ivv::catan::Player& Player)
{
    using ivv::catan::DevelopmentCard;
    constexpr DevelopmentCard Cards[] = {DevelopmentCard::Knights, DevelopmentCard::RoadBuilding,
        DevelopmentCard::YearOfPlenty, DevelopmentCard::Monopoly};
    size_t Count = 0;
    for (DevelopmentCard Card : Cards) Count += Player.GetPurchasedCardCount(Card);
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

FCatanBotTopology BuildBotTopology(ivv::catan::GameController& Game)
{
    FCatanBotTopology Result;
    const auto& Hexes = Game.GetMap().GetGexes();
    auto Nodes = Game.GetMap().GetNodes();
    Result.NodeHexes.SetNum(static_cast<int32>(Nodes.size()));
    for (int32 NodeId = 0; NodeId < static_cast<int32>(Nodes.size()); ++NodeId)
        for (const ivv::catan::Gex* Hex : Nodes[NodeId].getNeighborGexs())
        {
            const ptrdiff_t HexId = Hex - Hexes.data();
            if (HexId >= 0 && HexId < static_cast<ptrdiff_t>(Hexes.size()))
                Result.NodeHexes[NodeId].Add(static_cast<int32>(HexId));
        }
    const auto Roads = Game.GetMap().GetFacets();
    Result.RoadNodes.SetNum(static_cast<int32>(Roads.size()));
    for (int32 RoadId = 0; RoadId < static_cast<int32>(Roads.size()); ++RoadId)
    {
        TArray<int32> Ends;
        for (const ivv::catan::Node* Node : Roads[RoadId].GetNeighborNodes()) Ends.Add(Node->index);
        Result.RoadNodes[RoadId] = Ends.Num() == 2 ? FIntPoint(Ends[0], Ends[1]) : FIntPoint(INDEX_NONE);
    }
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
    ActiveTradeTarget.Reset();
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
    if (View.ActiveDeal.bIsActive && IsBotPlayer(View.ActiveDeal.TargetPlayer))
    {
        BotTradeResponseDelay -= DeltaSeconds;
        if (BotTradeResponseDelay > 0.0f) return;
        FString Error;
        const FCatanPlayerView* Recipient = View.Players.FindByPredicate(
            [this](const FCatanPlayerView& Player) { return Player.Name == ActiveTradeTarget; });
        const FCatanPlayerView* Offerer = View.Players.FindByPredicate(
            [&View](const FCatanPlayerView& Player) { return Player.Name == View.ActiveDeal.OfferingPlayer; });
        const bool bAccept = Recipient && FCatanBotStrategy::ShouldAcceptTrade(*Recipient, Offerer,
            View.ActiveDeal.Offered, View.ActiveDeal.Requested, View);
        const auto& Deal = Game->GetActivDeal();
        try
        {
            const FString BotName = ActiveTradeTarget;
            if (bAccept) Game->SetDeal(TCHAR_TO_UTF8(*ActiveTradeTarget), Deal->buy, Deal->sell);
            else Game->CancelDeal(TCHAR_TO_UTF8(*ActiveTradeTarget));
            ActiveTradeTarget.Reset();
            CompleteCommand(true, FString::Printf(TEXT("%s %s the trade"), *BotName,
                bAccept ? TEXT("accepted") : TEXT("declined")), Error);
        }
        catch (const std::exception&)
        {
            const FString BotName = ActiveTradeTarget;
            Game->CancelDeal(TCHAR_TO_UTF8(*BotName));
            ActiveTradeTarget.Reset();
            CompleteCommand(true, FString::Printf(TEXT("%s declined the trade"), *BotName), Error);
        }
        return;
    }
    if (View.ActiveDeal.bIsActive) return;
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
    if (const UCatanNetworkSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>() : nullptr;
        Network && Network->IsDedicatedActive() && Network->IsDedicatedPlaying())
        return Network->GetDedicatedView();
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
                    Player->PendingDevelopmentCards = PlayerState->PrivateView.PendingDevelopmentCards;
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
        Player.PendingDevelopmentCards = 0;
    }
    return View;
}

bool UCatanGameSubsystem::HasAuthoritativeGame() const
{
    if (const UCatanNetworkSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>() : nullptr;
        Network && Network->IsDedicatedActive()) return false;
    const UWorld* World = GetWorld();
    return Game != nullptr && (!World || World->GetNetMode() != NM_Client);
}

FString UCatanGameSubsystem::LanSavePath() const
{
    FString Override;
    if (FParse::Value(FCommandLine::Get(), TEXT("CatanSaveFile="), Override)
        && !Override.TrimStartAndEnd().IsEmpty())
        return FPaths::ConvertRelativePathToFull(Override);
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("SaveGames"), TEXT("lan-host.catan"));
}

bool UCatanGameSubsystem::HasLanSavedGame() const
{
    return IFileManager::Get().FileExists(*LanSavePath());
}

bool UCatanGameSubsystem::GetLanSavedPlayerNames(TArray<FString>& Names, FString& Error) const
{
    FCatanLanSaveEnvelope Envelope;
    if (!ReadLanSaveEnvelope(LanSavePath(), Envelope, Error)) return false;
    Names = MoveTemp(Envelope.PlayerNames);
    Error.Reset();
    return true;
}

bool UCatanGameSubsystem::SaveLanGame(FString& Error) const
{
    if (!Game || PlayerNames.Num() < 2)
    {
        Error = TEXT("There is no LAN game to save");
        return false;
    }
    try
    {
        const std::string Core = Game->SerializeState();
        FCatanLanSaveEnvelope Envelope;
        Envelope.PlayerNames = PlayerNames;
        Envelope.ActiveTradeTarget = ActiveTradeTarget;
        Envelope.StatusMessage = StatusMessage;
        Envelope.PendingRobberHex = PendingRobberHex;
        Envelope.RobberVictims = RobberVictims;
        Envelope.EventLog = EventLog;
        Envelope.CoreState.Append(reinterpret_cast<const uint8*>(Core.data()),
            static_cast<int32>(Core.size()));

        FBufferArchive Writer;
        uint32 Magic = CatanLanSaveMagic;
        uint32 Version = CatanLanSaveVersion;
        Writer << Magic << Version << Envelope.PlayerNames << Envelope.ActiveTradeTarget
            << Envelope.StatusMessage << Envelope.PendingRobberHex << Envelope.RobberVictims
            << Envelope.EventLog << Envelope.CoreState;

        const FString Path = LanSavePath();
        const FString Directory = FPaths::GetPath(Path);
        IFileManager::Get().MakeDirectory(*Directory, true);
        const FString Temporary = Path + TEXT(".tmp");
        if (!FFileHelper::SaveArrayToFile(Writer, *Temporary)
            || !IFileManager::Get().Move(*Path, *Temporary, true, true, false, true))
        {
            IFileManager::Get().Delete(*Temporary, false, true);
            Error = TEXT("Could not write the LAN saved game");
            return false;
        }
        UE_LOG(LogTemp, Display, TEXT("CATAN_SAVE wrote path=%s bytes=%d players=%d"),
            *Path, Writer.Num(), PlayerNames.Num());
        Error.Reset();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = UTF8_TO_TCHAR(Exception.what());
        return false;
    }
}

bool UCatanGameSubsystem::LoadLanSavedGame(FString& Error)
{
    FCatanLanSaveEnvelope Envelope;
    if (!ReadLanSaveEnvelope(LanSavePath(), Envelope, Error)) return false;
    try
    {
        const std::string Core(reinterpret_cast<const char*>(Envelope.CoreState.GetData()),
            static_cast<size_t>(Envelope.CoreState.Num()));
        std::unique_ptr<ivv::catan::GameController> Restored =
            ivv::catan::GameController::DeserializeState(Core);
        TSet<FString> SavedNames;
        for (const FString& Name : Envelope.PlayerNames) SavedNames.Add(Name.ToLower());
        TSet<FString> CoreNames;
        for (const std::string& Name : Restored->GetPlayerNames())
            CoreNames.Add(FString(UTF8_TO_TCHAR(Name.c_str())).ToLower());
        bool bNamesMatch = SavedNames.Num() == CoreNames.Num();
        for (const FString& Name : SavedNames) bNamesMatch = bNamesMatch && CoreNames.Contains(Name);
        if (!bNamesMatch)
            throw std::invalid_argument("Saved lobby names do not match the game state");

        Game = std::move(Restored);
        PlayerNames = MoveTemp(Envelope.PlayerNames);
        ActiveTradeTarget = MoveTemp(Envelope.ActiveTradeTarget);
        StatusMessage = Envelope.StatusMessage.IsEmpty()
            ? TEXT("Saved LAN game restored") : MoveTemp(Envelope.StatusMessage);
        PendingRobberHex = Envelope.PendingRobberHex;
        RobberVictims = MoveTemp(Envelope.RobberVictims);
        EventLog = MoveTemp(Envelope.EventLog);
        BoardAction = ECatanBoardAction::Automatic;
        PendingBuildAction = ECatanBoardAction::Automatic;
        PendingBuildTargetId = INDEX_NONE;
        BotPlayers.Reset();
        LastResources.Reset();
        LastLargestArmy.Reset();
        LastLongestRoad.Reset();
        CaptureResourceChanges();
        AppendEvent(TEXT("Saved LAN game restored"));
        const FString CurrentPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
        UE_LOG(LogTemp, Display, TEXT("CATAN_SAVE restored path=%s players=%d current=%s phase=%d"),
            *LanSavePath(), PlayerNames.Num(), *CurrentPlayer, static_cast<int32>(Game->GetStep()));
        Error.Reset();
        OnGameStateChanged.Broadcast();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = FString::Printf(TEXT("Could not restore saved LAN game: %s"),
            UTF8_TO_TCHAR(Exception.what()));
        return false;
    }
}

bool UCatanGameSubsystem::CanLocalPlayerAct(const FCatanGameView& View) const
{
    if (const UCatanNetworkSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>() : nullptr;
        Network && Network->IsDedicatedActive())
        return !View.CurrentPlayer.IsEmpty() && View.CurrentPlayer == Network->GetDedicatedPlayerName();
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
    if (BotTurnPlayer != View.CurrentPlayer)
    {
        BotTurnPlayer = View.CurrentPlayer;
        bBotDevelopmentAttempted = false;
        bBotTradeAttempted = false;
    }
    UE_LOG(LogTemp, Display, TEXT("CATAN_BOT turn=%s phase=%d nodes=%d roads=%d hexes=%d"),
        *View.CurrentPlayer, static_cast<int32>(View.Phase), View.ValidNodeTargets.Num(),
        View.ValidRoadTargets.Num(), View.ValidHexTargets.Num());
    FString Error;
    const FCatanBotTopology Topology = BuildBotTopology(*Game);
    const FCatanPlayerView* Player = View.Players.FindByPredicate(
        [&View](const FCatanPlayerView& Item) { return Item.Name == View.CurrentPlayer; });
    const int32 PlayerId = Player ? Player->Id : INDEX_NONE;

    if (View.PendingRobberHex != INDEX_NONE && !View.RobberVictims.IsEmpty())
    {
        TryChooseRobberVictim(FCatanBotStrategy::ChooseRobberVictim(View), Error);
        return;
    }
    switch (View.Phase)
    {
    case ECatanGamePhase::SetupSettlement:
        if (const int32 Target = FCatanBotStrategy::ChooseSettlement(
            View, Topology, View.ValidNodeTargets, PlayerId); Target != INDEX_NONE)
            TryBuildSettlement(Target, Error);
        return;
    case ECatanGamePhase::SetupRoad:
    case ECatanGamePhase::RoadBuilding:
        if (const int32 Target = FCatanBotStrategy::ChooseRoad(
            View, Topology, View.ValidRoadTargets, PlayerId); Target != INDEX_NONE)
            TryBuildRoad(Target, Error);
        else if (View.Phase == ECatanGamePhase::RoadBuilding)
            TryPass(Error);
        return;
    case ECatanGamePhase::RollDice:
        if (Player && Player->Knights > 0 && !bBotDevelopmentAttempted)
        {
            bBotDevelopmentAttempted = true;
            if (TryUseDevelopmentCard(ECatanDevelopmentCard::Knight,
                ECatanResource::Wood, ECatanResource::Clay, Error)) return;
        }
        TryRollDice(Error);
        return;
    case ECatanGamePhase::DropCards:
        {
            if (!Player) return;
            TryDropResources(FCatanBotStrategy::ChooseDiscard(*Player,
                View.RequiredDiscardCount, View), Error);
        }
        return;
    case ECatanGamePhase::MoveRobber:
        if (const int32 Target = FCatanBotStrategy::ChooseRobberHex(
            View, Topology, View.ValidHexTargets, PlayerId); Target != INDEX_NONE)
            TryMoveRobber(Target, Error);
        return;
    case ECatanGamePhase::CommonPlay:
        break;
    case ECatanGamePhase::Finished:
        return;
    }

    if (View.BoardAction == ECatanBoardAction::BuildCity)
    {
        if (const int32 Target = FCatanBotStrategy::ChooseCity(
            View, Topology, View.ValidNodeTargets, PlayerId); Target != INDEX_NONE)
            TryBuildCity(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }
    if (View.BoardAction == ECatanBoardAction::BuildSettlement)
    {
        if (const int32 Target = FCatanBotStrategy::ChooseSettlement(
            View, Topology, View.ValidNodeTargets, PlayerId); Target != INDEX_NONE)
            TryBuildSettlement(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }
    if (View.BoardAction == ECatanBoardAction::BuildRoad)
    {
        if (const int32 Target = FCatanBotStrategy::ChooseRoad(
            View, Topology, View.ValidRoadTargets, PlayerId); Target != INDEX_NONE)
            TryBuildRoad(Target, Error);
        else BoardAction = ECatanBoardAction::Automatic;
        return;
    }

    if (!Player) return;
    const FCatanResourceView& Have = Player->Resources;
    if (Player->FreeCities > 0 && View.bHasCityTarget && Have.Hay >= 2 && Have.Stone >= 3)
    {
        SelectBoardAction(ECatanBoardAction::BuildCity);
        return;
    }
    if (Player->FreeSettlements > 0 && Have.Wood > 0 && Have.Clay > 0
        && Have.Hay > 0 && Have.Sheep > 0 && View.bHasSettlementTarget)
    {
        SelectBoardAction(ECatanBoardAction::BuildSettlement);
        return;
    }
    if (!bBotDevelopmentAttempted)
    {
        if (Player->RoadBuildingCards > 0 && Player->FreeRoads > 0 && View.bHasRoadTarget)
        {
            bBotDevelopmentAttempted = true;
            if (TryUseDevelopmentCard(ECatanDevelopmentCard::RoadBuilding,
                ECatanResource::Wood, ECatanResource::Clay, Error)) return;
        }
        if (Player->YearOfPlentyCards > 0)
        {
            const auto Resources = FCatanBotStrategy::ChooseYearOfPlenty(*Player, View);
            bBotDevelopmentAttempted = true;
            if (TryUseDevelopmentCard(ECatanDevelopmentCard::YearOfPlenty,
                Resources.Key, Resources.Value, Error)) return;
        }
        if (Player->MonopolyCards > 0)
        {
            const ECatanResource Resource = FCatanBotStrategy::ChooseMonopoly(View, PlayerId);
            if (FCatanBotStrategy::MonopolyGain(View, PlayerId, Resource) >= 3)
            {
                bBotDevelopmentAttempted = true;
                if (TryUseDevelopmentCard(ECatanDevelopmentCard::Monopoly,
                    Resource, ECatanResource::Wood, Error)) return;
            }
        }
        if (Player->Knights > 0 && !Player->bHasLargestArmy)
        {
            bBotDevelopmentAttempted = true;
            if (TryUseDevelopmentCard(ECatanDevelopmentCard::Knight,
                ECatanResource::Wood, ECatanResource::Clay, Error)) return;
        }
    }

    if (const FCatanBotBankTrade Trade = FCatanBotStrategy::ChooseBankTrade(*Player, View);
        Trade.bValid && TryBankTrade(Trade.From, Trade.To, Error)) return;

    if (!bBotTradeAttempted)
    {
        bBotTradeAttempted = true;
        const FCatanBotPlayerTrade Trade = FCatanBotStrategy::ChoosePlayerTrade(*Player, View);
        const FCatanPlayerView* Target = nullptr;
        for (const FCatanPlayerView& Candidate : View.Players)
        {
            if (Candidate.Id == PlayerId || Candidate.VictoryPoints >= 9 || Candidate.ResourceCards <= 0) continue;
            if (!Target || Candidate.VictoryPoints < Target->VictoryPoints
                || (Candidate.VictoryPoints == Target->VictoryPoints
                    && Candidate.ResourceCards > Target->ResourceCards))
                Target = &Candidate;
        }
        if (Trade.bValid && Target
            && TryOfferTrade(Trade.Offered, Trade.Requested, Target->Name, Error)) return;
    }

    const int32 TotalResources = Have.Wood + Have.Clay + Have.Hay + Have.Sheep + Have.Stone;
    const bool bNearlyCity = View.bHasCityTarget && Have.Hay >= 1 && Have.Stone >= 2;
    const bool bNearlySettlement = View.bHasSettlementTarget
        && Have.Wood + Have.Clay + Have.Hay + Have.Sheep >= 3;
    if (Have.Hay > 0 && Have.Sheep > 0 && Have.Stone > 0
        && (!bNearlyCity && !bNearlySettlement || TotalResources >= 8))
    {
        if (TryBuyDevelopmentCard(Error)) return;
    }
    if (Player->FreeRoads > 0 && View.bHasRoadTarget && Have.Wood > 0 && Have.Clay > 0
        && (!View.bHasSettlementTarget || TotalResources >= 7))
    {
        SelectBoardAction(ECatanBoardAction::BuildRoad);
        return;
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
        View.ActiveDeal.TargetPlayer = ActiveTradeTarget;
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
        PlayerView.PendingDevelopmentCards = CountPendingDevelopmentCards(Player);
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
    if (View.Phase == ECatanGamePhase::CommonPlay)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Nodes.Num()); ++Index)
        {
            View.bHasSettlementTarget = View.bHasSettlementTarget || Game->CanBuildSettlement(Index);
            View.bHasCityTarget = View.bHasCityTarget || Game->CanBuildCastle(Index);
            if (View.bHasSettlementTarget && View.bHasCityTarget) break;
        }
    }
    else if (View.Phase == ECatanGamePhase::SetupSettlement)
    {
        View.bHasSettlementTarget = !View.ValidNodeTargets.IsEmpty();
    }
    const bool bRoadTargets = View.Phase == ECatanGamePhase::SetupRoad
        || View.Phase == ECatanGamePhase::RoadBuilding
        || (View.Phase == ECatanGamePhase::CommonPlay && BoardAction == ECatanBoardAction::BuildRoad);
    if (bRoadTargets)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Roads.Num()); ++Index)
            if (Game->CanBuildRoad(Index)) View.ValidRoadTargets.Add(Index);
    }
    if (View.Phase == ECatanGamePhase::CommonPlay)
    {
        for (int32 Index = 0; Index < static_cast<int32>(View.Roads.Num()); ++Index)
            if (Game->CanBuildRoad(Index))
            {
                View.bHasRoadTarget = true;
                break;
            }
    }
    else if (View.Phase == ECatanGamePhase::SetupRoad || View.Phase == ECatanGamePhase::RoadBuilding)
    {
        View.bHasRoadTarget = !View.ValidRoadTargets.IsEmpty();
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
        PublishAuthoritativeState();
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
    const FCatanResourceView& Requested, const FString& TargetPlayer, FString& Error)
{
    if (!HasAuthoritativeGame()) return RouteRemoteCommand(
        ECatanServerCommand::OfferTrade, 0, 0, TargetPlayer, Offered, Requested, Error);
    const FString OfferingPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
    if (TargetPlayer.IsEmpty() || TargetPlayer == OfferingPlayer || !PlayerNames.Contains(TargetPlayer))
        return CompleteCommand(false, TEXT("Choose another player to receive the offer"), Error);
    try
    {
        Game->SetDeal(Game->GetCurrentPlayer(), ToResourceMap(Offered), ToResourceMap(Requested));
        ActiveTradeTarget = TargetPlayer;
        if (IsBotPlayer(TargetPlayer))
            BotTradeResponseDelay = bBotAutoplay ? 0.05f : 0.75f;
        return CompleteCommand(true,
            FString::Printf(TEXT("Trade offered to %s"), *TargetPlayer), Error);
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
    if (!CatanTradePolicy::CanAccept(Player, ActiveTradeTarget))
        return CompleteCommand(false, TEXT("Only the selected recipient can accept this trade"), Error);
    try
    {
        Game->SetDeal(TCHAR_TO_UTF8(*Player), Deal->buy, Deal->sell);
        ActiveTradeTarget.Reset();
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
        const FString OfferingPlayer = UTF8_TO_TCHAR(Game->GetCurrentPlayer().c_str());
        if (!CatanTradePolicy::CanCancel(Player, OfferingPlayer, ActiveTradeTarget))
            return CompleteCommand(false, TEXT("Only the offerer or recipient can cancel this trade"), Error);
        Game->CancelDeal(TCHAR_TO_UTF8(*Player));
        ActiveTradeTarget.Reset();
        return CompleteCommand(true, TEXT("Trade cancelled"), Error);
    }
    catch (const std::exception& Exception)
    {
        return CompleteCommand(false, UTF8_TO_TCHAR(Exception.what()), Error);
    }
}

void UCatanGameSubsystem::SelectBoardAction(ECatanBoardAction Action)
{
    PendingBuildAction = ECatanBoardAction::Automatic;
    PendingBuildTargetId = INDEX_NONE;
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

bool UCatanGameSubsystem::SelectPendingBuildTarget(ECatanBoardAction Action, int32 TargetId,
    FString& Error)
{
    const FCatanGameView View = GetSnapshot();
    const bool bValidTarget = CatanInteractionPolicy::CanSelectBuildTarget(View, Action, TargetId);
    if (!CanLocalPlayerAct(View) || !bValidTarget)
    {
        Error = TEXT("The selected build target is no longer available");
        return false;
    }
    PendingBuildAction = Action;
    PendingBuildTargetId = TargetId;
    Error.Reset();
    OnGameStateChanged.Broadcast();
    return true;
}

bool UCatanGameSubsystem::ConfirmPendingBuildTarget(FString& Error)
{
    if (!HasPendingBuildTarget())
    {
        Error = TEXT("No build target is selected");
        return false;
    }
    const ECatanBoardAction Action = PendingBuildAction;
    const int32 TargetId = PendingBuildTargetId;
    const FCatanGameView View = GetSnapshot();
    const bool bStillValid = CatanInteractionPolicy::CanSelectBuildTarget(View, Action, TargetId);
    if (!CanLocalPlayerAct(View) || !bStillValid)
    {
        CancelPendingBuildTarget();
        Error = TEXT("The selected build target is no longer available");
        return false;
    }

    bool bSucceeded = false;
    switch (Action)
    {
    case ECatanBoardAction::BuildSettlement: bSucceeded = TryBuildSettlement(TargetId, Error); break;
    case ECatanBoardAction::BuildRoad: bSucceeded = TryBuildRoad(TargetId, Error); break;
    case ECatanBoardAction::BuildCity: bSucceeded = TryBuildCity(TargetId, Error); break;
    default: Error = TEXT("Unsupported build action"); break;
    }
    PendingBuildAction = ECatanBoardAction::Automatic;
    PendingBuildTargetId = INDEX_NONE;
    OnGameStateChanged.Broadcast();
    return bSucceeded;
}

void UCatanGameSubsystem::CancelPendingBuildTarget()
{
    if (!HasPendingBuildTarget()) return;
    PendingBuildAction = ECatanBoardAction::Automatic;
    PendingBuildTargetId = INDEX_NONE;
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
    if (bSucceeded) PublishAuthoritativeState();
    return bSucceeded;
}

bool UCatanGameSubsystem::RouteRemoteCommand(ECatanServerCommand Command, int32 First, int32 Second,
    const FString& Text, const FCatanResourceView& FirstResources,
    const FCatanResourceView& SecondResources, FString& Error)
{
    if (UCatanNetworkSubsystem* Network = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>() : nullptr;
        Network && Network->IsDedicatedActive())
        return Network->SendDedicatedCommand(Command, First, Second, Text,
            FirstResources, SecondResources, Error);
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
        Private.PendingDevelopmentCards = Player.PendingDevelopmentCards;
        Player.Resources = {};
        Player.Knights = Player.RoadBuildingCards = Player.YearOfPlentyCards = Player.MonopolyCards = 0;
        Player.PendingDevelopmentCards = 0;
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
    if (GetWorld()->GetNetMode() == NM_ListenServer
        && State->NetworkMode == ECatanNetworkMode::Playing)
    {
        FString SaveError;
        if (!SaveLanGame(SaveError))
            UE_LOG(LogTemp, Error, TEXT("CATAN_SAVE failed: %s"), *SaveError);
    }
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
