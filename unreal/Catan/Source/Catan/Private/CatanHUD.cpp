#include "CatanHUD.h"

#include "CatanHUDWidget.h"

void ACatanHUD::BeginPlay()
{
    Super::BeginPlay();
    if (UCatanHUDWidget* Widget = CreateWidget<UCatanHUDWidget>(GetOwningPlayerController()))
    {
        Widget->AddToViewport(10);
    }
}
