#include "CatanMobileUIPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanMobileComboBoxMetricsTest,
    "Catan.UX.MobileComboBoxMetrics", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanMobileComboBoxMetricsTest::RunTest(const FString&)
{
    const FCatanComboBoxMetrics Mobile = CatanMobileUIPolicy::ComboBoxMetrics(20, true);
    const FCatanComboBoxMetrics Desktop = CatanMobileUIPolicy::ComboBoxMetrics(20, false);
    TestTrue(TEXT("mobile dropdown row exceeds the 64px touch minimum"), Mobile.MinimumRowHeight >= 64.0f);
    TestTrue(TEXT("mobile popup text is comfortably readable"), Mobile.PopupFontSize >= 28);
    TestTrue(TEXT("mobile popup has generous vertical padding"), Mobile.PopupPadding.Top >= 12.0f);
    TestEqual(TEXT("closed mobile field stays compact"), Mobile.ClosedFontSize, 20);
    TestTrue(TEXT("closed mobile padding is smaller than popup padding"),
        Mobile.ClosedContentPadding.Top < Mobile.PopupPadding.Top);
    TestTrue(TEXT("mobile list can show several large rows"),
        Mobile.MaximumListHeight >= Mobile.MinimumRowHeight * 5.0f);
    TestEqual(TEXT("desktop call sites keep their requested font size"), Desktop.ClosedFontSize, 20);
    TestTrue(TEXT("desktop rows remain smaller than mobile rows"),
        Desktop.MinimumRowHeight < Mobile.MinimumRowHeight);
    TestTrue(TEXT("an open popup uses large row layout"),
        CatanMobileUIPolicy::ShouldUsePopupRowLayout(true, false));
    TestFalse(TEXT("selected content stays compact even before popup closes"),
        CatanMobileUIPolicy::ShouldUsePopupRowLayout(true, true));
    TestFalse(TEXT("closed combo content never inherits popup row size"),
        CatanMobileUIPolicy::ShouldUsePopupRowLayout(false, false));
    TestTrue(TEXT("mobile buttons meet the large touch target"),
        CatanMobileUIPolicy::MinimumTouchTargetHeight(true) >= 72.0f);
    TestTrue(TEXT("desktop controls remain compact"),
        CatanMobileUIPolicy::MinimumTouchTargetHeight(false)
            < CatanMobileUIPolicy::MinimumTouchTargetHeight(true));
    return true;
}
#endif
