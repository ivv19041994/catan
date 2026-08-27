#include "CatanVictoryVisibilityPolicy.h"

FCatanVisibleVictoryState CatanVictoryVisibilityPolicy::Resolve(int32 PublicPoints,
    int32 TotalPoints, int32 VictoryPointCards, bool bOwner, bool bGameFinished)
{
    const bool bReveal = bOwner || bGameFinished;
    return {bReveal ? TotalPoints : PublicPoints, bReveal ? VictoryPointCards : 0};
}
