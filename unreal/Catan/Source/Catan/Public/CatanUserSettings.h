#pragma once

#include "CoreMinimal.h"
#include "CatanTextResources.h"

enum class ECatanColorVisionMode : uint8
{
    Standard,
    HighContrast,
    Deuteranopia,
    Protanopia,
    Tritanopia
};

struct FCatanUserPreferences
{
    FString PlayerName = TEXT("Player");
    ECatanLanguage Language = ECatanLanguage::English;
    bool bOnboardingCompleted = false;
    float EffectsVolume = 0.75f;
    float MusicVolume = 0.30f;
    bool bHapticsEnabled = true;
    ECatanColorVisionMode ColorVisionMode = ECatanColorVisionMode::Standard;
};

struct FCatanDedicatedSession
{
    FString Address;
    FString LobbyToken;
    FString PlayerToken;
    FString PlayerName;

    bool IsValid() const
    {
        return !Address.IsEmpty() && !LobbyToken.IsEmpty() && !PlayerToken.IsEmpty();
    }
};

class CATAN_API FCatanUserSettings final
{
public:
    static FCatanUserPreferences Load(const FString& Filename = FString());
    static void Save(const FCatanUserPreferences& Preferences, const FString& Filename = FString());
    static FString NormalizePlayerName(const FString& Name);
    static float NormalizeVolume(float Volume);
    static FString ColorVisionModeCode(ECatanColorVisionMode Mode);
    static ECatanColorVisionMode ParseColorVisionMode(const FString& Code);
    static FCatanDedicatedSession LoadDedicatedSession(const FString& Filename = FString());
    static void SaveDedicatedSession(const FCatanDedicatedSession& Session,
        const FString& Filename = FString());
    static void ClearDedicatedSession(const FString& Filename = FString());
};
