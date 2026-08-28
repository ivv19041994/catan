#include "CatanHUDWidget.h"

#include "CatanGameSubsystem.h"
#include "CatanNetworkSubsystem.h"
#include "CatanGameState.h"
#include "CatanGameMode.h"
#include "CatanPlayerController.h"
#include "CatanPlayerState.h"
#include "CatanTradePolicy.h"
#include "CatanInteractionPolicy.h"
#include "CatanTextResources.h"
#include "CatanUserSettings.h"
#include "CatanMobileUIPolicy.h"
#include "CatanTouchComboBoxString.h"
#include "CatanPlayerStatusPanelPolicy.h"
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
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/SafeZone.h"
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
#include "Misc/ScopeExit.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Containers/Ticker.h"

namespace
{
constexpr int32 SetupMainIndex = 0;
constexpr int32 SetupOnlineIndex = 1;
constexpr int32 SetupLocalNetworkIndex = 2;
constexpr int32 SetupDedicatedServerIndex = 3;
constexpr int32 SetupBotsIndex = 4;
constexpr int32 SetupSettingsIndex = 5;

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

FString PhaseHint(ECatanGamePhase Phase, ECatanLanguage Language)
{
    FString Key;
    switch (Phase)
    {
    case ECatanGamePhase::SetupSettlement: Key = TEXT("Click a free intersection to place a settlement"); break;
    case ECatanGamePhase::SetupRoad: Key = TEXT("Click an adjacent edge to place a road"); break;
    case ECatanGamePhase::RollDice: Key = TEXT("Roll both dice to start the turn"); break;
    case ECatanGamePhase::CommonPlay: Key = TEXT("Choose an action, then click its target on the board"); break;
    case ECatanGamePhase::DropCards: Key = TEXT("Choose exactly half of the shown player's resources to discard"); break;
    case ECatanGamePhase::MoveRobber: Key = TEXT("Click a different hex to move the robber"); break;
    case ECatanGamePhase::RoadBuilding: Key = TEXT("Click up to two valid road edges"); break;
    case ECatanGamePhase::Finished: Key = TEXT("Game finished"); break;
    }
    return FCatanTextResources::Get(Language, Key);
}

FString PhaseTitle(ECatanGamePhase Phase, ECatanLanguage Language)
{
    FString Key;
    switch (Phase)
    {
    case ECatanGamePhase::SetupSettlement: Key = TEXT("Setup: place settlement"); break;
    case ECatanGamePhase::SetupRoad: Key = TEXT("Setup: place road"); break;
    case ECatanGamePhase::RollDice: Key = TEXT("Roll dice"); break;
    case ECatanGamePhase::CommonPlay: Key = TEXT("Build and trade"); break;
    case ECatanGamePhase::DropCards: Key = TEXT("Discard resources"); break;
    case ECatanGamePhase::MoveRobber: Key = TEXT("Move robber"); break;
    case ECatanGamePhase::RoadBuilding: Key = TEXT("Road Building card"); break;
    case ECatanGamePhase::Finished: Key = TEXT("Finished"); break;
    }
    if (Key.IsEmpty()) Key = TEXT("Unknown phase");
    return FCatanTextResources::Get(Language, Key);
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

}

TSharedRef<SWidget> UCatanHUDWidget::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        UserPreferences = FCatanUserSettings::Load();
        FString LanguageOverride;
        if (FParse::Value(FCommandLine::Get(), TEXT("CatanLanguage="), LanguageOverride))
            UserPreferences.Language = FCatanTextResources::ParseLanguage(LanguageOverride);
        BuildLayout();
    }
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
        if (FParse::Param(FCommandLine::Get(), TEXT("CatanPlacementPreview")))
        {
            bSetupPanelOpen = false;
            if (ACatanGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ACatanGameMode>() : nullptr)
                Mode->StartSinglePlayerGame(TEXT("Player"), 1);
            else
                GameSubsystem->StartLocalGame({TEXT("Player"), TEXT("Player 2")});
            const FCatanGameView View = GameSubsystem->GetSnapshot();
            FString Error;
            if (!View.ValidNodeTargets.IsEmpty())
                GameSubsystem->SelectPendingBuildTarget(
                    ECatanBoardAction::BuildSettlement, View.ValidNodeTargets[0], Error);
            UE_LOG(LogTemp, Display, TEXT("CATAN_BUILD_CONFIRM_PREVIEW target=%d pending=%d"),
                View.ValidNodeTargets.IsEmpty() ? INDEX_NONE : View.ValidNodeTargets[0],
                GameSubsystem->HasPendingBuildTarget());
        }
        else Refresh();
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
    struct FResourceBadge { FString Name; FLinearColor Color; };
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
    PlayersListSizeBox = WidgetTree->ConstructWidget<USizeBox>();
    PlayersListSizeBox->SetHeightOverride(
        FCatanPlayerStatusPanelPolicy::Resolve(false).ViewportHeight);
    PlayerDetails->AddChildToVerticalBox(PlayersListSizeBox);
    PlayersScroll = WidgetTree->ConstructWidget<UScrollBox>();
    PlayersScroll->SetOrientation(EOrientation::Orient_Vertical);
    PlayersScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    PlayersScroll->SetAlwaysShowScrollbar(false);
    PlayersListSizeBox->SetContent(PlayersScroll);
    UVerticalBox* PlayerRows = WidgetTree->ConstructWidget<UVerticalBox>();
    PlayersScroll->AddChild(PlayerRows);
    PlayersText = AddText(PlayerRows, FString(),
        FCatanPlayerStatusPanelPolicy::Resolve(false).FontSize);

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
    const TArray<FString> ResourceNames = {TEXT("Wood"), TEXT("Clay"), TEXT("Hay"), TEXT("Sheep"), TEXT("Stone")};
    for (int32 ResourceIndex = 0; ResourceIndex < 5; ++ResourceIndex)
    {
        const FString& ResourceName = ResourceNames[ResourceIndex];
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetBrushColor(ResourceColor(ResourceIndex));
        Card->SetPadding(FMargin(10));
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Card->SetContent(Row);
        UVerticalBoxSlot* RowSlot = DropPanel->AddChildToVerticalBox(Card);
        RowSlot->SetPadding(FMargin(3, 5));
        UCommonTextBlock* Label = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Label->SetText(FText::FromString(Localize(ResourceName)));
        RegisterLocalizedText(Label, ResourceName);
        FSlateFontInfo Font = Label->GetFont(); Font.Size = 22; Label->SetFont(Font);
        UHorizontalBoxSlot* LabelSlot = Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        UComboBoxString* Input = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
        ConfigureComboBox(Input, 24);
        Input->AddOption(TEXT("0"));
        Input->SetSelectedOption(TEXT("0"));
        Input->OnSelectionChanged.AddDynamic(this, &UCatanHUDWidget::UpdateDropConfirmation);
        USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
        InputSize->SetMinDesiredWidth(150.0f);
        InputSize->SetMinDesiredHeight(56.0f);
        InputSize->AddChild(Input);
        Row->AddChildToHorizontalBox(InputSize);
        DropInputs.Add(Input);
    }
    ConfirmDropButton = AddButton(DropPanel, TEXT("CONFIRM DISCARD"));
    ConfirmDropButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ConfirmDiscard);

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
    DevelopmentModeSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();
    DevelopmentPanel->AddChildToVerticalBox(DevelopmentModeSwitcher);

    UVerticalBox* DevelopmentRoot = WidgetTree->ConstructWidget<UVerticalBox>();
    DevelopmentModeSwitcher->AddChild(DevelopmentRoot);
    DevelopmentAvailabilityText = AddText(DevelopmentRoot, FString(), 15);
    KnightButton = AddButton(DevelopmentRoot, TEXT("PLAY KNIGHT"));
    RoadBuildingButton = AddButton(DevelopmentRoot, TEXT("PLAY ROAD BUILDING"));
    YearOfPlentyButton = AddButton(DevelopmentRoot, TEXT("PLAY YEAR OF PLENTY"));
    MonopolyButton = AddButton(DevelopmentRoot, TEXT("PLAY MONOPOLY"));
    UButton* CloseCards = AddButton(DevelopmentRoot, TEXT("CLOSE"));
    KnightButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayKnight);
    RoadBuildingButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PlayRoadBuilding);
    YearOfPlentyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowYearOfPlentyParameters);
    MonopolyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowMonopolyParameters);
    CloseCards->OnClicked.AddDynamic(this, &UCatanHUDWidget::CloseDevelopmentCards);

    UVerticalBox* PlentyPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    DevelopmentModeSwitcher->AddChild(PlentyPanel);
    AddText(PlentyPanel, TEXT("YEAR OF PLENTY"), 24)->SetJustification(ETextJustify::Center);
    AddText(PlentyPanel, TEXT("Choose exactly two resources"), 18)->SetJustification(ETextJustify::Center);
    for (int32 Index = 0; Index < ResourceNames.Num(); ++Index)
    {
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetBrushColor(ResourceColor(Index));
        Card->SetPadding(FMargin(6));
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Card->SetContent(Row);
        UCommonTextBlock* Name = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Name->SetText(FText::FromString(Localize(ResourceNames[Index])));
        RegisterLocalizedText(Name, ResourceNames[Index]);
        FSlateFontInfo Font = Name->GetFont(); Font.Size = 22; Name->SetFont(Font);
        UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
        NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        NameSlot->SetVerticalAlignment(VAlign_Center);
        UComboBoxString* Input = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
        ConfigureComboBox(Input, 24);
        for (int32 Count = 0; Count <= 2; ++Count) Input->AddOption(FString::FromInt(Count));
        Input->SetSelectedOption(TEXT("0"));
        Input->OnSelectionChanged.AddDynamic(this, &UCatanHUDWidget::UpdatePlentySelection);
        USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
        InputSize->SetMinDesiredWidth(150.0f);
        InputSize->SetMinDesiredHeight(56.0f);
        InputSize->AddChild(Input);
        Row->AddChildToHorizontalBox(InputSize);
        PlentyPanel->AddChildToVerticalBox(Card)->SetPadding(FMargin(2));
        PlentyInputs.Add(Input);
    }
    UHorizontalBox* PlentyActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlentyPanel->AddChildToVerticalBox(PlentyActions);
    UVerticalBox* PlentyConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* PlentyBackBox = WidgetTree->ConstructWidget<UVerticalBox>();
    PlentyActions->AddChildToHorizontalBox(PlentyConfirmBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    PlentyActions->AddChildToHorizontalBox(PlentyBackBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ConfirmPlentyButton = AddButton(PlentyConfirmBox, TEXT("CONFIRM"));
    UButton* PlentyBack = AddButton(PlentyBackBox, TEXT("BACK"));
    ConfirmPlentyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ConfirmYearOfPlenty);
    PlentyBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::CancelDevelopmentParameters);

    UVerticalBox* MonopolyPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    DevelopmentModeSwitcher->AddChild(MonopolyPanel);
    AddText(MonopolyPanel, TEXT("MONOPOLY"), 24)->SetJustification(ETextJustify::Center);
    AddText(MonopolyPanel, TEXT("Choose one resource from every opponent"), 18)
        ->SetJustification(ETextJustify::Center);
    for (int32 Index = 0; Index < ResourceNames.Num(); ++Index)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(ResourceColor(Index));
        UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Text->SetText(FText::FromString(Localize(ResourceNames[Index])));
        RegisterLocalizedText(Text, ResourceNames[Index]);
        Text->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Text->GetFont(); Font.Size = 22; Text->SetFont(Font);
        Button->AddChild(Text);
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetMinDesiredHeight(58.0f);
        Size->AddChild(Button);
        MonopolyPanel->AddChildToVerticalBox(Size)->SetPadding(FMargin(2));
        MonopolyResourceButtons.Add(Button);
    }
    MonopolyResourceButtons[0]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectMonopolyWood);
    MonopolyResourceButtons[1]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectMonopolyClay);
    MonopolyResourceButtons[2]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectMonopolyHay);
    MonopolyResourceButtons[3]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectMonopolySheep);
    MonopolyResourceButtons[4]->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectMonopolyStone);
    UHorizontalBox* MonopolyActions = WidgetTree->ConstructWidget<UHorizontalBox>();
    MonopolyPanel->AddChildToVerticalBox(MonopolyActions);
    UVerticalBox* MonopolyConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>();
    UVerticalBox* MonopolyBackBox = WidgetTree->ConstructWidget<UVerticalBox>();
    MonopolyActions->AddChildToHorizontalBox(MonopolyConfirmBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    MonopolyActions->AddChildToHorizontalBox(MonopolyBackBox)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    UButton* ConfirmMonopolyButton = AddButton(MonopolyConfirmBox, TEXT("CONFIRM"));
    UButton* MonopolyBack = AddButton(MonopolyBackBox, TEXT("BACK"));
    ConfirmMonopolyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ConfirmMonopoly);
    MonopolyBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::CancelDevelopmentParameters);

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

    auto AddResourceButton = [this](UVerticalBox* Parent, const FString& Name, int32 Index,
        TArray<TObjectPtr<UButton>>& Buttons)
    {
        UButton* Button = WidgetTree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(ResourceColor(Index));
        UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>();
        Text->SetText(FText::FromString(Localize(Name)));
        RegisterLocalizedText(Text, Name);
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
    TradingPlayer = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
    ConfigureComboBox(TradingPlayer, 22);
    USizeBox* RecipientSize = WidgetTree->ConstructWidget<USizeBox>();
    RecipientSize->SetMinDesiredHeight(56.0f);
    RecipientSize->AddChild(TradingPlayer);
    PlayerTradePanel->AddChildToVerticalBox(RecipientSize);
    UHorizontalBox* PlayerColumns = WidgetTree->ConstructWidget<UHorizontalBox>();
    PlayerTradePanel->AddChildToVerticalBox(PlayerColumns);
    auto AddResourceInputs = [this, PlayerColumns, &ResourceNames](const FString& Label,
        TArray<TObjectPtr<UComboBoxString>>& Inputs)
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
            Name->SetText(FText::FromString(Localize(ResourceNames[Index])));
            RegisterLocalizedText(Name, ResourceNames[Index]);
            FSlateFontInfo Font = Name->GetFont(); Font.Size = 22; Name->SetFont(Font);
            UHorizontalBoxSlot* NameSlot = Row->AddChildToHorizontalBox(Name);
            NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            NameSlot->SetVerticalAlignment(VAlign_Center);
            UComboBoxString* Input = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
            ConfigureComboBox(Input, 24);
            for (int32 Count = 0; Count <= 5; ++Count)
                Input->AddOption(FString::FromInt(Count));
            Input->SetSelectedOption(TEXT("0"));
            USizeBox* InputSize = WidgetTree->ConstructWidget<USizeBox>();
            InputSize->SetMinDesiredWidth(150.0f);
            InputSize->SetMinDesiredHeight(56.0f);
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
    MainPlayerNameText = AddText(SetupPanel, FString(), 18);
    UButton* OnlineMode = AddButton(SetupPanel, TEXT("ONLINE"));
    UButton* BotMode = AddButton(SetupPanel, TEXT("PLAY AGAINST BOTS"));
    UButton* SettingsMode = AddButton(SetupPanel, TEXT("SETTINGS"));
    OnlineMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowOnlineSetup);
    BotMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowBotSetup);
    SettingsMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowSettings);

    UVerticalBox* OnlinePanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(OnlinePanel);
    AddText(OnlinePanel, TEXT("ONLINE"), 27);
    AddText(OnlinePanel, TEXT("Choose how to connect"), 19);
    UButton* LocalNetworkMode = AddButton(OnlinePanel, TEXT("LOCAL NETWORK"));
    UButton* DedicatedServerMode = AddButton(OnlinePanel, TEXT("DEDICATED SERVER"));
    UButton* OnlineBack = AddButton(OnlinePanel, TEXT("BACK"));
    LocalNetworkMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowLocalNetworkSetup);
    DedicatedServerMode->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowDedicatedServerSetup);
    OnlineBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowMainSetup);

    UScrollBox* LocalNetworkScroll = WidgetTree->ConstructWidget<UScrollBox>();
    LocalNetworkScroll->SetOrientation(EOrientation::Orient_Vertical);
    LocalNetworkScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
    SetupSwitcher->AddChild(LocalNetworkScroll);
    UVerticalBox* LocalNetworkPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    LocalNetworkScroll->AddChild(LocalNetworkPanel);
    AddText(LocalNetworkPanel, TEXT("LOCAL NETWORK"), 27);
    AddText(LocalNetworkPanel, TEXT("Host a LAN lobby, find one automatically, or enter its address."), 15);
    LobbyNameInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    LobbyNameInput->SetText(FText::FromString(TEXT("Catan LAN Lobby")));
    LobbyNameInput->SetHintText(FText::FromString(TEXT("Lobby name")));
    LobbyNameInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    LocalNetworkPanel->AddChildToVerticalBox(LobbyNameInput);
    UButton* HostGame = AddButton(LocalNetworkPanel, TEXT("HOST ONLINE (LAN)"));
    HostGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::HostLanLobby);
    AddText(LocalNetworkPanel, TEXT("SAVED GAMES"), 17);
    SavedGameInput = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
    ConfigureComboBox(SavedGameInput, 17);
    SavedGameInput->AddOption(TEXT("No saved games"));
    SavedGameInput->SetSelectedIndex(0);
    SavedGameInput->OnSelectionChanged.AddDynamic(this, &UCatanHUDWidget::UpdateSavedGameSelection);
    LocalNetworkPanel->AddChildToVerticalBox(SavedGameInput);
    LoadLanButton = AddButton(LocalNetworkPanel, TEXT("LOAD SAVED LAN GAME"));
    LoadLanButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::LoadLanLobby);
    LoadLanButton->SetIsEnabled(GameSubsystem && GameSubsystem->HasLanSavedGame());
    LobbyResults = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
    ConfigureComboBox(LobbyResults, 20);
    LobbyResults->AddOption(TEXT("No search results yet"));
    LobbyResults->SetSelectedIndex(0);
    LocalNetworkPanel->AddChildToVerticalBox(LobbyResults);
    UButton* SearchGame = AddButton(LocalNetworkPanel, TEXT("REFRESH LAN LOBBIES"));
    UButton* JoinGame = AddButton(LocalNetworkPanel, TEXT("JOIN SELECTED"));
    SearchGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::FindLanLobbies);
    JoinGame->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinSelectedLobby);
    ManualAddressInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    ManualAddressInput->SetHintText(FText::FromString(TEXT("Host address, e.g. 192.168.1.20:7777")));
    ManualAddressInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    LocalNetworkPanel->AddChildToVerticalBox(ManualAddressInput);
    UButton* JoinAddress = AddButton(LocalNetworkPanel, TEXT("JOIN BY ADDRESS"));
    JoinAddress->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinManualLobby);
    UButton* LocalNetworkBack = AddButton(LocalNetworkPanel, TEXT("BACK"));
    LocalNetworkBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowOnlineSetup);
    NetworkStatusTexts.Add(AddText(LocalNetworkPanel, TEXT("LAN ready"), 14));

    UVerticalBox* DedicatedServerPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(DedicatedServerPanel);
    AddText(DedicatedServerPanel, TEXT("DEDICATED SERVER"), 27);
    AddText(DedicatedServerPanel, TEXT("Create a new game or join an existing lobby by token."), 15);
    DedicatedAddressInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    const FCatanDedicatedSession SavedDedicated = NetworkSubsystem
        ? NetworkSubsystem->GetSavedDedicatedSession() : FCatanDedicatedSession{};
    DedicatedAddressInput->SetText(FText::FromString(SavedDedicated.IsValid()
        ? SavedDedicated.Address : TEXT("127.0.0.1:17777")));
    DedicatedAddressInput->SetHintText(FText::FromString(TEXT("Server IP, e.g. 192.168.1.20:17777")));
    DedicatedAddressInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    DedicatedServerPanel->AddChildToVerticalBox(DedicatedAddressInput);
    ResumeDedicatedButton = AddButton(DedicatedServerPanel, SavedDedicated.IsValid()
        ? FString::Printf(TEXT("RECONNECT AS %s"), *SavedDedicated.PlayerName)
        : TEXT("RECONNECT TO SAVED GAME"));
    ResumeDedicatedButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ResumeDedicatedLobby);
    ResumeDedicatedButton->SetVisibility(SavedDedicated.IsValid()
        ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    UButton* CreateDedicated = AddButton(DedicatedServerPanel, TEXT("CREATE GAME ON SERVER"));
    CreateDedicated->OnClicked.AddDynamic(this, &UCatanHUDWidget::CreateDedicatedLobby);
    DedicatedLobbyTokenInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    DedicatedLobbyTokenInput->SetHintText(FText::FromString(TEXT("Lobby token, e.g. ABCD-EFGH")));
    DedicatedLobbyTokenInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    DedicatedServerPanel->AddChildToVerticalBox(DedicatedLobbyTokenInput);
    UButton* JoinDedicated = AddButton(DedicatedServerPanel, TEXT("JOIN GAME BY LOBBY TOKEN"));
    JoinDedicated->OnClicked.AddDynamic(this, &UCatanHUDWidget::JoinDedicatedLobby);
    UButton* DedicatedServerBack = AddButton(DedicatedServerPanel, TEXT("BACK"));
    DedicatedServerBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowOnlineSetup);
    NetworkStatusTexts.Add(AddText(DedicatedServerPanel, TEXT("Server ready"), 14));
    UE_LOG(LogTemp, Display,
        TEXT("CATAN_ONLINE_MENU_SPLIT ready pages=chooser,local,dedicated dedicatedScroll=0"));

    UVerticalBox* BotPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(BotPanel);
    AddText(BotPanel, TEXT("PLAY AGAINST BOTS"), 27);
    AddText(BotPanel, TEXT("Choose the total number of players."), 17);
    PlayerCount = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
    ConfigureComboBox(PlayerCount, 22);
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

    UVerticalBox* SettingsPanel = WidgetTree->ConstructWidget<UVerticalBox>();
    SetupSwitcher->AddChild(SettingsPanel);
    AddText(SettingsPanel, TEXT("SETTINGS"), 27);
    AddText(SettingsPanel, TEXT("PLAYER NAME"), 18);
    SettingsNameInput = WidgetTree->ConstructWidget<UEditableTextBox>();
    SettingsNameInput->SetText(FText::FromString(UserPreferences.PlayerName));
    SettingsNameInput->SetHintText(FText::FromString(Localize(TEXT("Player name"))));
    SettingsNameInput->SetForegroundColor(FLinearColor(0.04f, 0.055f, 0.075f, 1.0f));
    SettingsPanel->AddChildToVerticalBox(SettingsNameInput);
    AddText(SettingsPanel, TEXT("LANGUAGE"), 18);
    SettingsLanguageInput = WidgetTree->ConstructWidget<UCatanTouchComboBoxString>();
    ConfigureComboBox(SettingsLanguageInput, 22);
    SettingsLanguageInput->AddOption(TEXT("English"));
    SettingsLanguageInput->AddOption(TEXT("Русский"));
    SettingsLanguageInput->SetSelectedIndex(UserPreferences.Language == ECatanLanguage::Russian ? 1 : 0);
    SettingsPanel->AddChildToVerticalBox(SettingsLanguageInput);
    UButton* SaveSettingsButton = AddButton(SettingsPanel, TEXT("SAVE SETTINGS"));
    UButton* SettingsBack = AddButton(SettingsPanel, TEXT("BACK"));
    SaveSettingsButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SaveSettings);
    SettingsBack->OnClicked.AddDynamic(this, &UCatanHUDWidget::ShowMainSetup);

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
    AddText(LobbyPanel, TEXT("GAME LOBBY"), 32);
    LobbyAddressText = AddText(LobbyPanel, TEXT("Host address"), 16);
    LobbyPlayersText = AddText(LobbyPanel, TEXT("Waiting for players..."), 20);
    ReadyButton = AddButton(LobbyPanel, TEXT("READY"));
    StartLobbyButton = AddButton(LobbyPanel, TEXT("START GAME"));
    CopyLobbyTokenButton = AddButton(LobbyPanel, TEXT("COPY LOBBY TOKEN"));
    UButton* LeaveLobbyButton = AddButton(LobbyPanel, TEXT("LEAVE LOBBY"));
    ReadyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::ToggleLobbyReady);
    StartLobbyButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::StartLobbyMatch);
    CopyLobbyTokenButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::CopyDedicatedLobbyToken);
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
    ApplyLanguage();
#if PLATFORM_ANDROID
    const FCatanComboBoxMetrics ComboMetrics = CatanMobileUIPolicy::ComboBoxMetrics(22, true);
#else
    const FCatanComboBoxMetrics ComboMetrics = CatanMobileUIPolicy::ComboBoxMetrics(22, false);
#endif
    UE_LOG(LogTemp, Display,
        TEXT("CATAN_COMBO_STYLE ready widgets=%d popupText=white rowHeight=%.0f font=%d"),
        8 + DropInputs.Num() + OfferedInputs.Num() + RequestedInputs.Num(),
        ComboMetrics.MinimumRowHeight, ComboMetrics.PopupFontSize);
}

void UCatanHUDWidget::ConfigureComboBox(UComboBoxString* ComboBox, int32 FontSize)
{
    if (!ComboBox) return;
#if PLATFORM_ANDROID
    constexpr bool bMobile = true;
#else
    constexpr bool bMobile = false;
#endif
    const FCatanComboBoxMetrics Metrics = CatanMobileUIPolicy::ComboBoxMetrics(FontSize, bMobile);
    FTableRowStyle ItemStyle = ComboBox->GetItemStyle();
    ItemStyle.SetTextColor(FSlateColor(FLinearColor::White));
    ItemStyle.SetSelectedTextColor(FSlateColor(FLinearColor::White));
    ComboBox->SetItemStyle(ItemStyle);
    FSlateFontInfo Font = ComboBox->GetFont();
    Font.Size = Metrics.ClosedFontSize;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
    ComboBox->Font = Font;
    ComboBox->ForegroundColor = FSlateColor(FLinearColor(0.025f, 0.035f, 0.05f, 1.0f));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
    ComboBox->SetContentPadding(Metrics.ClosedContentPadding);
    ComboBox->SetMaxListHeight(Metrics.MaximumListHeight);
    if (UCatanTouchComboBoxString* TouchCombo = Cast<UCatanTouchComboBoxString>(ComboBox))
        TouchCombo->ConfigurePopup(Metrics.PopupFontSize, Metrics.MinimumRowHeight, Metrics.PopupPadding);
    ComboBox->OnOpening.AddDynamic(this, &UCatanHUDWidget::ReportComboOpening);
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
    const FCatanPlayerStatusPanelMetrics PlayerMetrics = FCatanPlayerStatusPanelPolicy::Resolve(bCompact);
    if (PlayersListSizeBox) PlayersListSizeBox->SetHeightOverride(PlayerMetrics.ViewportHeight);
    if (PlayersText)
    {
        FSlateFontInfo Font = PlayersText->GetFont();
        Font.Size = PlayerMetrics.FontSize;
        PlayersText->SetFont(Font);
    }
    if (PreviousPlayerStatusCount > 0)
        UE_LOG(LogTemp, Display, TEXT("CATAN_PLAYER_STATUS rows=%d scroll=1 compact=%d viewport=%.0f"),
            PreviousPlayerStatusCount, bCompactLayout, PlayerMetrics.ViewportHeight);

    if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(LeftDetailsButton->GetChildAt(0)))
        Label->SetText(FText::FromString(Localize(bLeftDetailsOpen
            ? TEXT("HIDE EVENTS & HELP") : TEXT("SHOW EVENTS & HELP"))));
    if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(RightDetailsButton->GetChildAt(0)))
        Label->SetText(FText::FromString(Localize(bRightDetailsOpen
            ? TEXT("HIDE PLAYERS & COSTS") : TEXT("SHOW PLAYERS & COSTS"))));

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
            Label->SetText(FText::FromString(Localize(bCompactLayout ? Entry.Compact : Entry.Full)));
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
    TextBlock->SetText(FText::FromString(Localize(Text)));
    if (!Text.IsEmpty()) RegisterLocalizedText(TextBlock, Text);
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
    Text->SetText(FText::FromString(Localize(Label)));
    RegisterLocalizedText(Text, Label);
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

FString UCatanHUDWidget::Localize(const FString& Key) const
{
    return FCatanTextResources::Get(UserPreferences.Language, Key);
}

void UCatanHUDWidget::RegisterLocalizedText(UCommonTextBlock* Widget, const FString& Key)
{
    if (Widget && !Key.IsEmpty()) LocalizedTexts.Emplace(Widget, Key);
}

void UCatanHUDWidget::ApplyLanguage()
{
    for (const TPair<TWeakObjectPtr<UCommonTextBlock>, FString>& Entry : LocalizedTexts)
        if (Entry.Key.IsValid()) Entry.Key->SetText(FText::FromString(Localize(Entry.Value)));

    if (MainPlayerNameText)
        MainPlayerNameText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
            *Localize(TEXT("PLAYER")), *UserPreferences.PlayerName)));
    if (SettingsNameInput)
    {
        SettingsNameInput->SetText(FText::FromString(UserPreferences.PlayerName));
        SettingsNameInput->SetHintText(FText::FromString(Localize(TEXT("Player name"))));
    }
    if (SettingsLanguageInput)
    {
        const int32 Selected = UserPreferences.Language == ECatanLanguage::Russian ? 1 : 0;
        SettingsLanguageInput->ClearOptions();
        SettingsLanguageInput->AddOption(TEXT("English"));
        SettingsLanguageInput->AddOption(UserPreferences.Language == ECatanLanguage::Russian
            ? TEXT("Русский") : TEXT("Russian"));
        SettingsLanguageInput->SetSelectedIndex(Selected);
    }
    if (PlayerCount)
    {
        const int32 Selected = FMath::Max(0, PlayerCount->GetSelectedIndex());
        PlayerCount->ClearOptions();
        PlayerCount->AddOption(Localize(TEXT("2 players")));
        PlayerCount->AddOption(Localize(TEXT("3 players")));
        PlayerCount->AddOption(Localize(TEXT("4 players")));
        PlayerCount->SetSelectedIndex(Selected);
    }
    if (SettlementButton) SettlementButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Build a settlement: wood + clay + hay + sheep"))));
    if (RoadButton) RoadButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Build a road: wood + clay"))));
    if (CityButton) CityButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Upgrade your settlement to a city: 2 hay + 3 ore"))));
    if (BuyCardButton) BuyCardButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Buy a development card: hay + sheep + ore"))));
    if (KnightButton) KnightButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Move the robber and steal from an adjacent player"))));
    if (RoadBuildingButton) RoadBuildingButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Place two roads for free"))));
    if (YearOfPlentyButton) YearOfPlentyButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Take the two selected resources"))));
    if (MonopolyButton) MonopolyButton->SetToolTipText(FText::FromString(
        Localize(TEXT("Take the selected resource from every opponent"))));
    UpdateActionLabels();
}

void UCatanHUDWidget::Refresh()
{
    if (!GameSubsystem || !PhaseText) return;
    ON_SCOPE_EXIT { UpdateActionPanelVisibility(); };
    RefreshSavedGameOptions();
    SetModalSize(680.0f, 650.0f);
    SetModalPosition(FVector2D::ZeroVector);
    if (NetworkSubsystem)
    {
        if (ResumeDedicatedButton)
        {
            const bool bCanResume = NetworkSubsystem->HasSavedDedicatedSession()
                && !NetworkSubsystem->IsDedicatedActive();
            ResumeDedicatedButton->SetVisibility(bCanResume
                ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
            if (bCanResume)
                if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(ResumeDedicatedButton->GetChildAt(0)))
                    Label->SetText(FText::FromString(FString::Printf(TEXT("RECONNECT AS %s"),
                        *NetworkSubsystem->GetSavedDedicatedSession().PlayerName)));
        }
        for (UCommonTextBlock* NetworkStatusText : NetworkStatusTexts)
            if (NetworkStatusText)
                NetworkStatusText->SetText(FText::FromString(NetworkSubsystem->GetStatus()));
        if (LobbyResults)
        {
            const int32 Previous = LobbyResults->GetSelectedIndex();
            LobbyResults->ClearOptions();
            const TArray<FCatanDiscoveredLobby>& Results = NetworkSubsystem->GetDiscoveredLobbies();
            for (const FCatanDiscoveredLobby& Lobby : Results)
                LobbyResults->AddOption(FString::Printf(TEXT("%s — %d/%d — %d ms"),
                    *Lobby.Name, Lobby.Players, Lobby.Capacity, Lobby.PingMs));
            if (Results.IsEmpty()) LobbyResults->AddOption(Localize(TEXT("No LAN lobbies found")));
            LobbyResults->SetSelectedIndex(FMath::Clamp(Previous, 0, LobbyResults->GetOptionCount() - 1));
        }
    }
    if (NetworkSubsystem && NetworkSubsystem->IsDedicatedActive()
        && !NetworkSubsystem->IsDedicatedPlaying())
    {
        bSetupPanelOpen = false;
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(8);
        const TArray<FCatanLobbyPlayerView>& Players = NetworkSubsystem->GetDedicatedLobbyPlayers();
        FString Rows;
        bool bAllReady = Players.Num() >= 2;
        bool bLocalReady = false;
        bool bLocalHost = false;
        for (const FCatanLobbyPlayerView& Player : Players)
        {
            Rows += FString::Printf(TEXT("%s %s%s\n"), Player.bReady ? TEXT("✓") : TEXT("○"),
                *Player.Name, Player.bHost ? TEXT("  [HOST]") : TEXT(""));
            bAllReady = bAllReady && Player.bReady;
            if (Player.Name == NetworkSubsystem->GetDedicatedPlayerName())
            {
                bLocalReady = Player.bReady;
                bLocalHost = Player.bHost;
            }
        }
        LobbyPlayersText->SetText(FText::FromString(Rows + TEXT("\n2–4 players; everyone must be ready.")));
        LobbyAddressText->SetText(FText::FromString(FString::Printf(
            TEXT("Server: %s\nLobby token: %s\nYour private player token: %s"),
            *NetworkSubsystem->GetDedicatedAddress(), *NetworkSubsystem->GetDedicatedLobbyToken(),
            *NetworkSubsystem->GetDedicatedPlayerToken())));
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(ReadyButton->GetChildAt(0)))
            Label->SetText(FText::FromString(Localize(bLocalReady ? TEXT("NOT READY") : TEXT("READY"))));
        StartLobbyButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        StartLobbyButton->SetIsEnabled(bLocalHost && bAllReady && Players.Num() <= 4);
        CopyLobbyTokenButton->SetVisibility(ESlateVisibility::Visible);
        ScheduleAutomatedLobbyLeave(Players.Num(), bLocalHost);
        return;
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
        bool bAllExpectedConnected = NetworkState->ExpectedPlayerNames.IsEmpty();
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
        if (!NetworkState->ExpectedPlayerNames.IsEmpty())
        {
            bAllExpectedConnected = true;
            Rows += TEXT("\n") + Localize(TEXT("Expected players:")) + TEXT("\n");
            for (const FString& Expected : NetworkState->ExpectedPlayerNames)
            {
                const bool bConnected = NetworkState->LobbyPlayers.ContainsByPredicate(
                    [&Expected](const FCatanLobbyPlayerView& Player)
                    { return Player.Name.Equals(Expected, ESearchCase::IgnoreCase); });
                bAllExpectedConnected = bAllExpectedConnected && bConnected;
                Rows += FString::Printf(TEXT("%s %s%s\n"), bConnected ? TEXT("✓") : TEXT("○"),
                    *Expected, bConnected ? TEXT("") : *FString::Printf(TEXT("  [%s]"),
                        *Localize(TEXT("WAITING"))));
            }
        }
        LobbyPlayersText->SetText(FText::FromString(Rows + (NetworkState->ExpectedPlayerNames.IsEmpty()
            ? TEXT("\n2–4 players; everyone must be ready.")
            : TEXT("\n") + Localize(TEXT("The restored game starts after every expected name reconnects and is ready.")))));
        LobbyAddressText->SetText(FText::FromString(FString::Printf(TEXT("Share this address: %s"),
            NetworkSubsystem ? *NetworkSubsystem->GetLocalAddress() : TEXT("port 7777"))));
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(ReadyButton->GetChildAt(0)))
            Label->SetText(FText::FromString(Localize(bLocalReady ? TEXT("NOT READY") : TEXT("READY"))));
        StartLobbyButton->SetVisibility(bLocalHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        StartLobbyButton->SetIsEnabled(bLocalHost && bAllReady && bAllExpectedConnected
            && NetworkState->LobbyPlayers.Num() <= 4);
        CopyLobbyTokenButton->SetVisibility(ESlateVisibility::Collapsed);
        ScheduleAutomatedLobbyLeave(NetworkState->LobbyPlayers.Num(), bLocalHost);
        return;
    }
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const bool bLocalTurn = GameSubsystem->CanLocalPlayerAct(View);
    PhaseText->SetText(FText::FromString(PhaseTitle(View.Phase, UserPreferences.Language)
        + TEXT("\n") + Localize(TEXT("Current:")) + TEXT(" ") + View.CurrentPlayer));
    DiceText->SetText(View.FirstDie > 0
        ? FText::FromString(Localize(TEXT("Dice:")) + FString::Printf(TEXT(" %d + %d = %d"),
            View.FirstDie, View.SecondDie, View.FirstDie + View.SecondDie))
        : FText::GetEmpty());
    HintText->SetText(FText::FromString(bLocalTurn
        ? PhaseHint(View.Phase, UserPreferences.Language)
        : Localize(TEXT("Waiting for")) + TEXT(" ") + View.CurrentPlayer));
    StatusText->SetText(FText::FromString(Localize(View.StatusMessage)));
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
        Events += FString::Printf(TEXT("• %s\n"), *Localize(View.EventLog[Index]));
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
        HandTitleText->SetText(FText::FromString(Localize(TEXT("YOUR HAND")) + FString::Printf(
            TEXT(" — %d "), VisibleLocalPlayer->ResourceCards) + Localize(TEXT("RESOURCE CARDS"))));
        FString DevelopmentSummary = Localize(TEXT("DEV")) + FString::Printf(TEXT(" %d  |  "),
            VisibleLocalPlayer->DevelopmentCards)
            + Localize(TEXT("Knight")) + FString::Printf(TEXT(" %d  "), VisibleLocalPlayer->Knights)
            + Localize(TEXT("Roads")) + FString::Printf(TEXT(" %d  "), VisibleLocalPlayer->RoadBuildingCards)
            + Localize(TEXT("Plenty")) + FString::Printf(TEXT(" %d  "), VisibleLocalPlayer->YearOfPlentyCards)
            + Localize(TEXT("Monopoly")) + FString::Printf(TEXT(" %d"), VisibleLocalPlayer->MonopolyCards);
        if (VisibleLocalPlayer->PendingDevelopmentCards > 0)
            DevelopmentSummary += FString::Printf(TEXT("  |  %d "), VisibleLocalPlayer->PendingDevelopmentCards)
                + Localize(TEXT("ready next turn"));
        DevelopmentHandText->SetText(FText::FromString(DevelopmentSummary));
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
                ToastText->SetText(FText::FromString(Localize(TEXT("YOUR RESOURCES:")) + TEXT(" ") + Changes));
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
        if (Player.bHasLongestRoad) Awards += TEXT("  ★") + Localize(TEXT("ROAD"));
        if (Player.bHasLargestArmy) Awards += TEXT("  ★") + Localize(TEXT("ARMY"));
        FString DisplayName = FCatanPlayerStatusPanelPolicy::CompactName(Player.Name);
        if (Player.bIsLocalPlayer) DisplayName += TEXT(" [") + Localize(TEXT("YOU")) + TEXT("]");
        if (Player.bIsBot) DisplayName += TEXT(" [") + Localize(TEXT("BOT")) + TEXT("]");
        Players += FString::Printf(TEXT("%s %s  ·  VP %d  ·  RES %d  ·  DEV %d\n"),
            Player.bIsCurrent ? TEXT("▶") : TEXT(" "),
            *DisplayName, Player.VictoryPoints, Player.ResourceCards, Player.DevelopmentCards);
        Players += FString::Printf(TEXT("   S %d  ·  C %d  ·  R %d%s\n"),
            Player.FreeSettlements, Player.FreeCities, Player.FreeRoads, *Awards);
        if (Player.bResourcesVisible)
            ResourceDigest = FString::Printf(TEXT("%d/%d/%d/%d/%d"),
                Player.Resources.Wood, Player.Resources.Clay, Player.Resources.Hay,
                Player.Resources.Sheep, Player.Resources.Stone);
    }
    PlayersText->SetText(FText::FromString(Players));
    if (PreviousPlayerStatusCount != View.Players.Num())
    {
        PreviousPlayerStatusCount = View.Players.Num();
        UE_LOG(LogTemp, Display, TEXT("CATAN_PLAYER_STATUS rows=%d scroll=1 compact=%d viewport=%.0f"),
            View.Players.Num(), bCompactLayout,
            FCatanPlayerStatusPanelPolicy::Resolve(bCompactLayout).ViewportHeight);
    }
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
        CostLine(*Localize(TEXT("ROAD")), Have, 1, 1, 0, 0, 0) + TEXT("\n")
        + CostLine(*Localize(TEXT("SETTLEMENT")), Have, 1, 1, 1, 1, 0) + TEXT("\n")
        + CostLine(*Localize(TEXT("CITY")), Have, 0, 0, 2, 0, 3) + TEXT("\n")
        + CostLine(*Localize(TEXT("DEV CARD")), Have, 0, 0, 1, 1, 1)));
    FString Availability;
    if (!bPlay && !bRoll) Availability = PhaseHint(View.Phase, UserPreferences.Language);
    else if (bRoll) Availability = Localize(TEXT("Roll the dice before building or trading."));
    else
    {
        TArray<FString> Missing;
        if (!bCanRoad) Missing.Add(Localize(TEXT("road")));
        if (!bCanSettlement) Missing.Add(Localize(TEXT("settlement")));
        if (!bCanCity) Missing.Add(Localize(TEXT("city")));
        if (!bCanCard) Missing.Add(Localize(TEXT("development card")));
        Availability = Missing.IsEmpty()
            ? Localize(TEXT("All purchases are affordable. Choose an action."))
            : Localize(TEXT("Need more resources for:")) + TEXT(" ")
                + FString::Join(Missing, TEXT(", ")) + TEXT(".");
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
        Label->SetText(FText::FromString(Localize(bCompactLayout
            ? TEXT("DEV") : (ReadyCards > 0 ? TEXT("USE DEV") : TEXT("VIEW DEV")))));
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
        const int32 SetupPage = SetupSwitcher ? SetupSwitcher->GetActiveWidgetIndex() : SetupMainIndex;
        if (SetupPage == SetupOnlineIndex)
            SetModalSize(760.0f, 520.0f);
        else if (SetupPage == SetupLocalNetworkIndex)
            SetModalSize(900.0f, 720.0f);
        else if (SetupPage == SetupDedicatedServerIndex)
            SetModalSize(760.0f, NetworkSubsystem
                && NetworkSubsystem->HasSavedDedicatedSession() ? 680.0f : 590.0f);
        else if (SetupPage == SetupBotsIndex)
            SetModalSize(760.0f, 540.0f);
        else if (SetupPage == SetupSettingsIndex)
            SetModalSize(760.0f, 560.0f);
        else
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
                TEXT("%s — %d VP | %d VP cards | %d dev | %d resources\n  %d settlements, %d cities, %d roads remaining\n"),
                *Player.Name, Player.VictoryPoints, Player.VictoryPointCards,
                Player.DevelopmentCards, Player.ResourceCards,
                Player.FreeSettlements, Player.FreeCities, Player.FreeRoads);
        }
        WinnerText->SetText(FText::FromString(Standings));
        bDevelopmentPanelOpen = false;
        bTradePanelOpen = false;
    }
    else if (GameSubsystem->HasPendingBuildTarget())
    {
        SetModalSize(620.0f, 360.0f);
        SetModalPosition(FVector2D(bCompactLayout ? 250.0f : 360.0f, 0.0f));
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(7);
        FString Prompt;
        switch (GameSubsystem->GetPendingBuildAction())
        {
        case ECatanBoardAction::BuildRoad: Prompt = Localize(TEXT("Build this road?")); break;
        case ECatanBoardAction::BuildCity: Prompt = Localize(TEXT("Upgrade this settlement to a city?")); break;
        default: Prompt = Localize(TEXT("Build this settlement?")); break;
        }
        ConfirmationText->SetText(FText::FromString(Prompt + TEXT("\n")
            + Localize(TEXT("The selected target is highlighted in red."))));
    }
    else if (PendingExpensiveAction != 0)
    {
        SetModalSize(680.0f, 420.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(7);
        ConfirmationText->SetText(FText::FromString(Localize(TEXT("Buy a random development card?"))
            + TEXT("\n") + Localize(TEXT("This costs 1 hay, 1 sheep and 1 ore."))));
    }
    else if (View.Phase == ECatanGamePhase::DropCards && bLocalTurn && LocalPlayer)
    {
        SetModalSize(900.0f, 620.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(0);
        DropTitle->SetText(FText::FromString(Localize(TEXT("DISCARD")) + FString::Printf(TEXT(" %d "),
            View.RequiredDiscardCount) + Localize(TEXT("RESOURCES")) + TEXT(" — ") + View.CurrentPlayer));
        const bool bResetDrop = LastDropPlayer != View.CurrentPlayer;
        UpdateDropLimits(LocalPlayer->Resources, bResetDrop);
        if (bResetDrop) LastDropPlayer = View.CurrentPlayer;
        UpdateDropConfirmation(FString(), ESelectInfo::Direct);
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
        ShowCard(RoadBuildingButton, (bPlay || bRoll) && LocalPlayer->RoadBuildingCards > 0);
        ShowCard(YearOfPlentyButton, (bPlay || bRoll) && LocalPlayer->YearOfPlentyCards > 0);
        ShowCard(MonopolyButton, (bPlay || bRoll) && LocalPlayer->MonopolyCards > 0);
        if (DevelopmentModeSwitcher && DevelopmentModeSwitcher->GetActiveWidgetIndex() == 1
            && (!(bPlay || bRoll) || LocalPlayer->YearOfPlentyCards <= 0))
            DevelopmentModeSwitcher->SetActiveWidgetIndex(0);
        if (DevelopmentModeSwitcher && DevelopmentModeSwitcher->GetActiveWidgetIndex() == 2
            && (!(bPlay || bRoll) || LocalPlayer->MonopolyCards <= 0))
            DevelopmentModeSwitcher->SetActiveWidgetIndex(0);
        const int32 ReadyCount = LocalPlayer->Knights + LocalPlayer->RoadBuildingCards
            + LocalPlayer->YearOfPlentyCards + LocalPlayer->MonopolyCards;
        const int32 PassiveVictoryCards = FMath::Max(0, LocalPlayer->DevelopmentCards
            - ReadyCount - LocalPlayer->PendingDevelopmentCards);
        FString CardState;
        if (ReadyCount > 0)
            CardState += Localize(TEXT("Ready to play:")) + FString::Printf(TEXT(" %d."), ReadyCount);
        if (LocalPlayer->PendingDevelopmentCards > 0)
            CardState += (CardState.IsEmpty() ? TEXT("") : TEXT("\n"))
                + Localize(TEXT("Bought this turn:"))
                + FString::Printf(TEXT(" %d — "), LocalPlayer->PendingDevelopmentCards)
                + Localize(TEXT("available next turn."));
        if (PassiveVictoryCards > 0)
            CardState += (CardState.IsEmpty() ? TEXT("") : TEXT("\n"))
                + Localize(TEXT("Victory point cards:")) + FString::Printf(TEXT(" %d — "), PassiveVictoryCards)
                + Localize(TEXT("passive, they are never played."));
        if (CardState.IsEmpty()) CardState = Localize(TEXT("You have no development cards available to play."));
        DevelopmentAvailabilityText->SetText(FText::FromString(CardState));
    }
    else if (bTradePanelOpen && LocalPlayer && bLocalTurn && bPlay)
    {
        SetModalSize(1040.0f, 760.0f);
        ModalBorder->SetVisibility(ESlateVisibility::Visible);
        ModalSwitcher->SetActiveWidgetIndex(3);
        CurrentBankTradeRates = LocalPlayer->TradeRates;
        UpdatePlayerTradeLimits(LocalPlayer->Resources);
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

void UCatanHUDWidget::RefreshSavedGameOptions()
{
    if (!GameSubsystem || !SavedGameInput || !LoadLanButton) return;
    const TArray<FCatanLanSaveSummary> Saves = GameSubsystem->ListLanSavedGames();
    FString Digest;
    for (const FCatanLanSaveSummary& Save : Saves)
        Digest += Save.SlotId + TEXT("|") + Save.Label + (Save.bValid ? TEXT("1;") : TEXT("0;"));
    if (Digest != SavedGameCatalogDigest)
    {
        const FString PreviousSlot = SavedGameSlotIds.IsValidIndex(SavedGameInput->GetSelectedIndex())
            ? SavedGameSlotIds[SavedGameInput->GetSelectedIndex()] : FString();
        SavedGameCatalogDigest = Digest;
        SavedGameSlotIds.Reset();
        SavedGameSlotValid.Reset();
        SavedGameInput->ClearOptions();
        int32 Selected = INDEX_NONE;
        int32 FirstValid = INDEX_NONE;
        for (const FCatanLanSaveSummary& Save : Saves)
        {
            const int32 Index = SavedGameSlotIds.Add(Save.SlotId);
            SavedGameSlotValid.Add(Save.bValid);
            SavedGameInput->AddOption(Save.Label);
            if (Save.SlotId == PreviousSlot) Selected = Index;
            if (FirstValid == INDEX_NONE && Save.bValid) FirstValid = Index;
        }
        if (Saves.IsEmpty()) SavedGameInput->AddOption(Localize(TEXT("No saved games")));
        SavedGameInput->SetSelectedIndex(Selected != INDEX_NONE ? Selected
            : (FirstValid != INDEX_NONE ? FirstValid : 0));
        const int32 ActiveIndex = SavedGameInput->GetSelectedIndex();
        UE_LOG(LogTemp, Display, TEXT("CATAN_SAVE_CATALOG slots=%d valid=%d selectedValid=%d"), Saves.Num(),
            Saves.FilterByPredicate([](const FCatanLanSaveSummary& Save) { return Save.bValid; }).Num(),
            SavedGameSlotValid.IsValidIndex(ActiveIndex) && SavedGameSlotValid[ActiveIndex]);
    }
    const int32 Index = SavedGameInput->GetSelectedIndex();
    LoadLanButton->SetIsEnabled(SavedGameSlotValid.IsValidIndex(Index) && SavedGameSlotValid[Index]);
}

void UCatanHUDWidget::UpdateActionPanelVisibility()
{
    if (!ActionBorder || !ModalBorder) return;
    const ESlateVisibility ModalVisibility = ModalBorder->GetVisibility();
    const bool bModalOpen = ModalVisibility != ESlateVisibility::Collapsed
        && ModalVisibility != ESlateVisibility::Hidden;
    const bool bShowActions = !bModalOpen;
    ActionBorder->SetVisibility(bShowActions ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bShowActions != bActionPanelWasVisible)
    {
        bActionPanelWasVisible = bShowActions;
        UE_LOG(LogTemp, Display, TEXT("CATAN_ACTION_PANEL modal=%d visible=%d"),
            bModalOpen, bShowActions);
    }
}

void UCatanHUDWidget::ApplyUIPreview()
{
    FString Preview;
    if (!FParse::Value(FCommandLine::Get(), TEXT("CatanUIPreview="), Preview)) return;
    ModalBorder->SetVisibility(ESlateVisibility::Visible);
    if (Preview.Equals(TEXT("Game"), ESearchCase::IgnoreCase))
    {
        bSetupPanelOpen = false;
        ModalBorder->SetVisibility(ESlateVisibility::Collapsed);
    }
    else if (Preview.Equals(TEXT("Players4"), ESearchCase::IgnoreCase))
    {
        bSetupPanelOpen = false;
        ModalBorder->SetVisibility(ESlateVisibility::Collapsed);
        bLeftDetailsOpen = false;
        bRightDetailsOpen = true;
        ApplyAdaptiveLayout(true);
    }
    else if (Preview.Equals(TEXT("Bank"), ESearchCase::IgnoreCase))
    {
        SetModalSize(1040.0f, 760.0f);
        ModalSwitcher->SetActiveWidgetIndex(3);
        TradeModeSwitcher->SetActiveWidgetIndex(0);
        CurrentBankTradeRates.Wood = 4;
        CurrentBankTradeRates.Clay = 3;
        CurrentBankTradeRates.Hay = 4;
        CurrentBankTradeRates.Sheep = 2;
        CurrentBankTradeRates.Stone = 4;
        UpdateBankSelectionStyles();
        if (!bUIPreviewReported)
            UE_LOG(LogTemp, Display, TEXT("CATAN_BANK_LABELS rates=4,3,4,2,4"));
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
        FCatanResourceView PreviewResources;
        PreviewResources.Wood = 1;
        PreviewResources.Clay = 2;
        PreviewResources.Hay = 3;
        PreviewResources.Sheep = 4;
        PreviewResources.Stone = 7;
        UpdatePlayerTradeLimits(PreviewResources);
        if (!bUIPreviewReported && OfferedInputs.Num() == 5 && RequestedInputs.Num() == 5)
            UE_LOG(LogTemp, Display, TEXT("CATAN_PLAYER_TRADE_LIMITS max=%d,%d,%d,%d,%d receive=%d"),
                OfferedInputs[0]->GetOptionCount() - 1, OfferedInputs[1]->GetOptionCount() - 1,
                OfferedInputs[2]->GetOptionCount() - 1, OfferedInputs[3]->GetOptionCount() - 1,
                OfferedInputs[4]->GetOptionCount() - 1, RequestedInputs[0]->GetOptionCount() - 1);
    }
    else if (Preview.Equals(TEXT("Discard"), ESearchCase::IgnoreCase))
    {
        SetModalSize(900.0f, 620.0f);
        ModalSwitcher->SetActiveWidgetIndex(0);
        DropTitle->SetText(FText::FromString(TEXT("DISCARD 4 RESOURCES — PLAYER")));
        FCatanResourceView PreviewResources;
        PreviewResources.Wood = PreviewResources.Clay = PreviewResources.Hay
            = PreviewResources.Sheep = PreviewResources.Stone = 8;
        UpdateDropLimits(PreviewResources, true);
        if (!bUIPreviewReported)
            UE_LOG(LogTemp, Display, TEXT("CATAN_DISCARD_LIMITS max=8,8,8,8,8 integerDropdowns=1"));
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
    else if (Preview.Equals(TEXT("Development"), ESearchCase::IgnoreCase))
    {
        SetModalSize(760.0f, 650.0f);
        ModalSwitcher->SetActiveWidgetIndex(2);
        DevelopmentModeSwitcher->SetActiveWidgetIndex(0);
    }
    else if (Preview.Equals(TEXT("DevelopmentPlenty"), ESearchCase::IgnoreCase))
    {
        SetModalSize(760.0f, 650.0f);
        ModalSwitcher->SetActiveWidgetIndex(2);
        DevelopmentModeSwitcher->SetActiveWidgetIndex(1);
        ResetPlentyInputs();
        if (!bUIPreviewReported)
            UE_LOG(LogTemp, Display, TEXT("CATAN_DEVELOPMENT_MENU mode=plenty totalLimit=2"));
    }
    else if (Preview.Equals(TEXT("DevelopmentMonopoly"), ESearchCase::IgnoreCase))
    {
        SetModalSize(760.0f, 650.0f);
        ModalSwitcher->SetActiveWidgetIndex(2);
        DevelopmentModeSwitcher->SetActiveWidgetIndex(2);
        UpdateMonopolySelectionStyles();
        if (!bUIPreviewReported)
            UE_LOG(LogTemp, Display, TEXT("CATAN_DEVELOPMENT_MENU mode=monopoly singleSelection=1"));
    }
    else if (Preview.Equals(TEXT("Online"), ESearchCase::IgnoreCase))
    {
        ModalSwitcher->SetActiveWidgetIndex(6);
        if (!bUIPreviewReported)
            SetupSwitcher->SetActiveWidgetIndex(SetupOnlineIndex);
    }
    else if (Preview.Equals(TEXT("LocalNetwork"), ESearchCase::IgnoreCase))
    {
        ModalSwitcher->SetActiveWidgetIndex(6);
        if (!bUIPreviewReported)
            SetupSwitcher->SetActiveWidgetIndex(SetupLocalNetworkIndex);
    }
    else if (Preview.Equals(TEXT("DedicatedServer"), ESearchCase::IgnoreCase))
    {
        ModalSwitcher->SetActiveWidgetIndex(6);
        if (!bUIPreviewReported)
            SetupSwitcher->SetActiveWidgetIndex(SetupDedicatedServerIndex);
    }
    else if (Preview.Equals(TEXT("Bots"), ESearchCase::IgnoreCase))
    {
        SetModalSize(760.0f, 540.0f);
        ModalSwitcher->SetActiveWidgetIndex(6);
        SetupSwitcher->SetActiveWidgetIndex(SetupBotsIndex);
    }
    else if (Preview.Equals(TEXT("Settings"), ESearchCase::IgnoreCase))
    {
        SetModalSize(760.0f, 560.0f);
        ModalSwitcher->SetActiveWidgetIndex(6);
        SetupSwitcher->SetActiveWidgetIndex(SetupSettingsIndex);
        UE_LOG(LogTemp, Display, TEXT("CATAN_SETTINGS_PREVIEW name=%s language=%s"),
            *UserPreferences.PlayerName, *FCatanTextResources::LanguageCode(UserPreferences.Language));
    }
    if (!bUIPreviewReported)
    {
        bUIPreviewReported = true;
        UE_LOG(LogTemp, Display, TEXT("CATAN_UI_PREVIEW ready mode=%s"), *Preview);
    }
}

void UCatanHUDWidget::RunHUDGraphSmoke()
{
    if (!GameSubsystem)
        GameSubsystem = GetGameInstance()->GetSubsystem<UCatanGameSubsystem>();
    if (!NetworkSubsystem)
        NetworkSubsystem = GetGameInstance()->GetSubsystem<UCatanNetworkSubsystem>();
    int32 Edges = 0;
    int32 Failures = 0;
    auto Verify = [&Edges, &Failures](const TCHAR* Edge, bool bCondition)
    {
        ++Edges;
        if (!bCondition)
        {
            ++Failures;
            UE_LOG(LogTemp, Error, TEXT("CATAN_HUD_GRAPH FAIL edge=%s"), Edge);
        }
        else UE_LOG(LogTemp, Display, TEXT("CATAN_HUD_GRAPH edge=%s"), Edge);
    };

    bSetupPanelOpen = true;
    ShowMainSetup();
    Verify(TEXT("main-modal-hides-actions"), ActionBorder->GetVisibility() == ESlateVisibility::Collapsed);
    Verify(TEXT("main-online"), (ShowOnlineSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupOnlineIndex));
    Verify(TEXT("online-local"), (ShowLocalNetworkSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupLocalNetworkIndex));
    ManualAddressInput->SetText(FText::GetEmpty());
    JoinManualLobby();
    Verify(TEXT("local-empty-address-rejected"), NetworkSubsystem
        && NetworkSubsystem->GetStatus().Contains(TEXT("Enter host IP")));
    JoinSelectedLobby();
    Verify(TEXT("local-empty-selection-rejected"), NetworkSubsystem
        && NetworkSubsystem->GetStatus().Contains(TEXT("Select a discovered lobby")));
    Verify(TEXT("local-back-online"), (ShowOnlineSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupOnlineIndex));
    Verify(TEXT("online-dedicated"), (ShowDedicatedServerSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupDedicatedServerIndex));
    const FText SavedDedicatedAddress = DedicatedAddressInput->GetText();
    DedicatedAddressInput->SetText(FText::GetEmpty());
    CreateDedicatedLobby();
    Verify(TEXT("dedicated-empty-create-address-rejected"), NetworkSubsystem
        && NetworkSubsystem->GetStatus().Contains(TEXT("Enter server IP")));
    JoinDedicatedLobby();
    Verify(TEXT("dedicated-empty-join-address-rejected"), NetworkSubsystem
        && NetworkSubsystem->GetStatus().Contains(TEXT("Enter server IP")));
    DedicatedAddressInput->SetText(SavedDedicatedAddress);
    Verify(TEXT("dedicated-resume-availability"), ResumeDedicatedButton
        && (ResumeDedicatedButton->GetVisibility() == ESlateVisibility::Visible)
            == NetworkSubsystem->HasSavedDedicatedSession());
    Verify(TEXT("dedicated-back-online"), (ShowOnlineSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupOnlineIndex));
    Verify(TEXT("online-back-main"), (ShowMainSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupMainIndex));
    Verify(TEXT("main-bots"), (ShowBotSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupBotsIndex));
    Verify(TEXT("bots-back-main"), (ShowMainSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupMainIndex));
    Verify(TEXT("main-settings"), (ShowSettings(), SetupSwitcher->GetActiveWidgetIndex() == SetupSettingsIndex));
    SaveSettings();
    Verify(TEXT("settings-save-main"), SetupSwitcher->GetActiveWidgetIndex() == SetupMainIndex);
    ShowSettings();
    Verify(TEXT("settings-back-main"), (ShowMainSetup(), SetupSwitcher->GetActiveWidgetIndex() == SetupMainIndex));

    ShowBotSetup();
    StartBotMatch();
    Verify(TEXT("bots-start-game"), !bSetupPanelOpen && GameSubsystem->GetSnapshot().Players.Num() == 2);
    Verify(TEXT("game-shows-actions"), ActionBorder->GetVisibility() == ESlateVisibility::Visible);
    StartNewGame();
    ShowMainSetup();

    bSetupPanelOpen = false;
    ShowDevelopmentCards();
    Verify(TEXT("game-development"), bDevelopmentPanelOpen
        && DevelopmentModeSwitcher->GetActiveWidgetIndex() == 0);
    ShowYearOfPlentyParameters();
    Verify(TEXT("development-plenty"), DevelopmentModeSwitcher->GetActiveWidgetIndex() == 1);
    CancelDevelopmentParameters();
    Verify(TEXT("plenty-back-development"), DevelopmentModeSwitcher->GetActiveWidgetIndex() == 0);
    ShowMonopolyParameters();
    Verify(TEXT("development-monopoly"), DevelopmentModeSwitcher->GetActiveWidgetIndex() == 2);
    CancelDevelopmentParameters();
    Verify(TEXT("monopoly-back-development"), DevelopmentModeSwitcher->GetActiveWidgetIndex() == 0);
    CloseDevelopmentCards();
    Verify(TEXT("development-close-game"), !bDevelopmentPanelOpen);

    ShowTrading();
    Verify(TEXT("game-trade-bank"), bTradePanelOpen && TradeModeSwitcher->GetActiveWidgetIndex() == 0);
    ShowPlayerTrade();
    Verify(TEXT("trade-bank-player"), TradeModeSwitcher->GetActiveWidgetIndex() == 1);
    ShowBankTrade();
    Verify(TEXT("trade-player-bank"), TradeModeSwitcher->GetActiveWidgetIndex() == 0);
    CloseTrading();
    Verify(TEXT("trade-close-game"), !bTradePanelOpen);

    BuyDevelopmentCard();
    Verify(TEXT("game-buy-confirmation"), PendingExpensiveAction == 2);
    Verify(TEXT("confirmation-hides-actions"), ActionBorder->GetVisibility() == ESlateVisibility::Collapsed);
    CancelExpensiveAction();
    Verify(TEXT("confirmation-cancel-game"), PendingExpensiveAction == 0);
    Verify(TEXT("confirmation-cancel-restores-actions"), ActionBorder->GetVisibility() == ESlateVisibility::Visible);
    ApplyAdaptiveLayout(true);
    ToggleLeftDetails();
    Verify(TEXT("game-open-left-details"), bLeftDetailsOpen);
    ToggleLeftDetails();
    Verify(TEXT("left-details-close-game"), !bLeftDetailsOpen);
    ToggleRightDetails();
    Verify(TEXT("game-open-right-details"), bRightDetailsOpen);
    ToggleRightDetails();
    Verify(TEXT("right-details-close-game"), !bRightDetailsOpen);
    ApplyAdaptiveLayout(false);
    StartNewGame();
    ShowMainSetup();
    Verify(TEXT("game-new-game-main"), bSetupPanelOpen
        && SetupSwitcher->GetActiveWidgetIndex() == SetupMainIndex);

    if (Failures == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("CATAN_HUD_GRAPH PASS edges=%d failures=0"), Edges);
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("CATAN_HUD_GRAPH FAIL edges=%d failures=%d"), Edges, Failures);
    }
}

void UCatanHUDWidget::ScheduleAutomatedLobbyLeave(int32 LobbyPlayerCount, bool bLocalHost)
{
    if (bAutoLeaveScheduled || !FParse::Param(FCommandLine::Get(), TEXT("CatanAutoLeaveLobby"))) return;
    int32 RequiredPlayers = 1;
    FParse::Value(FCommandLine::Get(), TEXT("CatanAutoLeaveWhenPlayers="), RequiredPlayers);
    if (LobbyPlayerCount < FMath::Max(1, RequiredPlayers)) return;
    bAutoLeaveScheduled = true;
    UE_LOG(LogTemp, Display, TEXT("CATAN_HUD_GRAPH leave-scheduled role=%s players=%d"),
        bLocalHost ? TEXT("host") : TEXT("client"), LobbyPlayerCount);
    TWeakObjectPtr<UCatanHUDWidget> WeakThis(this);
    FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
    {
        if (WeakThis.IsValid()) WeakThis->LeaveLobby();
        return false;
    }), 0.5f);
}

void UCatanHUDWidget::SetModalSize(float Width, float Height)
{
    if (UCanvasPanelSlot* Slot = ModalBorder ? Cast<UCanvasPanelSlot>(ModalBorder->Slot) : nullptr)
        Slot->SetSize(FVector2D(Width, Height));
}

void UCatanHUDWidget::SetModalPosition(const FVector2D& Position)
{
    if (UCanvasPanelSlot* Slot = ModalBorder ? Cast<UCanvasPanelSlot>(ModalBorder->Slot) : nullptr)
        Slot->SetPosition(Position);
}

void UCatanHUDWidget::ReportComboOpening()
{
    UE_LOG(LogTemp, Display, TEXT("CATAN_COMBO_OPEN"));
}

void UCatanHUDWidget::HostLanLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    NetworkSubsystem->HostLobby(PlayerName, LobbyNameInput->GetText().ToString());
}

void UCatanHUDWidget::LoadLanLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName)) return;
    const int32 Index = SavedGameInput ? SavedGameInput->GetSelectedIndex() : INDEX_NONE;
    if (!SavedGameSlotIds.IsValidIndex(Index) || !SavedGameSlotValid.IsValidIndex(Index)
        || !SavedGameSlotValid[Index]) return;
    NetworkSubsystem->HostSavedLobby(PlayerName, SavedGameSlotIds[Index]);
}

void UCatanHUDWidget::UpdateSavedGameSelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    (void)SelectedItem;
    (void)SelectionType;
    if (!LoadLanButton || !SavedGameInput) return;
    const int32 Index = SavedGameInput->GetSelectedIndex();
    LoadLanButton->SetIsEnabled(SavedGameSlotValid.IsValidIndex(Index) && SavedGameSlotValid[Index]);
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
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupOnlineIndex);
    SetModalSize(760.0f, 520.0f);
    UE_LOG(LogTemp, Display, TEXT("CATAN_ONLINE_NAV page=chooser"));
}

void UCatanHUDWidget::ShowLocalNetworkSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupLocalNetworkIndex);
    SetModalSize(900.0f, 720.0f);
    UE_LOG(LogTemp, Display, TEXT("CATAN_ONLINE_NAV page=local"));
}

void UCatanHUDWidget::ShowDedicatedServerSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupDedicatedServerIndex);
    SetModalSize(760.0f, 590.0f);
    UE_LOG(LogTemp, Display, TEXT("CATAN_ONLINE_NAV page=dedicated"));
}

void UCatanHUDWidget::ShowBotSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupBotsIndex);
    SetModalSize(760.0f, 540.0f);
}

void UCatanHUDWidget::ShowSettings()
{
    if (SettingsNameInput) SettingsNameInput->SetText(FText::FromString(UserPreferences.PlayerName));
    if (SettingsLanguageInput)
        SettingsLanguageInput->SetSelectedIndex(
            UserPreferences.Language == ECatanLanguage::Russian ? 1 : 0);
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupSettingsIndex);
    SetModalSize(760.0f, 560.0f);
}

void UCatanHUDWidget::SaveSettings()
{
    UserPreferences.PlayerName = FCatanUserSettings::NormalizePlayerName(
        SettingsNameInput ? SettingsNameInput->GetText().ToString() : UserPreferences.PlayerName);
    UserPreferences.Language = SettingsLanguageInput && SettingsLanguageInput->GetSelectedIndex() == 1
        ? ECatanLanguage::Russian : ECatanLanguage::English;
    FCatanUserSettings::Save(UserPreferences);
    ApplyLanguage();
    ToastText->SetText(FText::FromString(Localize(TEXT("Settings saved"))));
    ToastBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
    ToastBorder->SetRenderOpacity(1.0f);
    ToastRemaining = 2.5f;
    ShowMainSetup();
    Refresh();
    UE_LOG(LogTemp, Display, TEXT("CATAN_SETTINGS saved name=%s language=%s"),
        *UserPreferences.PlayerName, *FCatanTextResources::LanguageCode(UserPreferences.Language));
}

void UCatanHUDWidget::ShowMainSetup()
{
    if (SetupSwitcher) SetupSwitcher->SetActiveWidgetIndex(SetupMainIndex);
    if (MainPlayerNameText)
        MainPlayerNameText->SetText(FText::FromString(FString::Printf(TEXT("%s: %s"),
            *Localize(TEXT("PLAYER")), *UserPreferences.PlayerName)));
    SetModalSize(900.0f, 650.0f);
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

void UCatanHUDWidget::CreateDedicatedLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName) || !NetworkSubsystem || !DedicatedAddressInput) return;
    NetworkSubsystem->CreateDedicatedLobby(DedicatedAddressInput->GetText().ToString(), PlayerName,
        LobbyNameInput ? LobbyNameInput->GetText().ToString() : TEXT("Catan lobby"));
}

void UCatanHUDWidget::JoinDedicatedLobby()
{
    FString PlayerName;
    if (!GetValidatedPlayerName(PlayerName) || !NetworkSubsystem
        || !DedicatedAddressInput || !DedicatedLobbyTokenInput) return;
    NetworkSubsystem->JoinDedicatedLobby(DedicatedAddressInput->GetText().ToString(),
        DedicatedLobbyTokenInput->GetText().ToString(), PlayerName);
}

void UCatanHUDWidget::ResumeDedicatedLobby()
{
    if (NetworkSubsystem) NetworkSubsystem->ResumeSavedDedicatedLobby();
}

void UCatanHUDWidget::CopyDedicatedLobbyToken()
{
    if (!NetworkSubsystem || !NetworkSubsystem->IsDedicatedActive()) return;
    FPlatformApplicationMisc::ClipboardCopy(*NetworkSubsystem->GetDedicatedLobbyToken());
}

bool UCatanHUDWidget::GetValidatedPlayerName(FString& OutName)
{
    OutName = FCatanUserSettings::NormalizePlayerName(UserPreferences.PlayerName);
    return !OutName.IsEmpty();
}

void UCatanHUDWidget::ToggleLobbyReady()
{
    if (NetworkSubsystem && NetworkSubsystem->IsDedicatedActive())
    {
        const FCatanLobbyPlayerView* Local = NetworkSubsystem->GetDedicatedLobbyPlayers().FindByPredicate(
            [this](const FCatanLobbyPlayerView& Player)
            { return Player.Name == NetworkSubsystem->GetDedicatedPlayerName(); });
        NetworkSubsystem->SetDedicatedReady(!(Local && Local->bReady));
        return;
    }
    if (ACatanPlayerController* Controller = Cast<ACatanPlayerController>(GetOwningPlayer()))
    {
        const ACatanPlayerState* State = Controller->GetPlayerState<ACatanPlayerState>();
        Controller->ServerSetLobbyReady(!(State && State->bLobbyReady));
    }
}

void UCatanHUDWidget::StartLobbyMatch()
{
    if (NetworkSubsystem && NetworkSubsystem->IsDedicatedActive())
    {
        NetworkSubsystem->StartDedicatedGame();
        return;
    }
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
    GameSubsystem->SelectBoardAction(ECatanBoardAction::BuildCity);
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
    if (DevelopmentModeSwitcher) DevelopmentModeSwitcher->SetActiveWidgetIndex(0);
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
    Resources.Wood = FCString::Atoi(*DropInputs[0]->GetSelectedOption());
    Resources.Clay = FCString::Atoi(*DropInputs[1]->GetSelectedOption());
    Resources.Hay = FCString::Atoi(*DropInputs[2]->GetSelectedOption());
    Resources.Sheep = FCString::Atoi(*DropInputs[3]->GetSelectedOption());
    Resources.Stone = FCString::Atoi(*DropInputs[4]->GetSelectedOption());
    FString Error;
    GameSubsystem->TryDropResources(Resources, Error);
}

void UCatanHUDWidget::UpdateDropConfirmation(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (!ConfirmDropButton || !GameSubsystem || DropInputs.Num() != 5) return;
    FCatanResourceView Selected;
    Selected.Wood = FCString::Atoi(*DropInputs[0]->GetSelectedOption());
    Selected.Clay = FCString::Atoi(*DropInputs[1]->GetSelectedOption());
    Selected.Hay = FCString::Atoi(*DropInputs[2]->GetSelectedOption());
    Selected.Sheep = FCString::Atoi(*DropInputs[3]->GetSelectedOption());
    Selected.Stone = FCString::Atoi(*DropInputs[4]->GetSelectedOption());
    const FCatanGameView View = GameSubsystem->GetSnapshot();
    const FCatanPlayerView* LocalPlayer = View.Players.FindByPredicate(
        [](const FCatanPlayerView& Player) { return Player.bIsLocalPlayer && Player.bResourcesVisible; });
    ConfirmDropButton->SetIsEnabled(LocalPlayer &&
        CatanInteractionPolicy::IsDiscardSelectionValid(
            Selected, LocalPlayer->Resources, View.RequiredDiscardCount));
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

void UCatanHUDWidget::PlayDevelopmentCard(ECatanDevelopmentCard Card)
{
    FString Error;
    const bool bSucceeded = GameSubsystem->TryUseDevelopmentCard(
        Card, ECatanResource::Wood, ECatanResource::Wood, Error);
    if (bSucceeded) bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::ShowYearOfPlentyParameters()
{
    ResetPlentyInputs();
    if (DevelopmentModeSwitcher) DevelopmentModeSwitcher->SetActiveWidgetIndex(1);
    SetModalSize(760.0f, 650.0f);
}

void UCatanHUDWidget::ShowMonopolyParameters()
{
    MonopolySelection = ECatanResource::Wood;
    UpdateMonopolySelectionStyles();
    if (DevelopmentModeSwitcher) DevelopmentModeSwitcher->SetActiveWidgetIndex(2);
}

void UCatanHUDWidget::ConfirmYearOfPlenty()
{
    TArray<ECatanResource> Selected;
    for (int32 Resource = 0; Resource < PlentyInputs.Num(); ++Resource)
        for (int32 Count = 0; Count < FCString::Atoi(*PlentyInputs[Resource]->GetSelectedOption()); ++Count)
            Selected.Add(static_cast<ECatanResource>(Resource));
    if (Selected.Num() != 2) return;
    FString Error;
    if (GameSubsystem->TryUseDevelopmentCard(ECatanDevelopmentCard::YearOfPlenty,
        Selected[0], Selected[1], Error))
        bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::ConfirmMonopoly()
{
    FString Error;
    if (GameSubsystem->TryUseDevelopmentCard(ECatanDevelopmentCard::Monopoly,
        MonopolySelection, MonopolySelection, Error))
        bDevelopmentPanelOpen = false;
    Refresh();
}

void UCatanHUDWidget::CancelDevelopmentParameters()
{
    if (DevelopmentModeSwitcher) DevelopmentModeSwitcher->SetActiveWidgetIndex(0);
    Refresh();
}

void UCatanHUDWidget::ResetPlentyInputs()
{
    bUpdatingPlentyInputs = true;
    for (UComboBoxString* Input : PlentyInputs)
    {
        Input->ClearOptions();
        for (int32 Count = 0; Count <= 2; ++Count) Input->AddOption(FString::FromInt(Count));
        Input->SetSelectedOption(TEXT("0"));
    }
    bUpdatingPlentyInputs = false;
    UpdatePlentySelection(FString(), ESelectInfo::Direct);
}

void UCatanHUDWidget::UpdatePlentySelection(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingPlentyInputs || PlentyInputs.Num() != 5) return;
    int32 Values[5]{};
    int32 Total = 0;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        Values[Index] = FMath::Clamp(FCString::Atoi(*PlentyInputs[Index]->GetSelectedOption()), 0, 2);
        Total += Values[Index];
    }
    bUpdatingPlentyInputs = true;
    for (int32 Index = 0; Index < 5; ++Index)
    {
        UComboBoxString* Input = PlentyInputs[Index];
        const int32 MaxValue = CatanInteractionPolicy::YearOfPlentyMaxForResource(
            Total - Values[Index]);
        const int32 Value = FMath::Min(Values[Index], MaxValue);
        if (Input->GetOptionCount() != MaxValue + 1)
        {
            Input->ClearOptions();
            for (int32 Count = 0; Count <= MaxValue; ++Count)
                Input->AddOption(FString::FromInt(Count));
        }
        Input->SetSelectedOption(FString::FromInt(Value));
        Values[Index] = Value;
    }
    bUpdatingPlentyInputs = false;
    FCatanResourceView Selection;
    Selection.Wood = Values[0];
    Selection.Clay = Values[1];
    Selection.Hay = Values[2];
    Selection.Sheep = Values[3];
    Selection.Stone = Values[4];
    if (ConfirmPlentyButton) ConfirmPlentyButton->SetIsEnabled(
        CatanInteractionPolicy::IsYearOfPlentySelectionComplete(Selection));
}

void UCatanHUDWidget::UpdateMonopolySelectionStyles()
{
    for (int32 Index = 0; Index < MonopolyResourceButtons.Num(); ++Index)
    {
        const bool bSelected = Index == static_cast<int32>(MonopolySelection);
        MonopolyResourceButtons[Index]->SetRenderScale(bSelected ? FVector2D(1.06f) : FVector2D(1.0f));
        MonopolyResourceButtons[Index]->SetRenderOpacity(bSelected ? 1.0f : 0.52f);
    }
}

void UCatanHUDWidget::SelectMonopolyWood() { MonopolySelection = ECatanResource::Wood; UpdateMonopolySelectionStyles(); }
void UCatanHUDWidget::SelectMonopolyClay() { MonopolySelection = ECatanResource::Clay; UpdateMonopolySelectionStyles(); }
void UCatanHUDWidget::SelectMonopolyHay() { MonopolySelection = ECatanResource::Hay; UpdateMonopolySelectionStyles(); }
void UCatanHUDWidget::SelectMonopolySheep() { MonopolySelection = ECatanResource::Sheep; UpdateMonopolySelectionStyles(); }
void UCatanHUDWidget::SelectMonopolyStone() { MonopolySelection = ECatanResource::Stone; UpdateMonopolySelectionStyles(); }

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
    static const TCHAR* ResourceNames[] = { TEXT("Wood"), TEXT("Clay"), TEXT("Hay"), TEXT("Sheep"), TEXT("Stone") };
    const int32 Rates[] = {
        CurrentBankTradeRates.Wood, CurrentBankTradeRates.Clay, CurrentBankTradeRates.Hay,
        CurrentBankTradeRates.Sheep, CurrentBankTradeRates.Stone
    };
    for (int32 Index = 0; Index < BankFromButtons.Num(); ++Index)
    {
        const bool bSelected = Index == static_cast<int32>(BankFromSelection);
        BankFromButtons[Index]->SetRenderScale(bSelected ? FVector2D(1.06f) : FVector2D(1.0f));
        BankFromButtons[Index]->SetRenderOpacity(bSelected ? 1.0f : 0.52f);
        if (UCommonTextBlock* Label = Cast<UCommonTextBlock>(BankFromButtons[Index]->GetChildAt(0)))
            Label->SetText(FText::FromString(FString::Printf(TEXT("%dx %s"), Rates[Index],
                *Localize(ResourceNames[Index]))));
    }
    for (int32 Index = 0; Index < BankToButtons.Num(); ++Index)
    {
        const bool bSelected = Index == static_cast<int32>(BankToSelection);
        BankToButtons[Index]->SetRenderScale(bSelected ? FVector2D(1.06f) : FVector2D(1.0f));
        BankToButtons[Index]->SetRenderOpacity(bSelected ? 1.0f : 0.52f);
    }
}

void UCatanHUDWidget::UpdatePlayerTradeLimits(const FCatanResourceView& Resources)
{
    const int32 Limits[] = {
        Resources.Wood, Resources.Clay, Resources.Hay, Resources.Sheep, Resources.Stone
    };
    for (int32 Index = 0; Index < OfferedInputs.Num() && Index < UE_ARRAY_COUNT(Limits); ++Index)
    {
        UComboBoxString* Input = OfferedInputs[Index];
        const int32 MaxValue = FMath::Max(0, Limits[Index]);
        const int32 PreviousValue = FMath::Clamp(
            FCString::Atoi(*Input->GetSelectedOption()), 0, MaxValue);
        if (Input->GetOptionCount() == MaxValue + 1
            && Input->GetOptionAtIndex(MaxValue) == FString::FromInt(MaxValue))
            continue;
        Input->ClearOptions();
        for (int32 Count = 0; Count <= MaxValue; ++Count)
            Input->AddOption(FString::FromInt(Count));
        Input->SetSelectedOption(FString::FromInt(PreviousValue));
    }
}

void UCatanHUDWidget::UpdateDropLimits(const FCatanResourceView& Resources, bool bReset)
{
    const int32 Limits[] = {
        Resources.Wood, Resources.Clay, Resources.Hay, Resources.Sheep, Resources.Stone
    };
    if (bReset) FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    for (int32 Index = 0; Index < DropInputs.Num() && Index < UE_ARRAY_COUNT(Limits); ++Index)
    {
        UComboBoxString* Input = DropInputs[Index];
        const int32 MaxValue = FMath::Max(0, Limits[Index]);
        const int32 PreviousValue = bReset ? 0 : FMath::Clamp(
            FCString::Atoi(*Input->GetSelectedOption()), 0, MaxValue);
        if (Input->GetOptionCount() != MaxValue + 1
            || Input->GetOptionAtIndex(MaxValue) != FString::FromInt(MaxValue))
        {
            Input->ClearOptions();
            for (int32 Count = 0; Count <= MaxValue; ++Count)
                Input->AddOption(FString::FromInt(Count));
        }
        Input->SetSelectedOption(FString::FromInt(PreviousValue));
    }
}

void UCatanHUDWidget::ResetTradeInputs()
{
    FSlateApplication::Get().ClearKeyboardFocus(EFocusCause::SetDirectly);
    for (UComboBoxString* Input : OfferedInputs) Input->SetSelectedOption(TEXT("0"));
    for (UComboBoxString* Input : RequestedInputs) Input->SetSelectedOption(TEXT("0"));
}

void UCatanHUDWidget::OfferTrade()
{
    if (OfferedInputs.Num() != 5 || RequestedInputs.Num() != 5) return;
    auto ReadResources = [](const TArray<TObjectPtr<UComboBoxString>>& Inputs)
    {
        FCatanResourceView Resources;
        Resources.Wood = FCString::Atoi(*Inputs[0]->GetSelectedOption());
        Resources.Clay = FCString::Atoi(*Inputs[1]->GetSelectedOption());
        Resources.Hay = FCString::Atoi(*Inputs[2]->GetSelectedOption());
        Resources.Sheep = FCString::Atoi(*Inputs[3]->GetSelectedOption());
        Resources.Stone = FCString::Atoi(*Inputs[4]->GetSelectedOption());
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
    TArray<FString> Names{UserPreferences.PlayerName};
    for (int32 Index = 1; Index < Count; ++Index)
        Names.Add(FString::Printf(TEXT("Player %d"), Index + 1));
    bSetupPanelOpen = false;
    GameSubsystem->StartLocalGame(Names);
}

void UCatanHUDWidget::UpdatePlayerCount(FString SelectedItem, ESelectInfo::Type SelectionType)
{
}

void UCatanHUDWidget::ConfirmExpensiveAction()
{
    if (GameSubsystem && GameSubsystem->HasPendingBuildTarget())
    {
        FString Error;
        GameSubsystem->ConfirmPendingBuildTarget(Error);
        return;
    }
    const int32 Action = PendingExpensiveAction;
    PendingExpensiveAction = 0;
    if (Action == 2)
    {
        FString Error;
        GameSubsystem->TryBuyDevelopmentCard(Error);
    }
    Refresh();
}

void UCatanHUDWidget::CancelExpensiveAction()
{
    if (GameSubsystem && GameSubsystem->HasPendingBuildTarget())
    {
        GameSubsystem->CancelPendingBuildTarget();
        return;
    }
    PendingExpensiveAction = 0;
    Refresh();
}

void UCatanHUDWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
