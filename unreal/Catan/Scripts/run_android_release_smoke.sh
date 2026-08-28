#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_root="$(cd "$project_dir/../.." && pwd)"
version="$(tr -d '[:space:]' < "$repo_root/VERSION")"
apk="${1:-$repo_root/dist/release/catan-$version-android-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
artifacts="$(mktemp -d "${TMPDIR:-/tmp}/catan-android-release-smoke.XXXXXX")"

fail() { echo "FAIL: $1" >&2; echo "Artifacts: $artifacts" >&2; exit 1; }
[[ -f "$apk" ]] || fail "Shipping APK not found: $apk"
command -v adb >/dev/null 2>&1 || fail "adb is not available"

if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$artifacts/emulator.log" 2>&1 &
fi
adb wait-for-device
for attempt in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
[[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "emulator did not finish booting"

adb install -r "$apk" >/dev/null || fail "Shipping APK installation failed"
installed_version="$(adb shell dumpsys package "$package_name" \
  | sed -n 's/.*versionName=//p' | head -n 1 | tr -d '\r')"
[[ "$installed_version" == "$version" ]] \
  || fail "installed version $installed_version does not match VERSION $version"

adb logcat -c
adb shell am force-stop "$package_name"
adb shell am start -n "$activity" >/dev/null || fail "Shipping app launch failed"
foreground=0
for attempt in {1..60}; do
  top_activity="$(adb shell dumpsys activity activities | rg -m1 'topResumedActivity' || true)"
  pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r')"
  if [[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* && -n "$pid" ]]; then
    foreground=1
    break
  fi
  sleep 1
done
(( foreground )) || fail "Shipping GameActivity did not reach foreground"

sleep 12
log_file="$artifacts/logcat.txt"
adb logcat -d >"$log_file"
fatal_pattern='No Vulkan driver found|Unable to run on this device|Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal|RequestExit\(1'
if rg -q "$fatal_pattern" "$log_file"; then
  rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
  fail "late Shipping crash or RHI failure detected"
fi
top_activity="$(adb shell dumpsys activity activities | rg -m1 'topResumedActivity' || true)"
[[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* ]] \
  || fail "Shipping app left the foreground"
adb exec-out screencap -p >"$artifacts/shipping-main.png"
[[ -s "$artifacts/shipping-main.png" ]] || fail "Shipping screenshot is empty"

echo "PASS: Android Shipping startup version=$version package=$package_name"
echo "APK: $apk"
echo "Artifacts: $artifacts"
