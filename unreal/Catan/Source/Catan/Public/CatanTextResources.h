#pragma once

#include "CoreMinimal.h"

enum class ECatanLanguage : uint8
{
    English,
    Russian
};

class ICatanTextResources
{
public:
    virtual ~ICatanTextResources() = default;
    virtual FString Get(const FString& Key) const = 0;
    virtual bool Has(const FString& Key) const = 0;
};

class CATAN_API FCatanTextResources final
{
public:
    static const ICatanTextResources& For(ECatanLanguage Language);
    static FString Get(ECatanLanguage Language, const FString& Key);
    static bool HasTranslation(ECatanLanguage Language, const FString& Key);
    static TArray<FString> MissingTranslations(ECatanLanguage Language,
        const TArray<FString>& Keys);
    static FString LanguageCode(ECatanLanguage Language);
    static ECatanLanguage ParseLanguage(const FString& Code);
};
