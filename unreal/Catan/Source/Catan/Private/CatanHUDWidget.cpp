#include "CatanHUDWidget.h"

#include "CatanGameSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/SpinBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
UBorder* AddPanel(UWidgetTree* Tree, UCanvasPanel* Canvas, const FAnchors& Anchors,
    const FVector2D& Alignment, const FMargin& Offsets)
{
    UBorder* Border = Tree->ConstructWidget<UBorder>();
    Border->SetBrushColor(FLinearColor(0.018f, 0.024f, 0.035f, 0.92f));
    Border->SetPadding(FMargin(18.0f));
    UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Border);
    Slot->SetAnchors(Anchors);
    Slot->SetAlignment(Alignment);
    Slot->SetOffsets(Offsets);
    return Border;
}

FString PhaseHint(ECatanGamePhase Phase)
{
    switch (Phase)
    {
    case ECatanGamePhase::SetupSettlement: return TEXT("Click a free intersection to place a settlement");
    case ECatanGamePhase::SetupRoad: return TEXT("Click an adjacent edge to place a road");
    case ECatanGamePhase::RollDice: return TEXT("Roll both dice to start the turn");
    case ECatanGamePhase::CommonPlay: return TEXT("Choose an action, then click its target on the board");
    case ECatanGamePhase::DropCards: return TEXT("Choose exactly half of the shown player's resources to discard");
    case ECatanGamePhase::MoveRobber: return TEXT("Click a different hex to move the robber");
    case ECatanGamePhase::RoadBuilding: return TEXT("Click up to two valid road edges");
    case ECatanGamePhase::Finished: return TEXT("Game finished");
    }
    return FString();
}

FString PhaseTitle(ECatanGamePhase Phase)
{
    switch (Phase)
    {
    case ECatanGamePhase::SetupSettlement: return TEXT("Setup: place settlement");
    case ECatanGamePhase::SetupRoad: return TEXT("Setup: place road");
    case ECatanGamePhase::RollDice: return TEXT("Roll dice");
    case ECatanGamePhase::CommonPlay: return TEXT("Build and trade");
    case ECatanGamePhase::DropCards: return TEXT("Discard resources");
    case ECatanGamePhase::MoveRobber: return TEXT("Move robber");
    case ECatanGamePhase::RoadBuilding: return TEXT("Road Building card");
    case ECatanGamePhase::Finished: return TEXT("Finished");
    }
    return TEXT("Unknown phase");
}

ECatanResource SelectedResource(const UComboBoxString* Combo)
{
    const FString Value = Combo ? Combo->GetSelectedOption() : TEXT("Wood");
    if (Value == TEXT("Clay")) return ECatanResource::Clay;
    if (Value == TEXT("Hay")) return ECatanResource::Hay;
    if (Value == TEXT("Sheep")) return ECatanResource::Sheep;
    if (Value == TEXT("Stone")) return ECatanResource::Stone;
    return ECatanResource::Wood;
}

FString ResourceSummary(const FCatanResourceView& Resources)
{
    return FString::Printf(TEXT("W %d  C %d  H %d  S %d  O %d"),
        Resources.Wood, Resources.Clay, Resources.Hay, Resources.Sheep, Resources.Stone);
}
}

TSharedRef<SWidget> UCatanHUDWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget) BuildLayout();
    return Super::RebuildWidget();
}

void UCatanHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    GameSubsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (GameSubsystem)
    {
        GameSubsystem->OnGameStateChanged.AddDynamic(this, &UCatanHUDWidget::Refresh);
        Refresh();
    }
}

void UCatanHUDWidget::NativeDestruct()
{
    if (GameSubsystem)
    {
        GameSubsystem->OnGameStateChanged.RemoveDynamic(this, &UCatanHUDWidget::Refresh);
    }
    Super::NativeDestruct();
}

void UCatanHUDWidget::BuildLayout()
{
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget = Canvas;

    UBorder* InfoBorder = AddPanel(WidgetTree, Canvas, FAnchors(0, 0), FVector2D::ZeroVector,
        FMargin(24, 24, 470, 235));
    UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
    InfoBorder->SetContent(Info);
    AddText(Info, TEXT("CATAN"), 32);
    PhaseText = AddText(Info, TEXT("Starting..."), 22);
    DiceText = AddText(Info, FString(), 19);
    HintText = AddText(Info, FString(), 17);
    StatusText = AddText(Info, FString(), 16);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.72f, 0.18f)));

    UBorder* PlayerBorder = AddPanel(WidgetTree, Canvas, FAnchors(1, 0), FVector2D(1, 0),
        FMargin(-24, 24, 440, 365));
    UVerticalBox* PlayerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayerBorder->SetContent(PlayerPanel);
    AddText(PlayerPanel, TEXT("PLAYERS"), 22);
    PlayersText = AddText(PlayerPanel, FString(), 17);

    UBorder* ActionBorder = AddPanel(WidgetTree, Canvas, FAnchors(0.5f, 1), FVector2D(0.5f, 1),
        FMargin(0, -24, 1100, 115));
    UVerticalBox* ActionPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ActionBorder->SetContent(ActionPanel);
    AddText(ActionPanel, TEXT("ACTIONS"), 18);
    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
    ActionPanel->AddChildToVerticalBox(Buttons);

    auto AddAction = [this, Buttons](const FString& Label)
    {
        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* BoxSlot = Buttons->AddChildToHorizontalBox(Box);
        BoxSlot->SetPadding(FMargin(5));
        return AddButton(Box, Label);
    };
    RollButton = AddAction(TEXT("ROLL DICE"));
    SettlementButton = AddAction(TEXT("SETTLEMENT"));
    RoadButton = AddAction(TEXT("ROAD"));
    CityButton = AddAction(TEXT("CITY"));
    BuyCardButton = AddAction(TEXT("BUY DEV"));
    UseCardButton = AddAction(TEXT("USE DEV"));
    TradeButton = AddAction(TEXT("TRADE"));
    PassButton = AddAction(TEXT("END TURN"));

    RollButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::RollDice);
    SettlementButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectSettlement);
    RoadButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectRoad);
    CityButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectCity);
    BuyCardButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::BuyDevelopmentCard);
    UseCardButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowDevelopmentCards);
    TradeButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowTrading);
    PassButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PassTurn);

    ModalBorder = AddPanel(WidgetTree, Canvas, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f),
        FMargin(0, 0, 680, 650));
    ModalSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
    ModalBorder->SetContent(ModalSwitcher);

    UVerticalBox* DropPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(DropPanel);
    DropTitle = AddText(DropPanel, TEXT("DISCARD RESOURCES"), 25);
    constexpr const TCHAR* ResourceNames[] = {TEXT("Wood"), TEXT("Clay"), TEXT("Hay"), TEXT("Sheep"), TEXT("Stone")};
    for (const TCHAR* ResourceName : ResourceNames)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        UVerticalBoxSlot* RowSlot = DropPanel->AddChildToVerticalBox(Row);
        RowSlot->SetPadding(FMargin(2, 5));
        UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Label->SetText(FText::FromString(ResourceName));
        FSlateFontInfo Font = Label->GetFont(); Font.Size = 18; Label->SetFont(Font);
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        USpinBox* Input = WidgetTree->ConstructWidget<USpinBox>();
        Input->SetMinValue(0.0f);
        Input->SetMaxValue(0.0f);
        Input->SetMinSliderValue(0.0f);
        Input->SetMaxSliderValue(0.0f);
        Input->SetDelta(1.0f);
        Input->SetMinFractionalDigits(0);
        Input->SetMaxFractionalDigits(0);
        Row->AddChildToHorizontalBox(Input);
        DropInputs.Add(Input);
    }
    UButton* ConfirmDrop = AddButton(DropPanel, TEXT("CONFIRM DISCARD"));
    ConfirmDrop->OnClicked.AddDynamic(this, &UCatanHUDWidget::ConfirmDiscard);

    UVerticalBox* VictimPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(VictimPanel);
    AddText(VictimPanel, TEXT("CHOOSE A PLAYER TO STEAL FROM"), 24);
    for (int32 Index = 0; Index < 3; ++Index)
    {
        VictimButtons.Add(AddButton(VictimPanel, TEXT("Player")));
    }
    VictimButtons[0]->OnClicked.AddDynamic(this, &UCatanHUDWidget::ChooseVictim0);
    VictimButtons[1]->OnClicked.AddDynamic(this, &UCatanHUDWidget::ChooseVictim1);
    VictimButtons[2]->OnClicked.AddDynamic(this, &UCatanHUDWidget::ChooseVictim2);

    UVerticalBox* DevelopmentPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(DevelopmentPanel);
    AddText(DevelopmentPanel, TEXT("DEVELOPMENT CARDS"), 25);
    KnightButton = AddButton(DevelopmentPanel, TEXT("PLAY KNIGHT"));
    RoadBuildingButton = AddButton(DevelopmentPanel, TEXT("PLAY ROAD BUILDING"));
    AddText(DevelopmentPanel, TEXT("Resources for Year of Plenty / Monopoly"), 16);
    FirstResource = WidgetTree->ConstructWidget<UComboBoxString>();
    SecondResource = WidgetTree->ConstructWidget<UComboBoxString>();
    for (const TCHAR* ResourceName : ResourceNames)
    {
        FirstResource->AddOption(ResourceName);
        SecondResource->AddOption(ResourceName);
    }
    FirstResource->SetSelectedOption(TEXT("Wood"));
    SecondResource->SetSelectedOption(TEXT("Clay"));
    DevelopmentPanel->AddChildToVerticalBox(FirstResource);
    DevelopmentPanel->AddChildToVerticalBox(SecondResource);
    YearOfPlentyButton = AddButton(DevelopmentPanel, TEXT("PLAY YEAR OF PLENTY"));
    MonopolyButton = AddButton(DevelopmentPanel, TEXT("PLAY MONOPOLY (FIRST RESOURCE)"));
    UButton* CloseCards = AddButton(DevelopmentPanel, TEXT("CLOSE"));
    KnightButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayKnight);
    RoadBuildingButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayRoadBuilding);
    YearOfPlentyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayYearOfPlenty);
    MonopolyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayMonopoly);
    CloseCards->OnClicked.AddDynamic(this, &UCatanHUDWidget::CloseDevelopmentCards);

    UVerticalBox* TradePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(TradePanel);
    AddText(TradePanel, TEXT("TRADE"), 25);
    AddText(TradePanel, TEXT("Bank trade — port discounts are applied automatically"), 16);
    BankFromResource = WidgetTree->ConstructWidget<UComboBoxString>();
    BankToResource = WidgetTree->ConstructWidget<UComboBoxString>();
    for (const TCHAR* ResourceName : ResourceNames)
    {
        BankFromResource->AddOption(ResourceName);
        BankToResource->AddOption(ResourceName);
    }
    BankFromResource->SetSelectedOption(TEXT("Wood"));
    BankToResource->SetSelectedOption(TEXT("Clay"));
    TradePanel->AddChildToVerticalBox(BankFromResource);
    TradePanel->AddChildToVerticalBox(BankToResource);
    UButton* BankTrade = AddButton(TradePanel, TEXT("TRADE WITH BANK"));
    BankTrade->OnClicked.AddDynamic(this, &UCatanHUDWidget::TradeWithBank);

    AddText(TradePanel, TEXT("Player offer — W / C / H / S / O"), 16);
    auto AddResourceInputs = [this, TradePanel](const FString& Label, TArray<TObjectPtr<USpinBox>>& Inputs)
    {
        AddText(TradePanel, Label, 16);
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        TradePanel->AddChildToVerticalBox(Row);
        for (int32 Index = 0; Index < 5; ++Index)
        {
            USpinBox* Input = WidgetTree->ConstructWidget<USpinBox>();
            Input->SetMinValue(0.0f);
            Input->SetMaxValue(99.0f);
            Input->SetDelta(1.0f);
            Input->SetMinFractionalDigits(0);
            Input->SetMaxFractionalDigits(0);
            UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(Input);
            Slot->SetPadding(FMargin(3));
            Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            Inputs.Add(Input);
        }
    };
    AddResourceInputs(TEXT("YOU OFFER"), OfferedInputs);
    AddResourceInputs(TEXT("YOU REQUEST"), RequestedInputs);
    UButton* CreateOffer = AddButton(TradePanel, TEXT("CREATE OFFER"));
    UButton* CloseTrade = AddButton(TradePanel, TEXT("CLOSE"));
    CreateOffer->OnClicked.AddDynamic(this, &UCatanHUDWidget::OfferTrade);
    CloseTrade->OnClicked.AddDynamic(this, &UCatanHUDWidget::CloseTrading);

    UVerticalBox* DealPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(DealPanel);
    AddText(DealPanel, TEXT("ACTIVE PLAYER TRADE"), 25);
    DealText = AddText(DealPanel, FString(), 19);
    TradingPlayer = WidgetTree->ConstructWidget<UComboBoxString>();
    DealPanel->AddChildToVerticalBox(TradingPlayer);
    UButton* AcceptDeal = AddButton(DealPanel, TEXT("ACCEPT AS SELECTED PLAYER"));
    UButton* CancelDeal = AddButton(DealPanel, TEXT("DECLINE / CANCEL"));
    AcceptDeal->OnClicked.AddDynamic(this, &UCatanHUDWidget::AcceptTrade);
    CancelDeal->OnClicked.AddDynamic(this, &UCatanHUDWidget::CancelTrade);

    UVerticalBox* WinnerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(WinnerPanel);
    AddText(WinnerPanel, TEXT("GAME OVER"), 32);
    WinnerText = AddText(WinnerPanel, FString(), 22);
    UButton* NewGame = AddButton(WinnerPanel, TEXT("NEW GAME"));
    UButton* ExitGame = AddButton(WinnerPanel, TEXT("EXIT"));
    NewGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartNewGame);
    ExitGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::QuitGame);
    ModalBorder->SetVisibility(ESlateVisibility::Collapsed);
}

UCommonTextBlock* UCatanHUDWidget::AddText(UVerticalBox* Parent, const FString& Text, int32 Size)
{
    UCommonTextBlock* TextBlock = WidgetTree->ConstructWidget<UCommonTextBlock>();
    TextBlock->SetText(FText::FromString(Text));
    FSlateFontInfo Font = TextBlock->GetFont();
    Font.Size = Size;
    TextBlock->SetFont(Font);
    TextBlock->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.93f, 0.98f)));
    TextBlock->SetAutoWrapText(true);
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(TextBlock);
    Slot->SetPadding(FMargin(2, 3));
    return TextBlock;
}

UButton* UCatanHUDWidget::AddButton(UVerticalBox* Parent, const FString& Label)
{
    UButton* Button = WidgetTree->ConstructWidget<UButton>();
    Button->SetBackgroundColor(FLinearColor(0.10f, 0.24f, 0.42f, 1.0f));
    UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>();
    Text->SetText(FText::FromString(Label));
    Text->SetJustification(ETextJustify::Center);
    Text->SetMargin(FMargin(12, 9));
    Button->AddChild(Text);
    Parent->AddChildToVerticalBox(Button);
    return Button;
}

void UCatanHUDWidget::Refresh()
{
    if (!GameSubsystem || !PhaseText) return;
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    PhaseText->SetText(FText::FromString(FString::Printf(
        TEXT("%s\nCurrent: %s"), *PhaseTitle(View.Phase), *View.CurrentPlayer)));
    DiceText->SetText(View.FirstDie > 0
        ? FText::FromString(FString::Printf(TEXT("Dice: %d + %d = %d"), View.FirstDie, View.SecondDie, View.FirstDie + View.SecondDie))
        : FText::GetEmpty());
    HintText->SetText(FText::FromString(PhaseHint(View.Phase)));
    StatusText->SetText(FText::FromString(View.StatusMessage));

    FString Players;
    for (const FCatanPlayerView& Player : View.Players)
    {
        Players += FString::Printf(
            TEXT("%s %s  |  VP %d  DEV %d\nW %d  C %d  H %d  S %d  O %d\nPieces: %d settlements, %d cities, %d roads\n\n"),
            Player.bIsCurrent ? TEXT("▶") : TEXT(" "), *Player.Name,
            Player.VictoryPoints, Player.DevelopmentCards,
            Player.Resources.Wood, Player.Resources.Clay, Player.Resources.Hay,
            Player.Resources.Sheep, Player.Resources.Stone,
            Player.FreeSettlements, Player.FreeCities, Player.FreeRoads);
    }
    PlayersText->SetText(FText::FromString(Players));

    const bool bRoll = View.Phase == ECatanGamePhase::RollDice;
    const bool bPlay = View.Phase == ECatanGamePhase::CommonPlay;
    RollButton->SetIsEnabled(bRoll);
    SettlementButton->SetIsEnabled(bPlay);
    RoadButton->SetIsEnabled(bPlay || View.Phase == ECatanGamePhase::RoadBuilding);
    CityButton->SetIsEnabled(bPlay);
    BuyCardButton->SetIsEnabled(bPlay);
    TradeButton->SetIsEnabled(bPlay);
    PassButton->SetIsEnabled(bPlay);
    const FCatanPlayerView* CurrentPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsCurrent; });
    const int32 ReadyCards = CurrentPlayer
        ? CurrentPlayer->Knights + CurrentPlayer->RoadBuildingCards
            + CurrentPlayer->YearOfPlentyCards + CurrentPlayer->MonopolyCards
        : 0;
    UseCardButton->SetIsEnabled((bPlay || bRoll) && ReadyCards > 0);

    if (View.Phase == ECatanGamePhase::Finished)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(5);
        FString Standings = FString::Printf(TEXT("%s WINS!\n\nFINAL SCORE\n"), *View.Winner);
        for (const FCatanPlayerView& Player : View.Players)
        {
            Standings += FString::Printf(TEXT("%s — %d VP\n"), *Player.Name, Player.VictoryPoints);
        }
        WinnerText->SetText(FText::FromString(Standings));
        bDevelopmentPanelOpen = false;
        bTradePanelOpen = false;
    }
    else if (View.Phase == ECatanGamePhase::DropCards && CurrentPlayer)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(0);
        DropTitle->SetText(FText::FromString(FString::Printf(
            TEXT("DISCARD %d RESOURCES — %s"), View.RequiredDiscardCount, *View.CurrentPlayer)));
        const int32 Holdings[] = {
            CurrentPlayer->Resources.Wood, CurrentPlayer->Resources.Clay, CurrentPlayer->Resources.Hay,
            CurrentPlayer->Resources.Sheep, CurrentPlayer->Resources.Stone
        };
        if (LastDropPlayer != View.CurrentPlayer)
        {
            for (USpinBox* Input : DropInputs) Input->SetValue(0.0f);
            LastDropPlayer = View.CurrentPlayer;
        }
        for (int32 Index = 0; Index < DropInputs.Num(); ++Index)
        {
            DropInputs[Index]->SetMaxValue(Holdings[Index]);
            DropInputs[Index]->SetMaxSliderValue(Holdings[Index]);
        }
    }
    else if (View.PendingRobberHex != INDEX_NONE && !View.RobberVictims.IsEmpty())
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(1);
        for (int32 Index = 0; Index < VictimButtons.Num(); ++Index)
        {
            const bool bVisible = View.RobberVictims.IsValidIndex(Index);
            VictimButtons[Index]->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (bVisible)
            {
                if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(VictimButtons[Index]->GetChildAt(0)))
                    Label->SetText(FText::FromString(View.RobberVictims[Index]));
            }
        }
    }
    else if (View.ActiveDeal.bIsActive)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(4);
        DealText->SetText(FText::FromString(FString::Printf(
            TEXT("%s offers:\n%s\n\nand requests:\n%s"),
            *View.ActiveDeal.OfferingPlayer, *ResourceSummary(View.ActiveDeal.Offered),
            *ResourceSummary(View.ActiveDeal.Requested))));
        const FString PreviousSelection = TradingPlayer->GetSelectedOption();
        TradingPlayer->ClearOptions();
        for (const FCatanPlayerView& Player : View.Players)
        {
            if (Player.Name != View.ActiveDeal.OfferingPlayer) TradingPlayer->AddOption(Player.Name);
        }
        if (!PreviousSelection.IsEmpty() && TradingPlayer->FindOptionIndex(PreviousSelection) != INDEX_NONE)
            TradingPlayer->SetSelectedOption(PreviousSelection);
        else if (TradingPlayer->GetOptionCount() > 0)
            TradingPlayer->SetSelectedIndex(0);
        bTradePanelOpen = false;
    }
    else if (bDevelopmentPanelOpen && CurrentPlayer && (bPlay || bRoll))
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(2);
        KnightButton->SetIsEnabled(CurrentPlayer->Knights > 0);
        RoadBuildingButton->SetIsEnabled(bPlay && CurrentPlayer->RoadBuildingCards > 0);
        YearOfPlentyButton->SetIsEnabled(bPlay && CurrentPlayer->YearOfPlentyCards > 0);
        MonopolyButton->SetIsEnabled(bPlay && CurrentPlayer->MonopolyCards > 0);
    }
    else if (bTradePanelOpen && CurrentPlayer && bPlay)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(3);
        const int32 Holdings[] = {
            CurrentPlayer->Resources.Wood, CurrentPlayer->Resources.Clay, CurrentPlayer->Resources.Hay,
            CurrentPlayer->Resources.Sheep, CurrentPlayer->Resources.Stone
        };
        for (int32 Index = 0; Index < OfferedInputs.Num(); ++Index)
        {
            OfferedInputs[Index]->SetMaxValue(Holdings[Index]);
            OfferedInputs[Index]->SetMaxSliderValue(Holdings[Index]);
        }
    }
    else
    {
        ModalBorder->SetVisibility(ESlateVisibility::Collapsed);
        if (View.Phase != ECatanGamePhase::DropCards) LastDropPlayer.Reset();
    }
}

void UCatanHUDWidget::RollDice()
{
    FString Error;
    GameSubsystem->TryRollDice(Error);
}

void UCatanHUDWidget::SelectSettlement()
{
    GameSubsystem->SelectBoardAction(ECatanBoardAction::BuildSettlement);
}

void UCatanHUDWidget::SelectRoad()
{
    GameSubsystem->SelectBoardAction(ECatanBoardAction::BuildRoad);
}

void UCatanHUDWidget::SelectCity()
{
    GameSubsystem->SelectBoardAction(ECatanBoardAction::BuildCity);
}

void UCatanHUDWidget::BuyDevelopmentCard()
{
    FString Error;
    GameSubsystem->TryBuyDevelopmentCard(Error);
}

void UCatanHUDWidget::ShowDevelopmentCards()
{
    bDevelopmentPanelOpen = true;
    bTradePanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::ShowTrading()
{
    bTradePanelOpen = true;
    bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::PassTurn()
{
    FString Error;
    GameSubsystem->TryPass(Error);
}

void UCatanHUDWidget::ConfirmDiscard()
{
    if (DropInputs.Num() != 5) return;
    FCatanResourceView Resources;
    Resources.Wood = FMath::RoundToInt(DropInputs[0]->GetValue());
    Resources.Clay = FMath::RoundToInt(DropInputs[1]->GetValue());
    Resources.Hay = FMath::RoundToInt(DropInputs[2]->GetValue());
    Resources.Sheep = FMath::RoundToInt(DropInputs[3]->GetValue());
    Resources.Stone = FMath::RoundToInt(DropInputs[4]->GetValue());
    FString Error;
    GameSubsystem->TryDropResources(Resources, Error);
}

void UCatanHUDWidget::ChooseVictim0() { ChooseVictim(0); }
void UCatanHUDWidget::ChooseVictim1() { ChooseVictim(1); }
void UCatanHUDWidget::ChooseVictim2() { ChooseVictim(2); }

void UCatanHUDWidget::ChooseVictim(int32 Index)
{
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    if (!View.RobberVictims.IsValidIndex(Index)) return;
    FString Error;
    GameSubsystem->TryChooseRobberVictim(View.RobberVictims[Index], Error);
}

void UCatanHUDWidget::PlayKnight() { PlayDevelopmentCard(ECatanDevelopmentCard::Knight); }
void UCatanHUDWidget::PlayRoadBuilding() { PlayDevelopmentCard(ECatanDevelopmentCard::RoadBuilding); }
void UCatanHUDWidget::PlayYearOfPlenty() { PlayDevelopmentCard(ECatanDevelopmentCard::YearOfPlenty); }
void UCatanHUDWidget::PlayMonopoly() { PlayDevelopmentCard(ECatanDevelopmentCard::Monopoly); }

void UCatanHUDWidget::PlayDevelopmentCard(ECatanDevelopmentCard Card)
{
    FString Error;
    const bool bSucceeded = GameSubsystem->TryUseDevelopmentCard(
        Card, SelectedResource(FirstResource), SelectedResource(SecondResource), Error);
    if (bSucceeded) bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::CloseDevelopmentCards()
{
    bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::TradeWithBank()
{
    FString Error;
    GameSubsystem->TryBankTrade(SelectedResource(BankFromResource), SelectedResource(BankToResource), Error);
}

void UCatanHUDWidget::OfferTrade()
{
    if (OfferedInputs.Num() != 5 || RequestedInputs.Num() != 5) return;
    auto ReadResources = [](const TArray<TObjectPtr<USpinBox>>& Inputs)
    {
        FCatanResourceView Resources;
        Resources.Wood = FMath::RoundToInt(Inputs[0]->GetValue());
        Resources.Clay = FMath::RoundToInt(Inputs[1]->GetValue());
        Resources.Hay = FMath::RoundToInt(Inputs[2]->GetValue());
        Resources.Sheep = FMath::RoundToInt(Inputs[3]->GetValue());
        Resources.Stone = FMath::RoundToInt(Inputs[4]->GetValue());
        return Resources;
    };
    FString Error;
    GameSubsystem->TryOfferTrade(ReadResources(OfferedInputs), ReadResources(RequestedInputs), Error);
}

void UCatanHUDWidget::AcceptTrade()
{
    FString Error;
    GameSubsystem->TryAcceptTrade(TradingPlayer->GetSelectedOption(), Error);
}

void UCatanHUDWidget::CancelTrade()
{
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const FString Player = TradingPlayer->GetSelectedOption().IsEmpty()
        ? View.ActiveDeal.OfferingPlayer : TradingPlayer->GetSelectedOption();
    FString Error;
    GameSubsystem->TryCancelTrade(Player, Error);
}

void UCatanHUDWidget::CloseTrading()
{
    bTradePanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::StartNewGame()
{
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    TArray<FString> Names;
    for (const FCatanPlayerView& Player : View.Players) Names.Add(Player.Name);
    GameSubsystem->StartLocalGame(Names);
}

void UCatanHUDWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
