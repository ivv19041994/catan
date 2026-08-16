#include "CatanHUD.h"

#include "CatanHUDWidget.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void ACatanHUD::BeginPlay()
{
    Super::BeginPlay();
    if (UCatanHUDWidget* Widget = CreateWidget<UCatanHUDWidget>(GetOwningPlayerController()))
    {
        CatanWidget = Widget;
        Widget->AddToViewport(10);
        // Android's activity override can leave a bare final flag that is not
        // recognized by FParse::Param on every engine/platform combination.
        if (FParse::Param(FCommandLine::Get(), TEXT("CatanHUDGraphSmoke"))
            || FString(FCommandLine::Get()).Contains(TEXT("-CatanHUDGraphSmoke")))
            Widget->RunHUDGraphSmoke();
    }
}

bool ACatanHUD::IsModalOpen() const
{
    return CatanWidget && CatanWidget->IsModalOpen();
}
