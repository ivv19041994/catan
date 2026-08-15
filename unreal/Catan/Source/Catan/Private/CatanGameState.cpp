#include "CatanGameState.h"

#include "CatanGameSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

ACatanGameState::ACatanGameState()
{
    bReplicates = true;
}

void ACatanGameState::BeginPlay()
{
    Super::BeginPlay();

    // Template_Default contains a large checkerboard floor at Z=0.  It is not
    // part of the Catan scene and remains visible through/reflected by the sea,
    // so remove it on every machine (GameMode does not exist on clients).
    for (TActorIterator<AStaticMeshActor> It(GetWorld()); It; ++It)
    {
        UStaticMeshComponent* MeshComponent = It->GetStaticMeshComponent();
        UStaticMesh* Mesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
        if (Mesh && Mesh->GetPathName().Equals(
            TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor")))
        {
            It->SetActorHiddenInGame(true);
            It->SetActorEnableCollision(false);
            MeshComponent->SetVisibility(false, true);
            MeshComponent->SetHiddenInGame(true, true);
        }
    }
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
