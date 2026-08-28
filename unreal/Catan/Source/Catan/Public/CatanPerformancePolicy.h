#pragma once

#include "CoreMinimal.h"

struct FCatanPerformanceSample
{
    float FrameMs = 0.0f;
    float GameThreadMs = 0.0f;
    float RenderThreadMs = 0.0f;
    float GpuMs = 0.0f;
};

struct FCatanPerformanceThresholds
{
    int32 MinimumSamples = 600;
    float MinimumAverageFps = 38.0f;
    float MaximumFrameP95Ms = 30.0f;
    float MaximumFrameP99Ms = 50.0f;
    float MaximumHitchPercent = 2.0f;
    float MaximumGameThreadP95Ms = 16.0f;
    float MaximumRenderThreadP95Ms = 16.0f;
    float MaximumGpuP95Ms = 24.0f;
    float HitchThresholdMs = 50.0f;
};

struct FCatanPerformanceSummary
{
    int32 Samples = 0;
    float AverageFps = 0.0f;
    float FrameP50Ms = 0.0f;
    float FrameP95Ms = 0.0f;
    float FrameP99Ms = 0.0f;
    float GameThreadP95Ms = 0.0f;
    float RenderThreadP95Ms = 0.0f;
    float GpuP95Ms = 0.0f;
    float HitchPercent = 0.0f;
    bool bGpuTimingAvailable = false;
    bool bPassed = false;
};

class CATAN_API FCatanPerformancePolicy
{
public:
    static constexpr int32 AndroidTargetFps = 45;
    static FCatanPerformanceThresholds AndroidBaseline();
    static FCatanPerformanceSummary Summarize(
        const TArray<FCatanPerformanceSample>& Samples,
        const FCatanPerformanceThresholds& Thresholds = AndroidBaseline());
};
