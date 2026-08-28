#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
apk="${1:-$project_dir/Builds/AndroidPolish/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
artifact_dir="$(mktemp -d /tmp/catan-android-polish.XXXXXX)"
log_file="$artifact_dir/logcat.txt"

fail() { print -u2 "FAIL: $1\nArtifacts: $artifact_dir"; exit 1; }
[[ -f "$apk" ]] || fail "APK not found: $apk"
command -v adb >/dev/null 2>&1 || fail "adb is unavailable"
if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
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

capture_log() {
  local pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
  if [[ -n "$pid" ]]; then adb logcat -d --pid="$pid" >"$log_file"
  else adb logcat -d >"$log_file" 2>/dev/null || : >"$log_file"; fi
}
wait_for() {
  local marker="$1" context="$2"
  for _ in {1..75}; do
    capture_log
    rg -q 'Assertion failed|Ensure condition failed|Fatal error|FATAL EXCEPTION|Fatal signal' "$log_file" \
      && fail "$context crashed"
    rg -q "$marker" "$log_file" && return
    sleep 1
  done
  fail "$context marker was not observed"
}
launch() {
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline "'$1'" >/dev/null \
    || fail "could not launch $2"
  for _ in {1..60}; do
    [[ -n "$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)" ]] && return
    sleep 0.25
  done
  capture_log
  fail "$2 process did not start"
}

launch '-CatanUIPreview=Settings -CatanLanguage=ru -CatanColorVision=high-contrast' settings
wait_for 'CATAN_POLISH_SETTINGS effects=5 music=5 haptics=2 palettes=5 touch=72 scroll=1' settings
capture_log
rg -q 'CATAN_ACCESSIBILITY palette=high-contrast .*labels=always' "$log_file" \
  || fail "high-contrast palette was not applied"
sleep 2
adb exec-out screencap -p >"$artifact_dir/settings-top.png"
adb shell input swipe 1410 680 1410 300 600
sleep 1
adb exec-out screencap -p >"$artifact_dir/settings-bottom.png"

launch '-CatanUIPreview=FinalDashboard -CatanLanguage=ru' final-dashboard
wait_for 'CATAN_FINAL_DASHBOARD rows=4 winner=Player vpCardsRevealed=4 scroll=1 touch=72' final-dashboard
sleep 2
adb exec-out screencap -p >"$artifact_dir/final-dashboard-top.png"
adb shell input swipe 1200 650 1200 320 500
sleep 1
adb exec-out screencap -p >"$artifact_dir/final-dashboard-bottom.png"

launch '-CatanAutoBots=3 -CatanUIPreview=Game -CatanColorVision=high-contrast' audio-feedback
wait_for 'CATAN_AUDIO effects=[0-9.]+ music=[0-9.]+ haptics=[01]' audio-feedback
wait_for 'CATAN_AUDIO effects=[0-9.]+ music=[0-9.]+ haptics=[01] palette=high-contrast live=1' audio-feedback
wait_for 'CATAN_SMOKE client board ready' audio-feedback
wait_for 'CATAN_AUDIO music-started duration=16 volume=[0-9.]+ component=1' audio-feedback
sleep 2
adb exec-out screencap -p >"$artifact_dir/audio-game.png"

for image in "$artifact_dir"/*.png; do [[ -s "$image" ]] || fail "empty screenshot: $image"; done
print "PASS: Android polish settings, high-contrast palette, large touch targets, audio/haptics and final dashboard"
print "APK: $apk"
print "Artifacts: $artifact_dir"
