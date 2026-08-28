#if WITH_DEV_AUTOMATION_TESTS

#include "CatanEventTextFormatter.h"
#include "CatanTextResources.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanEventTextFormatterTest,
    "Catan.UX.LocalizedEventHistory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanEventTextFormatterTest::RunTest(const FString&)
{
    const ECatanLanguage Russian = ECatanLanguage::Russian;
    TestEqual(TEXT("fixed game event is translated"),
        FCatanEventTextFormatter::Format(TEXT("City built"), Russian),
        FString(TEXT("Замок построен")));
    TestEqual(TEXT("resource delta preserves player and count"),
        FCatanEventTextFormatter::Format(TEXT("Alice: +2 resource cards"), Russian),
        FString(TEXT("Alice: +2 карт ресурсов")));
    TestEqual(TEXT("trade event preserves recipient"),
        FCatanEventTextFormatter::Format(TEXT("Trade offered to Борис"), Russian),
        FString(TEXT("Обмен предложен игроку Борис")));
    TestEqual(TEXT("award event preserves player"),
        FCatanEventTextFormatter::Format(TEXT("★ Alice claimed Longest Road"), Russian),
        FString(TEXT("★ Alice получил награду «Самая длинная дорога»")));
    TestEqual(TEXT("dedicated lobby event preserves player"),
        FCatanEventTextFormatter::Format(TEXT("Alice left the lobby"), Russian),
        FString(TEXT("Alice покинул лобби")));
    TestEqual(TEXT("English history remains protocol-compatible"),
        FCatanEventTextFormatter::Format(TEXT("Road built"), ECatanLanguage::English),
        FString(TEXT("Road built")));
    TestEqual(TEXT("unknown history remains readable"),
        FCatanEventTextFormatter::Format(TEXT("Future event"), Russian),
        FString(TEXT("Future event")));
    TestEqual(TEXT("city action uses nominative menu label"),
        FCatanTextResources::Get(Russian, TEXT("CITY")), FString(TEXT("ЗАМОК")));
    TestEqual(TEXT("resource hint keeps the genitive form without colliding with the menu"),
        FCatanTextResources::Get(Russian, TEXT("missing city")), FString(TEXT("замка")));
    TestEqual(TEXT("resource badge keeps uppercase label"),
        FCatanTextResources::Get(Russian, TEXT("WOOD")), FString(TEXT("ДЕРЕВО")));
    TestEqual(TEXT("resource selector keeps title-case label"),
        FCatanTextResources::Get(Russian, TEXT("RESOURCE WOOD")), FString(TEXT("Дерево")));
    TestEqual(TEXT("player heading keeps uppercase label"),
        FCatanTextResources::Get(Russian, TEXT("PLAYER")), FString(TEXT("ИГРОК")));
    TestEqual(TEXT("player button keeps title-case label"),
        FCatanTextResources::Get(Russian, TEXT("PLAYER LABEL")), FString(TEXT("Игрок")));
    TestEqual(TEXT("monopoly title keeps uppercase label"),
        FCatanTextResources::Get(Russian, TEXT("MONOPOLY")), FString(TEXT("МОНОПОЛИЯ")));
    TestEqual(TEXT("monopoly count keeps title-case label"),
        FCatanTextResources::Get(Russian, TEXT("MONOPOLY CARD")), FString(TEXT("Монополия")));
    TestEqual(TEXT("semantic resource key never leaks into English UI"),
        FCatanTextResources::Get(ECatanLanguage::English, TEXT("RESOURCE WOOD")),
        FString(TEXT("Wood")));
    const TPair<const TCHAR*, const TCHAR*> EnglishResources[] = {
        {TEXT("RESOURCE WOOD"), TEXT("Wood")}, {TEXT("RESOURCE CLAY"), TEXT("Clay")},
        {TEXT("RESOURCE HAY"), TEXT("Hay")}, {TEXT("RESOURCE SHEEP"), TEXT("Sheep")},
        {TEXT("RESOURCE STONE"), TEXT("Stone")}};
    for (const TPair<const TCHAR*, const TCHAR*>& Resource : EnglishResources)
        TestEqual(TEXT("all semantic resource keys stay internal"),
            FCatanTextResources::Get(ECatanLanguage::English, Resource.Key),
            FString(Resource.Value));
    TestEqual(TEXT("semantic phase key never leaks into English UI"),
        FCatanTextResources::Get(ECatanLanguage::English, TEXT("PHASE ROLL DICE")),
        FString(TEXT("Roll dice")));
    TestEqual(TEXT("semantic missing-cost key never leaks into English UI"),
        FCatanTextResources::Get(ECatanLanguage::English, TEXT("missing city")),
        FString(TEXT("city")));
    return true;
}

#endif
