#include "CatanResourceBankVisualPolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanResourceBankVisualPolicyTest,
    "Catan.Rendering.ResourceBankPiles",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanResourceBankVisualPolicyTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    const FCatanResourcePileVisual Empty = FCatanResourceBankVisualPolicy::Resolve(0);
    TestEqual(TEXT("empty pile reports zero"), Empty.Count, 0);
    TestFalse(TEXT("empty 3D deck and its number disappear"), Empty.bVisible);
    TestEqual(TEXT("empty pile has no geometry thickness"), Empty.Height, 0.0f);

    float PreviousHeight = 0.0f;
    for (int32 Count = 1; Count <= 19; ++Count)
    {
        const FCatanResourcePileVisual Pile = FCatanResourceBankVisualPolicy::Resolve(Count);
        TestTrue(FString::Printf(TEXT("pile %d is visible"), Count), Pile.bVisible);
        TestTrue(FString::Printf(TEXT("pile %d is thicker than predecessor"), Count),
            Pile.Height > PreviousHeight);
        PreviousHeight = Pile.Height;
    }
    TestEqual(TEXT("negative transport values clamp to empty"),
        FCatanResourceBankVisualPolicy::Resolve(-1).Count, 0);
    TestEqual(TEXT("values above physical supply clamp to nineteen"),
        FCatanResourceBankVisualPolicy::Resolve(20).Count, 19);
    return true;
}
