#include "CatanCameraPawn.h"
#include "CatanHUD.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
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
    DesiredArmLength = SpringArm->TargetArmLength;
}

void ACatanCameraPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    PlayerInputComponent->BindAxis(TEXT("MoveForward"), this, &ACatanCameraPawn::MoveForward);
    PlayerInputComponent->BindAxis(TEXT("MoveRight"), this, &ACatanCameraPawn::MoveRight);
    PlayerInputComponent->BindAxis(TEXT("Zoom"), this, &ACatanCameraPawn::Zoom);
    PlayerInputComponent->BindAxis(TEXT("Rotate"), this, &ACatanCameraPawn::Rotate);
}

void ACatanCameraPawn::FocusPlacement(const FVector& WorldLocation)
{
    if (!bPlacementFocusActive) PlacementReturnLocation = GetActorLocation();
    const float ScreenClearance = FMath::Clamp(DesiredArmLength * 0.16f, 180.0f, 560.0f);
    PlacementFocusLocation = FVector(WorldLocation.X, WorldLocation.Y, GetActorLocation().Z)
        + GetActorRightVector() * ScreenClearance;
    PlacementFocusLocation.X = FMath::Clamp(PlacementFocusLocation.X, -3000.0f, 3000.0f);
    PlacementFocusLocation.Y = FMath::Clamp(PlacementFocusLocation.Y, -3000.0f, 3000.0f);
    bPlacementFocusActive = true;
    bReturningFromPlacement = false;
}

void ACatanCameraPawn::RestorePlacementFocus()
{
    if (!bPlacementFocusActive) return;
    PlacementFocusLocation = PlacementReturnLocation;
    bReturningFromPlacement = true;
}

void ACatanCameraPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (bPlacementFocusActive)
    {
        const FVector Next = FMath::VInterpTo(
            GetActorLocation(), PlacementFocusLocation, DeltaSeconds, 7.5f);
        SetActorLocation(Next);
        if (bReturningFromPlacement && Next.Equals(PlacementFocusLocation, 1.0f))
        {
            SetActorLocation(PlacementFocusLocation);
            bPlacementFocusActive = false;
            bReturningFromPlacement = false;
        }
        SpringArm->TargetArmLength = FMath::FInterpTo(
            SpringArm->TargetArmLength, DesiredArmLength, DeltaSeconds, 12.0f);
        return;
    }
    float EffectiveForward = ForwardInput;
    float EffectiveRight = RightInput;
    FVector MousePan = FVector::ZeroVector;
    if (APlayerController* Controller = Cast<APlayerController>(GetController()))
    {
        const float DirectForward = (Controller->IsInputKeyDown(EKeys::W) || Controller->IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f)
            - (Controller->IsInputKeyDown(EKeys::S) || Controller->IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);
        const float DirectRight = (Controller->IsInputKeyDown(EKeys::D) || Controller->IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f)
            - (Controller->IsInputKeyDown(EKeys::A) || Controller->IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f);
        if (!FMath::IsNearlyZero(DirectForward)) EffectiveForward = DirectForward;
        if (!FMath::IsNearlyZero(DirectRight)) EffectiveRight = DirectRight;

        if (Controller->IsInputKeyDown(EKeys::RightMouseButton))
        {
            float MouseX = 0.0f;
            float MouseY = 0.0f;
            Controller->GetInputMouseDelta(MouseX, MouseY);
            const float DragScale = FMath::Clamp(DesiredArmLength * 0.0018f, 0.9f, 7.0f);
            MousePan = (-GetActorRightVector() * MouseX + GetActorForwardVector() * MouseY) * DragScale;
        }
#if PLATFORM_ANDROID
        const ACatanHUD* HUD = Controller->GetHUD<ACatanHUD>();
        const bool bUIConsumesTouch = HUD && HUD->IsModalOpen();
        float TouchOneX = 0.0f;
        float TouchOneY = 0.0f;
        float TouchTwoX = 0.0f;
        float TouchTwoY = 0.0f;
        bool bTouchOne = false;
        bool bTouchTwo = false;
        Controller->GetInputTouchState(ETouchIndex::Touch1, TouchOneX, TouchOneY, bTouchOne);
        Controller->GetInputTouchState(ETouchIndex::Touch2, TouchTwoX, TouchTwoY, bTouchTwo);
        if (bUIConsumesTouch)
        {
            bTrackingTouch = false;
            bTrackingPinch = false;
        }
        else if (bTouchOne && bTouchTwo)
        {
            const float PinchDistance = FVector2D::Distance(
                FVector2D(TouchOneX, TouchOneY), FVector2D(TouchTwoX, TouchTwoY));
            if (bTrackingPinch)
                DesiredArmLength = FMath::Clamp(DesiredArmLength
                    - (PinchDistance - PreviousPinchDistance) * 3.2f, 420.0f, 5200.0f);
            PreviousPinchDistance = PinchDistance;
            bTrackingPinch = true;
            bTrackingTouch = false;
        }
        else if (bTouchOne)
        {
            const FVector2D TouchPosition(TouchOneX, TouchOneY);
            if (bTrackingTouch)
            {
                const FVector2D TouchDelta = TouchPosition - PreviousTouchPosition;
                const float DragScale = FMath::Clamp(DesiredArmLength * 0.0021f, 1.1f, 8.0f);
                MousePan += (-GetActorRightVector() * TouchDelta.X
                    + GetActorForwardVector() * TouchDelta.Y) * DragScale;
            }
            PreviousTouchPosition = TouchPosition;
            bTrackingTouch = true;
            bTrackingPinch = false;
        }
        else
        {
            bTrackingTouch = false;
            bTrackingPinch = false;
        }
#endif
    }

    const float MoveSpeed = FMath::GetMappedRangeValueClamped(
        FVector2D(420.0f, 5200.0f), FVector2D(520.0f, 1700.0f), DesiredArmLength);
    const FVector KeyboardPan = (GetActorForwardVector() * EffectiveForward + GetActorRightVector() * EffectiveRight)
        .GetClampedToMaxSize(1.0f) * MoveSpeed * DeltaSeconds;
    const FVector Delta = KeyboardPan + MousePan;
    AddActorWorldOffset(FVector(Delta.X, Delta.Y, 0.0f));
    FVector Location = GetActorLocation();
    Location.X = FMath::Clamp(Location.X, -3000.0f, 3000.0f);
    Location.Y = FMath::Clamp(Location.Y, -3000.0f, 3000.0f);
    SetActorLocation(Location);

    AddActorWorldRotation(FRotator(0.0f, RotateInput * 65.0f * DeltaSeconds, 0.0f));
    if (!FMath::IsNearlyZero(ZoomInput))
    {
        const float ZoomStep = FMath::Clamp(DesiredArmLength * 0.18f, 170.0f, 620.0f);
        DesiredArmLength = FMath::Clamp(DesiredArmLength - ZoomInput * ZoomStep, 420.0f, 5200.0f);
    }
    SpringArm->TargetArmLength = FMath::FInterpTo(
        SpringArm->TargetArmLength, DesiredArmLength, DeltaSeconds, 12.0f);
}
