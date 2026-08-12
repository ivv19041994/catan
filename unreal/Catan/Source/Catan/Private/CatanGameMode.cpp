#include "CatanGameMode.h"

#include "CatanBoardActor.h"
#include "CatanCameraPawn.h"
#include "CatanHUD.h"
#include "CatanPlayerController.h"

ACatanGameMode::ACatanGameMode()
{
    DefaultPawnClass = ACatanCameraPawn::StaticClass();
    PlayerControllerClass = ACatanPlayerController::StaticClass();
    HUDClass = ACatanHUD::StaticClass();
}

void ACatanGameMode::BeginPlay()
{
    Super::BeginPlay();
    GetWorld()->SpawnActor<ACatanBoardActor>(
        ACatanBoardActor::StaticClass(), FVector(0.0f, 0.0f, 20.0f), FRotator::ZeroRotator);
}
