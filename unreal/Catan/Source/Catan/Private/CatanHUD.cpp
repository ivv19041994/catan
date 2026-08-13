#include "CatanHUD.h"

#include "CatanHUDWidget.h"

void ACatanHUD::BeginPlay()
{
    Super::BeginPlay();
    if (UCatanHUDWidget* Widget = CreateWidget<UCatanHUDWidget>(GetOwningPlayerController()))
    {
        CatanWidget = Widget;
        Widget->AddToViewport(10);
    }
}

bool ACatanHUD::IsModalOpen() const
{
    return CatanWidget && CatanWidget->IsModalOpen();
}
