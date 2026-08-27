#pragma once

#include "CoreMinimal.h"

struct FCatanPlayerStatusPanelMetrics
{
    int32 FontSize = 14;
    float ViewportHeight = 220.0f;
    float EstimatedRowHeight = 44.0f;
    bool bScrollable = true;
};

struct FCatanPlayerStatusPanelPolicy
{
    static FCatanPlayerStatusPanelMetrics Resolve(bool bCompact)
    {
        return bCompact
            ? FCatanPlayerStatusPanelMetrics{14, 230.0f, 44.0f, true}
            : FCatanPlayerStatusPanelMetrics{15, 240.0f, 46.0f, true};
    }

    static FString CompactName(const FString& Name, int32 MaximumCharacters = 18)
    {
        if (Name.Len() <= MaximumCharacters) return Name;
        return Name.Left(FMath::Max(1, MaximumCharacters - 1)) + TEXT("…");
    }
};
