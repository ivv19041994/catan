#pragma once

#include "CoreMinimal.h"
#include "CatanViewTypes.h"

namespace CatanTradePolicy
{
inline bool CanAccept(const FString& Responder, const FString& Recipient)
{
    return !Recipient.IsEmpty() && Responder == Recipient;
}

inline bool CanCancel(const FString& Responder, const FString& Offerer, const FString& Recipient)
{
    return !Responder.IsEmpty() && (Responder == Offerer || Responder == Recipient);
}

inline bool CanAfford(const FCatanResourceView& Have, const FCatanResourceView& Requested)
{
    return Have.Wood >= Requested.Wood && Have.Clay >= Requested.Clay
        && Have.Hay >= Requested.Hay && Have.Sheep >= Requested.Sheep
        && Have.Stone >= Requested.Stone;
}
}
