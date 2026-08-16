#pragma once

#include "CatanViewTypes.h"

namespace CatanInteractionPolicy
{
inline int32 ResourceTotal(const FCatanResourceView& Resources)
{
    return Resources.Wood + Resources.Clay + Resources.Hay
        + Resources.Sheep + Resources.Stone;
}

inline bool IsDiscardSelectionValid(const FCatanResourceView& Selected,
    const FCatanResourceView& Hand, int32 RequiredCount)
{
    return RequiredCount > 0 && ResourceTotal(Selected) == RequiredCount
        && Selected.Wood >= 0 && Selected.Wood <= Hand.Wood
        && Selected.Clay >= 0 && Selected.Clay <= Hand.Clay
        && Selected.Hay >= 0 && Selected.Hay <= Hand.Hay
        && Selected.Sheep >= 0 && Selected.Sheep <= Hand.Sheep
        && Selected.Stone >= 0 && Selected.Stone <= Hand.Stone;
}

inline int32 YearOfPlentyMaxForResource(int32 OtherSelected)
{
    return FMath::Clamp(2 - OtherSelected, 0, 2);
}

inline bool IsYearOfPlentySelectionComplete(const FCatanResourceView& Selected)
{
    return ResourceTotal(Selected) == 2
        && Selected.Wood >= 0 && Selected.Wood <= 2
        && Selected.Clay >= 0 && Selected.Clay <= 2
        && Selected.Hay >= 0 && Selected.Hay <= 2
        && Selected.Sheep >= 0 && Selected.Sheep <= 2
        && Selected.Stone >= 0 && Selected.Stone <= 2;
}

inline bool CanSelectBuildTarget(const FCatanGameView& View, ECatanBoardAction Action,
    int32 TargetId)
{
    const bool bActionMatchesPhase =
        (Action == ECatanBoardAction::BuildSettlement
            && (View.Phase == ECatanGamePhase::SetupSettlement
                || (View.Phase == ECatanGamePhase::CommonPlay
                    && View.BoardAction == ECatanBoardAction::BuildSettlement)))
        || (Action == ECatanBoardAction::BuildRoad
            && (View.Phase == ECatanGamePhase::SetupRoad
                || View.Phase == ECatanGamePhase::RoadBuilding
                || (View.Phase == ECatanGamePhase::CommonPlay
                    && View.BoardAction == ECatanBoardAction::BuildRoad)))
        || (Action == ECatanBoardAction::BuildCity
            && View.Phase == ECatanGamePhase::CommonPlay
            && View.BoardAction == ECatanBoardAction::BuildCity);
    return bActionMatchesPhase && (Action == ECatanBoardAction::BuildRoad
        ? View.ValidRoadTargets.Contains(TargetId)
        : View.ValidNodeTargets.Contains(TargetId));
}
}
