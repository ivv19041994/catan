#pragma once

#include "CoreMinimal.h"
#include "Components/ComboBoxString.h"

#include "CatanTouchComboBoxString.generated.h"

UCLASS()
class CATAN_API UCatanTouchComboBoxString final : public UComboBoxString
{
    GENERATED_BODY()

public:
    void ConfigurePopup(int32 InFontSize, float InMinimumRowHeight, const FMargin& InPadding);

protected:
    virtual TSharedRef<SWidget> HandleGenerateWidget(TSharedPtr<FString> Item) const override;
    virtual void HandleSelectionChanged(TSharedPtr<FString> Item,
        ESelectInfo::Type SelectionType) override;

private:
    int32 PopupFontSize = 22;
    float MinimumPopupRowHeight = 48.0f;
    FMargin PopupPadding = FMargin(16.0f, 9.0f);
    mutable bool bGeneratingSelectedContent = false;
};
