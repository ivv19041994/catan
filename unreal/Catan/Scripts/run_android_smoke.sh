#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
apk="${1:-$project_dir/Builds/AndroidLatest/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
log_dir="$(mktemp -d /tmp/catan-android-smoke.XXXXXX)"
log_file="$log_dir/logcat.txt"
screenshot="$log_dir/main-menu.png"
started_emulator=0

fail() {
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $log_dir"
  exit 1
}

if [[ ! -f "$apk" ]]; then
  fail "APK not found: $apk"
fi
if ! command -v adb >/dev/null 2>&1; then
  fail "adb is not available"
fi

if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  print "Starting $avd with host Vulkan renderer..."
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$log_dir/emulator.log" 2>&1 &
  started_emulator=1
fi

adb wait-for-device
for attempt in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
[[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "emulator did not finish booting"

renderer="$(adb shell dumpsys SurfaceFlinger 2>/dev/null | rg -m1 'GLES:' || true)"
print "Renderer: ${renderer:-unknown}"
adb install -r "$apk" >/dev/null || fail "APK installation failed"
adb logcat -c
adb shell am force-stop "$package_name"
adb shell am start -n "$activity" >/dev/null || fail "activity launch failed"

fatal_pattern='No Vulkan driver found|Unable to run on this device|Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal|Lock at Offset|RequestExit\(1'
initialized=0
menu_ready=0
for attempt in {1..60}; do
  adb logcat -d >"$log_file"
  if rg -q "$fatal_pattern" "$log_file"; then
    rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
    fail "fatal Android/RHI condition detected"
  fi
  rg -q 'Passed GEngineLoop.Init\(\)' "$log_file" && initialized=1
  rg -q 'CATAN_MENU backdrop ready' "$log_file" && menu_ready=1
  (( initialized && menu_ready )) && break
  sleep 1
done
(( initialized )) || fail "UE engine initialization marker was not observed"
(( menu_ready )) || fail "Catan main-menu marker was not observed"

# Some renderer failures happen several frames after the menu is created.
sleep 10
adb logcat -d >"$log_file"
if rg -q "$fatal_pattern" "$log_file"; then
  rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
  fail "late Android/RHI failure detected"
fi
top_activity="$(adb shell dumpsys activity activities | rg -m1 'topResumedActivity' || true)"
[[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* ]] \
  || fail "Catan GameActivity is no longer in the foreground: $top_activity"

adb exec-out screencap -p >"$screenshot"
[[ -s "$screenshot" ]] || fail "Android screenshot is empty"

print "PASS: Android engine initialization, main menu and post-start stability"
print "APK: $apk"
print "Artifacts: $log_dir"
if (( started_emulator )); then
  print "Emulator was started by this script and left running for inspection."
fi
