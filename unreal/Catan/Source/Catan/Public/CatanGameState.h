#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "CatanNetworkTypes.h"

#include "CatanGameState.generated.h"

UCLASS()
class CATAN_API ACatanGameState final : public AGameStateBase
{
    GENERATED_BODY()

public:
    ACatanGameState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_NetworkState, BlueprintReadOnly)
    ECatanNetworkMode NetworkMode = ECatanNetworkMode::MainMenu;

    UPROPERTY(ReplicatedUsing=OnRep_NetworkState, BlueprintReadOnly)
    TArray<FCatanLobbyPlayerView> LobbyPlayers;

    UPROPERTY(ReplicatedUsing=OnRep_NetworkState, BlueprintReadOnly)
    FCatanGameView PublicView;

    UPROPERTY(ReplicatedUsing=OnRep_NetworkState, BlueprintReadOnly)
    FString LobbyName;

    UPROPERTY(ReplicatedUsing=OnRep_NetworkState, BlueprintReadOnly)
    FString HostAddress;

    UFUNCTION()
    void OnRep_NetworkState();

    void NotifyLocalProxy() const;
};
