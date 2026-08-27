#pragma once

#include "CoreMinimal.h"

struct FCatanShadowSettings
{
    float DynamicDistance = 0.0f;
    float FarDistance = 0.0f;
    int32 Cascades = 0;
    int32 FarCascades = 0;
    int32 MaxResolution = 0;
};

namespace CatanLightingPolicy
{
    constexpr float MaximumCameraArm = 5200.0f;
    constexpr float MaximumCameraPan = 3000.0f;
    constexpr float BoardRadius = 2400.0f;

    CATAN_API float RequiredShadowDistance();
    CATAN_API FCatanShadowSettings Settings(bool bMobile);
}
