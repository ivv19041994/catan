#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"

#include "CatanHUDWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UCatanGameSubsystem;
class UVerticalBox;

UCLASS()
class CATAN_API UCatanHUDWidget final : public UCommonActivatableWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient) TObjectPtr<UCatanGameSubsystem> GameSubsystem;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PhaseText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DiceText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PlayersText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> HintText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UButton> RollButton;
    UPROPERTY(Transient) TObjectPtr<UButton> SettlementButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RoadButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CityButton;
    UPROPERTY(Transient) TObjectPtr<UButton> BuyCardButton;
    UPROPERTY(Transient) TObjectPtr<UButton> PassButton;

    void BuildLayout();
    UCommonTextBlock* AddText(UVerticalBox* Parent, const FString& Text, int32 Size);
    UButton* AddButton(UVerticalBox* Parent, const FString& Label);

    UFUNCTION() void Refresh();
    UFUNCTION() void RollDice();
    UFUNCTION() void SelectSettlement();
    UFUNCTION() void SelectRoad();
    UFUNCTION() void SelectCity();
    UFUNCTION() void BuyDevelopmentCard();
    UFUNCTION() void PassTurn();
};
