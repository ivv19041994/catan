#include "CatanPlayerState.h"

#include "CatanGameSubsystem.h"
#include "Net/UnrealNetwork.h"

ACatanPlayerState::ACatanPlayerState()
{
    bReplicates = true;
    bUseCustomPlayerNames = true;
}

void ACatanPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACatanPlayerState, DisplayName);
    DOREPLIFETIME(ACatanPlayerState, bLobbyReady);
    DOREPLIFETIME(ACatanPlayerState, bLobbyHost);
    DOREPLIFETIME_CONDITION(ACatanPlayerState, PrivateView, COND_OwnerOnly);
}

void ACatanPlayerState::OnRep_LobbyState()
{
    if (const UGameInstance* Instance = GetGameInstance())
        if (UCatanGameSubsystem* Proxy = Instance->GetSubsystem<UCatanGameSubsystem>())
            Proxy->NotifyNetworkStateChanged();
}

void ACatanPlayerState::OnRep_PrivateState()
{
    OnRep_LobbyState();
}
