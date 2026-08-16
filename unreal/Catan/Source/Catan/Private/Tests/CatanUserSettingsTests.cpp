#if WITH_DEV_AUTOMATION_TESTS

#include "CatanTextResources.h"
#include "CatanUserSettings.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanTextResourcesTest,
    "Catan.Settings.TextResources", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanTextResourcesTest::RunTest(const FString&)
{
    TestEqual(TEXT("English implementation returns the source text"),
        FCatanTextResources::Get(ECatanLanguage::English, TEXT("SETTINGS")), FString(TEXT("SETTINGS")));
    TestEqual(TEXT("Russian implementation translates known UI text"),
        FCatanTextResources::Get(ECatanLanguage::Russian, TEXT("SETTINGS")), FString(TEXT("НАСТРОЙКИ")));
    TestEqual(TEXT("Russian implementation safely falls back for unknown keys"),
        FCatanTextResources::Get(ECatanLanguage::Russian, TEXT("Unregistered text")),
        FString(TEXT("Unregistered text")));
    TestEqual(TEXT("language parsing is stable"), FCatanTextResources::ParseLanguage(TEXT("ru")),
        ECatanLanguage::Russian);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanUserSettingsPersistenceTest,
    "Catan.Settings.Persistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCatanUserSettingsPersistenceTest::RunTest(const FString&)
{
    const FString Filename = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(),
        TEXT("CatanSettingsTest-"), TEXT(".ini"));
    IFileManager::Get().Delete(*Filename, false, true);

    FCatanUserPreferences Saved;
    Saved.PlayerName = TEXT("  Test\tPlayer\n ");
    Saved.Language = ECatanLanguage::Russian;
    FCatanUserSettings::Save(Saved, Filename);
    const FCatanUserPreferences Loaded = FCatanUserSettings::Load(Filename);

    TestEqual(TEXT("player name is normalized and persisted"), Loaded.PlayerName,
        FString(TEXT("Test Player")));
    TestEqual(TEXT("language is persisted"), Loaded.Language, ECatanLanguage::Russian);
    TestEqual(TEXT("empty names use the documented default"),
        FCatanUserSettings::NormalizePlayerName(TEXT(" \n ")), FString(TEXT("Player")));
    TestEqual(TEXT("names are capped at 24 characters"),
        FCatanUserSettings::NormalizePlayerName(TEXT("123456789012345678901234567890")).Len(), 24);

    IFileManager::Get().Delete(*Filename, false, true);
    return true;
}

#endif
