#pragma once

#include "CoreMinimal.h"
#include "CatanTextResources.h"

struct FCatanUserPreferences
{
    FString PlayerName = TEXT("Player");
    ECatanLanguage Language = ECatanLanguage::English;
};

class CATAN_API FCatanUserSettings final
{
public:
    static FCatanUserPreferences Load(const FString& Filename = FString());
    static void Save(const FCatanUserPreferences& Preferences, const FString& Filename = FString());
    static FString NormalizePlayerName(const FString& Name);
};
