#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "CatanPlayerController.generated.h"

UCLASS()
class CATAN_API ACatanPlayerController final : public APlayerController
{
    GENERATED_BODY()

public:
    ACatanPlayerController();
};
