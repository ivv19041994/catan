#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
default_apk="$project_dir/Builds/AndroidPerformance/Catan-arm64.apk"
[[ -f "$default_apk" ]] || default_apk="$project_dir/Builds/Android/Catan-arm64.apk"
[[ -f "$default_apk" ]] || default_apk="$project_dir/Builds/AndroidLatest/Catan-arm64.apk"
apk="${1:-$default_apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
warmup="${CATAN_PERF_WARMUP_SECONDS:-5}"
duration="${CATAN_PERF_DURATION_SECONDS:-20}"
artifact_dir="${CATAN_PERF_ARTIFACT_DIR:-$(mktemp -d /tmp/catan-android-perf.XXXXXX)}"
log_file="$artifact_dir/logcat.txt"

mkdir -p "$artifact_dir"
fail() {
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $artifact_dir"
  exit 1
}

[[ -f "$apk" ]] || fail "APK not found: $apk"
command -v adb >/dev/null 2>&1 || fail "adb is unavailable"

if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  print "Starting $avd with host Vulkan renderer..."
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$artifact_dir/emulator.log" 2>&1 &
fi

adb wait-for-device
for _ in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
[[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "Android device did not finish booting"

adb install -r "$apk" >/dev/null || fail "APK installation failed"
adb wait-for-device
sleep 2
{
  print "serial=$(adb get-serialno | tr -d '\r')"
  print "model=$(adb shell getprop ro.product.model | tr -d '\r')"
  print "manufacturer=$(adb shell getprop ro.product.manufacturer | tr -d '\r')"
  print "android=$(adb shell getprop ro.build.version.release | tr -d '\r')"
  print "api=$(adb shell getprop ro.build.version.sdk | tr -d '\r')"
  print "renderer=$(adb shell dumpsys SurfaceFlinger 2>/dev/null | rg -m1 'GLES:' || print unknown)"
  print "apk=$apk"
  print "warmupSeconds=$warmup"
  print "durationSeconds=$duration"
} >"$artifact_dir/device.txt"
adb shell dumpsys thermalservice >"$artifact_dir/thermal-before.txt" 2>/dev/null || true
adb shell dumpsys gfxinfo "$package_name" reset >/dev/null 2>&1 || true
adb logcat -c || fail "could not clear Android log buffer"
adb shell am force-stop "$package_name" || fail "could not stop the previous app process"
adb shell am start -n "$activity" --es cmdline \
  "'-CatanAutoBots=3 -CatanUIPreview=Game -CatanPerformanceBaseline -CatanPerfWarmup=$warmup -CatanPerfDuration=$duration'" \
  >/dev/null || fail "performance scenario did not launch"

fatal_pattern='No Vulkan driver found|Unable to run on this device|Assertion failed|Ensure condition failed|Fatal error|FATAL EXCEPTION|Fatal signal'
result=""
for _ in {1..180}; do
  app_pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
  if [[ -n "$app_pid" ]]; then adb logcat -d --pid="$app_pid" >"$log_file"; fi
  if rg -q "$fatal_pattern" "$log_file" 2>/dev/null; then
    rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
    fail "fatal runtime condition during performance capture"
  fi
  result="$(rg 'CATAN_PERF_RESULT' "$log_file" 2>/dev/null | tail -1 || true)"
  [[ -n "$result" ]] && break
  sleep 1
done
[[ -n "$result" ]] || fail "CATAN_PERF_RESULT was not produced"
rg -q 'CATAN_MOBILE_PACING requested=45 actual=45 maxFps=45' "$log_file" \
  || fail "Android runtime did not apply the 45 FPS thermal cap"
rg -q 'CATAN_PERF_SCENARIO autoplay=1 players=4 delay=0.48' "$log_file" \
  || fail "four-player gameplay did not drive the performance scenario"

print -r -- "$result" >"$artifact_dir/result.txt"
rg 'CATAN_PERF_BEGIN|CATAN_PERF_RESULT|CATAN_PERF_LIMITS' "$log_file" \
  >"$artifact_dir/metrics.txt" || true
adb shell dumpsys gfxinfo "$package_name" framestats >"$artifact_dir/gfxinfo.txt" 2>/dev/null || true
adb shell dumpsys meminfo "$package_name" >"$artifact_dir/meminfo.txt" 2>/dev/null || true
adb shell dumpsys thermalservice >"$artifact_dir/thermal-after.txt" 2>/dev/null || true
app_pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
if [[ -n "$app_pid" ]]; then
  adb shell top -b -n 1 -p "$app_pid" >"$artifact_dir/cpu.txt" 2>/dev/null || true
fi
adb exec-out screencap -p >"$artifact_dir/performance-board.png"
[[ -s "$artifact_dir/performance-board.png" ]] || fail "performance screenshot is empty"

print -r -- "$result"
print "Device: $(tr '\n' ' ' <"$artifact_dir/device.txt")"
[[ "$result" == *"pass=1"* ]] || fail "Android performance thresholds were exceeded"
print "PASS: reproducible Android board baseline met all frame/thread/GPU thresholds"
print "Artifacts: $artifact_dir"
