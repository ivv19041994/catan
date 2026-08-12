#include "CatanCameraPawn.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"

ACatanCameraPawn::ACatanCameraPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = SceneRoot;

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(SceneRoot);
    SpringArm->TargetArmLength = 2600.0f;
    SpringArm->SetRelativeRotation(FRotator(-58.0f, 0.0f, 0.0f));
    SpringArm->bDoCollisionTest = false;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);
}

void ACatanCameraPawn::BeginPlay()
{
    Super::BeginPlay();
    SetActorLocation(FVector(0.0f, 0.0f, 80.0f));
}

void ACatanCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ACatanCameraPawn::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ACatanCameraPawn::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Zoom"), this, &ACatanCameraPawn::Zoom);
    PlayerInputComponent->BindAxis(TEXT("Rotate"), this, &ACatanCameraPawn::Rotate);
}

void ACatanCameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const FVector Delta = (GetActorForwardVector() * ForwardInput + GetActorRightVector() * RightInput)
        * 900.0f * DeltaSeconds;
    AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.0f));
    AddActorWorldRotation(FRotator(0.0f, RotateInput * 65.0f * DeltaSeconds, 0.0f));
    SpringArm->TargetArmLength = FMath::Clamp(
        SpringArm->TargetArmLength - ZoomInput * 240.0f, 900.0f, 4200.0f);
}
