#include "CatanPlayerStatusPanelPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanPlayerStatusPanelPolicyTest,
    "Catan.UX.PlayerStatusPanel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanPlayerStatusPanelPolicyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    for (const bool bCompact : {false, true})
    {
        const FCatanPlayerStatusPanelMetrics Metrics = FCatanPlayerStatusPanelPolicy::Resolve(bCompact);
        TestTrue(TEXT("player list always supports touch scrolling"), Metrics.bScrollable);
        TestTrue(TEXT("four compact rows fit before scrolling is needed"),
            Metrics.ViewportHeight >= Metrics.EstimatedRowHeight * 4.0f);
        TestTrue(TEXT("status font stays readable"), Metrics.FontSize >= 14);
    }
    TestEqual(TEXT("short names stay intact"),
        FCatanPlayerStatusPanelPolicy::CompactName(TEXT("Player")), FString(TEXT("Player")));
    const FString Long = FCatanPlayerStatusPanelPolicy::CompactName(TEXT("Extremely Long Player Name"));
    TestTrue(TEXT("long name is bounded"), Long.Len() <= 18);
    TestTrue(TEXT("truncated name keeps an ellipsis"), Long.EndsWith(TEXT("…")));
    return true;
}
