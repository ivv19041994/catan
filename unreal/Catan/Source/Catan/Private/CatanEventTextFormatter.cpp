#include "CatanEventTextFormatter.h"

namespace
{
bool TranslateSuffix(const FString& Event, const TCHAR* Suffix, ECatanLanguage Language,
    FString& Out)
{
    const FString Source(Suffix);
    if (!Event.EndsWith(Source)) return false;
    Out = Event.LeftChop(Source.Len()) + FCatanTextResources::Get(Language, Source);
    return true;
}
}

FString FCatanEventTextFormatter::Format(const FString& Event, ECatanLanguage Language)
{
    if (Event.IsEmpty() || Language == ECatanLanguage::English) return Event;
    const FString Exact = FCatanTextResources::Get(Language, Event);
    if (Exact != Event) return Exact;

    const FString BotPrefix = TEXT("Single-player game started with ");
    if (Event.StartsWith(BotPrefix))
    {
        const FString CountAndSuffix = Event.Mid(BotPrefix.Len());
        const int32 Count = FCString::Atoi(*CountAndSuffix);
        return FCatanTextResources::Get(Language, TEXT("Single-player game started with"))
            + FString::Printf(TEXT(" %d "), Count)
            + FCatanTextResources::Get(Language,
                Count == 1 ? TEXT("event one bot") : TEXT("event multiple bots"));
    }

    const FString OfferPrefix = TEXT("Trade offered to ");
    if (Event.StartsWith(OfferPrefix))
        return FCatanTextResources::Get(Language, TEXT("Trade offered to")) + TEXT(" ")
            + Event.Mid(OfferPrefix.Len());

    FString Result;
    if (TranslateSuffix(Event, TEXT(" accepted the trade"), Language, Result)
        || TranslateSuffix(Event, TEXT(" declined the trade"), Language, Result)
        || TranslateSuffix(Event, TEXT(" claimed Largest Army"), Language, Result)
        || TranslateSuffix(Event, TEXT(" lost Largest Army"), Language, Result)
        || TranslateSuffix(Event, TEXT(" claimed Longest Road"), Language, Result)
        || TranslateSuffix(Event, TEXT(" lost Longest Road"), Language, Result)
        || TranslateSuffix(Event, TEXT(" left the lobby"), Language, Result))
        return Result;

    int32 Separator = INDEX_NONE;
    if (Event.FindChar(TEXT(':'), Separator))
    {
        const FString Delta = Event.Mid(Separator + 1).TrimStartAndEnd();
        if (Delta.EndsWith(TEXT(" resource card")) || Delta.EndsWith(TEXT(" resource cards")))
        {
            const int32 Space = Delta.Find(TEXT(" "));
            if (Space != INDEX_NONE)
                return Event.Left(Separator + 1) + TEXT(" ") + Delta.Left(Space) + TEXT(" ")
                    + FCatanTextResources::Get(Language, TEXT("event resource cards"));
        }
    }
    return Event;
}
