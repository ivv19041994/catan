#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"

#include "CatanBoardActor.generated.h"

class UMaterialInterface;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;

UCLASS()
class CATAN_API ACatanBoardActor final : public AActor
{
    GENERATED_BODY()

public:
    ACatanBoardActor();
    virtual void BeginPlay() override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> HexMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> NodeSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RoadSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> Labels;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> BasicMaterial;

    void BuildBoard();
    void BuildHexes();
    void BuildNodes();
    void BuildRoads();
    void RefreshPieces();
    void ShowStatus(const FString& Message, const FColor& Color = FColor::White) const;

    UFUNCTION()
    void HandleSlotClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);
};
