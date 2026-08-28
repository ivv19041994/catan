#include "CatanUserSettings.h"

#include "Misc/ConfigCacheIni.h"

namespace
{
constexpr const TCHAR* SettingsSection = TEXT("Catan.UserSettings");
constexpr const TCHAR* DedicatedSection = TEXT("Catan.DedicatedSession");

const FString& ResolveFilename(const FString& Filename)
{
    return Filename.IsEmpty() ? GGameUserSettingsIni : Filename;
}

FString NormalizeCredential(const FString& Value, int32 MaximumLength)
{
    FString Result = Value.TrimStartAndEnd().Left(MaximumLength);
    Result.ReplaceInline(TEXT("\t"), TEXT(""));
    Result.ReplaceInline(TEXT("\r"), TEXT(""));
    Result.ReplaceInline(TEXT("\n"), TEXT(""));
    return Result;
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
            Section->GetBool(TEXT("OnboardingCompleted"), Result.bOnboardingCompleted);
            if (Section->GetString(TEXT("EffectsVolume"), Value))
                Result.EffectsVolume = NormalizeVolume(FCString::Atof(*Value));
            if (Section->GetString(TEXT("MusicVolume"), Value))
                Result.MusicVolume = NormalizeVolume(FCString::Atof(*Value));
            Section->GetBool(TEXT("HapticsEnabled"), Result.bHapticsEnabled);
            if (Section->GetString(TEXT("ColorVisionMode"), Value))
                Result.ColorVisionMode = ParseColorVisionMode(Value);
        }
    }
    else
    {
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("PlayerName"), Value, ConfigFilename))
            Result.PlayerName = NormalizePlayerName(Value);
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("Language"), Value, ConfigFilename))
            Result.Language = FCatanTextResources::ParseLanguage(Value);
        if (GConfig) GConfig->GetBool(SettingsSection, TEXT("OnboardingCompleted"),
            Result.bOnboardingCompleted, ConfigFilename);
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("EffectsVolume"), Value, ConfigFilename))
            Result.EffectsVolume = NormalizeVolume(FCString::Atof(*Value));
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("MusicVolume"), Value, ConfigFilename))
            Result.MusicVolume = NormalizeVolume(FCString::Atof(*Value));
        if (GConfig) GConfig->GetBool(SettingsSection, TEXT("HapticsEnabled"),
            Result.bHapticsEnabled, ConfigFilename);
        if (GConfig && GConfig->GetString(SettingsSection, TEXT("ColorVisionMode"), Value, ConfigFilename))
            Result.ColorVisionMode = ParseColorVisionMode(Value);
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
        Section.Add(TEXT("OnboardingCompleted"), FConfigValue(
            Preferences.bOnboardingCompleted ? TEXT("True") : TEXT("False"),
            FConfigValue::EValueType::Set));
        Section.Add(TEXT("EffectsVolume"), FConfigValue(FString::SanitizeFloat(
            NormalizeVolume(Preferences.EffectsVolume)), FConfigValue::EValueType::Set));
        Section.Add(TEXT("MusicVolume"), FConfigValue(FString::SanitizeFloat(
            NormalizeVolume(Preferences.MusicVolume)), FConfigValue::EValueType::Set));
        Section.Add(TEXT("HapticsEnabled"), FConfigValue(
            Preferences.bHapticsEnabled ? TEXT("True") : TEXT("False"),
            FConfigValue::EValueType::Set));
        Section.Add(TEXT("ColorVisionMode"), FConfigValue(
            ColorVisionModeCode(Preferences.ColorVisionMode), FConfigValue::EValueType::Set));
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
    GConfig->SetBool(SettingsSection, TEXT("OnboardingCompleted"),
        Preferences.bOnboardingCompleted, ConfigFilename);
    GConfig->SetFloat(SettingsSection, TEXT("EffectsVolume"),
        NormalizeVolume(Preferences.EffectsVolume), ConfigFilename);
    GConfig->SetFloat(SettingsSection, TEXT("MusicVolume"),
        NormalizeVolume(Preferences.MusicVolume), ConfigFilename);
    GConfig->SetBool(SettingsSection, TEXT("HapticsEnabled"),
        Preferences.bHapticsEnabled, ConfigFilename);
    GConfig->SetString(SettingsSection, TEXT("ColorVisionMode"),
        *ColorVisionModeCode(Preferences.ColorVisionMode), ConfigFilename);
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

float FCatanUserSettings::NormalizeVolume(float Volume)
{
    return FMath::Clamp(FMath::IsFinite(Volume) ? Volume : 0.0f, 0.0f, 1.0f);
}

FString FCatanUserSettings::ColorVisionModeCode(ECatanColorVisionMode Mode)
{
    switch (Mode)
    {
    case ECatanColorVisionMode::HighContrast: return TEXT("high-contrast");
    case ECatanColorVisionMode::Deuteranopia: return TEXT("deuteranopia");
    case ECatanColorVisionMode::Protanopia: return TEXT("protanopia");
    case ECatanColorVisionMode::Tritanopia: return TEXT("tritanopia");
    default: return TEXT("standard");
    }
}

ECatanColorVisionMode FCatanUserSettings::ParseColorVisionMode(const FString& Code)
{
    if (Code.Equals(TEXT("high-contrast"), ESearchCase::IgnoreCase))
        return ECatanColorVisionMode::HighContrast;
    if (Code.Equals(TEXT("deuteranopia"), ESearchCase::IgnoreCase))
        return ECatanColorVisionMode::Deuteranopia;
    if (Code.Equals(TEXT("protanopia"), ESearchCase::IgnoreCase))
        return ECatanColorVisionMode::Protanopia;
    if (Code.Equals(TEXT("tritanopia"), ESearchCase::IgnoreCase))
        return ECatanColorVisionMode::Tritanopia;
    return ECatanColorVisionMode::Standard;
}

FCatanDedicatedSession FCatanUserSettings::LoadDedicatedSession(const FString& Filename)
{
    FCatanDedicatedSession Result;
    const FString& ConfigFilename = ResolveFilename(Filename);
    auto Read = [&Result](const FConfigSection& Section)
    {
        Section.GetString(TEXT("Address"), Result.Address);
        Section.GetString(TEXT("LobbyToken"), Result.LobbyToken);
        Section.GetString(TEXT("PlayerToken"), Result.PlayerToken);
        Section.GetString(TEXT("PlayerName"), Result.PlayerName);
    };
    if (!Filename.IsEmpty())
    {
        FConfigFile Config;
        Config.Read(ConfigFilename);
        if (const FConfigSection* Section = Config.FindSection(DedicatedSection)) Read(*Section);
    }
    else if (GConfig)
    {
        GConfig->GetString(DedicatedSection, TEXT("Address"), Result.Address, ConfigFilename);
        GConfig->GetString(DedicatedSection, TEXT("LobbyToken"), Result.LobbyToken, ConfigFilename);
        GConfig->GetString(DedicatedSection, TEXT("PlayerToken"), Result.PlayerToken, ConfigFilename);
        GConfig->GetString(DedicatedSection, TEXT("PlayerName"), Result.PlayerName, ConfigFilename);
    }
    Result.Address = NormalizeCredential(Result.Address, 255);
    Result.LobbyToken = NormalizeCredential(Result.LobbyToken, 128);
    Result.PlayerToken = NormalizeCredential(Result.PlayerToken, 128);
    Result.PlayerName = NormalizePlayerName(Result.PlayerName);
    if (!Result.IsValid()) Result = {};
    return Result;
}

void FCatanUserSettings::SaveDedicatedSession(const FCatanDedicatedSession& Session,
    const FString& Filename)
{
    const FCatanDedicatedSession Clean{
        NormalizeCredential(Session.Address, 255),
        NormalizeCredential(Session.LobbyToken, 128),
        NormalizeCredential(Session.PlayerToken, 128),
        NormalizePlayerName(Session.PlayerName)};
    if (!Clean.IsValid()) { ClearDedicatedSession(Filename); return; }
    const FString& ConfigFilename = ResolveFilename(Filename);
    if (!Filename.IsEmpty())
    {
        FConfigFile Config;
        Config.Read(ConfigFilename);
        FConfigSection Section;
        Section.Add(TEXT("Address"), FConfigValue(Clean.Address, FConfigValue::EValueType::Set));
        Section.Add(TEXT("LobbyToken"), FConfigValue(Clean.LobbyToken, FConfigValue::EValueType::Set));
        Section.Add(TEXT("PlayerToken"), FConfigValue(Clean.PlayerToken, FConfigValue::EValueType::Set));
        Section.Add(TEXT("PlayerName"), FConfigValue(Clean.PlayerName, FConfigValue::EValueType::Set));
        Config.Remove(DedicatedSection);
        Config.Add(DedicatedSection, MoveTemp(Section));
        Config.Dirty = true;
        Config.bCanSaveAllSections = true;
        Config.Write(ConfigFilename, false);
        return;
    }
    if (!GConfig) return;
    GConfig->SetString(DedicatedSection, TEXT("Address"), *Clean.Address, ConfigFilename);
    GConfig->SetString(DedicatedSection, TEXT("LobbyToken"), *Clean.LobbyToken, ConfigFilename);
    GConfig->SetString(DedicatedSection, TEXT("PlayerToken"), *Clean.PlayerToken, ConfigFilename);
    GConfig->SetString(DedicatedSection, TEXT("PlayerName"), *Clean.PlayerName, ConfigFilename);
    GConfig->Flush(false, ConfigFilename);
}

void FCatanUserSettings::ClearDedicatedSession(const FString& Filename)
{
    const FString& ConfigFilename = ResolveFilename(Filename);
    if (!Filename.IsEmpty())
    {
        FConfigFile Config;
        Config.Read(ConfigFilename);
        Config.Remove(DedicatedSection);
        Config.Dirty = true;
        Config.bCanSaveAllSections = true;
        Config.Write(ConfigFilename, false);
        return;
    }
    if (!GConfig) return;
    GConfig->EmptySection(DedicatedSection, ConfigFilename);
    GConfig->Flush(false, ConfigFilename);
}
