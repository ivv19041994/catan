#pragma once

#include "CoreMinimal.h"

struct FCatanComboBoxMetrics
{
    int32 ClosedFontSize = 0;
    int32 PopupFontSize = 0;
    float MinimumRowHeight = 0.0f;
    FMargin ClosedContentPadding;
    FMargin PopupPadding;
    float MaximumListHeight = 0.0f;
};

namespace CatanMobileUIPolicy
{
    CATAN_API FCatanComboBoxMetrics ComboBoxMetrics(int32 RequestedFontSize, bool bMobile);
}
