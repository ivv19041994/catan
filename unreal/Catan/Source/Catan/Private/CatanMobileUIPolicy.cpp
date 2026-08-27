#include "CatanMobileUIPolicy.h"

FCatanComboBoxMetrics CatanMobileUIPolicy::ComboBoxMetrics(int32 RequestedFontSize, bool bMobile)
{
    FCatanComboBoxMetrics Result;
    Result.ClosedFontSize = RequestedFontSize;
    Result.PopupFontSize = bMobile ? FMath::Max(RequestedFontSize, 30) : RequestedFontSize;
    Result.MinimumRowHeight = bMobile ? 82.0f : 48.0f;
    Result.ClosedContentPadding = bMobile ? FMargin(16.0f, 6.0f) : FMargin(16.0f, 9.0f);
    Result.PopupPadding = bMobile ? FMargin(26.0f, 15.0f) : FMargin(16.0f, 9.0f);
    Result.MaximumListHeight = bMobile ? 620.0f : 420.0f;
    return Result;
}
