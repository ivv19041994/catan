#pragma once

#include "CoreMinimal.h"

struct FCatanResourcePileVisual
{
    int32 Count = 0;
    float Height = 0.0f;
    bool bVisible = false;
};

struct FCatanResourceBankVisualPolicy
{
    static FCatanResourcePileVisual Resolve(int32 RawCount)
    {
        FCatanResourcePileVisual Result;
        Result.Count = FMath::Clamp(RawCount, 0, 19);
        Result.bVisible = Result.Count > 0;
        Result.Height = Result.bVisible ? 6.0f + Result.Count * 5.5f : 0.0f;
        return Result;
    }
};
