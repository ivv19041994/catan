#include "CatanHUDWidget.h"

#include "CatanGameSubsystem.h"
#include "CatanNetworkSubsystem.h"
#include "CatanGameState.h"
#include "CatanGameMode.h"
#include "CatanPlayerController.h"
#include "CatanPlayerState.h"
#include "CatanTradePolicy.h"
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
#include "Components/SizeBox.h"
#include "Components/SafeZone.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/Texture2D.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

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

FLinearColor ResourceColor(int32 Index)
{
    static const FLinearColor Colors[] = {
        FLinearColor(0.08f, 0.52f, 0.16f), FLinearColor(0.76f, 0.18f, 0.05f),
        FLinearColor(0.95f, 0.70f, 0.06f), FLinearColor(0.48f, 0.82f, 0.28f),
        FLinearColor(0.42f, 0.48f, 0.58f)
    };
    return Colors[FMath::Clamp(Index, 0, 4)];
}

void ConfigureIntegerInput(USpinBox* Input, int32 MaxValue = 99)
{
    Input->SetValue(0.0f);
    Input->SetMinValue(0.0f);
    Input->SetMaxValue(static_cast<float>(MaxValue));
    Input->SetMinSliderValue(0.0f);
    Input->SetMaxSliderValue(static_cast<float>(MaxValue));
    Input->SetDelta(1.0f);
    Input->SetMinFractionalDigits(0);
    Input->SetMaxFractionalDigits(0);
    Input->SetAlwaysUsesDeltaSnap(true);
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

bool UCatanHUDWidget::IsModalOpen() const
{
    return ModalBorder && ModalBorder->GetVisibility() != ESlateVisibility::Collapsed;
}

void UCatanHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    const FVector2D ViewSize = MyGeometry.GetLocalSize();
    const bool bShouldUseCompactLayout = PLATFORM_ANDROID || ViewSize.X < 1700.0f || ViewSize.Y < 900.0f;
    if (!bAdaptiveLayoutInitialized || bShouldUseCompactLayout != bCompactLayout)
    {
        ApplyAdaptiveLayout(bShouldUseCompactLayout);
    }
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
    USafeZone* SafeZone = WidgetTree->ConstructWidget<USafeZone>();
    SafeZone->SetSidesToPad(true, true, true, true);
    UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>();
    SafeZone->AddChild(Canvas);
    WidgetTree->RootWidget = SafeZone;

    InfoBorder = AddPanel(WidgetTree, Canvas, FAnchors(0, 0), FVector2D::ZeroVector,
        FMargin(24, 24, 470, 235));
    UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>();
    InfoBorder->SetContent(Info);
    AddText(Info, TEXT("CATAN"), 32);
    PhaseText = AddText(Info, TEXT("Starting..."), 22);
    DiceText = AddText(Info, FString(), 19);
    LeftDetailsButton = AddButton(Info, TEXT("SHOW EVENTS & HELP"));
    LeftDetailsButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ToggleLeftDetails);
    InfoDetails = WidgetTree->ConstructWidget<UVerticalBox>();
    Info->AddChildToVerticalBox(InfoDetails);
    HintText = AddText(InfoDetails, FString(), 17);
    StatusText = AddText(InfoDetails, FString(), 16);
    StatusText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.72f, 0.18f)));

    EventBorder = AddPanel(WidgetTree, Canvas, FAnchors(0, 0), FVector2D::ZeroVector,
        FMargin(24, 280, 470, 290));
    UVerticalBox* EventPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    EventBorder->SetContent(EventPanel);
    AddText(EventPanel, TEXT("EVENTS"), 20);
    EventText = AddText(EventPanel, FString(), 15);

    PlayerBorder = AddPanel(WidgetTree, Canvas, FAnchors(1, 0), FVector2D(1, 0),
        FMargin(-24, 24, 440, 455));
    UVerticalBox* PlayerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayerBorder->SetContent(PlayerPanel);
    HandTitleText = AddText(PlayerPanel, TEXT("YOUR HAND"), 22);
    UHorizontalBox* ResourceBadges = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayerPanel->AddChildToVerticalBox(ResourceBadges);
    struct FResourceBadge { const TCHAR* Name; FLinearColor Color; };
    const FResourceBadge Badges[] = {
        {TEXT("WOOD"), FLinearColor(0.08f, 0.52f, 0.16f)},
        {TEXT("CLAY"), FLinearColor(0.76f, 0.18f, 0.05f)},
        {TEXT("HAY"), FLinearColor(0.95f, 0.70f, 0.06f)},
        {TEXT("SHEEP"), FLinearColor(0.48f, 0.82f, 0.28f)},
        {TEXT("ORE"), FLinearColor(0.42f, 0.48f, 0.58f)}
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
        Count->SetText(FText::AsNumber(0));
        Count->SetJustification(ETextJustify::Center);
        FSlateFontInfo SymbolFont = Count->GetFont(); SymbolFont.Size = 22; Count->SetFont(SymbolFont);
        Icon->SetContent(Count);
        ResourceCountTexts.Add(Count);
        BadgeBox->AddChildToVerticalBox(Icon);
        UCommonTextBlock* Name = AddText(BadgeBox, Badge.Name, 10);
        Name->SetJustification(ETextJustify::Center);
    }
    RightDetailsButton = AddButton(PlayerPanel, TEXT("SHOW PLAYERS & COSTS"));
    RightDetailsButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ToggleRightDetails);
    PlayerDetails = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayerPanel->AddChildToVerticalBox(PlayerDetails);
    DevelopmentHandText = AddText(PlayerDetails, TEXT("Development: 0"), 14);
    AddText(PlayerDetails, TEXT("PLAYERS"), 20);
    PlayersText = AddText(PlayerDetails, FString(), 17);

    CostBorder = AddPanel(WidgetTree, Canvas, FAnchors(1, 0), FVector2D(1, 0),
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

    ActionBorder = AddPanel(WidgetTree, Canvas, FAnchors(0.5f, 1), FVector2D(0.5f, 1),
        FMargin(0, -24, 1100, 155));
    if (UCanvasPanelSlot* ActionSlot = Cast<UCanvasPanelSlot>(ActionBorder->Slot))
    {
        ActionSlot->SetAutoSize(true);
        ActionSlot->SetPosition(FVector2D(0, -24));
    }
    UVerticalBox* ActionPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ActionBorder->SetContent(ActionPanel);
    ActionTitle = AddText(ActionPanel, TEXT("ACTIONS"), 18);
    ActionTitle->SetJustification(ETextJustify::Center);
    ActionButtons = WidgetTree->ConstructWidget<UWrapBox>();
    ActionButtons->SetHorizontalAlignment(HAlign_Center);
    ActionButtons->SetInnerSlotPadding(FVector2D(5.0f, 4.0f));
    ActionButtons->SetExplicitWrapSize(true);
    ActionButtons->SetWrapSize(1100.0f);
    UVerticalBoxSlot* ButtonsRow = ActionPanel->AddChildToVerticalBox(ActionButtons);
    ButtonsRow->SetHorizontalAlignment(HAlign_Center);

    auto AddAction = [this](const FString& Label)
    {
        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        UWrapBoxSlot* BoxSlot = ActionButtons->AddChildToWrapBox(Box);
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
        FMargin(0, 0, 1040, 760));
    ModalBorder->SetClipping(EWidgetClipping::ClipToBounds);
    UScrollBox* ModalScroll = WidgetTree->ConstructWidget<UScrollBox>();
    ModalScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    ModalSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
    ModalScroll->AddChild(ModalSwitcher);
    ModalBorder->SetContent(ModalScroll);

    UVerticalBox* DropPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(DropPanel);
    DropTitle = AddText(DropPanel, TEXT("DISCARD RESOURCES"), 25);
    constexpr const TCHAR* ResourceNames[] = {TEXT("Wood"), TEXT("Clay"), TEXT("Hay"), TEXT("Sheep"), TEXT("Stone")};
    for (int32 ResourceIndex = 0; ResourceIndex < 5; ++ResourceIndex)
    {
        const TCHAR* ResourceName = ResourceNames[ResourceIndex];
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetBrushColor(ResourceColor(ResourceIndex));
        Card->SetPadding(FMargin(10));
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Card->SetContent(Row);
        UVerticalBoxSlot* RowSlot = DropPanel->AddChildToVerticalBox(Card);
        RowSlot->SetPadding(FMargin(3, 5));
        UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Label->SetText(FText::FromString(ResourceName));
        FSlateFontInfo Font = Label->GetFont(); Font.Size = 18; Label->SetFont(Font);
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        USpinBox* Input = WidgetTree->ConstructWidget<USpinBox>();
        ConfigureIntegerInput(Input, 0);
        USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
        InputSize->SetMinDesiredWidth(180.0f);
        InputSize->SetMinDesiredHeight(68.0f);
        InputSize->AddChild(Input);
        Row->AddChildToHorizontalBox(InputSize);
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
    DevelopmentAvailabilityText = AddText(DevelopmentPanel, FString(), 15);
    KnightButton = AddButton(DevelopmentPanel, TEXT("PLAY KNIGHT"));
    RoadBuildingButton = AddButton(DevelopmentPanel, TEXT("PLAY ROAD BUILDING"));
    DevelopmentResourcePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    DevelopmentPanel->AddChildToVerticalBox(DevelopmentResourcePanel);
    AddText(DevelopmentResourcePanel, TEXT("Resources for Year of Plenty / Monopoly"), 16);
    FirstResource = WidgetTree->ConstructWidget<UComboBoxString>();
    SecondResource = WidgetTree->ConstructWidget<UComboBoxString>();
    for (const TCHAR* ResourceName : ResourceNames)
    {
        FirstResource->AddOption(ResourceName);
        SecondResource->AddOption(ResourceName);
    }
    FirstResource->SetSelectedOption(TEXT("Wood"));
    SecondResource->SetSelectedOption(TEXT("Clay"));
    DevelopmentResourcePanel->AddChildToVerticalBox(FirstResource);
    DevelopmentResourcePanel->AddChildToVerticalBox(SecondResource);
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
    AddText(TradePanel, TEXT("TRADE"), 28)->SetJustification(ETextJustify::Center);
    UHorizontalBox* TradeTabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    TradePanel->AddChildToVerticalBox(TradeTabs);
    auto AddTradeTab = [this, TradeTabs](const FString& Label)
    {
        UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* Slot = TradeTabs->AddChildToHorizontalBox(Box);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        Slot->SetPadding(FMargin(4));
        return AddButton(Box, Label);
    };
    UButton* BankTab = AddTradeTab(TEXT("BANK"));
    UButton* PlayerTab = AddTradeTab(TEXT("OTHER PLAYER"));
    BankTab->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowBankTrade);
    PlayerTab->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowPlayerTrade);
    TradeModeSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
    TradePanel->AddChildToVerticalBox(TradeModeSwitcher);

    auto AddResourceButton = [this](UVerticalBox* Parent, const TCHAR* Name, int32 Index,
        TArray<TObjectPtr<UButton>>& Buttons)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(ResourceColor(Index));
        UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Text->SetText(FText::FromString(Name));
        Text->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 18; Text->SetFont(Font);
        Button->AddChild(Text);
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetMinDesiredWidth(170.0f);
        Size->SetMinDesiredHeight(56.0f);
        Size->AddChild(Button);
        UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Size);
        Slot->SetPadding(FMargin(2));
        Buttons.Add(Button);
    };

    UVerticalBox* BankPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    TradeModeSwitcher->AddChild(BankPanel);
    AddText(BankPanel, TEXT("Choose one resource to give and one to receive"), 18)
        ->SetJustification(ETextJustify::Center);
    BankRateText = AddText(BankPanel, FString(), 15);
    BankRateText->SetJustification(ETextJustify::Center);
    UHorizontalBox* BankColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
    BankPanel->AddChildToVerticalBox(BankColumns);
    UVerticalBox* BankGive = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* BankReceive = WidgetTree->ConstructWidget<UVerticalBox>();
    UHorizontalBoxSlot* GiveSlot = BankColumns->AddChildToHorizontalBox(BankGive);
    GiveSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); GiveSlot->SetPadding(FMargin(8));
    UHorizontalBoxSlot* ReceiveSlot = BankColumns->AddChildToHorizontalBox(BankReceive);
    ReceiveSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ReceiveSlot->SetPadding(FMargin(8));
    AddText(BankGive, TEXT("YOU GIVE"), 20)->SetJustification(ETextJustify::Center);
    AddText(BankReceive, TEXT("YOU RECEIVE"), 20)->SetJustification(ETextJustify::Center);
    for (int32 Index = 0; Index < 5; ++Index)
    {
        AddResourceButton(BankGive, ResourceNames[Index], Index, BankFromButtons);
        AddResourceButton(BankReceive, ResourceNames[Index], Index, BankToButtons);
    }
    BankFromButtons[0]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankFromWood);
    BankFromButtons[1]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankFromClay);
    BankFromButtons[2]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankFromHay);
    BankFromButtons[3]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankFromSheep);
    BankFromButtons[4]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankFromStone);
    BankToButtons[0]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankToWood);
    BankToButtons[1]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankToClay);
    BankToButtons[2]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankToHay);
    BankToButtons[3]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankToSheep);
    BankToButtons[4]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectBankToStone);
    UHorizontalBox* BankActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    BankPanel->AddChildToVerticalBox(BankActions);
    UVerticalBox* BankConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* BankCloseBox = WidgetTree->ConstructWidget<UVerticalBox>();
    BankActions->AddChildToHorizontalBox(BankConfirmBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    BankActions->AddChildToHorizontalBox(BankCloseBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* BankTrade = AddButton(BankConfirmBox, TEXT("CONFIRM BANK TRADE"));
    UButton* BankClose = AddButton(BankCloseBox, TEXT("CLOSE"));
    BankTrade->OnClicked.AddDynamic(this, &UCatanHUDWidget::TradeWithBank);
    BankClose->OnClicked.AddDynamic(this, &UCatanHUDWidget::CloseTrading);

    UVerticalBox* PlayerTradePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    TradeModeSwitcher->AddChild(PlayerTradePanel);
    AddText(PlayerTradePanel, TEXT("OFFER TO"), 18);
    TradingPlayer = WidgetTree->ConstructWidget<UComboBoxString>();
    USizeBox* RecipientSize = WidgetTree->ConstructWidget<USizeBox>();
    RecipientSize->SetMinDesiredHeight(56.0f);
    RecipientSize->AddChild(TradingPlayer);
    PlayerTradePanel->AddChildToVerticalBox(RecipientSize);
    UHorizontalBox* PlayerColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayerTradePanel->AddChildToVerticalBox(PlayerColumns);
    auto AddResourceInputs = [this, PlayerColumns, &ResourceNames](const FString& Label,
        TArray<TObjectPtr<USpinBox>>& Inputs)
    {
        UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>();
        UHorizontalBoxSlot* ColumnSlot = PlayerColumns->AddChildToHorizontalBox(Column);
        ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ColumnSlot->SetPadding(FMargin(8));
        AddText(Column, Label, 20)->SetJustification(ETextJustify::Center);
        for (int32 Index = 0; Index < 5; ++Index)
        {
            UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
            Card->SetBrushColor(ResourceColor(Index));
            Card->SetPadding(FMargin(6));
            UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
            Card->SetContent(Row);
            UCommonTextBlock* Name = WidgetTree->ConstructWidget<UCommonTextBlock>();
            Name->SetText(FText::FromString(ResourceNames[Index]));
            FSlateFontInfo Font = Name->GetFont(); Font.Size = 18; Name->SetFont(Font);
            UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
            NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            NameSlot->SetVerticalAlignment(VAlign_Center);
            USpinBox* Input = WidgetTree->ConstructWidget<USpinBox>();
            ConfigureIntegerInput(Input);
            USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
            InputSize->SetMinDesiredWidth(140.0f);
            InputSize->SetMinDesiredHeight(50.0f);
            InputSize->AddChild(Input);
            Row->AddChildToHorizontalBox(InputSize);
            UVerticalBoxSlot* CardSlot = Column->AddChildToVerticalBox(Card);
            CardSlot->SetPadding(FMargin(2));
            Inputs.Add(Input);
        }
    };
    AddResourceInputs(TEXT("YOU GIVE"), OfferedInputs);
    AddResourceInputs(TEXT("YOU RECEIVE"), RequestedInputs);
    UHorizontalBox* PlayerActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayerTradePanel->AddChildToVerticalBox(PlayerActions);
    UVerticalBox* OfferBox = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* PlayerCloseBox = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayerActions->AddChildToHorizontalBox(OfferBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PlayerActions->AddChildToHorizontalBox(PlayerCloseBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* CreateOffer = AddButton(OfferBox, TEXT("SEND OFFER"));
    UButton* CloseTrade = AddButton(PlayerCloseBox, TEXT("CLOSE"));
    CreateOffer->OnClicked.AddDynamic(this, &UCatanHUDWidget::OfferTrade);
    CloseTrade->OnClicked.AddDynamic(this, &UCatanHUDWidget::CloseTrading);

    UVerticalBox* DealPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(DealPanel);
    AddText(DealPanel, TEXT("ACTIVE PLAYER TRADE"), 25);
    DealText = AddText(DealPanel, FString(), 19);
    AcceptDealButton = AddButton(DealPanel, TEXT("ACCEPT OFFER"));
    CancelDealButton = AddButton(DealPanel, TEXT("DECLINE OFFER"));
    AcceptDealButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::AcceptTrade);
    CancelDealButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::CancelTrade);

    UVerticalBox* WinnerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    ModalSwitcher->AddChild(WinnerPanel);
    AddText(WinnerPanel, TEXT("GAME OVER"), 32);
    WinnerText = AddText(WinnerPanel, FString(), 22);
    UButton* NewGame = AddButton(WinnerPanel, TEXT("NEW GAME"));
    UButton* ExitGame = AddButton(WinnerPanel, TEXT("EXIT"));
    NewGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartNewGame);
    ExitGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::QuitGame);

    SetupSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
    ModalSwitcher->AddChild(SetupSwitcher);

    UVerticalBox* SetupPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(SetupPanel);
    AddText(SetupPanel, TEXT("CATAN"), 32);
    AddText(SetupPanel, TEXT("Choose a game mode"), 20);
    PlayerSlotLabels.Add(AddText(SetupPanel, TEXT("PLAYER NAME"), 18));
    UEditableTextBox* MainName = WidgetTree->ConstructWidget<UEditableTextBox>();
    MainName->SetText(FText::FromString(TEXT("Player")));
    MainName->SetHintText(FText::FromString(TEXT("Player name")));
    MainName->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    SetupPanel->AddChildToVerticalBox(MainName);
    PlayerNameInputs.Add(MainName);
    UButton* OnlineMode = AddButton(SetupPanel, TEXT("ONLINE"));
    UButton* BotMode = AddButton(SetupPanel, TEXT("PLAY AGAINST BOTS"));
    OnlineMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowOnlineSetup);
    BotMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowBotSetup);

    UVerticalBox* OnlinePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(OnlinePanel);
    AddText(OnlinePanel, TEXT("ONLINE — LOCAL NETWORK"), 27);
    AddText(OnlinePanel, TEXT("Host a LAN lobby, find one automatically, or enter its address."), 16);
    LobbyNameInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    LobbyNameInput->SetText(FText::FromString(TEXT("Catan LAN Lobby")));
    LobbyNameInput->SetHintText(FText::FromString(TEXT("Lobby name")));
    LobbyNameInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    OnlinePanel->AddChildToVerticalBox(LobbyNameInput);
    UButton* HostGame = AddButton(OnlinePanel, TEXT("HOST ONLINE (LAN)"));
    HostGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::HostLanLobby);
    LobbyResults = WidgetTree->ConstructWidget<UComboBoxString>();
    LobbyResults->AddOption(TEXT("No search results yet"));
    LobbyResults->SetSelectedIndex(0);
    OnlinePanel->AddChildToVerticalBox(LobbyResults);
    UButton* SearchGame = AddButton(OnlinePanel, TEXT("REFRESH LAN LOBBIES"));
    UButton* JoinGame = AddButton(OnlinePanel, TEXT("JOIN SELECTED"));
    SearchGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::FindLanLobbies);
    JoinGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinSelectedLobby);
    ManualAddressInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    ManualAddressInput->SetHintText(FText::FromString(TEXT("Host address, e.g. 192.168.1.20:7777")));
    ManualAddressInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    OnlinePanel->AddChildToVerticalBox(ManualAddressInput);
    UButton* JoinAddress = AddButton(OnlinePanel, TEXT("JOIN BY ADDRESS"));
    JoinAddress->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinManualLobby);
    UButton* OnlineBack = AddButton(OnlinePanel, TEXT("BACK"));
    OnlineBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowMainSetup);
    NetworkStatusText = AddText(OnlinePanel, TEXT("LAN ready"), 14);

    UVerticalBox* BotPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(BotPanel);
    AddText(BotPanel, TEXT("PLAY AGAINST BOTS"), 27);
    AddText(BotPanel, TEXT("Choose the total number of players."), 17);
    PlayerCount = WidgetTree->ConstructWidget<UComboBoxString>();
    PlayerCount->AddOption(TEXT("2 players"));
    PlayerCount->AddOption(TEXT("3 players"));
    PlayerCount->AddOption(TEXT("4 players"));
    PlayerCount->SetSelectedIndex(0);
    PlayerCount->OnSelectionChanged.AddDynamic(this, &UCatanHUDWidget::UpdatePlayerCount);
    BotPanel->AddChildToVerticalBox(PlayerCount);
    UButton* Bots = AddButton(BotPanel, TEXT("START BOT GAME"));
    Bots->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartBotMatch);
    UButton* BotBack = AddButton(BotPanel, TEXT("BACK"));
    BotBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowMainSetup);

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

void UCatanHUDWidget::ApplyAdaptiveLayout(bool bCompact)
{
    const bool bEnteringCompactLayout = bCompact && (!bAdaptiveLayoutInitialized || !bCompactLayout);
    bCompactLayout = bCompact;
    bAdaptiveLayoutInitialized = true;
    if (!InfoBorder || !EventBorder || !PlayerBorder || !CostBorder) return;

    LeftDetailsButton->SetVisibility(bCompact ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    RightDetailsButton->SetVisibility(bCompact ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (!bCompact)
    {
        bLeftDetailsOpen = true;
        bRightDetailsOpen = true;
    }
    else if (bEnteringCompactLayout)
    {
        bLeftDetailsOpen = false;
        bRightDetailsOpen = false;
    }

    InfoDetails->SetVisibility(!bCompact || bLeftDetailsOpen
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    EventBorder->SetVisibility(!bCompact || bLeftDetailsOpen
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    PlayerDetails->SetVisibility(!bCompact || bRightDetailsOpen
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CostBorder->SetVisibility(!bCompact || bRightDetailsOpen
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(InfoBorder->Slot))
        Slot->SetSize(FVector2D(470.0f, bCompact
            ? (bLeftDetailsOpen ? 330.0f : 260.0f) : 235.0f));
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(EventBorder->Slot))
        Slot->SetPosition(FVector2D(24.0f, bCompact && bLeftDetailsOpen ? 370.0f : 280.0f));
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PlayerBorder->Slot))
        Slot->SetSize(FVector2D(440.0f, bCompact
            ? (bRightDetailsOpen ? 560.0f : 240.0f) : 455.0f));
    if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CostBorder->Slot))
        Slot->SetPosition(FVector2D(-24.0f, bCompact && bRightDetailsOpen ? 610.0f : 500.0f));

    if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(LeftDetailsButton->GetChildAt(0)))
        Label->SetText(FText::FromString(bLeftDetailsOpen ? TEXT("HIDE EVENTS & HELP") : TEXT("SHOW EVENTS & HELP")));
    if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(RightDetailsButton->GetChildAt(0)))
        Label->SetText(FText::FromString(bRightDetailsOpen ? TEXT("HIDE PLAYERS & COSTS") : TEXT("SHOW PLAYERS & COSTS")));

    if (ActionButtons)
    {
        ActionButtons->SetWrapSize(bCompact ? 760.0f : 1100.0f);
        ActionButtons->SetInnerSlotPadding(bCompact ? FVector2D(3.0f, 3.0f) : FVector2D(5.0f, 4.0f));
    }
    if (ActionTitle) ActionTitle->SetVisibility(bCompact ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
    UpdateActionLabels();
}

void UCatanHUDWidget::UpdateActionLabels()
{
    struct FActionLabel
    {
        UButton* Button;
        const TCHAR* Compact;
        const TCHAR* Full;
    };
    const FActionLabel Labels[] = {
        {RollButton, TEXT("ROLL"), TEXT("ROLL DICE")},
        {SettlementButton, TEXT("SETTLE"), TEXT("SETTLEMENT")},
        {RoadButton, TEXT("ROAD"), TEXT("ROAD")},
        {CityButton, TEXT("CITY"), TEXT("CITY")},
        {BuyCardButton, TEXT("BUY DEV"), TEXT("BUY DEV")},
        {UseCardButton, TEXT("DEV"), TEXT("USE DEV")},
        {TradeButton, TEXT("TRADE"), TEXT("TRADE")},
        {PassButton, TEXT("END"), TEXT("END TURN")}
    };
    for (const FActionLabel& Entry : Labels)
    {
        if (UCommonTextBlock* Label = Entry.Button
            ? Cast<UCommonTextBlock>(Entry.Button->GetChildAt(0)) : nullptr)
            Label->SetText(FText::FromString(bCompactLayout ? Entry.Compact : Entry.Full));
    }
}

void UCatanHUDWidget::ToggleLeftDetails()
{
    bLeftDetailsOpen = !bLeftDetailsOpen;
    if (bCompactLayout && bLeftDetailsOpen) bRightDetailsOpen = false;
    ApplyAdaptiveLayout(bCompactLayout);
}

void UCatanHUDWidget::ToggleRightDetails()
{
    bRightDetailsOpen = !bRightDetailsOpen;
    if (bCompactLayout && bRightDetailsOpen) bLeftDetailsOpen = false;
    ApplyAdaptiveLayout(bCompactLayout);
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
    Text->SetMargin(FMargin(16, 12));
    Button->AddChild(Text);
    USizeBox* TouchTarget = WidgetTree->ConstructWidget<USizeBox>();
    TouchTarget->SetMinDesiredWidth(112.0f);
    TouchTarget->SetMinDesiredHeight(56.0f);
    TouchTarget->AddChild(Button);
    UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(TouchTarget);
    Slot->SetPadding(FMargin(2, 4));
    return Button;
}

void UCatanHUDWidget::Refresh()
{
    if (!GameSubsystem || !PhaseText) return;
    SetModalSize(680.0f, 650.0f);
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
        for (int32 Index = 0; Index < ResourceCountTexts.Num() && Index < 5; ++Index)
            ResourceCountTexts[Index]->SetText(FText::AsNumber(Counts[Index]));
        HandTitleText->SetText(FText::FromString(FString::Printf(TEXT("YOUR HAND — %d RESOURCE CARDS"),
            VisibleLocalPlayer->ResourceCards)));
        DevelopmentHandText->SetText(FText::FromString(FString::Printf(
            TEXT("DEV %d  |  Knight %d  Roads %d  Plenty %d  Monopoly %d%s"),
            VisibleLocalPlayer->DevelopmentCards, VisibleLocalPlayer->Knights,
            VisibleLocalPlayer->RoadBuildingCards, VisibleLocalPlayer->YearOfPlentyCards,
            VisibleLocalPlayer->MonopolyCards,
            VisibleLocalPlayer->PendingDevelopmentCards > 0
                ? *FString::Printf(TEXT("  |  %d ready next turn"), VisibleLocalPlayer->PendingDevelopmentCards)
                : TEXT(""))));
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
        && LocalPlayer && LocalPlayer->FreeSettlements > 0 && View.bHasSettlementTarget);
    SetActionVisible(RoadButton, bLocalTurn && ((bPlay && bCanRoad
        && LocalPlayer && LocalPlayer->FreeRoads > 0 && View.bHasRoadTarget)
        || (View.Phase == ECatanGamePhase::RoadBuilding && View.bHasRoadTarget)));
    SetActionVisible(CityButton, bLocalTurn && bPlay && bCanCity
        && LocalPlayer && LocalPlayer->FreeCities > 0 && View.bHasCityTarget);
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
    SetActionVisible(UseCardButton, bLocalTurn && (bPlay || bRoll)
        && LocalPlayer && LocalPlayer->DevelopmentCards > 0);
    if (UCommonTextBlock* Label = UseCardButton
        ? Cast<UCommonTextBlock>(UseCardButton->GetChildAt(0)) : nullptr)
        Label->SetText(FText::FromString(bCompactLayout
            ? TEXT("DEV") : (ReadyCards > 0 ? TEXT("USE DEV") : TEXT("VIEW DEV"))));
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
        SetModalSize(900.0f, 650.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(6);
        UpdatePlayerCount(PlayerCount->GetSelectedOption(), ESelectInfo::Direct);
    }
    else if (View.Phase == ECatanGamePhase::Finished)
    {
        SetModalSize(760.0f, 650.0f);
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
        SetModalSize(680.0f, 420.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(7);
        ConfirmationText->SetText(FText::FromString(PendingExpensiveAction == 1
            ? TEXT("Upgrade a settlement to a city?\nThis costs 2 hay and 3 ore. After confirming, choose your settlement on the board.")
            : TEXT("Buy a random development card?\nThis costs 1 hay, 1 sheep and 1 ore.")));
    }
    else if (View.Phase == ECatanGamePhase::DropCards && bLocalTurn && LocalPlayer)
    {
        SetModalSize(900.0f, 620.0f);
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
            FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
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
        SetModalSize(680.0f, 500.0f);
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
        SetModalSize(850.0f, 420.0f);
        const FString LocalName = LocalPlayer ? LocalPlayer->Name : FString();
        const bool bIsOfferer = LocalName == View.ActiveDeal.OfferingPlayer;
        const bool bIsRecipient = LocalName == View.ActiveDeal.TargetPlayer;
        const bool bCanAccept = bIsRecipient && LocalPlayer && LocalPlayer->bResourcesVisible
            && CatanTradePolicy::CanAfford(LocalPlayer->Resources, View.ActiveDeal.Requested);
        ModalBorder->SetVisibility(bIsOfferer || bIsRecipient
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bIsOfferer || bIsRecipient) ModalSwitcher->SetActiveWidgetIndex(4);
        DealText->SetText(FText::FromString(FString::Printf(
            TEXT("%s offers to %s:\n%s\n\nand requests:\n%s"),
            *View.ActiveDeal.OfferingPlayer, *View.ActiveDeal.TargetPlayer,
            *ResourceSummary(View.ActiveDeal.Offered), *ResourceSummary(View.ActiveDeal.Requested))));
        AcceptDealButton->SetVisibility(bIsRecipient
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        AcceptDealButton->SetIsEnabled(bCanAccept);
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(AcceptDealButton->GetChildAt(0)))
            Label->SetText(FText::FromString(bCanAccept
                ? TEXT("ACCEPT OFFER") : TEXT("NOT ENOUGH RESOURCES")));
        CancelDealButton->SetVisibility(bIsOfferer || bIsRecipient
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(CancelDealButton->GetChildAt(0)))
            Label->SetText(FText::FromString(bIsOfferer ? TEXT("WITHDRAW OFFER") : TEXT("DECLINE OFFER")));
        bTradePanelOpen = false;
    }
    else if (bDevelopmentPanelOpen && LocalPlayer && bLocalTurn && (bPlay || bRoll))
    {
        SetModalSize(760.0f, 650.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(2);
        auto ShowCard = [](UButton* Button, bool bVisible)
        {
            Button->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            Button->SetIsEnabled(bVisible);
        };
        ShowCard(KnightButton, LocalPlayer->Knights > 0);
        ShowCard(RoadBuildingButton, bPlay && LocalPlayer->RoadBuildingCards > 0);
        ShowCard(YearOfPlentyButton, bPlay && LocalPlayer->YearOfPlentyCards > 0);
        ShowCard(MonopolyButton, bPlay && LocalPlayer->MonopolyCards > 0);
        const int32 ReadyCount = LocalPlayer->Knights + LocalPlayer->RoadBuildingCards
            + LocalPlayer->YearOfPlentyCards + LocalPlayer->MonopolyCards;
        const int32 PassiveVictoryCards = FMath::Max(0, LocalPlayer->DevelopmentCards
            - ReadyCount - LocalPlayer->PendingDevelopmentCards);
        FString CardState;
        if (ReadyCount > 0)
            CardState += FString::Printf(TEXT("Ready to play: %d."), ReadyCount);
        if (LocalPlayer->PendingDevelopmentCards > 0)
            CardState += FString::Printf(TEXT("%sBought this turn: %d — available next turn."),
                CardState.IsEmpty() ? TEXT("") : TEXT("\n"), LocalPlayer->PendingDevelopmentCards);
        if (PassiveVictoryCards > 0)
            CardState += FString::Printf(TEXT("%sVictory point cards: %d — passive, they are never played."),
                CardState.IsEmpty() ? TEXT("") : TEXT("\n"), PassiveVictoryCards);
        if (CardState.IsEmpty()) CardState = TEXT("You have no development cards available to play.");
        DevelopmentAvailabilityText->SetText(FText::FromString(CardState));
        DevelopmentResourcePanel->SetVisibility(
            bPlay && (LocalPlayer->YearOfPlentyCards > 0 || LocalPlayer->MonopolyCards > 0)
                ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
    else if (bTradePanelOpen && LocalPlayer && bLocalTurn && bPlay)
    {
        SetModalSize(1040.0f, 760.0f);
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
        const FString PreviousRecipient = TradingPlayer->GetSelectedOption();
        TradingPlayer->ClearOptions();
        for (const FCatanPlayerView& Player : View.Players)
            if (Player.Name != LocalPlayer->Name) TradingPlayer->AddOption(Player.Name);
        if (!PreviousRecipient.IsEmpty()
            && TradingPlayer->FindOptionIndex(PreviousRecipient) != INDEX_NONE)
            TradingPlayer->SetSelectedOption(PreviousRecipient);
        else if (TradingPlayer->GetOptionCount() > 0)
            TradingPlayer->SetSelectedIndex(0);
        if (bTradeInputsNeedReset)
        {
            ResetTradeInputs();
            bTradeInputsNeedReset = false;
        }
        UpdateBankSelectionStyles();
    }
    else
    {
        ModalBorder->SetVisibility(ESlateVisibility::Collapsed);
        if (View.Phase != ECatanGamePhase::DropCards) LastDropPlayer.Reset();
    }
    ApplyUIPreview();
}

void UCatanHUDWidget::ApplyUIPreview()
{
    FString Preview;
    if (!FParse::Value(FCommandLine::Get(), TEXT("CatanUIPreview="), Preview)) return;
    ModalBorder->SetVisibility(ESlateVisibility::Visible);
    if (Preview.Equals(TEXT("Bank"), ESearchCase::IgnoreCase))
    {
        SetModalSize(1040.0f, 760.0f);
        ModalSwitcher->SetActiveWidgetIndex(3);
        TradeModeSwitcher->SetActiveWidgetIndex(0);
        BankRateText->SetText(FText::FromString(
            TEXT("Your bank rates: Wood 4:1 | Clay 3:1 | Hay 4:1 | Sheep 2:1 | Ore 4:1")));
        UpdateBankSelectionStyles();
    }
    else if (Preview.Equals(TEXT("PlayerTrade"), ESearchCase::IgnoreCase))
    {
        SetModalSize(1040.0f, 760.0f);
        ModalSwitcher->SetActiveWidgetIndex(3);
        TradeModeSwitcher->SetActiveWidgetIndex(1);
        if (TradingPlayer->GetOptionCount() == 0)
        {
            TradingPlayer->AddOption(TEXT("Bot 1"));
            TradingPlayer->AddOption(TEXT("Player 2"));
            TradingPlayer->SetSelectedIndex(0);
        }
    }
    else if (Preview.Equals(TEXT("Discard"), ESearchCase::IgnoreCase))
    {
        SetModalSize(900.0f, 620.0f);
        ModalSwitcher->SetActiveWidgetIndex(0);
        DropTitle->SetText(FText::FromString(TEXT("DISCARD 4 RESOURCES — PLAYER")));
        for (USpinBox* Input : DropInputs)
        {
            Input->SetMaxValue(8.0f);
            Input->SetMaxSliderValue(8.0f);
        }
    }
    else if (Preview.Equals(TEXT("IncomingTrade"), ESearchCase::IgnoreCase))
    {
        SetModalSize(850.0f, 420.0f);
        ModalSwitcher->SetActiveWidgetIndex(4);
        DealText->SetText(FText::FromString(
            TEXT("Player 2 offers to Player:\nW 2  C 0  H 0  S 0  O 0\n\nand requests:\nW 0  C 1  H 0  S 0  O 0")));
        AcceptDealButton->SetVisibility(ESlateVisibility::Visible);
        AcceptDealButton->SetIsEnabled(false);
        CancelDealButton->SetVisibility(ESlateVisibility::Visible);
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(AcceptDealButton->GetChildAt(0)))
            Label->SetText(FText::FromString(TEXT("NOT ENOUGH RESOURCES")));
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(CancelDealButton->GetChildAt(0)))
            Label->SetText(FText::FromString(TEXT("DECLINE OFFER")));
    }
}

void UCatanHUDWidget::SetModalSize(float Width, float Height)
{
    if (UCanvasPanelSlot* Slot = ModalBorder ? Cast<UCanvasPanelSlot>(ModalBorder->Slot) : nullptr)
        Slot->SetSize(FVector2D(Width, Height));
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

void UCatanHUDWidget::ShowOnlineSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(1);
}

void UCatanHUDWidget::ShowBotSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(2);
}

void UCatanHUDWidget::ShowMainSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(0);
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
    bTradeInputsNeedReset = true;
    if (TradeModeSwitcher) TradeModeSwitcher->SetActiveWidgetIndex(0);
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
    GameSubsystem->TryBankTrade(BankFromSelection, BankToSelection, Error);
}

void UCatanHUDWidget::ShowBankTrade()
{
    if (TradeModeSwitcher) TradeModeSwitcher->SetActiveWidgetIndex(0);
}

void UCatanHUDWidget::ShowPlayerTrade()
{
    if (TradeModeSwitcher) TradeModeSwitcher->SetActiveWidgetIndex(1);
}

void UCatanHUDWidget::SelectBankFromWood() { BankFromSelection = ECatanResource::Wood; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankFromClay() { BankFromSelection = ECatanResource::Clay; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankFromHay() { BankFromSelection = ECatanResource::Hay; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankFromSheep() { BankFromSelection = ECatanResource::Sheep; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankFromStone() { BankFromSelection = ECatanResource::Stone; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankToWood() { BankToSelection = ECatanResource::Wood; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankToClay() { BankToSelection = ECatanResource::Clay; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankToHay() { BankToSelection = ECatanResource::Hay; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankToSheep() { BankToSelection = ECatanResource::Sheep; UpdateBankSelectionStyles(); }
void UCatanHUDWidget::SelectBankToStone() { BankToSelection = ECatanResource::Stone; UpdateBankSelectionStyles(); }

void UCatanHUDWidget::UpdateBankSelectionStyles()
{
    for (int32 Index = 0; Index < BankFromButtons.Num(); ++Index)
    {
        const bool bSelected = Index == static_cast<int32>(BankFromSelection);
        BankFromButtons[Index]->SetRenderScale(bSelected ? FVector2D(1.06f) : FVector2D(1.0f));
        BankFromButtons[Index]->SetRenderOpacity(bSelected ? 1.0f : 0.52f);
    }
    for (int32 Index = 0; Index < BankToButtons.Num(); ++Index)
    {
        const bool bSelected = Index == static_cast<int32>(BankToSelection);
        BankToButtons[Index]->SetRenderScale(bSelected ? FVector2D(1.06f) : FVector2D(1.0f));
        BankToButtons[Index]->SetRenderOpacity(bSelected ? 1.0f : 0.52f);
    }
}

void UCatanHUDWidget::ResetTradeInputs()
{
    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    for (USpinBox* Input : OfferedInputs) Input->SetValue(0.0f);
    for (USpinBox* Input : RequestedInputs) Input->SetValue(0.0f);
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
    if (GameSubsystem->TryOfferTrade(ReadResources(OfferedInputs), ReadResources(RequestedInputs),
        TradingPlayer ? TradingPlayer->GetSelectedOption() : FString(), Error))
    {
        bTradePanelOpen = false;
        bTradeInputsNeedReset = true;
        Refresh();
    }
}

void UCatanHUDWidget::AcceptTrade()
{
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const FCatanPlayerView* LocalPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsLocalPlayer; });
    if (!LocalPlayer) return;
    FString Error;
    GameSubsystem->TryAcceptTrade(LocalPlayer->Name, Error);
}

void UCatanHUDWidget::CancelTrade()
{
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const FCatanPlayerView* LocalPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Item) { return Item.bIsLocalPlayer; });
    if (!LocalPlayer) return;
    FString Error;
    GameSubsystem->TryCancelTrade(LocalPlayer->Name, Error);
}

void UCatanHUDWidget::CloseTrading()
{
    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
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
