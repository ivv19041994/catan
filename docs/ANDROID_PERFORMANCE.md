# Android performance baseline

The mobile performance gate runs an automated four-player game on
the real production board. It exercises setup, dice, building, bots, shadows,
animations, and HUD updates. Loading and board construction are excluded by a
five-second warm-up; the following 20 seconds are sampled frame by frame inside
Unreal.

The checked baseline targets the project's 45 FPS Android device profile:

- at least 600 valid samples and 38 average FPS;
- frame-time p95 at most 30 ms and p99 at most 50 ms;
- at most 2% of frames above 50 ms;
- game-thread and render-thread p95 at most 16 ms;
- GPU p95 at most 24 ms when the RHI supplies GPU timestamps.

GPU timing availability is reported explicitly. A device whose driver does not
provide it is still checked against frame, game-thread, and render-thread gates.

Build a Development APK, then run:

```bash
cd unreal/Catan
Scripts/build_android_development.sh
Scripts/run_android_performance_baseline.sh Builds/AndroidPerformance/Catan-arm64.apk
```

If no Android device is attached, the script starts `UE_pixel_6_API_35`. Set
`CATAN_ANDROID_AVD` to use another emulator. A physical device is preferred for
thermal evaluation. The artifact directory contains the exact metric line,
device identity, before/after thermal state, CPU and memory snapshots,
SurfaceFlinger frame stats, full app log, and a screenshot.

The emulator result is a reproducible regression gate, not a claim about phone
battery life. Before a release candidate, repeat the same command on at least
one physical target phone and retain its artifact directory.
