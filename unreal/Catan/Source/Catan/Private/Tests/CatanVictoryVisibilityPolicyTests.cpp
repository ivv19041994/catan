#include "CatanVictoryVisibilityPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanVictoryVisibilityPolicyTest,
    "Catan.Security.HiddenVictoryCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanVictoryVisibilityPolicyTest::RunTest(const FString&)
{
    const FCatanVisibleVictoryState Opponent =
        CatanVictoryVisibilityPolicy::Resolve(7, 9, 2, false, false);
    TestEqual(TEXT("opponent sees only public points before finish"), Opponent.VictoryPoints, 7);
    TestEqual(TEXT("opponent sees no victory-card count before finish"), Opponent.VictoryPointCards, 0);

    const FCatanVisibleVictoryState Owner =
        CatanVictoryVisibilityPolicy::Resolve(7, 9, 2, true, false);
    TestEqual(TEXT("owner sees total points"), Owner.VictoryPoints, 9);
    TestEqual(TEXT("owner sees owned victory cards"), Owner.VictoryPointCards, 2);

    const FCatanVisibleVictoryState FinalDashboard =
        CatanVictoryVisibilityPolicy::Resolve(7, 9, 2, false, true);
    TestEqual(TEXT("final dashboard reveals total points"), FinalDashboard.VictoryPoints, 9);
    TestEqual(TEXT("final dashboard reveals victory cards"), FinalDashboard.VictoryPointCards, 2);
    return true;
}
#endif
