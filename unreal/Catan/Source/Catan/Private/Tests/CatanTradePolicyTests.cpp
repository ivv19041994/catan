#if WITH_DEV_AUTOMATION_TESTS

#include "CatanTradePolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanTradeResponseAuthorizationTest,
    "Catan.Trade.Authorization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanTradeResponseAuthorizationTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("selected recipient can accept"), CatanTradePolicy::CanAccept(TEXT("Buyer"), TEXT("Buyer")));
    TestFalse(TEXT("offerer cannot accept own offer"), CatanTradePolicy::CanAccept(TEXT("Seller"), TEXT("Buyer")));
    TestFalse(TEXT("unrelated player cannot accept"), CatanTradePolicy::CanAccept(TEXT("Third"), TEXT("Buyer")));
    TestFalse(TEXT("untargeted offer cannot be accepted"), CatanTradePolicy::CanAccept(TEXT("Buyer"), FString()));

    TestTrue(TEXT("offerer can withdraw"), CatanTradePolicy::CanCancel(TEXT("Seller"), TEXT("Seller"), TEXT("Buyer")));
    TestTrue(TEXT("recipient can decline"), CatanTradePolicy::CanCancel(TEXT("Buyer"), TEXT("Seller"), TEXT("Buyer")));
    TestFalse(TEXT("unrelated player cannot cancel"), CatanTradePolicy::CanCancel(TEXT("Third"), TEXT("Seller"), TEXT("Buyer")));

    FCatanResourceView Hand; Hand.Wood = 2; Hand.Clay = 1;
    FCatanResourceView Affordable; Affordable.Wood = 2;
    FCatanResourceView Unaffordable; Unaffordable.Stone = 1;
    TestTrue(TEXT("recipient can accept from private hand"), CatanTradePolicy::CanAfford(Hand, Affordable));
    TestFalse(TEXT("recipient action is disabled for an unaffordable request"),
        CatanTradePolicy::CanAfford(Hand, Unaffordable));
    return true;
}

#endif
