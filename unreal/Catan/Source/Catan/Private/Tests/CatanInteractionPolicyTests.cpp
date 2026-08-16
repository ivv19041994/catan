#include "CatanInteractionPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanDiscardSelectionPolicyTest,
    "Catan.UX.DiscardSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanDiscardSelectionPolicyTest::RunTest(const FString&)
{
    FCatanResourceView Hand;
    Hand.Wood = 4; Hand.Clay = 2; Hand.Hay = 1; Hand.Sheep = 1; Hand.Stone = 2;
    FCatanResourceView Exact;
    Exact.Wood = 2; Exact.Clay = 1; Exact.Hay = 1; Exact.Sheep = 0; Exact.Stone = 1;
    TestTrue(TEXT("exactly half of a ten-card hand is accepted"),
        CatanInteractionPolicy::IsDiscardSelectionValid(Exact, Hand, 5));
    Exact.Stone = 0;
    TestFalse(TEXT("too few cards cannot be confirmed"),
        CatanInteractionPolicy::IsDiscardSelectionValid(Exact, Hand, 5));
    Exact.Stone = 2;
    TestFalse(TEXT("too many cards cannot be confirmed"),
        CatanInteractionPolicy::IsDiscardSelectionValid(Exact, Hand, 5));
    Exact.Wood = 5; Exact.Clay = Exact.Hay = Exact.Sheep = Exact.Stone = 0;
    TestFalse(TEXT("selection cannot reveal or exceed a resource count in hand"),
        CatanInteractionPolicy::IsDiscardSelectionValid(Exact, Hand, 5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanBuildConfirmationPolicyTest,
    "Catan.UX.BuildConfirmation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanBuildConfirmationPolicyTest::RunTest(const FString&)
{
    FCatanGameView View;
    View.ValidNodeTargets = {7};
    View.ValidRoadTargets = {12};
    View.Phase = ECatanGamePhase::SetupSettlement;
    TestTrue(TEXT("setup settlement target is selectable"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildSettlement, 7));
    TestFalse(TEXT("wrong setup node is rejected"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildSettlement, 8));
    TestFalse(TEXT("city cannot be selected during setup"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildCity, 7));

    View.Phase = ECatanGamePhase::SetupRoad;
    TestTrue(TEXT("setup road target is selectable"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildRoad, 12));
    View.Phase = ECatanGamePhase::CommonPlay;
    View.BoardAction = ECatanBoardAction::BuildCity;
    TestTrue(TEXT("city target follows the selected common-play action"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildCity, 7));
    TestFalse(TEXT("road target cannot inject a different common-play action"),
        CatanInteractionPolicy::CanSelectBuildTarget(View, ECatanBoardAction::BuildRoad, 12));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanDevelopmentParameterPolicyTest,
    "Catan.UX.DevelopmentParameters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanDevelopmentParameterPolicyTest::RunTest(const FString&)
{
    TestEqual(TEXT("one resource can take both Plenty choices when no others are selected"),
        CatanInteractionPolicy::YearOfPlentyMaxForResource(0), 2);
    TestEqual(TEXT("one other choice leaves one option"),
        CatanInteractionPolicy::YearOfPlentyMaxForResource(1), 1);
    TestEqual(TEXT("two other choices disable the resource"),
        CatanInteractionPolicy::YearOfPlentyMaxForResource(2), 0);
    FCatanResourceView Selected;
    Selected.Wood = 2;
    TestTrue(TEXT("two copies of one resource are valid"),
        CatanInteractionPolicy::IsYearOfPlentySelectionComplete(Selected));
    Selected.Wood = 1; Selected.Clay = 1;
    TestTrue(TEXT("two different resources are valid"),
        CatanInteractionPolicy::IsYearOfPlentySelectionComplete(Selected));
    Selected.Clay = 0;
    TestFalse(TEXT("one selected resource cannot be confirmed"),
        CatanInteractionPolicy::IsYearOfPlentySelectionComplete(Selected));
    return true;
}
#endif
