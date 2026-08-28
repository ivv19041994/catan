#include "CatanPerformancePolicy.h"

namespace
{
float Percentile(TArray<float> Values, float Fraction)
{
    Values.RemoveAll([](float Value) { return !FMath::IsFinite(Value) || Value < 0.0f; });
    if (Values.IsEmpty()) return 0.0f;
    Values.Sort();
    const int32 Index = FMath::Clamp(
        FMath::CeilToInt(Fraction * static_cast<float>(Values.Num())) - 1,
        0, Values.Num() - 1);
    return Values[Index];
}
}

FCatanPerformanceThresholds FCatanPerformancePolicy::AndroidBaseline()
{
    return FCatanPerformanceThresholds{};
}

FCatanPerformanceSummary FCatanPerformancePolicy::Summarize(
    const TArray<FCatanPerformanceSample>& Samples,
    const FCatanPerformanceThresholds& Thresholds)
{
    FCatanPerformanceSummary Result;
    TArray<float> Frames;
    TArray<float> GameThread;
    TArray<float> RenderThread;
    TArray<float> Gpu;
    Frames.Reserve(Samples.Num());
    GameThread.Reserve(Samples.Num());
    RenderThread.Reserve(Samples.Num());
    Gpu.Reserve(Samples.Num());
    double TotalFrameMs = 0.0;
    int32 Hitches = 0;
    for (const FCatanPerformanceSample& Sample : Samples)
    {
        if (!FMath::IsFinite(Sample.FrameMs) || Sample.FrameMs <= 0.0f) continue;
        Frames.Add(Sample.FrameMs);
        TotalFrameMs += Sample.FrameMs;
        Hitches += Sample.FrameMs > Thresholds.HitchThresholdMs ? 1 : 0;
        if (FMath::IsFinite(Sample.GameThreadMs) && Sample.GameThreadMs >= 0.0f)
            GameThread.Add(Sample.GameThreadMs);
        if (FMath::IsFinite(Sample.RenderThreadMs) && Sample.RenderThreadMs >= 0.0f)
            RenderThread.Add(Sample.RenderThreadMs);
        if (FMath::IsFinite(Sample.GpuMs) && Sample.GpuMs > 0.0f)
            Gpu.Add(Sample.GpuMs);
    }

    Result.Samples = Frames.Num();
    if (Result.Samples == 0) return Result;
    Result.AverageFps = static_cast<float>(1000.0 * Result.Samples / TotalFrameMs);
    Result.FrameP50Ms = Percentile(Frames, 0.50f);
    Result.FrameP95Ms = Percentile(Frames, 0.95f);
    Result.FrameP99Ms = Percentile(Frames, 0.99f);
    Result.GameThreadP95Ms = Percentile(GameThread, 0.95f);
    Result.RenderThreadP95Ms = Percentile(RenderThread, 0.95f);
    Result.GpuP95Ms = Percentile(Gpu, 0.95f);
    Result.HitchPercent = 100.0f * static_cast<float>(Hitches) / Result.Samples;
    Result.bGpuTimingAvailable = !Gpu.IsEmpty();
    Result.bPassed = Result.Samples >= Thresholds.MinimumSamples
        && Result.AverageFps >= Thresholds.MinimumAverageFps
        && Result.FrameP95Ms <= Thresholds.MaximumFrameP95Ms
        && Result.FrameP99Ms <= Thresholds.MaximumFrameP99Ms
        && Result.HitchPercent <= Thresholds.MaximumHitchPercent
        && Result.GameThreadP95Ms <= Thresholds.MaximumGameThreadP95Ms
        && Result.RenderThreadP95Ms <= Thresholds.MaximumRenderThreadP95Ms
        && (!Result.bGpuTimingAvailable || Result.GpuP95Ms <= Thresholds.MaximumGpuP95Ms);
    return Result;
}
