#include "CatanTouchComboBoxString.h"

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
    if (!IsOpen()) return Super::HandleGenerateWidget(Item);
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
