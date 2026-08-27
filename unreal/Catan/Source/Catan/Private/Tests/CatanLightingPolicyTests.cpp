#include "CatanLightingPolicy.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanLightingCoverageTest,
    "Catan.Rendering.ShadowCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanLightingCoverageTest::RunTest(const FString&)
{
    const float Required = CatanLightingPolicy::RequiredShadowDistance();
    const FCatanShadowSettings Mobile = CatanLightingPolicy::Settings(true);
    const FCatanShadowSettings Desktop = CatanLightingPolicy::Settings(false);
    TestTrue(TEXT("mobile CSM covers every allowed camera position"), Mobile.DynamicDistance >= Required);
    TestTrue(TEXT("desktop CSM covers every allowed camera position"), Desktop.DynamicDistance >= Required);
    TestTrue(TEXT("far cascade extends beyond the primary CSM range"),
        Mobile.FarDistance > Mobile.DynamicDistance && Desktop.FarDistance > Desktop.DynamicDistance);
    TestTrue(TEXT("mobile keeps a bounded cascade budget"), Mobile.Cascades >= 3 && Mobile.Cascades <= 4);
    TestTrue(TEXT("desktop shadow resolution is not below mobile"),
        Desktop.MaxResolution >= Mobile.MaxResolution);
    return true;
}
#endif
