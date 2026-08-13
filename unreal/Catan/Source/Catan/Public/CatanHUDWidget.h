#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "CatanViewTypes.h"

#include "CatanHUDWidget.generated.h"

class UButton;
class UCommonTextBlock;
class UComboBoxString;
class UEditableTextBox;
class UCatanGameSubsystem;
class UCatanNetworkSubsystem;
class UBorder;
class USpinBox;
class UVerticalBox;
class UWidgetSwitcher;
enum class ECatanDevelopmentCard : uint8;

UCLASS()
class CATAN_API UCatanHUDWidget final : public UCommonActivatableWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(Transient) TObjectPtr<UCatanGameSubsystem> GameSubsystem;
    UPROPERTY(Transient) TObjectPtr<UCatanNetworkSubsystem> NetworkSubsystem;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PhaseText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DiceText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> PlayersText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> HandTitleText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DevelopmentHandText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DevelopmentAvailabilityText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> DevelopmentResourcePanel;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> ResourceCountTexts;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> HintText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> StatusText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> EventText;
    UPROPERTY(Transient) TObjectPtr<UBorder> ToastBorder;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ToastText;
    UPROPERTY(Transient) TObjectPtr<UButton> RollButton;
    UPROPERTY(Transient) TObjectPtr<UButton> SettlementButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RoadButton;
    UPROPERTY(Transient) TObjectPtr<UButton> CityButton;
    UPROPERTY(Transient) TObjectPtr<UButton> BuyCardButton;
    UPROPERTY(Transient) TObjectPtr<UButton> UseCardButton;
    UPROPERTY(Transient) TObjectPtr<UButton> TradeButton;
    UPROPERTY(Transient) TObjectPtr<UButton> PassButton;
    UPROPERTY(Transient) TObjectPtr<UBorder> ModalBorder;
    UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> ModalSwitcher;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DropTitle;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> DropInputs;
    UPROPERTY(Transient) TArray<TObjectPtr<UButton>> VictimButtons;
    UPROPERTY(Transient) TObjectPtr<UButton> KnightButton;
    UPROPERTY(Transient) TObjectPtr<UButton> RoadBuildingButton;
    UPROPERTY(Transient) TObjectPtr<UButton> YearOfPlentyButton;
    UPROPERTY(Transient) TObjectPtr<UButton> MonopolyButton;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> FirstResource;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> SecondResource;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> BankFromResource;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> BankToResource;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> OfferedInputs;
    UPROPERTY(Transient) TArray<TObjectPtr<USpinBox>> RequestedInputs;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> DealText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> BankRateText;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> TradingPlayer;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> WinnerText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> AvailabilityText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> BuildCostText;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> PlayerCount;
    UPROPERTY(Transient) TArray<TObjectPtr<UEditableTextBox>> PlayerNameInputs;
    UPROPERTY(Transient) TArray<TObjectPtr<UCommonTextBlock>> PlayerSlotLabels;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> ConfirmationText;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> LobbyNameInput;
    UPROPERTY(Transient) TObjectPtr<UEditableTextBox> ManualAddressInput;
    UPROPERTY(Transient) TObjectPtr<UComboBoxString> LobbyResults;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> NetworkStatusText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> LobbyPlayersText;
    UPROPERTY(Transient) TObjectPtr<UCommonTextBlock> LobbyAddressText;
    UPROPERTY(Transient) TObjectPtr<UButton> ReadyButton;
    UPROPERTY(Transient) TObjectPtr<UButton> StartLobbyButton;

    FString LastDropPlayer;
    bool bDevelopmentPanelOpen = false;
    bool bTradePanelOpen = false;
    bool bSetupPanelOpen = true;
    FString PreviousToastStatus;
    float ToastRemaining = 0.0f;
    float ResourcePulseRemaining = 0.0f;
    FString PreviousResourceDigest;
    FCatanResourceView PreviousLocalResources;
    bool bHavePreviousLocalResources = false;
    int32 PendingExpensiveAction = 0;

    void BuildLayout();
    UCommonTextBlock* AddText(UVerticalBox* Parent, const FString& Text, int32 Size);
    UButton* AddButton(UVerticalBox* Parent, const FString& Label);

    UFUNCTION() void Refresh();
    UFUNCTION() void RollDice();
    UFUNCTION() void SelectSettlement();
    UFUNCTION() void SelectRoad();
    UFUNCTION() void SelectCity();
    UFUNCTION() void BuyDevelopmentCard();
    UFUNCTION() void ShowDevelopmentCards();
    UFUNCTION() void ShowTrading();
    UFUNCTION() void PassTurn();
    UFUNCTION() void ConfirmDiscard();
    UFUNCTION() void ChooseVictim0();
    UFUNCTION() void ChooseVictim1();
    UFUNCTION() void ChooseVictim2();
    UFUNCTION() void PlayKnight();
    UFUNCTION() void PlayRoadBuilding();
    UFUNCTION() void PlayYearOfPlenty();
    UFUNCTION() void PlayMonopoly();
    UFUNCTION() void CloseDevelopmentCards();
    UFUNCTION() void TradeWithBank();
    UFUNCTION() void OfferTrade();
    UFUNCTION() void AcceptTrade();
    UFUNCTION() void CancelTrade();
    UFUNCTION() void CloseTrading();
    UFUNCTION() void StartNewGame();
    UFUNCTION() void ConfirmNewGame();
    UFUNCTION() void UpdatePlayerCount(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void ConfirmExpensiveAction();
    UFUNCTION() void CancelExpensiveAction();
    UFUNCTION() void QuitGame();
    UFUNCTION() void HostLanLobby();
    UFUNCTION() void StartBotMatch();
    UFUNCTION() void FindLanLobbies();
    UFUNCTION() void JoinSelectedLobby();
    UFUNCTION() void JoinManualLobby();
    UFUNCTION() void ToggleLobbyReady();
    UFUNCTION() void StartLobbyMatch();
    UFUNCTION() void LeaveLobby();

    void ChooseVictim(int32 Index);
    void PlayDevelopmentCard(ECatanDevelopmentCard Card);
    bool GetValidatedPlayerName(FString& OutName);
};
