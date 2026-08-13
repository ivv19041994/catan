#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"
#include "CatanNetworkTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "game_controller.hpp"
#include "Containers/Ticker.h"

#include <memory>

#include "CatanGameSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCatanGameStateChanged);

UCLASS()
class CATAN_API UCatanGameSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual ~UCatanGameSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Catan")
    void StartLocalGame(const TArray<FString>& Names);

    void StartBotGame(const FString& HumanName, int32 BotCount);
    void TickBots(float DeltaSeconds);
    bool IsBotPlayer(const FString& Name) const;

    void NotifyNetworkStateChanged();
    void PublishAuthoritativeState();
    bool HasAuthoritativeGame() const;
    bool CanLocalPlayerAct(const FCatanGameView& View) const;

    UFUNCTION(BlueprintPure, Category="Catan")
    FCatanGameView GetSnapshot() const;

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuildSettlement(int32 NodeId, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuildRoad(int32 RoadId, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuildCity(int32 NodeId, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryMoveRobber(int32 HexId, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryChooseRobberVictim(const FString& Victim, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryDropResources(const FCatanResourceView& Resources, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryRollDice(FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuyDevelopmentCard(FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryPass(FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryUseDevelopmentCard(ECatanDevelopmentCard Card, ECatanResource FirstResource,
        ECatanResource SecondResource, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBankTrade(ECatanResource From, ECatanResource To, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryOfferTrade(const FCatanResourceView& Offered, const FCatanResourceView& Requested, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryAcceptTrade(const FString& Player, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryCancelTrade(const FString& Player, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    void SelectBoardAction(ECatanBoardAction Action);

    UPROPERTY(BlueprintAssignable, Category="Catan")
    FOnCatanGameStateChanged OnGameStateChanged;

private:
    std::unique_ptr<ivv::catan::GameController> Game;
    TArray<FString> PlayerNames;
    FString LocalPlayerName;
    ECatanBoardAction BoardAction = ECatanBoardAction::Automatic;
    FString StatusMessage;
    int32 PendingRobberHex = INDEX_NONE;
    TArray<FString> RobberVictims;
    TArray<FString> EventLog;
    TMap<FString, FCatanResourceView> LastResources;
    FString LastLargestArmy;
    FString LastLongestRoad;
    TSet<FString> BotPlayers;
    float BotActionDelay = 0.0f;
    FRandomStream BotRandom;
    FTSTicker::FDelegateHandle BotTickerHandle;
    bool bBotAutoplay = false;
    bool bBotE2EExitRequested = false;
    int32 BotE2EActions = 0;
    int32 BotE2EUnchangedActions = 0;
    int32 BotE2EMaxActions = 12000;

    bool CompleteCommand(bool bSucceeded, const FString& Message, FString& Error);
    void AppendEvent(const FString& Message);
    void CaptureResourceChanges();
    void CaptureAwards();
    bool RouteRemoteCommand(ECatanServerCommand Command, int32 First, int32 Second,
        const FString& Text, const FCatanResourceView& FirstResources,
        const FCatanResourceView& SecondResources, FString& Error);
    FCatanGameView BuildAuthoritativeSnapshot() const;
    void PerformBotAction();
    bool TickBotTicker(float DeltaSeconds);
    FString BotStateFingerprint(const FCatanGameView& View) const;
    void FinishBotE2E(bool bSucceeded, const FString& Message);
};
