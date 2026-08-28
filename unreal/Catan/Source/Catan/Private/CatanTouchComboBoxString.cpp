#include "CatanTouchComboBoxString.h"

#include "CatanMobileUIPolicy.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

void UCatanTouchComboBoxString::ConfigurePopup(int32 InFontSize,
    float InMinimumRowHeight, const FMargin& InPadding)
{
    PopupFontSize = InFontSize;
    MinimumPopupRowHeight = InMinimumRowHeight;
    PopupPadding = InPadding;
}

TSharedRef<SWidget> UCatanTouchComboBoxString::HandleGenerateWidget(TSharedPtr<FString> Item) const
{
    if (!CatanMobileUIPolicy::ShouldUsePopupRowLayout(
        IsOpen(), bGeneratingSelectedContent))
        return Super::HandleGenerateWidget(Item);
    FSlateFontInfo PopupFont = GetFont();
    PopupFont.Size = PopupFontSize;
    return SNew(SBox)
        .MinDesiredHeight(MinimumPopupRowHeight)
        .Padding(PopupPadding)
        [
            SNew(STextBlock)
            .Text(FText::FromString(Item.IsValid() ? *Item : FString()))
            .Font(PopupFont)
            .ColorAndOpacity(FLinearColor::White)
        ];
}

void UCatanTouchComboBoxString::HandleSelectionChanged(TSharedPtr<FString> Item,
    ESelectInfo::Type SelectionType)
{
    TGuardValue<bool> GeneratingSelectedContent(bGeneratingSelectedContent, true);
    if (SelectionType != ESelectInfo::Direct)
    {
        const bool bPopupLayoutApplied = CatanMobileUIPolicy::ShouldUsePopupRowLayout(
            IsOpen(), bGeneratingSelectedContent);
        UE_LOG(LogTemp, Display,
            TEXT("CATAN_COMBO_SELECTION compact=1 popupRowApplied=%d selected=%s"),
            bPopupLayoutApplied, Item.IsValid() ? **Item : TEXT(""));
    }
    // Super broadcasts OnSelectionChanged after regenerating the compact selected
    // content. That callback may rebuild the whole modal (and destroy this widget),
    // so neither instrumentation nor member access is safe after this call.
    Super::HandleSelectionChanged(Item, SelectionType);
}
