#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"

#include "CatanBoardActor.generated.h"

class UMaterialInterface;
class UHierarchicalInstancedStaticMeshComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UTextRenderComponent;
class USoundWaveProcedural;
enum class ECatanResource : uint8;

UCLASS()
class CATAN_API ACatanBoardActor final : public AActor
{
    GENERATED_BODY()

public:
    ACatanBoardActor();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> HexMesh;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> EnvironmentMesh;

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
    TArray<TObjectPtr<UStaticMeshComponent>> BuildingParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> RoadPavingParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> FlagCloths;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> TokenSlots;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> Decorations;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> AnimatedResourceParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProceduralMeshComponent>> ResourceProceduralParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ResourceInstancedParts;

    TArray<uint8> RenderedHexResources;
    TArray<uint8> ResourceAnimationKinds;
    TArray<float> ResourceAnimationPhases;
    TArray<FVector> ResourceAnimationLocations;
    TArray<FVector> ResourceAnimationScales;
    TArray<FRotator> ResourceAnimationRotations;
    int32 ResourceGeneration = 0;
    float ResourceAnimationClock = 0.0f;
    float MobileAnimationAccumulator = 0.0f;
    TArray<int32> PreviousNodeOwners;
    TArray<int32> PreviousRoadOwners;
    TArray<FVector> BuildingBodyTargets;
    TArray<FVector> BuildingRoofTargets;
    TArray<int32> BuildingPartNodeIds;
    TArray<uint8> BuildingPartModes;
    TArray<bool> BuildingPartUsesPlayerColor;
    TArray<float> BuildingPartShades;
    TArray<FVector> BuildingPartScaleTargets;
    TArray<int32> RoadPavingRoadIds;
    TArray<bool> RoadPavingPartUsesPlayerColor;
    TArray<FVector> RoadPavingScaleTargets;
    TArray<FVector> FlagAnchors;
    TArray<float> FlagBaseYaws;
    TArray<float> FlagHalfLengths;
    TArray<float> FlagAnimationPhases;
    TArray<FVector> RoadScaleTargets;
    TArray<float> HexLabelSizeTargets;
    TArray<FVector> HexTokenScaleTargets;
    TArray<FQuat> DiceTargetRotations;
    FVector RobberTarget = FVector::ZeroVector;
    float DiceAnimationRemaining = 0.0f;
    float PieceAnimationRemaining = 0.0f;
    FString PreviousStatus;
    int32 PreviousFirstDie = 0;
    int32 PreviousSecondDie = 0;
    bool bBoardBuilt = false;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> Labels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTextRenderComponent>> PortLabels;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> RobberFigures;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> RobberPlaceholders;

    TArray<FVector> RobberFigureOffsets;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> DicePieces;

    UPROPERTY(Transient)
    TObjectPtr<USoundWaveProcedural> FeedbackSound;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> BasicMaterial;

    void BuildBoard();
    bool TryBuildBoard();
    void BuildEnvironment();
    void BuildShore();
    void BuildHexes();
    void CreateHexSection(int32 Index, const FVector& Center, ECatanResource Resource,
        const FLinearColor& Color);
    void BuildResourceDecorations();
    void BuildPorts();
    void BuildHexHitTargets();
    void BuildNodes();
    void BuildRoads();
    void BuildDice();
    void AnimateFeedback(float DeltaSeconds);
    void PlayFeedbackTone(float Frequency, float Duration, float Volume = 0.18f);
    UStaticMeshComponent* AddDecoration(const FString& Name, UStaticMesh* Mesh,
        const FVector& Location, const FVector& Scale, const FLinearColor& Color,
        const FRotator& Rotation = FRotator::ZeroRotator);
    UStaticMeshComponent* AddAuthoredDecoration(const FString& Name, UStaticMesh* Mesh,
        const FVector& Location, const FVector& Scale,
        const FRotator& Rotation = FRotator::ZeroRotator);

    UFUNCTION()
    void RefreshPieces();
    void ShowStatus(const FString& Message, const FColor& Color = FColor::White) const;

    UFUNCTION()
    void HandleSlotClicked(UPrimitiveComponent* TouchedComponent, FKey ButtonPressed);

    UFUNCTION()
    void HandleSlotTouched(ETouchIndex::Type FingerIndex, UPrimitiveComponent* TouchedComponent);
};
