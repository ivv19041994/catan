#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CatanMenuBackdropActor.generated.h"

class UMaterialInterface;
class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UStaticMeshComponent;

UCLASS()
class CATAN_API ACatanMenuBackdropActor final : public AActor
{
    GENERATED_BODY()

public:
    ACatanMenuBackdropActor();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> HexField;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UProceduralMeshComponent> Shore;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Sea;

    UPROPERTY(ReplicatedUsing=OnRep_Seed)
    int32 LayoutSeed = 0;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInterface> BasicMaterial;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UStaticMeshComponent>> ResourceParts;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UProceduralMeshComponent>> ResourcePyramids;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> ResourceInstancedParts;

    bool bBuilt = false;

    UFUNCTION()
    void OnRep_Seed();

    void BuildEnvironment();
    void BuildField();
    void BuildResourceCluster(int32 VisualId, const FVector& SurfaceCenter, uint8 Terrain);
};
