#include "CatanLightingPolicy.h"

float CatanLightingPolicy::RequiredShadowDistance()
{
    // Camera distance, maximum board-relative pan and the board itself all
    // contribute to the furthest visible caster. Keep transition headroom.
    return (MaximumCameraArm + MaximumCameraPan + BoardRadius) * 1.35f;
}

FCatanShadowSettings CatanLightingPolicy::Settings(bool bMobile)
{
    const float Required = RequiredShadowDistance();
    FCatanShadowSettings Result;
    Result.DynamicDistance = FMath::Max(Required, bMobile ? 16000.0f : 24000.0f);
    Result.FarDistance = FMath::Max(Result.DynamicDistance * 1.5f, 30000.0f);
    Result.Cascades = bMobile ? 3 : 4;
    Result.FarCascades = 1;
    Result.MaxResolution = bMobile ? 1024 : 2048;
    return Result;
}
