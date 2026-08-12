#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "game_controller.hpp"

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
    ECatanBoardAction BoardAction = ECatanBoardAction::Automatic;
    FString StatusMessage;
    int32 PendingRobberHex = INDEX_NONE;
    TArray<FString> RobberVictims;

    bool CompleteCommand(bool bSucceeded, const FString& Message, FString& Error);
};
