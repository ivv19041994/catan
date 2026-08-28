#pragma once

#include "CoreMinimal.h"
#include "CatanTextResources.h"

struct FCatanUserPreferences
{
    FString PlayerName = TEXT("Player");
    ECatanLanguage Language = ECatanLanguage::English;
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
    static FCatanDedicatedSession LoadDedicatedSession(const FString& Filename = FString());
    static void SaveDedicatedSession(const FCatanDedicatedSession& Session,
        const FString& Filename = FString());
    static void ClearDedicatedSession(const FString& Filename = FString());
};
