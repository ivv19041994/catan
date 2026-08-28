#if WITH_DEV_AUTOMATION_TESTS

#include "CatanAccessibilityPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanAccessibilityPaletteTest,
    "Catan.UX.AccessibilityPalette",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanAccessibilityPaletteTest::RunTest(const FString&)
{
    for (int32 Mode = 0; Mode <= 4; ++Mode)
    {
        TSet<FColor> UniqueResources;
        TSet<FColor> UniquePlayers;
        for (int32 Resource = 0; Resource < 5; ++Resource)
            UniqueResources.Add(FCatanAccessibilityPolicy::ResourceColor(Resource,
                static_cast<ECatanColorVisionMode>(Mode)).ToFColor(false));
        for (int32 Player = 0; Player < 4; ++Player)
            UniquePlayers.Add(FCatanAccessibilityPolicy::PlayerColor(Player,
                static_cast<ECatanColorVisionMode>(Mode)).ToFColor(false));
        TestEqual(TEXT("every resource remains visually distinct"), UniqueResources.Num(), 5);
        TestEqual(TEXT("every player remains visually distinct"), UniquePlayers.Num(), 4);
    }
    const FLinearColor BoardWood = FCatanAccessibilityPolicy::BoardResourceColor(
        0, ECatanColorVisionMode::HighContrast);
    const FLinearColor HudWood = FCatanAccessibilityPolicy::ResourceColor(
        0, ECatanColorVisionMode::HighContrast);
    TestTrue(TEXT("board palette keeps the same hue at a darker surface value"),
        BoardWood.B < HudWood.B && BoardWood.B > 0.0f);
    return true;
}

#endif
