#if WITH_DEV_AUTOMATION_TESTS

#include "CatanPerformancePolicy.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanPerformanceHealthyBaselineTest,
    "Catan.Performance.Android.HealthyBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanPerformanceHealthyBaselineTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("mobile target protects the thermal budget"),
        FCatanPerformancePolicy::AndroidTargetFps, 45);
    TArray<FCatanPerformanceSample> Samples;
    for (int32 Index = 0; Index < 100; ++Index)
    {
        FCatanPerformanceSample& Sample = Samples.Emplace_GetRef();
        Sample.FrameMs = Index == 99 ? 55.0f : 22.2f;
        Sample.GameThreadMs = 7.0f;
        Sample.RenderThreadMs = 8.0f;
        Sample.GpuMs = 12.0f;
    }
    FCatanPerformanceThresholds Thresholds = FCatanPerformancePolicy::AndroidBaseline();
    Thresholds.MinimumSamples = 100;
    const FCatanPerformanceSummary Summary = FCatanPerformancePolicy::Summarize(Samples, Thresholds);
    TestEqual(TEXT("all samples retained"), Summary.Samples, 100);
    TestTrue(TEXT("45 fps workload stays above the floor"), Summary.AverageFps > 44.0f);
    TestTrue(TEXT("nearest-rank p95 is stable"), FMath::IsNearlyEqual(Summary.FrameP95Ms, 22.2f));
    TestTrue(TEXT("one hitch is one percent"), FMath::IsNearlyEqual(Summary.HitchPercent, 1.0f));
    TestTrue(TEXT("GPU timing is reported"), Summary.bGpuTimingAvailable);
    TestTrue(TEXT("healthy workload passes"), Summary.bPassed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanPerformanceRegressionTest,
    "Catan.Performance.Android.RegressionRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanPerformanceRegressionTest::RunTest(const FString& Parameters)
{
    TArray<FCatanPerformanceSample> Samples;
    for (int32 Index = 0; Index < 20; ++Index)
    {
        FCatanPerformanceSample& Sample = Samples.Emplace_GetRef();
        Sample.FrameMs = 40.0f;
        Sample.GameThreadMs = 18.0f;
        Sample.RenderThreadMs = 19.0f;
        Sample.GpuMs = 30.0f;
    }
    FCatanPerformanceThresholds Thresholds = FCatanPerformancePolicy::AndroidBaseline();
    Thresholds.MinimumSamples = 20;
    const FCatanPerformanceSummary Summary = FCatanPerformancePolicy::Summarize(Samples, Thresholds);
    TestTrue(TEXT("slow workload is measured below 30 fps"), Summary.AverageFps < 30.0f);
    TestFalse(TEXT("slow workload is rejected"), Summary.bPassed);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCatanPerformanceMissingGpuTimingTest,
    "Catan.Performance.Android.MissingGpuTimingIsOptional",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCatanPerformanceMissingGpuTimingTest::RunTest(const FString& Parameters)
{
    TArray<FCatanPerformanceSample> Samples;
    for (int32 Index = 0; Index < 10; ++Index)
        Samples.Add({22.0f, 4.0f, 5.0f, 0.0f});
    FCatanPerformanceThresholds Thresholds = FCatanPerformancePolicy::AndroidBaseline();
    Thresholds.MinimumSamples = 10;
    const FCatanPerformanceSummary Summary = FCatanPerformancePolicy::Summarize(Samples, Thresholds);
    TestFalse(TEXT("unsupported GPU timer is explicit"), Summary.bGpuTimingAvailable);
    TestTrue(TEXT("unsupported GPU timer does not fail an otherwise healthy device"), Summary.bPassed);
    return true;
}

#endif
