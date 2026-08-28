#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
apk="${1:-$project_dir/Builds/AndroidPolish/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
artifact_dir="$(mktemp -d /tmp/catan-button-accessibility.XXXXXX)"
log_file="$artifact_dir/logcat.txt"
started_emulator=0

fail() { print -u2 "FAIL: $1\nArtifacts: $artifact_dir"; exit 1; }
cleanup() { (( started_emulator )) && adb emu kill >/dev/null 2>&1 || true; }
trap cleanup EXIT
[[ -f "$apk" ]] || fail "APK not found: $apk"
command -v adb >/dev/null 2>&1 || fail "adb is unavailable"
if [[ -z "$(adb devices | awk 'NR > 1 && $2 == "device" { print $1; exit }')" ]]; then
  [[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"
  "$emulator" -avd "$avd" -gpu host -feature Vulkan -no-snapshot-save \
    -netdelay none -netspeed full >"$artifact_dir/emulator.log" 2>&1 &
  started_emulator=1
fi
adb wait-for-device
for _ in {1..120}; do
  [[ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]] && break
  sleep 1
done
adb install -r "$apk" >/dev/null || fail "APK installation failed"

modes=(Game Players4 History ActionLabels Discard IncomingTrade Development
  DevelopmentPlenty DevelopmentMonopoly Bank PlayerTrade Online LocalNetwork
  DedicatedServer Bots Settings Onboarding FinalDashboard)
fatal_pattern='Assertion failed|Ensure condition failed|Fatal error|FATAL EXCEPTION|Fatal signal'
audit_failures=0
for mode in $modes; do
  print "Auditing buttons: $mode"
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline \
    "'-CatanUIPreview=$mode -CatanButtonAudit'" >/dev/null || fail "$mode launch failed"
  passed=0
  for _ in {1..75}; do
    pid="$(adb shell pidof -s "$package_name" 2>/dev/null | tr -d '\r' || true)"
    if [[ -n "$pid" ]]; then adb logcat -d --pid="$pid" >"$log_file"; else adb logcat -d >"$log_file"; fi
    rg -q "$fatal_pattern" "$log_file" && fail "$mode crashed"
    if rg -q "CATAN_BUTTON_AUDIT mode=$mode buttons=[1-9][0-9]* touchFailures=0 boundsFailures=0 critical=[0-9]+/[0-9]+ scrollReachable=[0-9]+ minTouch=72" "$log_file"; then
      passed=1
      break
    fi
    if rg -q "CATAN_BUTTON_AUDIT mode=$mode" "$log_file"; then
      rg "CATAN_BUTTON_AUDIT mode=$mode" "$log_file" >&2
      passed=2
      break
    fi
    sleep 1
  done
  if (( passed == 0 )); then
    print -u2 "FAIL: $mode button audit marker was not observed"
    (( ++audit_failures ))
  elif (( passed == 2 )); then
    print -u2 "FAIL: $mode has clipped, unreachable, or undersized buttons"
    (( ++audit_failures ))
  fi
  adb exec-out screencap -p >"$artifact_dir/${mode:l}.png"
  [[ -s "$artifact_dir/${mode:l}.png" ]] || fail "$mode screenshot is empty"
done

(( audit_failures == 0 )) || fail "$audit_failures button accessibility previews failed"
print "PASS: all active buttons are reachable, unclipped, and at least 72 px high"
print "APK: $apk"
print "Artifacts: $artifact_dir"
