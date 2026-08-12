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
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> HexMesh;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> HexSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> NodeSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RoadSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> BuildingBodies;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> BuildingRoofs;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> TokenSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> Decorations;

    TArray<uint8> RenderedHexResources;
    int32 ResourceGeneration = 0;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> Labels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> PortLabels;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> RobberPiece;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> RobberTop;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> BasicMaterial;

    void BuildBoard();
    void BuildEnvironment();
    void BuildHexes();
    void CreateHexSection(int32 Index, const FVector& Center, const FLinearColor& Color);
    void BuildResourceDecorations();
    void BuildPorts();
    void BuildHexHitTargets();
    void BuildNodes();
    void BuildRoads();
    UStaticMeshComponent* AddDecoration(const FString& Name, UStaticMesh* Mesh,
        const FVector& Location, const FVector& Scale, const FLinearColor& Color,
        const FRotator& Rotation = FRotator::ZeroRotator);

    UFUNCTION()
    void RefreshPieces();
    void ShowStatus(const FString& Message, const FColor& Color = FColor::White) const;

    UFUNCTION()
    void HandleSlotClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);
};
