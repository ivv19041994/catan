#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
apk="${1:-$project_dir/Builds/AndroidLatest/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
serial="${ANDROID_SERIAL:-emulator-5554}"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
log_dir="$(mktemp -d /tmp/catan-android-bot-e2e.XXXXXX)"

fail() {
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $log_dir"
  exit 1
}

[[ -f "$apk" ]] || fail "APK not found: $apk"
if [[ "$(adb -s "$serial" get-state 2>/dev/null || true)" != "device" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  "$emulator" -avd "$avd" -port "${serial#emulator-}" -gpu host -feature Vulkan \
    -no-snapshot-save >"$log_dir/emulator.log" 2>&1 &
fi
for _ in {1..180}; do
  if [[ "$(adb -s "$serial" get-state 2>/dev/null || true)" == "device" ]] \
      && [[ "$(adb -s "$serial" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; then
    break
  fi
  sleep 1
done
[[ "$(adb -s "$serial" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] \
  || fail "$serial did not finish booting"

adb -s "$serial" install -r "$apk" >/dev/null || fail "APK installation failed"
fatal_pattern='Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal|CATAN_BOT_E2E FAIL'
app_pid=""
capture_app_log() {
  if [[ -z "$app_pid" ]]; then
    app_pid="$(adb -s "$serial" shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
  fi
  if [[ -n "$app_pid" ]]; then adb -s "$serial" logcat -d --pid="$app_pid" >"$log_file"
  else : >"$log_file"
  fi
}
for bot_count in 1 2 3; do
  log_file="$log_dir/bots-${bot_count}.log"
  args="-CatanAutoBots=$bot_count -CatanPlayerName=AndroidE2E -CatanBotAutoplay -CatanBotMaxActions=12000"
  print "Running Android bot E2E with $((bot_count + 1)) players..."
  adb -s "$serial" logcat -c
  adb -s "$serial" shell am force-stop "$package_name"
  adb -s "$serial" shell am start -n "$activity" --es cmdline "'$args'" >/dev/null \
    || fail "could not launch $((bot_count + 1))-player game"
  app_pid=""
  for _ in {1..50}; do
    app_pid="$(adb -s "$serial" shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
    [[ -n "$app_pid" ]] && break
    sleep 0.1
  done
  [[ -n "$app_pid" ]] || fail "Catan process did not start"
  passed=0
  for _ in {1..180}; do
    capture_app_log
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "$((bot_count + 1))-player Android bot game failed"
    fi
    if rg -q 'CATAN_BOT_E2E PASS' "$log_file"; then passed=1; break; fi
    sleep 1
  done
  (( passed )) || fail "$((bot_count + 1))-player Android bot game timed out"
  rg 'CATAN_BOT_E2E PASS' "$log_file" | tail -n 1
done

print "PASS: Android bot games completed for 2, 3 and 4 players."
print "APK: $apk"
print "Artifacts: $log_dir"
