#include "CatanUserSettings.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr const TCHAR* SettingsSection = TEXT("Catan.UserSettings");

const FString& ResolveFilename(const FString& Filename)
{
    return Filename.IsEmpty() ? GGameUserSettingsIni : Filename;
}
}

FCatanUserPreferences FCatanUserSettings::Load(const FString& Filename)
{
    FCatanUserPreferences Result;
    const FString& ConfigFilename = ResolveFilename(Filename);
    FString Value;
    if (!Filename.IsEmpty())
    {
        FConfigFile Config;
        Config.Read(ConfigFilename);
        if (const FConfigSection* Section = Config.FindSection(SettingsSection))
        {
            if (Section->GetString(TEXT("PlayerName"), Value))
                Result.PlayerName = NormalizePlayerName(Value);
            if (Section->GetString(TEXT("Language"), Value))
                Result.Language = FCatanTextResources::ParseLanguage(Value);
        }
    }
    else
    {
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("PlayerName"), Value, ConfigFilename))
            Result.PlayerName = NormalizePlayerName(Value);
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("Language"), Value, ConfigFilename))
            Result.Language = FCatanTextResources::ParseLanguage(Value);
    }
    return Result;
}

void FCatanUserSettings::Save(const FCatanUserPreferences& Preferences, const FString& Filename)
{
    if (!GConfig) return;
    const FString& ConfigFilename = ResolveFilename(Filename);
    if (!Filename.IsEmpty())
    {
        FConfigFile Config;
        Config.Read(ConfigFilename);
        FConfigSection Section;
        Section.Add(TEXT("PlayerName"), FConfigValue(NormalizePlayerName(Preferences.PlayerName),
            FConfigValue::EValueType::Set));
        Section.Add(TEXT("Language"), FConfigValue(FCatanTextResources::LanguageCode(Preferences.Language),
            FConfigValue::EValueType::Set));
        Config.Remove(SettingsSection);
        Config.Add(SettingsSection, MoveTemp(Section));
        Config.Dirty = true;
        Config.bCanSaveAllSections = true;
        Config.Write(ConfigFilename, false);
        return;
    }
    GConfig->SetString(SettingsSection, TEXT("PlayerName"),
        *NormalizePlayerName(Preferences.PlayerName), ConfigFilename);
    GConfig->SetString(SettingsSection, TEXT("Language"),
        *FCatanTextResources::LanguageCode(Preferences.Language), ConfigFilename);
    GConfig->Flush(false, ConfigFilename);
}

FString FCatanUserSettings::NormalizePlayerName(const FString& Name)
{
    FString Result = Name.TrimStartAndEnd().Left(24);
    Result.ReplaceInline(TEXT("\t"), TEXT(" "));
    Result.ReplaceInline(TEXT("\r"), TEXT(" "));
    Result.ReplaceInline(TEXT("\n"), TEXT(" "));
    while (Result.Contains(TEXT("  "))) Result.ReplaceInline(TEXT("  "), TEXT(" "));
    return Result.IsEmpty() ? TEXT("Player") : Result;
}
