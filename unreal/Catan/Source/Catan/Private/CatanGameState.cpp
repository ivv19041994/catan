#include "CatanGameState.h"

#include "CatanGameSubsystem.h"
#include "Net/UnrealNetwork.h"

ACatanGameState::ACatanGameState()
{
    bReplicates = true;
}
void ACatanGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACatanGameState, NetworkMode);
    DOREPLIFETIME(ACatanGameState, LobbyPlayers);
    DOREPLIFETIME(ACatanGameState, PublicView);
    DOREPLIFETIME(ACatanGameState, LobbyName);
    DOREPLIFETIME(ACatanGameState, HostAddress);
}

void ACatanGameState::OnRep_NetworkState()
{
    NotifyLocalProxy();
}

void ACatanGameState::NotifyLocalProxy() const
{
    if (const UGameInstance* Instance = GetGameInstance())
        if (UCatanGameSubsystem* Proxy = Instance->GetSubsystem<UCatanGameSubsystem>())
            Proxy->NotifyNetworkStateChanged();
}
