#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CatanNetworkTypes.h"

#include "CatanPlayerController.generated.h"

UCLASS()
class CATAN_API ACatanPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    ACatanPlayerController();
    virtual void BeginPlay() override;

    FString RequestedPlayerName;

    UFUNCTION(Server, Reliable)
    void ServerSetLobbyReady(bool bReady);

    UFUNCTION(Server, Reliable)
    void ServerSetDisplayName(const FString& PlayerName);

    UFUNCTION(Server, Reliable)
    void ServerStartLobbyGame();

    UFUNCTION(Server, Reliable)
    void ServerExecuteCatanCommand(ECatanServerCommand Command, int32 First, int32 Second,
        const FString& Text, const FCatanResourceView& FirstResources,
        const FCatanResourceView& SecondResources);

    UFUNCTION(Client, Reliable)
    void ClientCatanCommandResult(bool bSuccess, const FString& Message);

private:
    FString LastAutomatedSetupKey;
    bool bReconnectSnapshotReported = false;
    bool bTradeE2EReported = false;
    int32 TradeE2ENextResource = 0;
    FString LastTradeE2EStateKey;
    void RunAutomatedSetupStep();
    void RunMultiplayerE2EStep();
};
