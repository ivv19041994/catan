#include "CatanHUDWidget.h"

#include "CatanGameSubsystem.h"
#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"

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
    case ECatanGamePhase::DropCards: return TEXT("Resource discard UI is the next interaction to implement");
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
        FMargin(0, -24, 900, 115));
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
    PassButton = AddAction(TEXT("END TURN"));

    RollButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::RollDice);
    SettlementButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectSettlement);
    RoadButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectRoad);
    CityButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::SelectCity);
    BuyCardButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::BuyDevelopmentCard);
    PassButton->OnClicked.AddDynamic(this, &UCatanHUDWidget::PassTurn);
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
    PassButton->SetIsEnabled(bPlay);
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

void UCatanHUDWidget::PassTurn()
{
    FString Error;
    GameSubsystem->TryPass(Error);
}
