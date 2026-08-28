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
    const TArray<FString> OnboardingKeys = {
        TEXT("WELCOME TO CATAN"), TEXT("Build, trade and race to 10 victory points."),
        TEXT("Set your name and language before you begin."), TEXT("TOUCH CONTROLS"),
        TEXT("Drag with one finger to move the camera."),
        TEXT("Pinch to zoom. Tap highlighted intersections, roads and hexes."),
        TEXT("Use the two side buttons to open help, players and build costs."),
        TEXT("YOUR TURN"), TEXT("Follow the phase hint in the top-left corner."),
        TEXT("Roll first, then build, trade or play a development card."),
        TEXT("End your turn when you are done. Your own resources are always visible."),
        TEXT("NEXT"), TEXT("START PLAYING"), TEXT("SKIP"),
        TEXT("STEP 1 OF 3"), TEXT("STEP 2 OF 3"), TEXT("STEP 3 OF 3")};
    TestEqual(TEXT("Russian onboarding has no fallback strings"),
        FCatanTextResources::MissingTranslations(ECatanLanguage::Russian, OnboardingKeys).Num(), 0);
    TestFalse(TEXT("missing Russian keys are detectable"),
        FCatanTextResources::HasTranslation(ECatanLanguage::Russian, TEXT("Missing key")));
    const TArray<FString> RuntimeKeys = {
        TEXT("Ready to host or join a game"), TEXT("Waiting for the dedicated server..."),
        TEXT("Enter server IP and optional port"), TEXT("Malformed create response"),
        TEXT("Malformed join response"), TEXT("Saved dedicated server address is invalid"),
        TEXT("Saved dedicated credentials are incomplete"), TEXT("Malformed resume response"),
        TEXT("No saved dedicated session"),
        TEXT("Your public name is not part of this saved game"),
        TEXT("LAN session service is unavailable"), TEXT("Creating LAN lobby..."),
        TEXT("Could not start LAN lobby"), TEXT("LAN search is already running..."),
        TEXT("Could not start LAN discovery socket"), TEXT("Searching the local network..."),
        TEXT("Select a discovered lobby first"), TEXT("Joining lobby..."),
        TEXT("Could not join the selected lobby"), TEXT("Could not resolve lobby address"),
        TEXT("Enter host IP address"), TEXT("Returned to main menu"),
        TEXT("Connection failed:"), TEXT("WINS!"), TEXT("FINAL SCORE"),
        TEXT("offers to"), TEXT("and requests:")};
    TestEqual(TEXT("Russian runtime and network UI has no fallback strings"),
        FCatanTextResources::MissingTranslations(ECatanLanguage::Russian, RuntimeKeys).Num(), 0);
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
    Saved.bOnboardingCompleted = true;
    FCatanUserSettings::Save(Saved, Filename);
    const FCatanUserPreferences Loaded = FCatanUserSettings::Load(Filename);

    TestEqual(TEXT("player name is normalized and persisted"), Loaded.PlayerName,
        FString(TEXT("Test Player")));
    TestEqual(TEXT("language is persisted"), Loaded.Language, ECatanLanguage::Russian);
    TestTrue(TEXT("completed onboarding is persisted"), Loaded.bOnboardingCompleted);
    TestEqual(TEXT("empty names use the documented default"),
        FCatanUserSettings::NormalizePlayerName(TEXT(" \n ")), FString(TEXT("Player")));
    TestEqual(TEXT("names are capped at 24 characters"),
        FCatanUserSettings::NormalizePlayerName(TEXT("123456789012345678901234567890")).Len(), 24);

    FCatanDedicatedSession Session;
    Session.Address = TEXT(" 192.168.1.20:17777\n");
    Session.LobbyToken = TEXT("ABCD-EFGH");
    Session.PlayerToken = TEXT("private-player-token-123456");
    Session.PlayerName = TEXT(" Test Player ");
    FCatanUserSettings::SaveDedicatedSession(Session, Filename);
    const FCatanDedicatedSession LoadedSession = FCatanUserSettings::LoadDedicatedSession(Filename);
    TestTrue(TEXT("dedicated reconnect credentials are persisted"), LoadedSession.IsValid());
    TestEqual(TEXT("dedicated address is normalized"), LoadedSession.Address,
        FString(TEXT("192.168.1.20:17777")));
    TestEqual(TEXT("private player token round-trips"), LoadedSession.PlayerToken,
        FString(TEXT("private-player-token-123456")));
    TestEqual(TEXT("saving dedicated credentials preserves user settings"),
        FCatanUserSettings::Load(Filename).Language, ECatanLanguage::Russian);
    FCatanUserSettings::ClearDedicatedSession(Filename);
    TestFalse(TEXT("cleared dedicated session cannot reconnect"),
        FCatanUserSettings::LoadDedicatedSession(Filename).IsValid());
    TestEqual(TEXT("clearing dedicated session preserves the player name"),
        FCatanUserSettings::Load(Filename).PlayerName, FString(TEXT("Test Player")));

    IFileManager::Get().Delete(*Filename, false, true);
    return true;
}

#endif
