#pragma once

#include "CoreMinimal.h"

struct FCatanVisibleVictoryState
{
    int32 VictoryPoints = 0;
    int32 VictoryPointCards = 0;
};

namespace CatanVictoryVisibilityPolicy
{
    CATAN_API FCatanVisibleVictoryState Resolve(int32 PublicPoints, int32 TotalPoints,
        int32 VictoryPointCards, bool bOwner, bool bGameFinished);
}
