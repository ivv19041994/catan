#include "CatanNetworkSubsystem.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Containers/Ticker.h"
#include "Sockets.h"

DEFINE_LOG_CATEGORY_STATIC(LogCatanLan, Log, All);

namespace
{
const FName CatanLobbyKey(TEXT("CATAN_LOBBY"));
constexpr int32 CatanDiscoveryPort = 15001;
constexpr ANSICHAR DiscoveryQuery[] = "CATAN_DISCOVER_V1";
constexpr TCHAR DiscoveryResponsePrefix[] = TEXT("CATAN_LOBBY_V1\t");
}

void UCatanNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    ConfigureLanAdapter();
    Status = TEXT("Choose Online to host or join a LAN lobby");

    FString AutoName = TEXT("Automation");
    FParse::Value(FCommandLine::Get(), TEXT("CatanAutoName="), AutoName);
    if (FParse::Param(FCommandLine::Get(), TEXT("CatanAutoHostLobby")))
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
            TWeakObjectPtr<UCatanNetworkSubsystem> WeakThis(this);
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis, AutoAddress, AutoName](float)
            {
                if (WeakThis.IsValid()) WeakThis->JoinManual(AutoAddress, AutoName);
                return false;
            }), 5.0f);
        }
    }
}

void UCatanNetworkSubsystem::Deinitialize()
{
    StopDiscoverySockets();
    if (IOnlineSubsystem* Online = IOnlineSubsystem::Get())
        if (IOnlineSessionPtr Sessions = Online->GetSessionInterface(); Sessions.IsValid())
        {
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionHandle);
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsHandle);
            Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionHandle);
        }
    Search.Reset();
    Super::Deinitialize();
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
    return FString::Printf(TEXT("?Name=%s"), *Safe);
}

void UCatanNetworkSubsystem::HostLobby(const FString& PlayerName, const FString& LobbyName)
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
    if (APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        Controller->ClientTravel(TEXT("/Engine/Maps/Templates/Template_Default"), TRAVEL_Absolute);
}

FString UCatanNetworkSubsystem::GetLocalAddress() const
{
    if (!LanAddress.IsEmpty()) return LanAddress + TEXT(":7777");
    bool bCanBindAll = false;
    TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    return Address->IsValid() ? Address->ToString(false) + TEXT(":7777") : TEXT("127.0.0.1:7777");
}
