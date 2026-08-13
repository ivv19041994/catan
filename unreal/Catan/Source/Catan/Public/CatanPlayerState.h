#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "CatanNetworkTypes.h"

#include "CatanPlayerState.generated.h"

UCLASS()
class CATAN_API ACatanPlayerState final : public APlayerState
{
    GENERATED_BODY()

public:
    ACatanPlayerState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual FString GetPlayerNameCustom() const override { return DisplayName; }

    UPROPERTY(Replicated, BlueprintReadOnly) FString DisplayName;
    UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly) bool bLobbyReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bLobbyHost = false;
    UPROPERTY(ReplicatedUsing=OnRep_PrivateState, BlueprintReadOnly) FCatanPrivatePlayerView PrivateView;

    UFUNCTION() void OnRep_LobbyState();
    UFUNCTION() void OnRep_PrivateState();
};
