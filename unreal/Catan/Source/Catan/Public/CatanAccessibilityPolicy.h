#pragma once

#include "CoreMinimal.h"
#include "CatanUserSettings.h"

class CATAN_API FCatanAccessibilityPolicy
{
public:
    static FLinearColor ResourceColor(int32 ResourceIndex, ECatanColorVisionMode Mode);
    static FLinearColor BoardResourceColor(int32 ResourceIndex, ECatanColorVisionMode Mode);
    static FLinearColor PlayerColor(int32 PlayerIndex, ECatanColorVisionMode Mode);
};
