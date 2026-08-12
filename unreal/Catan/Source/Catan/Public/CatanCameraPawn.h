#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"

#include "CatanCameraPawn.generated.h"

class UCameraComponent;
class USpringArmComponent;

UCLASS()
class CATAN_API ACatanCameraPawn final : public APawn
{
    GENERATED_BODY()

public:
    ACatanCameraPawn();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;

    float ForwardInput{};
    float RightInput{};
    float ZoomInput{};
    float RotateInput{};
    float DesiredArmLength = 2600.0f;

    void MoveForward(float Value) { ForwardInput = Value; }
    void MoveRight(float Value) { RightInput = Value; }
    void Zoom(float Value) { ZoomInput = Value; }
    void Rotate(float Value) { RotateInput = Value; }
};
