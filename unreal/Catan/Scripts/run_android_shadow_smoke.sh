#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
apk="${1:-$project_dir/Builds/AndroidShadow/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
log_dir="$(mktemp -d /tmp/catan-android-shadow.XXXXXX)"
log_file="$log_dir/logcat.txt"
screenshot="$log_dir/max-zoom-shadows.png"

fail() { print -u2 "FAIL: $1\nArtifacts: $log_dir"; exit 1; }
[[ -f "$apk" ]] || fail "APK not found: $apk"
command -v adb >/dev/null || fail "adb is unavailable"

if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$log_dir/emulator.log" 2>&1 &
fi
adb wait-for-device
for _ in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
[[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "emulator did not boot"

adb install -r "$apk" >/dev/null || fail "APK installation failed"
adb logcat -c
adb shell am force-stop "$package_name"
adb shell am start -n "$activity" --es cmdline \
  "'-CatanAutoBots=1 -CatanUIPreview=Game -CatanShadowVisualSmoke'" >/dev/null \
  || fail "game launch failed"

for _ in {1..90}; do
  adb logcat -d >"$log_file"
  rg -q 'CATAN_SHADOWS board-casters=[1-9][0-9]* far-shadow=1' "$log_file" \
    && rg -q 'CATAN_SHADOWS visual-camera arm=5200' "$log_file" && break
  rg -q 'Assertion failed|Ensure condition failed|Fatal error|FATAL EXCEPTION|Fatal signal' "$log_file" \
    && fail "fatal runtime condition"
  sleep 1
done

rg -q 'CATAN_SHADOWS configured mobile=1 dynamic=16000 far=30000 cascades=3 resolution=1024' "$log_file" \
  || fail "mobile shadow profile was not applied"
rg -q 'CATAN_SHADOWS board-casters=[1-9][0-9]* far-shadow=1' "$log_file" \
  || fail "board shadow casters were not configured"
rg -q 'CATAN_SHADOWS visual-camera arm=5200' "$log_file" \
  || fail "maximum camera distance was not exercised"
adb exec-out screencap -p >"$screenshot"
[[ -s "$screenshot" ]] || fail "screenshot is empty"

print "PASS: Android mobile shadow profile covers the board at maximum zoom"
print "Screenshot: $screenshot"
print "Artifacts: $log_dir"
