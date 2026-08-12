#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "game_controller.hpp"

#include <memory>

#include "CatanGameSubsystem.generated.h"

UCLASS()
class CATAN_API UCatanGameSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual ~UCatanGameSubsystem() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Catan")
    void StartLocalGame(const TArray<FString>& PlayerNames);

    UFUNCTION(BlueprintPure, Category="Catan")
    FCatanGameView GetSnapshot() const;

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuildSettlement(int32 NodeId, FString& Error);

    UFUNCTION(BlueprintCallable, Category="Catan")
    bool TryBuildRoad(int32 RoadId, FString& Error);

private:
    std::unique_ptr<ivv::catan::GameController> Game;
};
