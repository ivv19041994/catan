#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"

#include "CatanHUD.generated.h"

class UCatanHUDWidget;

UCLASS()
class CATAN_API ACatanHUD final : public AHUD
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    bool IsModalOpen() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UCatanHUDWidget> CatanWidget;
};
