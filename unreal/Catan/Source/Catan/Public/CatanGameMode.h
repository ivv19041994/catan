#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "CatanGameMode.generated.h"

class ACatanMenuBackdropActor;

UCLASS()
class CATAN_API ACatanGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACatanGameMode();
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController,
        const FUniqueNetIdRepl& UniqueId, const FString& Options,
        const FString& Portal = TEXT("")) override;

    void SetPlayerReady(APlayerController* Player, bool bReady);
    void SetPlayerDisplayName(APlayerController* Player, const FString& RequestedName);
    void StartLobbyGame(APlayerController* Requester);
    void PublishLobby();

private:
    bool bLobbyGameStarted = false;

    UPROPERTY(Transient)
    TObjectPtr<ACatanMenuBackdropActor> MenuBackdrop;
};
