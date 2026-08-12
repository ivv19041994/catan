#include "CatanNetworkSubsystem.h"

#include "OnlineSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCatanLan, Log, All);

namespace
{
const FName CatanLobbyKey(TEXT("CATAN_LOBBY"));
}

void UCatanNetworkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Status = TEXT("Choose Online to host or join a LAN lobby");
}

void UCatanNetworkSubsystem::Deinitialize()
{
    Search.Reset();
    Super::Deinitialize();
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
    Sessions->AddOnCreateSessionCompleteDelegate_Handle(
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
    UE_LOG(LogCatanLan, Display, TEXT("LAN create session=%s success=%d address=%s"),
        *SessionName.ToString(), bSuccess, *GetLocalAddress());
    Status = bSuccess ? TEXT("LAN lobby created") : TEXT("LAN lobby creation failed");
    OnNetworkChanged.Broadcast();
    if (bSuccess && GetWorld())
        GetWorld()->ServerTravel(FString::Printf(TEXT("/Engine/Maps/Templates/Template_Default?listen%s"), *PlayerOption(PendingPlayerName)));
}

void UCatanNetworkSubsystem::FindLobbies()
{
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (!Sessions.IsValid()) { Status = TEXT("LAN session service is unavailable"); OnNetworkChanged.Broadcast(); return; }
    Search = MakeShared<FOnlineSessionSearch>();
    Search->bIsLanQuery = true;
    Search->MaxSearchResults = 32;
    Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UCatanNetworkSubsystem::OnFindSessionsComplete));
    DiscoveredLobbies.Reset();
    Status = TEXT("Searching the local network...");
    OnNetworkChanged.Broadcast();
    Sessions->FindSessions(0, Search.ToSharedRef());
}

void UCatanNetworkSubsystem::OnFindSessionsComplete(bool bSuccess)
{
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
}

void UCatanNetworkSubsystem::JoinLobby(int32 Index, const FString& PlayerName)
{
    if (!Search.IsValid() || !Search->SearchResults.IsValidIndex(Index)) { Status = TEXT("Select a discovered lobby first"); OnNetworkChanged.Broadcast(); return; }
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    if (!Sessions.IsValid()) return;
    PendingPlayerName = PlayerName;
    Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UCatanNetworkSubsystem::OnJoinSessionComplete));
    Status = TEXT("Joining lobby...");
    OnNetworkChanged.Broadcast();
    Sessions->JoinSession(0, NAME_GameSession, Search->SearchResults[Index]);
}

void UCatanNetworkSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
    IOnlineSubsystem* Online = IOnlineSubsystem::Get();
    IOnlineSessionPtr Sessions = Online ? Online->GetSessionInterface() : nullptr;
    FString Address;
    if (Sessions.IsValid() && Sessions->GetResolvedConnectString(SessionName, Address))
    {
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
    bool bCanBindAll = false;
    TSharedRef<FInternetAddr> Address = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll);
    return Address->IsValid() ? Address->ToString(false) + TEXT(":7777") : TEXT("127.0.0.1:7777");
}
