#pragma once

#include "CoreMinimal.h"
#include "CatanNetworkTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "Net/Core/Connection/NetEnums.h"

#include "CatanNetworkSubsystem.generated.h"

class FOnlineSessionSearch;
class FOnlineSessionSearchResult;
class FSocket;
class UNetDriver;
class UWorld;

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
    UFUNCTION(BlueprintCallable) void CreateDedicatedLobby(const FString& Address,
        const FString& PlayerName, const FString& LobbyName);
    UFUNCTION(BlueprintCallable) void JoinDedicatedLobby(const FString& Address,
        const FString& LobbyToken, const FString& PlayerName);
    UFUNCTION(BlueprintCallable) void SetDedicatedReady(bool bReady);
    UFUNCTION(BlueprintCallable) void StartDedicatedGame();
    UFUNCTION(BlueprintCallable) void LeaveToMenu();
    UFUNCTION(BlueprintPure) FString GetLocalAddress() const;
    UFUNCTION(BlueprintPure) const TArray<FCatanDiscoveredLobby>& GetDiscoveredLobbies() const { return DiscoveredLobbies; }
    UFUNCTION(BlueprintPure) const FString& GetStatus() const { return Status; }
    const FString& GetPendingPlayerName() const { return PendingPlayerName; }
    bool IsDedicatedActive() const { return bDedicatedActive; }
    bool IsDedicatedPlaying() const { return bDedicatedPlaying; }
    const FCatanGameView& GetDedicatedView() const { return DedicatedView; }
    const TArray<FCatanLobbyPlayerView>& GetDedicatedLobbyPlayers() const { return DedicatedLobbyPlayers; }
    const FString& GetDedicatedLobbyToken() const { return DedicatedLobbyToken; }
    const FString& GetDedicatedPlayerToken() const { return DedicatedPlayerToken; }
    const FString& GetDedicatedAddress() const { return DedicatedAddress; }
    const FString& GetDedicatedPlayerName() const { return DedicatedPlayerName; }
    bool SendDedicatedCommand(ECatanServerCommand Command, int32 First, int32 Second,
        const FString& Text, const FCatanResourceView& FirstResources,
        const FCatanResourceView& SecondResources, FString& Error);

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
    FDelegateHandle DestroySessionHandle;
    FDelegateHandle NetworkFailureHandle;
    FDelegateHandle TravelFailureHandle;
    FTSTicker::FDelegateHandle DiscoveryHostTicker;
    FTSTicker::FDelegateHandle DiscoveryClientTicker;
    FSocket* DiscoveryHostSocket = nullptr;
    FSocket* DiscoveryClientSocket = nullptr;
    double DiscoveryDeadline = 0.0;
    bool bAutoJoinDiscovered = false;
    FString DedicatedAddress;
    FString DedicatedHost;
    int32 DedicatedPort = 17777;
    FString DedicatedLobbyToken;
    FString DedicatedPlayerToken;
    FString DedicatedPlayerName;
    TArray<FCatanLobbyPlayerView> DedicatedLobbyPlayers;
    FCatanGameView DedicatedView;
    FTSTicker::FDelegateHandle DedicatedPollTicker;
    bool bDedicatedActive = false;
    bool bDedicatedPlaying = false;
    bool bDedicatedRequestInFlight = false;
    bool bDedicatedBoardShown = false;
    bool bDedicatedAutoReady = false;
    bool bDedicatedReadyRequested = false;
    bool bDedicatedE2E = false;
    bool bDedicatedE2EFinished = false;
    int32 DedicatedAutoStartPlayers = 0;
    uint64 DedicatedGeneration = 0;
    bool bLeaveInProgress = false;
    FString ReturnToMenuStatus;

    void ConfigureLanAdapter();
    void StartDiscoveryHost();
    void StopDiscoverySockets();
    bool TickDiscoveryHost(float DeltaTime);
    bool TickDiscoveryClient(float DeltaTime);
    void FinishDiscovery();
    void OnCreateSessionComplete(FName SessionName, bool bSuccess);
    void OnFindSessionsComplete(bool bSuccess);
    void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);
    void OnDestroySessionComplete(FName SessionName, bool bSuccess);
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
        ENetworkFailure::Type FailureType, const FString& Error);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& Error);
    void CompleteReturnToMenu();
    FString PlayerOption(const FString& PlayerName) const;
    bool ParseDedicatedAddress(const FString& Address);
    void SendDedicatedRequest(const FString& Request, TFunction<void(const TArray<FString>&)> OnSuccess);
    bool TickDedicatedPoll(float DeltaTime);
    void PollDedicatedSnapshot();
    void ApplyDedicatedSnapshot(const FString& EncodedPayload);
    void ResetDedicatedConnection();
};
