#include "CatanHUDWidget.h"

#include "CatanGameSubsystem.h"
#include "CatanNetworkSubsystem.h"
#include "CatanGameState.h"
#include "CatanGameMode.h"
#include "CatanPlayerController.h"
#include "CatanPlayerState.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Spacer.h"
#include "Components/SpinBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Texture2D.h"

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

bool CanAfford(const FCatanResourceView& Have, int32 Wood, int32 Clay, int32 Hay, int32 Sheep, int32 Stone)
{
    return Have.Wood >= Wood && Have.Clay >= Clay && Have.Hay >= Hay
        && Have.Sheep >= Sheep && Have.Stone >= Stone;
}

FString CostLine(const TCHAR* Name, const FCatanResourceView& Have,
    int32 Wood, int32 Clay, int32 Hay, int32 Sheep, int32 Stone)
{
    return FString::Printf(TEXT("%s %s  W%d C%d H%d S%d O%d"),
        CanAfford(Have, Wood, Clay, Hay, Sheep, Stone) ? TEXT("✓") : TEXT("✕"),
        Name, Wood, Clay, Hay, Sheep, Stone);
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
    NetworkSubsystem = GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>();
    if (GameSubsystem)
    {
        GameSubsystem->OnGameStateChanged.AddDynamic(this, &UCatanHUDWidget::Refresh);
        Refresh();
    }
    if (NetworkSubsystem)
        NetworkSubsystem->OnNetworkChanged.AddDynamic(this, &UCatanHUDWidget::Refresh);
}

void UCatanHUDWidget::NativeDestruct()
{
    if (GameSubsystem)
    {
        GameSubsystem->OnGameStateChanged.RemoveDynamic(this, &UCatanHUDWidget::Refresh);
    }
    if (NetworkSubsystem)
        NetworkSubsystem->OnNetworkChanged.RemoveDynamic(this, &UCatanHUDWidget::Refresh);
    Super::NativeDestruct();
}

void UCatanHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (ToastRemaining > 0.0f && ToastBorder)
    {
        ToastRemaining = FMath::Max(0.0f, ToastRemaining - InDeltaTime);
        ToastBorder->SetRenderOpacity(FMath::Clamp(ToastRemaining * 1.6f, 0.0f, 1.0f));
        if (ToastRemaining <= 0.0f) ToastBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (PlayersText)
    {
        ResourcePulseRemaining = FMath::Max(0.0f, ResourcePulseRemaining - InDeltaTime);
        const float Scale = ResourcePulseRemaining > 0.0f
            ? 1.0f + FMath::Sin(ResourcePulseRemaining * 20.0f) * ResourcePulseRemaining * 0.08f
            : 1.0f;
        PlayersText->SetRenderScale(FVector2D(Scale));
    }
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

    UBorder* EventBorder = AddPanel(WidgetTree, Canvas, FAnchors(0, 0), FVector2D::ZeroVector,
        FMargin(24, 280, 470, 290));
    UVerticalBox* EventPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    EventBorder->SetContent(EventPanel);
    AddText(EventPanel, TEXT("EVENTS"), 20);
    EventText = AddText(EventPanel, FString(), 15);

    UBorder* PlayerBorder = AddPanel(WidgetTree, Canvas, FAnchors(1, 0), FVector2D(1, 0),
        FMargin(-24, 24, 440, 455));
    UVerticalBox* PlayerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayerBorder->SetContent(PlayerPanel);
    HandTitleText = AddText(PlayerPanel, TEXT("YOUR HAND"), 22);
    UHorizontalBox* ResourceBadges = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayerPanel->AddChildToVerticalBox(ResourceBadges);
    struct FResourceBadge { const TCHAR* Symbol; const TCHAR* Name; FLinearColor Color; };
    const FResourceBadge Badges[] = {
        {TEXT("W"), TEXT("WOOD"), FLinearColor(0.08f, 0.52f, 0.16f)},
        {TEXT("C"), TEXT("CLAY"), FLinearColor(0.76f, 0.18f, 0.05f)},
        {TEXT("H"), TEXT("HAY"), FLinearColor(0.95f, 0.70f, 0.06f)},
        {TEXT("S"), TEXT("SHEEP"), FLinearColor(0.48f, 0.82f, 0.28f)},
        {TEXT("O"), TEXT("ORE"), FLinearColor(0.42f, 0.48f, 0.58f)}
    };
    for (const FResourceBadge& Badge : Badges)
    {
        UVerticalBox* BadgeBox = WidgetTree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* BadgeSlot = ResourceBadges->AddChildToHorizontalBox(BadgeBox);
        BadgeSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        BadgeSlot->SetPadding(FMargin(3));
        UBorder* Icon = WidgetTree->ConstructWidget<UBorder>();
        Icon->SetBrushColor(Badge.Color);
        Icon->SetPadding(FMargin(8));
        UCommonTextBlock* Count = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Count->SetText(FText::FromString(FString::Printf(TEXT("%s  0"), Badge.Symbol)));
        Count->SetJustification(ETextJustify::Center);
        FSlateFontInfo SymbolFont = Count->GetFont(); SymbolFont.Size = 22; Count->SetFont(SymbolFont);
        Icon->SetContent(Count);
        ResourceCountTexts.Add(Count);
        BadgeBox->AddChildToVerticalBox(Icon);
        UCommonTextBlock* Name = AddText(BadgeBox, Badge.Name, 10);
        Name->SetJustification(ETextJustify::Center);
    }
    DevelopmentHandText = AddText(PlayerPanel, TEXT("Development: 0"), 14);
    AddText(PlayerPanel, TEXT("PLAYERS"), 20);
    PlayersText = AddText(PlayerPanel, FString(), 17);

    UBorder* CostBorder = AddPanel(WidgetTree, Canvas, FAnchors(1, 0), FVector2D(1, 0),
        FMargin(-24, 500, 440, 150));
    UVerticalBox* Costs = WidgetTree->ConstructWidget<UVerticalBox>();
    CostBorder->SetContent(Costs);
    AddText(Costs, TEXT("BUILD COSTS"), 18);
    BuildCostText = AddText(Costs, TEXT("Costs appear for the current player"), 14);

    ToastBorder = AddPanel(WidgetTree, Canvas, FAnchors(0.5f, 0), FVector2D(0.5f, 0),
        FMargin(0, 34, 520, 72));
    ToastText = WidgetTree->ConstructWidget<UCommonTextBlock>();
    ToastText->SetJustification(ETextJustify::Center);
    FSlateFontInfo ToastFont = ToastText->GetFont(); ToastFont.Size = 20; ToastText->SetFont(ToastFont);
    ToastBorder->SetContent(ToastText);
    ToastBorder->SetVisibility(ESlateVisibility::Collapsed);

    UBorder* ActionBorder = AddPanel(WidgetTree, Canvas, FAnchors(0.5f, 1), FVector2D(0.5f, 1),
        FMargin(0, -24, 1100, 155));
    if (UCanvasPanelSlot* ActionSlot = Cast<UCanvasPanelSlot>(ActionBorder->Slot))
    {
        ActionSlot->SetAutoSize(true);
        ActionSlot->SetPosition(FVector2D(0, -24));
    }
    UVerticalBox* ActionPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ActionBorder->SetContent(ActionPanel);
    UCommonTextBlock* ActionTitle = AddText(ActionPanel, TEXT("ACTIONS"), 18);
    ActionTitle->SetJustification(ETextJustify::Center);
    UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>();
    UVerticalBoxSlot* ButtonsRow = ActionPanel->AddChildToVerticalBox(Buttons);
    ButtonsRow->SetHorizontalAlignment(HAlign_Center);

    auto AddAction = [this, Buttons](const FString& Label)
    {
        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* BoxSlot = Buttons->AddChildToHorizontalBox(Box);
        BoxSlot->SetPadding(FMargin(5));
        BoxSlot->SetHorizontalAlignment(HAlign_Center);
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
    if (UTexture2D* CardAtlas = LoadObject<UTexture2D>(nullptr, TEXT("/Game/UI/development-card-icons-atlas.development-card-icons-atlas")))
    {
        UImage* CardIcons = WidgetTree->ConstructWidget<UImage>();
        CardIcons->SetBrushFromTexture(CardAtlas, true);
        CardIcons->SetDesiredSizeOverride(FVector2D(560.0f, 155.0f));
        DevelopmentPanel->AddChildToVerticalBox(CardIcons);
        UCommonTextBlock* CardLegend = AddText(DevelopmentPanel,
            TEXT("KNIGHT          ROADS          PLENTY          MONOPOLY          VICTORY"), 12);
        CardLegend->SetJustification(ETextJustify::Center);
    }
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
    BankRateText = AddText(TradePanel, FString(), 14);
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

    UVerticalBox* SetupPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(SetupPanel);
    AddText(SetupPanel, TEXT("CATAN"), 32);
    AddText(SetupPanel,
        TEXT("Play against local bots, host a discoverable LAN lobby, or join by search/IP."), 16);
    PlayerCount = WidgetTree->ConstructWidget<UComboBoxString>();
    PlayerCount->AddOption(TEXT("2 players"));
    PlayerCount->AddOption(TEXT("3 players"));
    PlayerCount->AddOption(TEXT("4 players"));
    PlayerCount->SetSelectedIndex(0);
    PlayerCount->OnSelectionChanged.AddDynamic(this, &UCatanHUDWidget::UpdatePlayerCount);
    SetupPanel->AddChildToVerticalBox(PlayerCount);
    constexpr const TCHAR* SlotColors[] = {TEXT("RED"), TEXT("BLUE"), TEXT("YELLOW"), TEXT("GREEN")};
    for (int32 Index = 0; Index < 4; ++Index)
    {
        PlayerSlotLabels.Add(AddText(SetupPanel,
            Index == 0 ? TEXT("YOUR PLAYER NAME — REQUIRED")
                : FString::Printf(TEXT("PLAYER %d — %s"), Index + 1, SlotColors[Index]),
            Index == 0 ? 20 : 16));
        UEditableTextBox* Name = WidgetTree->ConstructWidget<UEditableTextBox>();
        Name->SetText(Index == 0 ? FText::GetEmpty()
            : FText::FromString(FString::Printf(TEXT("Player %d"), Index + 1)));
        Name->SetHintText(FText::FromString(Index == 0
            ? TEXT("Enter your name before hosting or joining") : TEXT("Player name")));
        SetupPanel->AddChildToVerticalBox(Name);
        PlayerNameInputs.Add(Name);
        if (Index > 0)
        {
            Name->SetVisibility(ESlateVisibility::Collapsed);
            PlayerSlotLabels[Index]->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
    LobbyNameInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    LobbyNameInput->SetText(FText::FromString(TEXT("Catan LAN Lobby")));
    LobbyNameInput->SetHintText(FText::FromString(TEXT("Lobby name")));
    SetupPanel->AddChildToVerticalBox(LobbyNameInput);
    UButton* HostGame = AddButton(SetupPanel, TEXT("HOST ONLINE (LAN)"));
    HostGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::HostLanLobby);
    LobbyResults = WidgetTree->ConstructWidget<UComboBoxString>();
    LobbyResults->AddOption(TEXT("No search results yet"));
    LobbyResults->SetSelectedIndex(0);
    SetupPanel->AddChildToVerticalBox(LobbyResults);
    UButton* SearchGame = AddButton(SetupPanel, TEXT("REFRESH LAN LOBBIES"));
    UButton* JoinGame = AddButton(SetupPanel, TEXT("JOIN SELECTED"));
    SearchGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::FindLanLobbies);
    JoinGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinSelectedLobby);
    ManualAddressInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    ManualAddressInput->SetHintText(FText::FromString(TEXT("Host address, e.g. 192.168.1.20:7777")));
    SetupPanel->AddChildToVerticalBox(ManualAddressInput);
    UButton* JoinAddress = AddButton(SetupPanel, TEXT("JOIN BY ADDRESS"));
    JoinAddress->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinManualLobby);
    UButton* Bots = AddButton(SetupPanel, TEXT("PLAY AGAINST BOTS"));
    Bots->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartBotMatch);
    NetworkStatusText = AddText(SetupPanel, TEXT("LAN ready"), 14);

    UVerticalBox* ConfirmationPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(ConfirmationPanel);
    AddText(ConfirmationPanel, TEXT("CONFIRM ACTION"), 30);
    ConfirmationText = AddText(ConfirmationPanel, FString(), 19);
    UButton* ConfirmAction = AddButton(ConfirmationPanel, TEXT("CONFIRM"));
    UButton* CancelAction = AddButton(ConfirmationPanel, TEXT("CANCEL"));
    ConfirmAction->OnClicked.AddDynamic(this, &UCatanHUDWidget::ConfirmExpensiveAction);
    CancelAction->OnClicked.AddDynamic(this, &UCatanHUDWidget::CancelExpensiveAction);

    UVerticalBox* LobbyPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(LobbyPanel);
    AddText(LobbyPanel, TEXT("LAN LOBBY"), 32);
    LobbyAddressText = AddText(LobbyPanel, TEXT("Host address"), 16);
    LobbyPlayersText = AddText(LobbyPanel, TEXT("Waiting for players..."), 20);
    ReadyButton = AddButton(LobbyPanel, TEXT("READY"));
    StartLobbyButton = AddButton(LobbyPanel, TEXT("START GAME"));
    UButton* LeaveLobbyButton = AddButton(LobbyPanel, TEXT("LEAVE LOBBY"));
    ReadyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ToggleLobbyReady);
    StartLobbyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartLobbyMatch);
    LeaveLobbyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::LeaveLobby);
    ModalBorder->SetVisibility(ESlateVisibility::Collapsed);

    AvailabilityText = AddText(ActionPanel, FString(), 14);

    SettlementButton->SetToolTipText(FText::FromString(TEXT("Build a settlement: wood + clay + hay + sheep")));
    RoadButton->SetToolTipText(FText::FromString(TEXT("Build a road: wood + clay")));
    CityButton->SetToolTipText(FText::FromString(TEXT("Upgrade your settlement to a city: 2 hay + 3 ore")));
    BuyCardButton->SetToolTipText(FText::FromString(TEXT("Buy a development card: hay + sheep + ore")));
    KnightButton->SetToolTipText(FText::FromString(TEXT("Move the robber and steal from an adjacent player")));
    RoadBuildingButton->SetToolTipText(FText::FromString(TEXT("Place two roads for free")));
    YearOfPlentyButton->SetToolTipText(FText::FromString(TEXT("Take the two selected resources")));
    MonopolyButton->SetToolTipText(FText::FromString(TEXT("Take the selected resource from every opponent")));
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
    if (NetworkStatusText && NetworkSubsystem)
    {
        NetworkStatusText->SetText(FText::FromString(NetworkSubsystem->GetStatus()));
        if (LobbyResults)
        {
            const int32 Previous = LobbyResults->GetSelectedIndex();
            LobbyResults->ClearOptions();
            const TArray<FCatanDiscoveredLobby>& Results = NetworkSubsystem->GetDiscoveredLobbies();
            for (const FCatanDiscoveredLobby& Lobby : Results)
                LobbyResults->AddOption(FString::Printf(TEXT("%s — %d/%d — %d ms"),
                    *Lobby.Name, Lobby.Players, Lobby.Capacity, Lobby.PingMs));
            if (Results.IsEmpty()) LobbyResults->AddOption(TEXT("No LAN lobbies found"));
            LobbyResults->SetSelectedIndex(FMath::Clamp(Previous, 0, LobbyResults->GetOptionCount() - 1));
        }
    }
    if (const ACatanGameState* NetworkState = GetWorld() ? GetWorld()->GetGameState<ACatanGameState>() : nullptr;
        NetworkState && NetworkState->NetworkMode == ECatanNetworkMode::Lobby)
    {
        bSetupPanelOpen = false;
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(8);
        FString Rows;
        bool bAllReady = NetworkState->LobbyPlayers.Num() >= 2;
        bool bLocalReady = false;
        bool bLocalHost = false;
        const APlayerController* LocalController = GetOwningPlayer();
        const ACatanPlayerState* LocalState = LocalController ? LocalController->GetPlayerState<ACatanPlayerState>() : nullptr;
        for (const FCatanLobbyPlayerView& Player : NetworkState->LobbyPlayers)
        {
            Rows += FString::Printf(TEXT("%s %s%s\n"), Player.bReady ? TEXT("✓") : TEXT("○"),
                *Player.Name, Player.bHost ? TEXT("  [HOST]") : TEXT(""));
            bAllReady = bAllReady && Player.bReady;
            if (LocalState && Player.Name == LocalState->GetPlayerName())
            {
                bLocalReady = Player.bReady;
                bLocalHost = Player.bHost;
            }
        }
        LobbyPlayersText->SetText(FText::FromString(Rows + TEXT("\n2–4 players; everyone must be ready.")));
        LobbyAddressText->SetText(FText::FromString(FString::Printf(TEXT("Share this address: %s"),
            NetworkSubsystem ? *NetworkSubsystem->GetLocalAddress() : TEXT("port 7777"))));
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(ReadyButton->GetChildAt(0)))
            Label->SetText(FText::FromString(bLocalReady ? TEXT("NOT READY") : TEXT("READY")));
        StartLobbyButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        StartLobbyButton->SetIsEnabled(bLocalHost && bAllReady && NetworkState->LobbyPlayers.Num() <= 4);
        return;
    }
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const bool bLocalTurn = GameSubsystem->CanLocalPlayerAct(View);
    PhaseText->SetText(FText::FromString(FString::Printf(
        TEXT("%s\nCurrent: %s"), *PhaseTitle(View.Phase), *View.CurrentPlayer)));
    DiceText->SetText(View.FirstDie > 0
        ? FText::FromString(FString::Printf(TEXT("Dice: %d + %d = %d"), View.FirstDie, View.SecondDie, View.FirstDie + View.SecondDie))
        : FText::GetEmpty());
    HintText->SetText(FText::FromString(bLocalTurn
        ? PhaseHint(View.Phase)
        : FString::Printf(TEXT("Waiting for %s"), *View.CurrentPlayer)));
    StatusText->SetText(FText::FromString(View.StatusMessage));
    if (!PreviousToastStatus.IsEmpty() && PreviousToastStatus != View.StatusMessage)
    {
        ToastText->SetText(FText::FromString(View.StatusMessage));
        ToastBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
        ToastBorder->SetRenderOpacity(1.0f);
        ToastRemaining = 2.2f;
    }
    PreviousToastStatus = View.StatusMessage;
    FString Events;
    for (int32 Index = View.EventLog.Num() - 1; Index >= 0; --Index)
    {
        Events += FString::Printf(TEXT("• %s\n"), *View.EventLog[Index]);
    }
    EventText->SetText(FText::FromString(Events));

    FString Players;
    FString ResourceDigest;
    const FCatanPlayerView* VisibleLocalPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsLocalPlayer && Player.bResourcesVisible; });
    if (VisibleLocalPlayer)
    {
        const int32 Counts[] = {VisibleLocalPlayer->Resources.Wood, VisibleLocalPlayer->Resources.Clay,
            VisibleLocalPlayer->Resources.Hay, VisibleLocalPlayer->Resources.Sheep, VisibleLocalPlayer->Resources.Stone};
        constexpr const TCHAR* Symbols[] = {TEXT("W"), TEXT("C"), TEXT("H"), TEXT("S"), TEXT("O")};
        for (int32 Index = 0; Index < ResourceCountTexts.Num() && Index < 5; ++Index)
            ResourceCountTexts[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s  %d"), Symbols[Index], Counts[Index])));
        HandTitleText->SetText(FText::FromString(FString::Printf(TEXT("YOUR HAND — %d RESOURCE CARDS"),
            VisibleLocalPlayer->ResourceCards)));
        DevelopmentHandText->SetText(FText::FromString(FString::Printf(
            TEXT("DEV %d  |  Knight %d  Roads %d  Plenty %d  Monopoly %d"),
            VisibleLocalPlayer->DevelopmentCards, VisibleLocalPlayer->Knights,
            VisibleLocalPlayer->RoadBuildingCards, VisibleLocalPlayer->YearOfPlentyCards,
            VisibleLocalPlayer->MonopolyCards)));
        if (bHavePreviousLocalResources)
        {
            const int32 Before[] = {PreviousLocalResources.Wood, PreviousLocalResources.Clay,
                PreviousLocalResources.Hay, PreviousLocalResources.Sheep, PreviousLocalResources.Stone};
            constexpr const TCHAR* ResourceNames[] = {TEXT("wood"), TEXT("clay"), TEXT("hay"), TEXT("sheep"), TEXT("ore")};
            FString Changes;
            for (int32 Index = 0; Index < 5; ++Index)
            {
                const int32 Delta = Counts[Index] - Before[Index];
                if (Delta != 0) Changes += FString::Printf(TEXT("%s%+d %s"),
                    Changes.IsEmpty() ? TEXT("") : TEXT(", "), Delta, ResourceNames[Index]);
            }
            if (!Changes.IsEmpty())
            {
                ToastText->SetText(FText::FromString(FString::Printf(TEXT("YOUR RESOURCES: %s"), *Changes)));
                ToastBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
                ToastBorder->SetRenderOpacity(1.0f);
                ToastRemaining = 2.8f;
            }
        }
        PreviousLocalResources = VisibleLocalPlayer->Resources;
        bHavePreviousLocalResources = true;
    }
    for (const FCatanPlayerView& Player : View.Players)
    {
        FString Awards;
        if (Player.bHasLongestRoad) Awards += TEXT("  ★ LONGEST ROAD");
        if (Player.bHasLargestArmy) Awards += TEXT("  ★ LARGEST ARMY");
        Players += FString::Printf(TEXT("%s %s  |  VP %d  RES %d  DEV %d%s\n"),
            Player.bIsCurrent ? TEXT("▶") : TEXT(" "),
            *FString::Printf(TEXT("%s%s%s"), *Player.Name,
                Player.bIsLocalPlayer ? TEXT(" [YOU]") : TEXT(""), Player.bIsBot ? TEXT(" [BOT]") : TEXT("")),
            Player.VictoryPoints, Player.ResourceCards, Player.DevelopmentCards, *Awards);
        Players += FString::Printf(TEXT("Pieces: %d settlements, %d cities, %d roads\n\n"),
            Player.FreeSettlements, Player.FreeCities, Player.FreeRoads);
        if (Player.bResourcesVisible)
            ResourceDigest = FString::Printf(TEXT("%d/%d/%d/%d/%d"),
                Player.Resources.Wood, Player.Resources.Clay, Player.Resources.Hay,
                Player.Resources.Sheep, Player.Resources.Stone);
    }
    PlayersText->SetText(FText::FromString(Players));
    if (!PreviousResourceDigest.IsEmpty() && PreviousResourceDigest != ResourceDigest)
        ResourcePulseRemaining = 0.45f;
    PreviousResourceDigest = ResourceDigest;

    const bool bRoll = View.Phase == ECatanGamePhase::RollDice;
    const bool bPlay = View.Phase == ECatanGamePhase::CommonPlay;
    const FCatanPlayerView* CurrentPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsCurrent; });
    const FCatanPlayerView* LocalPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsLocalPlayer; });
    const FCatanResourceView EmptyResources;
    const FCatanResourceView& Have = LocalPlayer && LocalPlayer->bResourcesVisible
        ? LocalPlayer->Resources : EmptyResources;
    const bool bCanRoad = CanAfford(Have, 1, 1, 0, 0, 0);
    const bool bCanSettlement = CanAfford(Have, 1, 1, 1, 1, 0);
    const bool bCanCity = CanAfford(Have, 0, 0, 2, 0, 3);
    const bool bCanCard = CanAfford(Have, 0, 0, 1, 1, 1);
    auto SetActionVisible = [](UButton* Button, bool bVisible)
    {
        if (!Button) return;
        Button->SetIsEnabled(bVisible);
        if (UWidget* SlotWidget = Button->GetParent())
            SlotWidget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        else
            Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    };
    SetActionVisible(RollButton, bLocalTurn && bRoll);
    SetActionVisible(SettlementButton, bLocalTurn && bPlay && bCanSettlement
        && LocalPlayer && LocalPlayer->FreeSettlements > 0);
    SetActionVisible(RoadButton, bLocalTurn && ((bPlay && bCanRoad
        && LocalPlayer && LocalPlayer->FreeRoads > 0) || View.Phase == ECatanGamePhase::RoadBuilding));
    SetActionVisible(CityButton, bLocalTurn && bPlay && bCanCity
        && LocalPlayer && LocalPlayer->FreeCities > 0);
    SetActionVisible(BuyCardButton, bLocalTurn && bPlay && bCanCard);
    SetActionVisible(TradeButton, bLocalTurn && bPlay);
    SetActionVisible(PassButton, bLocalTurn && bPlay);
    BuildCostText->SetText(FText::FromString(
        CostLine(TEXT("ROAD"), Have, 1, 1, 0, 0, 0) + TEXT("\n")
        + CostLine(TEXT("SETTLEMENT"), Have, 1, 1, 1, 1, 0) + TEXT("\n")
        + CostLine(TEXT("CITY"), Have, 0, 0, 2, 0, 3) + TEXT("\n")
        + CostLine(TEXT("DEV CARD"), Have, 0, 0, 1, 1, 1)));
    FString Availability;
    if (!bPlay && !bRoll) Availability = PhaseHint(View.Phase);
    else if (bRoll) Availability = TEXT("Roll the dice before building or trading.");
    else
    {
        TArray<FString> Missing;
        if (!bCanRoad) Missing.Add(TEXT("road"));
        if (!bCanSettlement) Missing.Add(TEXT("settlement"));
        if (!bCanCity) Missing.Add(TEXT("city"));
        if (!bCanCard) Missing.Add(TEXT("development card"));
        Availability = Missing.IsEmpty()
            ? TEXT("All purchases are affordable. Choose an action.")
            : FString::Printf(TEXT("Need more resources for: %s."), *FString::Join(Missing, TEXT(", ")));
    }
    AvailabilityText->SetText(FText::FromString(Availability));
    const int32 ReadyCards = LocalPlayer
        ? LocalPlayer->Knights + LocalPlayer->RoadBuildingCards
            + LocalPlayer->YearOfPlentyCards + LocalPlayer->MonopolyCards
        : 0;
    SetActionVisible(UseCardButton, bLocalTurn && (bPlay || bRoll) && ReadyCards > 0);
    auto MarkSelected = [&View](UButton* Button, ECatanBoardAction Action)
    {
        Button->SetBackgroundColor(View.BoardAction == Action
            ? FLinearColor(0.92f, 0.58f, 0.08f, 1.0f)
            : FLinearColor(0.10f, 0.24f, 0.42f, 1.0f));
    };
    MarkSelected(SettlementButton, ECatanBoardAction::BuildSettlement);
    MarkSelected(RoadButton, ECatanBoardAction::BuildRoad);
    MarkSelected(CityButton, ECatanBoardAction::BuildCity);

    if (bSetupPanelOpen)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(6);
        UpdatePlayerCount(PlayerCount->GetSelectedOption(), ESelectInfo::Direct);
    }
    else if (View.Phase == ECatanGamePhase::Finished)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(5);
        FString Standings = FString::Printf(TEXT("%s WINS!\n\nFINAL SCORE\n"), *View.Winner);
        for (const FCatanPlayerView& Player : View.Players)
        {
            Standings += FString::Printf(
                TEXT("%s — %d VP | %d dev | %d resources\n  %d settlements, %d cities, %d roads remaining\n"),
                *Player.Name, Player.VictoryPoints, Player.DevelopmentCards,
                Player.ResourceCards,
                Player.FreeSettlements, Player.FreeCities, Player.FreeRoads);
        }
        WinnerText->SetText(FText::FromString(Standings));
        bDevelopmentPanelOpen = false;
        bTradePanelOpen = false;
    }
    else if (PendingExpensiveAction != 0)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(7);
        ConfirmationText->SetText(FText::FromString(PendingExpensiveAction == 1
            ? TEXT("Upgrade a settlement to a city?\nThis costs 2 hay and 3 ore. After confirming, choose your settlement on the board.")
            : TEXT("Buy a random development card?\nThis costs 1 hay, 1 sheep and 1 ore.")));
    }
    else if (View.Phase == ECatanGamePhase::DropCards && bLocalTurn && LocalPlayer)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(0);
        DropTitle->SetText(FText::FromString(FString::Printf(
            TEXT("DISCARD %d RESOURCES — %s"), View.RequiredDiscardCount, *View.CurrentPlayer)));
        const int32 Holdings[] = {
            LocalPlayer->Resources.Wood, LocalPlayer->Resources.Clay, LocalPlayer->Resources.Hay,
            LocalPlayer->Resources.Sheep, LocalPlayer->Resources.Stone
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
    else if (bLocalTurn && View.PendingRobberHex != INDEX_NONE && !View.RobberVictims.IsEmpty())
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
    else if (bDevelopmentPanelOpen && LocalPlayer && bLocalTurn && (bPlay || bRoll))
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(2);
        KnightButton->SetIsEnabled(LocalPlayer->Knights > 0);
        RoadBuildingButton->SetIsEnabled(bPlay && LocalPlayer->RoadBuildingCards > 0);
        YearOfPlentyButton->SetIsEnabled(bPlay && LocalPlayer->YearOfPlentyCards > 0);
        MonopolyButton->SetIsEnabled(bPlay && LocalPlayer->MonopolyCards > 0);
    }
    else if (bTradePanelOpen && LocalPlayer && bLocalTurn && bPlay)
    {
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(3);
        BankRateText->SetText(FText::FromString(FString::Printf(
            TEXT("Your bank rates: Wood %d:1 | Clay %d:1 | Hay %d:1 | Sheep %d:1 | Ore %d:1"),
            LocalPlayer->TradeRates.Wood, LocalPlayer->TradeRates.Clay,
            LocalPlayer->TradeRates.Hay, LocalPlayer->TradeRates.Sheep,
            LocalPlayer->TradeRates.Stone)));
        const int32 Holdings[] = {
            LocalPlayer->Resources.Wood, LocalPlayer->Resources.Clay, LocalPlayer->Resources.Hay,
            LocalPlayer->Resources.Sheep, LocalPlayer->Resources.Stone
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

void UCatanHUDWidget::HostLanLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    NetworkSubsystem->HostLobby(PlayerName, LobbyNameInput->GetText().ToString());
}

void UCatanHUDWidget::StartBotMatch()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    const int32 TotalPlayers = PlayerCount ? PlayerCount->GetSelectedIndex() + 2 : 2;
    if (ACatanGameMode* Mode = GetWorld()->GetAuthGameMode<ACatanGameMode>())
    {
        bSetupPanelOpen = false;
        Mode->StartSinglePlayerGame(PlayerName, TotalPlayers - 1);
    }
}

void UCatanHUDWidget::FindLanLobbies()
{
    NetworkSubsystem->FindLobbies();
}

void UCatanHUDWidget::JoinSelectedLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    NetworkSubsystem->JoinLobby(LobbyResults->GetSelectedIndex(), PlayerName);
}

void UCatanHUDWidget::JoinManualLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    NetworkSubsystem->JoinManual(ManualAddressInput->GetText().ToString(), PlayerName);
}

bool UCatanHUDWidget::GetValidatedPlayerName(FString& OutName)
{
    OutName = PlayerNameInputs.IsValidIndex(0)
        ? PlayerNameInputs[0]->GetText().ToString().TrimStartAndEnd().Left(24)
        : FString();
    if (!OutName.IsEmpty()) return true;
    ToastText->SetText(FText::FromString(TEXT("Enter your player name first")));
    ToastBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
    ToastBorder->SetRenderOpacity(1.0f);
    ToastRemaining = 3.0f;
    if (PlayerNameInputs.IsValidIndex(0)) PlayerNameInputs[0]->SetKeyboardFocus();
    return false;
}

void UCatanHUDWidget::ToggleLobbyReady()
{
    if (ACatanPlayerController* Controller = Cast<ACatanPlayerController>(GetOwningPlayer()))
    {
        const ACatanPlayerState* State = Controller->GetPlayerState<ACatanPlayerState>();
        Controller->ServerSetLobbyReady(!(State && State->bLobbyReady));
    }
}

void UCatanHUDWidget::StartLobbyMatch()
{
    if (ACatanPlayerController* Controller = Cast<ACatanPlayerController>(GetOwningPlayer())) Controller->ServerStartLobbyGame();
}

void UCatanHUDWidget::LeaveLobby()
{
    NetworkSubsystem->LeaveToMenu();
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
    PendingExpensiveAction = 1;
    Refresh();
}

void UCatanHUDWidget::BuyDevelopmentCard()
{
    PendingExpensiveAction = 2;
    Refresh();
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
    bSetupPanelOpen = true;
    bDevelopmentPanelOpen = false;
    bTradePanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::ConfirmNewGame()
{
    const int32 Count = PlayerCount ? PlayerCount->GetSelectedIndex() + 2 : 2;
    TArray<FString> Names;
    TSet<FString> UsedNames;
    for (int32 Index = 0; Index < Count && Index < PlayerNameInputs.Num(); ++Index)
    {
        FString Name = PlayerNameInputs[Index]->GetText().ToString().TrimStartAndEnd();
        if (Name.IsEmpty()) Name = FString::Printf(TEXT("Player %d"), Index + 1);
        if (UsedNames.Contains(Name))
        {
            ToastText->SetText(FText::FromString(TEXT("Player names must be unique")));
            ToastBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
            ToastBorder->SetRenderOpacity(1.0f);
            ToastRemaining = 3.0f;
            return;
        }
        UsedNames.Add(Name);
        Names.Add(Name);
    }
    bSetupPanelOpen = false;
    GameSubsystem->StartLocalGame(Names);
}

void UCatanHUDWidget::UpdatePlayerCount(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    for (int32 Index = 0; Index < PlayerNameInputs.Num(); ++Index)
    {
        PlayerNameInputs[Index]->SetVisibility(Index == 0
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (PlayerSlotLabels.IsValidIndex(Index))
            PlayerSlotLabels[Index]->SetVisibility(Index == 0
                ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UCatanHUDWidget::ConfirmExpensiveAction()
{
    const int32 Action = PendingExpensiveAction;
    PendingExpensiveAction = 0;
    if (Action == 1)
    {
        GameSubsystem->SelectBoardAction(ECatanBoardAction::BuildCity);
    }
    else if (Action == 2)
    {
        FString Error;
        GameSubsystem->TryBuyDevelopmentCard(Error);
    }
    Refresh();
}

void UCatanHUDWidget::CancelExpensiveAction()
{
    PendingExpensiveAction = 0;
    Refresh();
}

void UCatanHUDWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
