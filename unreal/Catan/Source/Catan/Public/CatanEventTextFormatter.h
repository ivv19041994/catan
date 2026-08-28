#pragma once

#include "CoreMinimal.h"
#include "CatanTextResources.h"

class CATAN_API FCatanEventTextFormatter final
{
public:
    static FString Format(const FString& Event, ECatanLanguage Language);
};
