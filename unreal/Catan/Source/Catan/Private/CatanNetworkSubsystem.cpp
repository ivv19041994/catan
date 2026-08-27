#include "CatanNetworkSubsystem.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Sockets.h"
#include "Async/Async.h"
#include "CatanGameMode.h"
#include "CatanGameSubsystem.h"
#include "dedicated_protocol.hpp"

DEFINE_LOG_CATEGORY_STATIC(LogCatanLan, Log, All);

namespace
{
const FName CatanLobbyKey(TEXT("CATAN_LOBBY"));
constexpr int32 CatanDiscoveryPort = 15001;
constexpr ANSICHAR DiscoveryQuery[] = "CATAN_DISCOVER_V1";
constexpr TCHAR DiscoveryResponsePrefix[] = TEXT("CATAN_LOBBY_V1\t");

FString ToFString(const std::string& Value)
{
    return FString(UTF8_TO_TCHAR(Value.c_str()));
}

std::string ToUtf8(const FString& Value)
{
    FTCHARToUTF8 Converted(*Value);
    return std::string(Converted.Get(), Converted.Length());
}

FString EncodeField(const FString& Value)
{
    return ToFString(ivv::catan::dedicated::protocol::HexEncode(ToUtf8(Value)));
}

FString ResourceField(const FCatanResourceView& Value)
{
    return FString::Printf(TEXT("%d,%d,%d,%d,%d"), Value.Wood, Value.Clay,
        Value.Hay, Value.Sheep, Value.Stone);
}
}

void UCatanNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ConfigureLanAdapter();
    Status = TEXT("Ready to host or join a game");
    if (GEngine)
    {
        NetworkFailureHandle = GEngine->OnNetworkFailure().AddUObject(
            this, &UCatanNetworkSubsystem::HandleNetworkFailure);
        TravelFailureHandle = GEngine->OnTravelFailure().AddUObject(
            this, &UCatanNetworkSubsystem::HandleTravelFailure);
    }

    FString AutoName = TEXT("Automation");
    FParse::Value(FCommandLine::Get(), TEXT("CatanAutoName="), AutoName);
    if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoHostSavedLobby")))
    {
        TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, AutoName](float)
        {
            if (WeakThis.IsValid()) WeakThis->HostSavedLobby(AutoName);
            return false;
        }), 2.0f);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoHostLobby")))
    {
        TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, AutoName](float)
        {
            if (WeakThis.IsValid()) WeakThis->HostLobby(AutoName, TEXT("Automated LAN Lobby"));
            return false;
        }), 2.0f);
    }
    else if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoFindJoin")))
    {
        bAutoJoinDiscovered = true;
        PendingPlayerName = AutoName;
        TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
        {
            if (WeakThis.IsValid()) WeakThis->FindLobbies();
            return false;
        }), 5.0f);
    }
    else
    {
        FString AutoAddress;
        if (FParse::Value(FCommandLine::Get(), TEXT("CatanAutoManualJoin="), AutoAddress))
        {
            // Keep the requested identity available while ClientTravel replaces the
            // world. The new local controller can begin play before the delayed
            // auto-join callback runs again on some platforms.
            PendingPlayerName = AutoName;
            TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, AutoAddress, AutoName](float)
            {
                if (WeakThis.IsValid()) WeakThis->JoinManual(AutoAddress, AutoName);
                return false;
            }), 5.0f);
        }
    }

    FString DedicatedServerAddress;
    if (FParse::Value(FCommandLine::Get(), TEXT("CatanDedicatedAddress="), DedicatedServerAddress))
    {
        FString DedicatedLobbyName = TEXT("Automated dedicated lobby");
        FParse::Value(FCommandLine::Get(), TEXT("CatanDedicatedLobbyName="), DedicatedLobbyName);
        FString JoinToken;
        FParse::Value(FCommandLine::Get(), TEXT("CatanDedicatedJoin="), JoinToken);
        bDedicatedAutoReady = FParse::Param(FCommandLine::Get(), TEXT("CatanDedicatedAutoReady"));
        bDedicatedE2E = FParse::Param(FCommandLine::Get(), TEXT("CatanDedicatedE2E"));
        FParse::Value(FCommandLine::Get(), TEXT("CatanDedicatedAutoStart="), DedicatedAutoStartPlayers);
        TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
            [WeakThis, DedicatedServerAddress, DedicatedLobbyName, JoinToken, AutoName](float)
            {
                if (!WeakThis.IsValid()) return false;
                if (JoinToken.IsEmpty())
                    WeakThis->CreateDedicatedLobby(DedicatedServerAddress, AutoName, DedicatedLobbyName);
                else
                    WeakThis->JoinDedicatedLobby(DedicatedServerAddress, JoinToken, AutoName);
                return false;
            }), 2.0f);
    }
}

void UCatanNetworkSubsystem::Deinitialize()
{
    if (GEngine)
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
        GEngine->OnTravelFailure().Remove(TravelFailureHandle);
    }
    ResetDedicatedConnection();
    StopDiscoverySockets();
    if (IOnlineSubsystem* Online = IOnlineSubsystem::Get())
        if (IOnlineSessionPtr Sessions = Online->GetSessionInterface(); Sessions.IsValid())
        {
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
            Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
            Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        }
    Search.Reset();
    Super::Deinitialize();
}

bool UCatanNetworkSubsystem::ParseDedicatedAddress(const FString& Address)
{
    FString Value = Address.TrimStartAndEnd();
    Value.RemoveFromStart(TEXT("tcp://"));
    FString Host = Value;
    FString PortText;
    if (Value.Split(TEXT(":"), &Host, &PortText, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
    {
        DedicatedPort = FCString::Atoi(*PortText);
        if (DedicatedPort < 1 || DedicatedPort > 65535) return false;
    }
    else DedicatedPort = 17777;
    Host = Host.TrimStartAndEnd();
    if (Host.IsEmpty()) return false;
    DedicatedHost = Host;
    DedicatedAddress = FString::Printf(TEXT("%s:%d"), *Host, DedicatedPort);
    return true;
}

void UCatanNetworkSubsystem::SendDedicatedRequest(const FString& Request,
    TFunction<void(const TArray<FString>&)> OnSuccess)
{
    if (bDedicatedRequestInFlight)
    {
        Status = TEXT("Waiting for the dedicated server...");
        OnNetworkChanged.Broadcast();
        return;
    }
    bDedicatedRequestInFlight = true;
    const FString Host = DedicatedHost;
    const int32 Port = DedicatedPort;
    const uint64 Generation = DedicatedGeneration;
    TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
    Async(EAsyncExecution::ThreadPool, [WeakThis, Host, Port, Generation, Request, OnSuccess = MoveTemp(OnSuccess)]() mutable
    {
        FString Response;
        FString Failure;
        ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
        bool bValidAddress = false;
        TSharedRef<FInternetAddr> Address = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        Address->SetIp(*Host, bValidAddress);
        Address->SetPort(Port);
        FSocket* Socket = bValidAddress
            ? Sockets->CreateSocket(NAME_Stream, TEXT("Catan dedicated client"), FNetworkProtocolTypes::IPv4)
            : nullptr;
        if (!Socket || !Socket->Connect(*Address)) Failure = FString::Printf(TEXT("Could not connect to %s:%d"), *Host, Port);
        if (Socket && Failure.IsEmpty())
        {
            const FString Line = Request + TEXT("\n");
            FTCHARToUTF8 Utf8(*Line);
            int32 TotalSent = 0;
            while (TotalSent < Utf8.Length())
            {
                int32 Sent = 0;
                if (!Socket->Send(reinterpret_cast<const uint8*>(Utf8.Get()) + TotalSent,
                    Utf8.Length() - TotalSent, Sent) || Sent <= 0) { Failure = TEXT("Dedicated server send failed"); break; }
                TotalSent += Sent;
            }
            TArray<uint8> Bytes;
            while (Failure.IsEmpty() && Bytes.Num() <= 1024 * 1024)
            {
                uint8 Buffer[4096]; int32 Read = 0;
                if (!Socket->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0) break;
                Bytes.Append(Buffer, Read);
                if (Bytes.Contains(static_cast<uint8>('\n'))) break;
            }
            Bytes.Add(0);
            Response = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()));
            Response.TrimEndInline();
            if (Response.IsEmpty() && Failure.IsEmpty()) Failure = TEXT("Dedicated server returned no response");
        }
        if (Socket) { Socket->Close(); Sockets->DestroySocket(Socket); }
        AsyncTask(ENamedThreads::GameThread, [WeakThis, Generation, Response, Failure, OnSuccess = MoveTemp(OnSuccess)]() mutable
        {
            if (!WeakThis.IsValid() || WeakThis->DedicatedGeneration != Generation) return;
            WeakThis->bDedicatedRequestInFlight = false;
            auto ReportFailure = [WeakThis](const FString& Message)
            {
                UE_LOG(LogCatanLan, Warning,
                    TEXT("CATAN_HUD_GRAPH request-failure dedicatedActive=%d leaving=%d error=%s"),
                    WeakThis->bDedicatedActive, WeakThis->bLeaveInProgress, *Message);
                if (WeakThis->bLeaveInProgress)
                {
                    WeakThis->ReturnToMenuStatus = FString::Printf(
                        TEXT("Left locally; server leave failed: %s"), *Message);
                    WeakThis->CompleteReturnToMenu();
                    return;
                }
                if (WeakThis->bDedicatedActive
                    && (Message.Contains(TEXT("Lobby token is invalid"))
                        || Message.Contains(TEXT("Player token is invalid"))))
                {
                    WeakThis->bLeaveInProgress = true;
                    WeakThis->ReturnToMenuStatus = FString::Printf(
                        TEXT("Dedicated lobby closed: %s"), *Message);
                    WeakThis->CompleteReturnToMenu();
                    return;
                }
                WeakThis->Status = Message;
                if (WeakThis->bDedicatedPlaying) WeakThis->DedicatedView.StatusMessage = Message;
                WeakThis->OnNetworkChanged.Broadcast();
                if (UCatanGameSubsystem* GameSubsystem = WeakThis->GetGameInstance()
                    ? WeakThis->GetGameInstance()->GetSubsystem<UCatanGameSubsystem>() : nullptr)
                    GameSubsystem->NotifyNetworkStateChanged();
            };
            if (!Failure.IsEmpty())
            {
                ReportFailure(Failure);
                return;
            }
            TArray<FString> Fields;
            Response.ParseIntoArray(Fields, TEXT("\t"), false);
            if (Fields.Num() >= 2 && Fields[0] == TEXT("ERR"))
            {
                const auto Message = ivv::catan::dedicated::protocol::HexDecode(ToUtf8(Fields[1]));
                ReportFailure(Message ? ToFString(*Message) : TEXT("Dedicated server rejected the request"));
                return;
            }
            if (Fields.IsEmpty() || Fields[0] != TEXT("OK"))
            {
                ReportFailure(TEXT("Malformed dedicated server response"));
                return;
            }
            OnSuccess(Fields);
        });
    });
}

void UCatanNetworkSubsystem::CreateDedicatedLobby(const FString& Address,
    const FString& PlayerName, const FString& LobbyName)
{
    ResetDedicatedConnection();
    if (!ParseDedicatedAddress(Address)) { Status = TEXT("Enter server IP and optional port"); OnNetworkChanged.Broadcast(); return; }
    Status = FString::Printf(TEXT("Connecting to dedicated server %s..."), *DedicatedAddress);
    OnNetworkChanged.Broadcast();
    SendDedicatedRequest(FString::Printf(TEXT("CREATE\t%s\t%s"), *EncodeField(PlayerName), *EncodeField(LobbyName)),
        [this](const TArray<FString>& Fields)
        {
            if (Fields.Num() != 5 || Fields[1] != TEXT("CREATED")) { Status = TEXT("Malformed create response"); OnNetworkChanged.Broadcast(); return; }
            DedicatedLobbyToken = Fields[2]; DedicatedPlayerToken = Fields[3];
            const auto Name = ivv::catan::dedicated::protocol::HexDecode(ToUtf8(Fields[4]));
            DedicatedPlayerName = Name ? ToFString(*Name) : TEXT("Player");
            bDedicatedActive = true;
            UE_LOG(LogCatanLan, Display, TEXT("CATAN_DEDICATED_CREATED lobby=%s name=%s"),
                *DedicatedLobbyToken, *DedicatedPlayerName);
            Status = TEXT("Dedicated lobby created. Share the lobby token.");
            DedicatedPollTicker = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &UCatanNetworkSubsystem::TickDedicatedPoll), 0.25f);
            PollDedicatedSnapshot(); OnNetworkChanged.Broadcast();
        });
}

void UCatanNetworkSubsystem::JoinDedicatedLobby(const FString& Address,
    const FString& LobbyToken, const FString& PlayerName)
{
    ResetDedicatedConnection();
    if (!ParseDedicatedAddress(Address)) { Status = TEXT("Enter server IP and optional port"); OnNetworkChanged.Broadcast(); return; }
    const FString CleanToken = LobbyToken.TrimStartAndEnd().ToUpper();
    Status = FString::Printf(TEXT("Joining dedicated lobby on %s..."), *DedicatedAddress);
    OnNetworkChanged.Broadcast();
    SendDedicatedRequest(FString::Printf(TEXT("JOIN\t%s\t%s"), *CleanToken, *EncodeField(PlayerName)),
        [this](const TArray<FString>& Fields)
        {
            if (Fields.Num() != 5 || Fields[1] != TEXT("JOINED")) { Status = TEXT("Malformed join response"); OnNetworkChanged.Broadcast(); return; }
            DedicatedLobbyToken = Fields[2]; DedicatedPlayerToken = Fields[3];
            const auto Name = ivv::catan::dedicated::protocol::HexDecode(ToUtf8(Fields[4]));
            DedicatedPlayerName = Name ? ToFString(*Name) : TEXT("Player");
            bDedicatedActive = true;
            UE_LOG(LogCatanLan, Display, TEXT("CATAN_DEDICATED_JOINED lobby=%s name=%s"),
                *DedicatedLobbyToken, *DedicatedPlayerName);
            Status = TEXT("Joined dedicated lobby");
            DedicatedPollTicker = FTSTicker::GetCoreTicker().AddTicker(
                FTickerDelegate::CreateUObject(this, &UCatanNetworkSubsystem::TickDedicatedPoll), 0.25f);
            PollDedicatedSnapshot(); OnNetworkChanged.Broadcast();
        });
}

void UCatanNetworkSubsystem::SetDedicatedReady(bool bReady)
{
    if (!bDedicatedActive) return;
    SendDedicatedRequest(FString::Printf(TEXT("READY\t%s\t%s\t%d"),
        *DedicatedLobbyToken, *DedicatedPlayerToken, bReady ? 1 : 0),
        [this](const TArray<FString>&) { PollDedicatedSnapshot(); });
}

void UCatanNetworkSubsystem::StartDedicatedGame()
{
    if (!bDedicatedActive) return;
    SendDedicatedRequest(FString::Printf(TEXT("START\t%s\t%s"), *DedicatedLobbyToken, *DedicatedPlayerToken),
        [this](const TArray<FString>&) { PollDedicatedSnapshot(); });
}

bool UCatanNetworkSubsystem::TickDedicatedPoll(float)
{
    if (!bDedicatedActive) return false;
    PollDedicatedSnapshot();
    return true;
}

void UCatanNetworkSubsystem::PollDedicatedSnapshot()
{
    if (!bDedicatedActive || bDedicatedRequestInFlight) return;
    SendDedicatedRequest(FString::Printf(TEXT("SNAPSHOT\t%s\t%s"), *DedicatedLobbyToken, *DedicatedPlayerToken),
        [this](const TArray<FString>& Fields)
        {
            if (Fields.Num() == 3 && Fields[1] == TEXT("SNAPSHOT")) ApplyDedicatedSnapshot(Fields[2]);
        });
}

void UCatanNetworkSubsystem::ApplyDedicatedSnapshot(const FString& EncodedPayload)
{
    const auto Payload = ivv::catan::dedicated::protocol::HexDecode(ToUtf8(EncodedPayload));
    std::string ParseError;
    const auto Snapshot = Payload
        ? ivv::catan::dedicated::protocol::DeserializeSnapshot(*Payload, ParseError) : std::nullopt;
    if (!Snapshot) { Status = ParseError.empty() ? TEXT("Invalid dedicated snapshot") : ToFString(ParseError); OnNetworkChanged.Broadcast(); return; }
    DedicatedLobbyPlayers.Reset();
    for (const auto& Source : Snapshot->lobby_players)
    {
        FCatanLobbyPlayerView& Target = DedicatedLobbyPlayers.Emplace_GetRef();
        Target.PlayerId = Source.id; Target.Name = ToFString(Source.name);
        Target.bReady = Source.ready; Target.bHost = Source.host;
    }
    DedicatedPlayerName = ToFString(Snapshot->local_player);
    bDedicatedPlaying = Snapshot->playing;
    if (!Snapshot->playing && bDedicatedAutoReady && !bDedicatedReadyRequested)
    {
        bDedicatedReadyRequested = true;
        SetDedicatedReady(true);
    }
    if (!Snapshot->playing && DedicatedAutoStartPlayers > 0
        && DedicatedLobbyPlayers.Num() == DedicatedAutoStartPlayers)
    {
        bool bLocalHost = false;
        bool bAllReady = true;
        for (const FCatanLobbyPlayerView& Player : DedicatedLobbyPlayers)
        {
            bAllReady = bAllReady && Player.bReady;
            if (Player.Name == DedicatedPlayerName) bLocalHost = Player.bHost;
        }
        if (bLocalHost && bAllReady)
        {
            DedicatedAutoStartPlayers = 0;
            StartDedicatedGame();
        }
    }
    if (Snapshot->playing)
    {
        FCatanGameView View;
        View.CurrentPlayer = ToFString(Snapshot->current_player); View.Step = ToFString(Snapshot->step);
        View.Phase = static_cast<ECatanGamePhase>(Snapshot->phase);
        View.BoardAction = static_cast<ECatanBoardAction>(Snapshot->board_action);
        View.FirstDie = Snapshot->first_die; View.SecondDie = Snapshot->second_die;
        View.Winner = ToFString(Snapshot->winner); View.StatusMessage = ToFString(Snapshot->status);
        View.RequiredDiscardCount = Snapshot->required_discard; View.PendingRobberHex = Snapshot->pending_robber_hex;
        for (const auto& Item : Snapshot->robber_victims) View.RobberVictims.Add(ToFString(Item));
        View.ActiveDeal.bIsActive = Snapshot->deal.active;
        View.ActiveDeal.OfferingPlayer = ToFString(Snapshot->deal.offering_player);
        View.ActiveDeal.TargetPlayer = ToFString(Snapshot->deal.target_player);
        auto CopyResources = [](const ivv::catan::dedicated::Resources& Source, FCatanResourceView& Target)
        { Target.Wood = Source.wood; Target.Clay = Source.clay; Target.Hay = Source.hay; Target.Sheep = Source.sheep; Target.Stone = Source.stone; };
        CopyResources(Snapshot->deal.offered, View.ActiveDeal.Offered);
        CopyResources(Snapshot->deal.requested, View.ActiveDeal.Requested);
        for (int Value : Snapshot->valid_nodes) View.ValidNodeTargets.Add(Value);
        for (int Value : Snapshot->valid_roads) View.ValidRoadTargets.Add(Value);
        for (int Value : Snapshot->valid_hexes) View.ValidHexTargets.Add(Value);
        View.bHasSettlementTarget = Snapshot->has_settlement_target;
        View.bHasCityTarget = Snapshot->has_city_target; View.bHasRoadTarget = Snapshot->has_road_target;
        for (const auto& Item : Snapshot->events) View.EventLog.Add(ToFString(Item));
        for (const auto& Source : Snapshot->players)
        {
            FCatanPlayerView& Target = View.Players.Emplace_GetRef();
            Target.Id = Source.id; Target.Name = ToFString(Source.name); Target.bIsCurrent = Source.current;
            Target.bIsLocalPlayer = Source.local; Target.bResourcesVisible = Source.resources_visible;
            Target.VictoryPoints = Source.victory_points; Target.ResourceCards = Source.resource_cards;
            Target.VictoryPointCards = Source.victory_point_cards;
            Target.DevelopmentCards = Source.development_cards; Target.FreeSettlements = Source.free_settlements;
            Target.FreeCities = Source.free_cities; Target.FreeRoads = Source.free_roads;
            CopyResources(Source.resources, Target.Resources); CopyResources(Source.trade_rates, Target.TradeRates);
            Target.Knights = Source.knights; Target.RoadBuildingCards = Source.road_building;
            Target.YearOfPlentyCards = Source.year_of_plenty; Target.MonopolyCards = Source.monopoly;
            Target.PendingDevelopmentCards = Source.pending_development;
            Target.bHasLargestArmy = Source.largest_army; Target.bHasLongestRoad = Source.longest_road;
        }
        for (const auto& Source : Snapshot->hexes) { auto& Target = View.Hexes.Emplace_GetRef(); Target.Id = Source.id; Target.Resource = static_cast<ECatanResource>(Source.resource); Target.Dice = Source.dice; Target.bHasRobber = Source.robber; }
        for (const auto& Source : Snapshot->nodes) { auto& Target = View.Nodes.Emplace_GetRef(); Target.Id = Source.id; Target.OwnerId = Source.owner; Target.bIsCity = Source.city; }
        for (const auto& Source : Snapshot->roads) { auto& Target = View.Roads.Emplace_GetRef(); Target.Id = Source.id; Target.OwnerId = Source.owner; }
        DedicatedView = MoveTemp(View);
        Status = DedicatedView.StatusMessage;
        if (!bDedicatedBoardShown)
        {
            bDedicatedBoardShown = true;
            if (ACatanGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACatanGameMode>() : nullptr)
                Mode->ShowDedicatedGameBoard();
        }
        if (bDedicatedE2E && !bDedicatedE2EFinished)
        {
            bDedicatedE2EFinished = true;
            UE_LOG(LogCatanLan, Display, TEXT("CATAN_DEDICATED_E2E CONNECTED lobby=%s player=%s players=%d"),
                *DedicatedLobbyToken, *DedicatedPlayerName, DedicatedView.Players.Num());
        }
    }
    else Status = FString::Printf(TEXT("Dedicated lobby %s — share token %s"),
        *ToFString(Snapshot->lobby_name), *DedicatedLobbyToken);
    if (!Snapshot->playing)
        UE_LOG(LogCatanLan, Display, TEXT("CATAN_HUD_GRAPH dedicated-lobby players=%d"),
            DedicatedLobbyPlayers.Num());
    OnNetworkChanged.Broadcast();
    if (UCatanGameSubsystem* GameSubsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>())
        GameSubsystem->NotifyNetworkStateChanged();
}

bool UCatanNetworkSubsystem::SendDedicatedCommand(ECatanServerCommand Command, int32 First, int32 Second,
    const FString& Text, const FCatanResourceView& FirstResources,
    const FCatanResourceView& SecondResources, FString& Error)
{
    if (!bDedicatedActive || !bDedicatedPlaying) { Error = TEXT("Not connected to a dedicated game"); return false; }
    if (bDedicatedRequestInFlight) { Error = TEXT("Synchronizing with dedicated server"); return false; }
    const FString Request = FString::Printf(TEXT("COMMAND\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s"),
        *DedicatedLobbyToken, *DedicatedPlayerToken, static_cast<int32>(Command), First, Second,
        *EncodeField(Text), *ResourceField(FirstResources), *ResourceField(SecondResources));
    SendDedicatedRequest(Request, [this](const TArray<FString>& Fields)
    {
        if (Fields.Num() >= 3 && Fields[1] == TEXT("RESULT"))
        {
            const auto Message = ivv::catan::dedicated::protocol::HexDecode(ToUtf8(Fields[2]));
            if (Message) Status = ToFString(*Message);
        }
        PollDedicatedSnapshot();
    });
    Error.Reset();
    return true;
}

void UCatanNetworkSubsystem::ResetDedicatedConnection()
{
    ++DedicatedGeneration;
    FTSTicker::GetCoreTicker().RemoveTicker(DedicatedPollTicker);
    DedicatedPollTicker.Reset();
    bDedicatedActive = bDedicatedPlaying = bDedicatedBoardShown = false;
    bDedicatedRequestInFlight = false;
    bDedicatedReadyRequested = bDedicatedE2EFinished = false;
    DedicatedLobbyToken.Reset(); DedicatedPlayerToken.Reset(); DedicatedPlayerName.Reset();
    DedicatedLobbyPlayers.Reset(); DedicatedView = {};
}

void UCatanNetworkSubsystem::StopDiscoverySockets()
{
    FTSTicker::GetCoreTicker().RemoveTicker(DiscoveryHostTicker);
    FTSTicker::GetCoreTicker().RemoveTicker(DiscoveryClientTicker);
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (Sockets && DiscoveryHostSocket) Sockets->DestroySocket(DiscoveryHostSocket);
    if (Sockets && DiscoveryClientSocket) Sockets->DestroySocket(DiscoveryClientSocket);
    DiscoveryHostSocket = nullptr;
    DiscoveryClientSocket = nullptr;
}

void UCatanNetworkSubsystem::StartDiscoveryHost()
{
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Sockets || DiscoveryHostSocket) return;
    DiscoveryHostSocket = Sockets->CreateSocket(NAME_DGram, TEXT("Catan discovery host"), FNetworkProtocolTypes::IPv4);
    TSharedRef<FInternetAddr> BindAddress = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
    BindAddress->SetAnyAddress();
    BindAddress->SetPort(CatanDiscoveryPort);
    if (!DiscoveryHostSocket || !DiscoveryHostSocket->SetReuseAddr(false)
        || !DiscoveryHostSocket->SetNonBlocking(true) || !DiscoveryHostSocket->Bind(*BindAddress))
    {
        UE_LOG(LogCatanLan, Error, TEXT("Could not bind Catan discovery host on UDP %d"), CatanDiscoveryPort);
        if (DiscoveryHostSocket) Sockets->DestroySocket(DiscoveryHostSocket);
        DiscoveryHostSocket = nullptr;
        return;
    }
    DiscoveryHostTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCatanNetworkSubsystem::TickDiscoveryHost), 0.05f);
    UE_LOG(LogCatanLan, Display, TEXT("CATAN_E2E discovery host listening UDP %d"), CatanDiscoveryPort);
}

bool UCatanNetworkSubsystem::TickDiscoveryHost(float DeltaTime)
{
    if (!DiscoveryHostSocket) return false;
    uint32 Pending = 0;
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    while (DiscoveryHostSocket->HasPendingData(Pending))
    {
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(FMath::Min(Pending, 1024u) + 1);
        int32 Read = 0;
        TSharedRef<FInternetAddr> Sender = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        if (DiscoveryHostSocket->RecvFrom(Buffer.GetData(), Buffer.Num() - 1, Read, *Sender) && Read > 0)
        {
            Buffer[Read] = 0;
            const FString Query = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()));
            if (Query == UTF8_TO_TCHAR(DiscoveryQuery))
            {
                FString SafeName = PendingLobbyName;
                SafeName.ReplaceInline(TEXT("\t"), TEXT(" "));
                const FString Reply = FString::Printf(TEXT("%s%s\t%s"), DiscoveryResponsePrefix,
                    *SafeName, *GetLocalAddress());
                FTCHARToUTF8 Utf8(*Reply);
                int32 Sent = 0;
                DiscoveryHostSocket->SendTo(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length(), Sent, *Sender);
            }
        }
    }
    return true;
}

void UCatanNetworkSubsystem::ConfigureLanAdapter()
{
    FString ExplicitAddress;
    if (FParse::Value(FCommandLine::Get(), TEXT("MULTIHOME="), ExplicitAddress))
    {
        LanAddress = ExplicitAddress;
        return;
    }

    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    TArray<TSharedPtr<FInternetAddr>> Adapters;
    if (!Sockets || !Sockets->GetLocalAdapterAddresses(Adapters)) return;

    int32 BestScore = INDEX_NONE;
    for (const TSharedPtr<FInternetAddr>& Adapter : Adapters)
    {
        if (!Adapter.IsValid() || !Adapter->IsValid()) continue;
        const FString Candidate = Adapter->ToString(false);
        if (Candidate.Contains(TEXT(":")) || Candidate.StartsWith(TEXT("127."))
            || Candidate.StartsWith(TEXT("169.254.")) || Candidate == TEXT("0.0.0.0"))
            continue;
        int32 Score = 10;
        if (Candidate.StartsWith(TEXT("192.168."))) Score = 100;
        else if (Candidate.StartsWith(TEXT("10."))) Score = 90;
        else if (Candidate.StartsWith(TEXT("172.")))
        {
            TArray<FString> Parts;
            Candidate.ParseIntoArray(Parts, TEXT("."));
            const int32 SecondOctet = Parts.Num() > 1 ? FCString::Atoi(*Parts[1]) : 0;
            Score = SecondOctet >= 16 && SecondOctet <= 31 ? 80 : 10;
        }
        if (Score > BestScore)
        {
            BestScore = Score;
            LanAddress = Candidate;
        }
    }
    if (!LanAddress.IsEmpty())
    {
        UE_LOG(LogCatanLan, Display, TEXT("LAN adapter selected: %s"), *LanAddress);
    }
}

FString UCatanNetworkSubsystem::PlayerOption(const FString& PlayerName) const
{
    FString Safe = PlayerName.TrimStartAndEnd().Left(24);
    Safe.ReplaceInline(TEXT("?"), TEXT("_"));
    Safe.ReplaceInline(TEXT("&"), TEXT("_"));
    // "Name" is consumed/replaced by some OnlineSubsystem paths. Keep our
    // public identity in a game-owned travel option so PreLogin/InitNewPlayer
    // can validate a restored player before admitting it to the lobby.
    return FString::Printf(TEXT("?CatanName=%s"), *Safe);
}

void UCatanNetworkSubsystem::HostLobby(const FString& PlayerName, const FString& LobbyName)
{
    bHostingSavedLobby = false;
    SavedExpectedPlayerNames.Reset();
    BeginHostLobby(PlayerName, LobbyName);
}

void UCatanNetworkSubsystem::HostSavedLobby(const FString& PlayerName)
{
    UCatanGameSubsystem* Games = GetGameInstance()
        ? GetGameInstance()->GetSubsystem<UCatanGameSubsystem>() : nullptr;
    FString Error;
    TArray<FString> Expected;
    if (!Games || !Games->GetLanSavedPlayerNames(Expected, Error))
    {
        Status = Error.IsEmpty() ? TEXT("Could not read the saved LAN game") : Error;
        OnNetworkChanged.Broadcast();
        return;
    }
    const FString* CanonicalName = Expected.FindByPredicate([&PlayerName](const FString& Candidate)
    {
        return Candidate.Equals(PlayerName.TrimStartAndEnd(), ESearchCase::IgnoreCase);
    });
    if (!CanonicalName)
    {
        Status = TEXT("Your public name is not part of this saved game");
        OnNetworkChanged.Broadcast();
        return;
    }
    const FString HostName = *CanonicalName;
    bHostingSavedLobby = true;
    SavedExpectedPlayerNames = MoveTemp(Expected);
    BeginHostLobby(HostName, TEXT("Restored LAN game"));
}

void UCatanNetworkSubsystem::BeginHostLobby(const FString& PlayerName, const FString& LobbyName)
{
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (!Sessions.IsValid()) { Status = TEXT("LAN session service is unavailable"); OnNetworkChanged.Broadcast(); return; }
    PendingPlayerName = PlayerName;
    PendingLobbyName = LobbyName.IsEmpty() ? FString::Printf(TEXT("%s's lobby"), *PlayerName) : LobbyName.Left(40);
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch = true;
    Settings.bShouldAdvertise = true;
    Settings.bAllowJoinInProgress = true;
    Settings.bUsesPresence = false;
    Settings.NumPublicConnections = 4;
    Settings.Set(CatanLobbyKey, PendingLobbyName, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
    Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
    CreateSessionHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UCatanNetworkSubsystem::OnCreateSessionComplete));
    Status = TEXT("Creating LAN lobby...");
    if (!Sessions->CreateSession(0, NAME_GameSession, Settings))
    {
        Status = TEXT("Could not start LAN lobby");
        OnNetworkChanged.Broadcast();
    }
}

void UCatanNetworkSubsystem::OnCreateSessionComplete(FName SessionName, bool bSuccess)
{
    if (IOnlineSubsystem* Online = IOnlineSubsystem::Get())
        if (IOnlineSessionPtr Sessions = Online->GetSessionInterface(); Sessions.IsValid())
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
    UE_LOG(LogCatanLan, Display, TEXT("LAN create session=%s success=%d address=%s"),
        *SessionName.ToString(), bSuccess, *GetLocalAddress());
    Status = bSuccess ? TEXT("LAN lobby created") : TEXT("LAN lobby creation failed");
    OnNetworkChanged.Broadcast();
    if (bSuccess && GetWorld())
    {
        StartDiscoveryHost();
        GetWorld()->ServerTravel(FString::Printf(TEXT("/Engine/Maps/Templates/Template_Default?listen%s"), *PlayerOption(PendingPlayerName)));
    }
}

void UCatanNetworkSubsystem::FindLobbies()
{
    if (DiscoveryClientSocket)
    {
        Status = TEXT("LAN search is already running...");
        OnNetworkChanged.Broadcast();
        return;
    }
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    DiscoveryClientSocket = Sockets->CreateSocket(NAME_DGram, TEXT("Catan discovery client"), FNetworkProtocolTypes::IPv4);
    TSharedRef<FInternetAddr> BindAddress = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
    BindAddress->SetAnyAddress();
    BindAddress->SetPort(0);
    if (!DiscoveryClientSocket || !DiscoveryClientSocket->SetNonBlocking(true)
        || !DiscoveryClientSocket->SetBroadcast(true) || !DiscoveryClientSocket->Bind(*BindAddress))
    {
        Status = TEXT("Could not start LAN discovery socket");
        if (DiscoveryClientSocket) Sockets->DestroySocket(DiscoveryClientSocket);
        DiscoveryClientSocket = nullptr;
        OnNetworkChanged.Broadcast();
        return;
    }
    DiscoveredLobbies.Reset();
    Status = TEXT("Searching the local network...");
    OnNetworkChanged.Broadcast();
    auto SendQuery = [this, Sockets](const FString& Destination)
    {
        TSharedRef<FInternetAddr> Target = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        bool bValid = false;
        Target->SetIp(*Destination, bValid);
        Target->SetPort(CatanDiscoveryPort);
        int32 Sent = 0;
        if (bValid) DiscoveryClientSocket->SendTo(reinterpret_cast<const uint8*>(DiscoveryQuery),
            static_cast<int32>(sizeof(DiscoveryQuery) - 1), Sent, *Target);
    };
    SendQuery(TEXT("127.0.0.1"));
    if (!LanAddress.IsEmpty())
    {
        TArray<FString> Parts;
        LanAddress.ParseIntoArray(Parts, TEXT("."));
        if (Parts.Num() == 4) SendQuery(FString::Printf(TEXT("%s.%s.%s.255"), *Parts[0], *Parts[1], *Parts[2]));
    }
    DiscoveryDeadline = FPlatformTime::Seconds() + 3.0;
    DiscoveryClientTicker = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UCatanNetworkSubsystem::TickDiscoveryClient), 0.05f);
}

bool UCatanNetworkSubsystem::TickDiscoveryClient(float DeltaTime)
{
    if (!DiscoveryClientSocket) return false;
    ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    uint32 Pending = 0;
    while (DiscoveryClientSocket->HasPendingData(Pending))
    {
        TArray<uint8> Buffer;
        Buffer.SetNumUninitialized(FMath::Min(Pending, 2048u) + 1);
        int32 Read = 0;
        TSharedRef<FInternetAddr> Sender = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        if (DiscoveryClientSocket->RecvFrom(Buffer.GetData(), Buffer.Num() - 1, Read, *Sender) && Read > 0)
        {
            Buffer[Read] = 0;
            const FString Reply = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Buffer.GetData()));
            TArray<FString> Fields;
            Reply.ParseIntoArray(Fields, TEXT("\t"), false);
            if (Fields.Num() == 3 && Fields[0] == TEXT("CATAN_LOBBY_V1"))
            {
                if (!DiscoveredLobbies.ContainsByPredicate([&Fields](const FCatanDiscoveredLobby& Lobby)
                    { return Lobby.Address == Fields[2]; }))
                {
                    FCatanDiscoveredLobby& Lobby = DiscoveredLobbies.Emplace_GetRef();
                    Lobby.Name = Fields[1];
                    Lobby.Host = Sender->ToString(false);
                    Lobby.Address = Fields[2];
                    Lobby.Capacity = 4;
                    Lobby.Players = 1;
                }
            }
        }
    }
    if (FPlatformTime::Seconds() >= DiscoveryDeadline)
    {
        FinishDiscovery();
        return false;
    }
    return true;
}

void UCatanNetworkSubsystem::FinishDiscovery()
{
    if (ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
        if (DiscoveryClientSocket) Sockets->DestroySocket(DiscoveryClientSocket);
    DiscoveryClientSocket = nullptr;
    Status = FString::Printf(TEXT("Found %d LAN %s"), DiscoveredLobbies.Num(),
        DiscoveredLobbies.Num() == 1 ? TEXT("lobby") : TEXT("lobbies"));
    UE_LOG(LogCatanLan, Display, TEXT("LAN discovery success=1 results=%d"), DiscoveredLobbies.Num());
    OnNetworkChanged.Broadcast();
    if (bAutoJoinDiscovered)
    {
        bAutoJoinDiscovered = false;
        if (!DiscoveredLobbies.IsEmpty()) JoinLobby(0, PendingPlayerName);
        else UE_LOG(LogCatanLan, Error, TEXT("CATAN_E2E discovery found no lobby"));
    }
}

void UCatanNetworkSubsystem::OnFindSessionsComplete(bool bSuccess)
{
    if (IOnlineSubsystem* Online = IOnlineSubsystem::Get())
        if (IOnlineSessionPtr Sessions = Online->GetSessionInterface(); Sessions.IsValid())
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
    DiscoveredLobbies.Reset();
    if (bSuccess && Search.IsValid())
        for (const FOnlineSessionSearchResult& Result : Search->SearchResults)
        {
            FCatanDiscoveredLobby& Lobby = DiscoveredLobbies.Emplace_GetRef();
            Result.Session.SessionSettings.Get(CatanLobbyKey, Lobby.Name);
            if (Lobby.Name.IsEmpty()) Lobby.Name = TEXT("Catan LAN lobby");
            Lobby.Host = Result.Session.OwningUserName;
            Lobby.PingMs = Result.PingInMs;
            Lobby.Capacity = Result.Session.SessionSettings.NumPublicConnections;
            Lobby.Players = Lobby.Capacity - Result.Session.NumOpenPublicConnections;
        }
    Status = FString::Printf(TEXT("Found %d LAN %s"), DiscoveredLobbies.Num(), DiscoveredLobbies.Num() == 1 ? TEXT("lobby") : TEXT("lobbies"));
    UE_LOG(LogCatanLan, Display, TEXT("LAN discovery success=%d results=%d"), bSuccess, DiscoveredLobbies.Num());
    OnNetworkChanged.Broadcast();
    if (bAutoJoinDiscovered)
    {
        bAutoJoinDiscovered = false;
        if (!DiscoveredLobbies.IsEmpty()) JoinLobby(0, PendingPlayerName);
        else UE_LOG(LogCatanLan, Error, TEXT("CATAN_E2E discovery found no lobby"));
    }
}

void UCatanNetworkSubsystem::JoinLobby(int32 Index, const FString& PlayerName)
{
    if (DiscoveredLobbies.IsValidIndex(Index) && !DiscoveredLobbies[Index].Address.IsEmpty())
    {
        JoinManual(DiscoveredLobbies[Index].Address, PlayerName);
        return;
    }
    if (!Search.IsValid() || !Search->SearchResults.IsValidIndex(Index)) { Status = TEXT("Select a discovered lobby first"); OnNetworkChanged.Broadcast(); return; }
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (!Sessions.IsValid()) return;
    PendingPlayerName = PlayerName;
    Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
    JoinSessionHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCatanNetworkSubsystem::OnJoinSessionComplete));
    Status = TEXT("Joining lobby...");
    OnNetworkChanged.Broadcast();
    Sessions->JoinSession(0, NAME_GameSession, Search->SearchResults[Index]);
}

void UCatanNetworkSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (Sessions.IsValid()) Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        Status = TEXT("Could not join the selected lobby");
        OnNetworkChanged.Broadcast();
        return;
    }
    FString Address;
    if (Sessions.IsValid() && Sessions->GetResolvedConnectString(SessionName, Address))
    {
        UE_LOG(LogCatanLan, Display, TEXT("CATAN_E2E resolved discovered lobby: %s"), *Address);
        JoinManual(Address, PendingPlayerName);
        return;
    }
    Status = TEXT("Could not resolve lobby address");
    OnNetworkChanged.Broadcast();
}

void UCatanNetworkSubsystem::JoinManual(const FString& Address, const FString& PlayerName)
{
    FString Target = Address.TrimStartAndEnd();
    if (Target.IsEmpty()) { Status = TEXT("Enter host IP address"); OnNetworkChanged.Broadcast(); return; }
    PendingPlayerName = PlayerName.TrimStartAndEnd().Left(24);
    if (!LanAddress.IsEmpty() && (Target == LanAddress || Target.StartsWith(LanAddress + TEXT(":"))))
    {
        Target = TEXT("127.0.0.1") + Target.Mid(LanAddress.Len());
        UE_LOG(LogCatanLan, Display, TEXT("Local LAN address mapped to loopback: %s"), *Target);
    }
    Status = FString::Printf(TEXT("Connecting to %s..."), *Target);
    UE_LOG(LogCatanLan, Display, TEXT("LAN manual join player=%s target=%s"), *PlayerName, *Target);
    OnNetworkChanged.Broadcast();
    if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        Controller->ClientTravel(Target + PlayerOption(PlayerName), TRAVEL_Absolute);
}

void UCatanNetworkSubsystem::LeaveToMenu()
{
    if (bLeaveInProgress) return;
    bLeaveInProgress = true;
    ReturnToMenuStatus = TEXT("Returned to main menu");
    StopDiscoverySockets();
    if (bDedicatedActive)
    {
        ++DedicatedGeneration;
        FTSTicker::GetCoreTicker().RemoveTicker(DedicatedPollTicker);
        DedicatedPollTicker.Reset();
        bDedicatedRequestInFlight = false;
        SendDedicatedRequest(FString::Printf(TEXT("LEAVE\t%s\t%s"),
            *DedicatedLobbyToken, *DedicatedPlayerToken),
            [this](const TArray<FString>&) { CompleteReturnToMenu(); });
        return;
    }
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (Sessions.IsValid() && Sessions->GetNamedSession(NAME_GameSession))
    {
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
        DestroySessionHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
            FOnDestroySessionCompleteDelegate::CreateUObject(
                this, &UCatanNetworkSubsystem::OnDestroySessionComplete));
        if (Sessions->DestroySession(NAME_GameSession)) return;
        Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
    }
    CompleteReturnToMenu();
}

void UCatanNetworkSubsystem::OnDestroySessionComplete(FName, bool)
{
    if (IOnlineSubsystem* Online = IOnlineSubsystem::Get())
        if (IOnlineSessionPtr Sessions = Online->GetSessionInterface(); Sessions.IsValid())
            Sessions->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionHandle);
    CompleteReturnToMenu();
}

void UCatanNetworkSubsystem::HandleNetworkFailure(UWorld* World, UNetDriver*,
    ENetworkFailure::Type, const FString& Error)
{
    // PendingNetDriver failures (the common invalid-address case) are
    // broadcast without a world, but still belong to this game instance.
    if ((World && World->GetGameInstance() != GetGameInstance()) || bLeaveInProgress) return;
    bLeaveInProgress = true;
    ReturnToMenuStatus = FString::Printf(TEXT("Connection failed: %s"), *Error);
    UE_LOG(LogCatanLan, Warning, TEXT("CATAN_HUD_GRAPH connection-failure error=%s"), *Error);
    OnNetworkChanged.Broadcast();
    TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
    {
        if (WeakThis.IsValid()) WeakThis->CompleteReturnToMenu();
        return false;
    }), 0.1f);
}

void UCatanNetworkSubsystem::HandleTravelFailure(UWorld* World,
    ETravelFailure::Type, const FString& Error)
{
    HandleNetworkFailure(World, nullptr, ENetworkFailure::FailureReceived, Error);
}

void UCatanNetworkSubsystem::CompleteReturnToMenu()
{
    const FString CompletionStatus = ReturnToMenuStatus.IsEmpty()
        ? TEXT("Returned to main menu") : ReturnToMenuStatus;
    ResetDedicatedConnection();
    StopDiscoverySockets();
    DiscoveredLobbies.Reset();
    Search.Reset();
    PendingPlayerName.Reset();
    PendingLobbyName.Reset();
    bHostingSavedLobby = false;
    SavedExpectedPlayerNames.Reset();
    Status = CompletionStatus;
    ReturnToMenuStatus.Reset();
    bLeaveInProgress = false;
    OnNetworkChanged.Broadcast();
    UE_LOG(LogCatanLan, Display, TEXT("CATAN_HUD_GRAPH returned-main status=%s"), *Status);
    if (UWorld* World = GetWorld())
    {
        static const FName MainMap(TEXT("/Engine/Maps/Templates/Template_Default"));
        if (World->GetNetMode() == NM_Client)
        {
            if (APlayerController* Controller = World->GetFirstPlayerController())
                Controller->ClientTravel(MainMap.ToString(), TRAVEL_Absolute);
        }
        else UGameplayStatics::OpenLevel(World, MainMap, true);
    }
}

FString UCatanNetworkSubsystem::GetLocalAddress() const
{
    if (!LanAddress.IsEmpty()) return LanAddress + TEXT(":7777");
    bool bCanBindAll = false;
    TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    return Address->IsValid() ? Address->ToString(false) + TEXT(":7777") : TEXT("127.0.0.1:7777");
}
