#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"

#include "CatanGameMode.generated.h"

UCLASS()
class CATAN_API ACatanGameMode final : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACatanGameMode();
    virtual void BeginPlay() override;
};
