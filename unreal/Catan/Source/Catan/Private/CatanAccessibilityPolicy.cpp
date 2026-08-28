#include "CatanAccessibilityPolicy.h"

namespace
{
constexpr FLinearColor StandardResources[] = {
    FLinearColor(0.08f, 0.52f, 0.16f), FLinearColor(0.76f, 0.18f, 0.05f),
    FLinearColor(0.95f, 0.70f, 0.06f), FLinearColor(0.48f, 0.82f, 0.28f),
    FLinearColor(0.42f, 0.48f, 0.58f)};
constexpr FLinearColor HighContrastResources[] = {
    FLinearColor(0.02f, 0.32f, 0.78f), FLinearColor(0.95f, 0.28f, 0.02f),
    FLinearColor(1.00f, 0.82f, 0.02f), FLinearColor(0.58f, 0.18f, 0.82f),
    FLinearColor(0.22f, 0.25f, 0.30f)};
constexpr FLinearColor DeuteranopiaResources[] = {
    FLinearColor(0.10f, 0.40f, 0.78f), FLinearColor(0.90f, 0.36f, 0.05f),
    FLinearColor(0.96f, 0.76f, 0.12f), FLinearColor(0.52f, 0.28f, 0.74f),
    FLinearColor(0.40f, 0.46f, 0.54f)};
constexpr FLinearColor ProtanopiaResources[] = {
    FLinearColor(0.04f, 0.42f, 0.78f), FLinearColor(0.10f, 0.68f, 0.72f),
    FLinearColor(0.96f, 0.76f, 0.10f), FLinearColor(0.56f, 0.24f, 0.74f),
    FLinearColor(0.38f, 0.44f, 0.54f)};
constexpr FLinearColor TritanopiaResources[] = {
    FLinearColor(0.06f, 0.54f, 0.24f), FLinearColor(0.84f, 0.20f, 0.10f),
    FLinearColor(0.82f, 0.28f, 0.66f), FLinearColor(0.06f, 0.62f, 0.72f),
    FLinearColor(0.40f, 0.44f, 0.52f)};

const FLinearColor* ResourcePalette(ECatanColorVisionMode Mode)
{
    switch (Mode)
    {
    case ECatanColorVisionMode::HighContrast: return HighContrastResources;
    case ECatanColorVisionMode::Deuteranopia: return DeuteranopiaResources;
    case ECatanColorVisionMode::Protanopia: return ProtanopiaResources;
    case ECatanColorVisionMode::Tritanopia: return TritanopiaResources;
    default: return StandardResources;
    }
}
}

FLinearColor FCatanAccessibilityPolicy::ResourceColor(
    int32 ResourceIndex, ECatanColorVisionMode Mode)
{
    return ResourcePalette(Mode)[FMath::Clamp(ResourceIndex, 0, 4)];
}

FLinearColor FCatanAccessibilityPolicy::BoardResourceColor(
    int32 ResourceIndex, ECatanColorVisionMode Mode)
{
    FLinearColor Color = ResourceColor(ResourceIndex, Mode);
    Color.R *= 0.74f;
    Color.G *= 0.74f;
    Color.B *= 0.74f;
    Color.A = 1.0f;
    return Color;
}

FLinearColor FCatanAccessibilityPolicy::PlayerColor(
    int32 PlayerIndex, ECatanColorVisionMode Mode)
{
    constexpr FLinearColor Standard[] = {
        FLinearColor(0.85f, 0.08f, 0.05f), FLinearColor(0.05f, 0.35f, 0.90f),
        FLinearColor(0.95f, 0.72f, 0.04f), FLinearColor(0.10f, 0.70f, 0.25f)};
    constexpr FLinearColor Accessible[] = {
        FLinearColor(0.90f, 0.32f, 0.04f), FLinearColor(0.03f, 0.38f, 0.86f),
        FLinearColor(0.76f, 0.18f, 0.76f), FLinearColor(0.02f, 0.68f, 0.66f)};
    const FLinearColor* Palette = Mode == ECatanColorVisionMode::Standard
        ? Standard : Accessible;
    return Palette[FMath::Abs(PlayerIndex) % 4];
}
