#pragma once

#include "CoreMinimal.h"
#include "CatanNetworkTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Containers/Ticker.h"

#include "CatanNetworkSubsystem.generated.h"

class FOnlineSessionSearch;
class FOnlineSessionSearchResult;
class FSocket;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCatanNetworkChanged);

UCLASS()
class CATAN_API UCatanNetworkSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable) void HostLobby(const FString& PlayerName, const FString& LobbyName);
    UFUNCTION(BlueprintCallable) void FindLobbies();
    UFUNCTION(BlueprintCallable) void JoinLobby(int32 Index, const FString& PlayerName);
    UFUNCTION(BlueprintCallable) void JoinManual(const FString& Address, const FString& PlayerName);
    UFUNCTION(BlueprintCallable) void LeaveToMenu();
    UFUNCTION(BlueprintPure) FString GetLocalAddress() const;
    UFUNCTION(BlueprintPure) const TArray<FCatanDiscoveredLobby>& GetDiscoveredLobbies() const { return DiscoveredLobbies; }
    UFUNCTION(BlueprintPure) const FString& GetStatus() const { return Status; }

    UPROPERTY(BlueprintAssignable) FOnCatanNetworkChanged OnNetworkChanged;

private:
    TSharedPtr<FOnlineSessionSearch> Search;
    TArray<FCatanDiscoveredLobby> DiscoveredLobbies;
    FString PendingPlayerName;
    FString PendingLobbyName;
    FString Status;
    FString LanAddress;
    FDelegateHandle CreateSessionHandle;
    FDelegateHandle FindSessionsHandle;
    FDelegateHandle JoinSessionHandle;
    FTSTicker::FDelegateHandle DiscoveryHostTicker;
    FTSTicker::FDelegateHandle DiscoveryClientTicker;
    FSocket* DiscoveryHostSocket = nullptr;
    FSocket* DiscoveryClientSocket = nullptr;
    double DiscoveryDeadline = 0.0;
    bool bAutoJoinDiscovered = false;

    void ConfigureLanAdapter();
    void StartDiscoveryHost();
    void StopDiscoverySockets();
    bool TickDiscoveryHost(float DeltaTime);
    bool TickDiscoveryClient(float DeltaTime);
    void FinishDiscovery();
    void OnCreateSessionComplete(FName SessionName, bool bSuccess);
    void OnFindSessionsComplete(bool bSuccess);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    FString PlayerOption(const FString& PlayerName) const;
};
