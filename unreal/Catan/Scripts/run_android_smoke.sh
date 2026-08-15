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
fatal_pattern='No Vulkan driver found|Unable to run on this device|Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal|Lock at Offset|RequestExit\(1'

assert_running_without_fatal() {
  local context="$1"
  adb logcat -d >"$log_file"
  if rg -q "$fatal_pattern" "$log_file"; then
    rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
    fail "$context"
  fi
  local top_activity="$(adb shell dumpsys activity activities | rg -m1 'topResumedActivity' || true)"
  [[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* ]] \
    || fail "Catan GameActivity is no longer in the foreground: $top_activity"
}

test_combo_preview() {
  local mode="$1"
  local tap_x="$2"
  local tap_y="$3"
  local output="$log_dir/${mode:l}-dropdown.png"
  print "Testing $mode dropdown contrast and post-GC stability..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline "-CatanUIPreview=$mode" >/dev/null \
    || fail "$mode preview launch failed"
  local preview_ready=0
  local style_ready=0
  for attempt in {1..60}; do
    adb logcat -d >"$log_file"
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "$mode preview crashed during startup"
    fi
    rg -q "CATAN_UI_PREVIEW ready mode=$mode" "$log_file" && preview_ready=1
    rg -q 'CATAN_COMBO_STYLE ready widgets=15 popupText=white' "$log_file" && style_ready=1
    (( preview_ready && style_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "$mode preview marker was not observed"
  (( style_ready )) || fail "readable combo style was not applied to all 15 dropdowns"
  if [[ "$mode" == "PlayerTrade" ]]; then
    rg -q 'CATAN_PLAYER_TRADE_LIMITS max=1,2,3,4,7 receive=5' "$log_file" \
      || fail "Other Player give limits do not match the local hand"
  fi
  local combo_open=0
  # The preview marker can arrive one frame before Android starts routing touch to Slate.
  sleep 2
  for attempt in {1..3}; do
    adb shell input tap "$tap_x" "$tap_y"
    sleep 1
    adb logcat -d >"$log_file"
    rg -q 'CATAN_COMBO_OPEN' "$log_file" && combo_open=1
    (( combo_open )) && break
  done
  (( combo_open )) || fail "$mode dropdown did not open at $tap_x,$tap_y"
  adb shell am broadcast -a android.intent.action.RUN -e cmd 'obj gc' >/dev/null
  sleep 3
  assert_running_without_fatal "$mode dropdown failed while open after garbage collection"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "$mode dropdown screenshot is empty"
}

test_bank_preview() {
  local output="$log_dir/bank-rates.png"
  print "Testing per-resource bank rate labels..."
  adb logcat -c
  adb shell am force-stop "$package_name"
  adb shell am start -n "$activity" --es cmdline '-CatanUIPreview=Bank' >/dev/null \
    || fail "bank preview launch failed"
  local preview_ready=0
  local labels_ready=0
  for attempt in {1..60}; do
    adb logcat -d >"$log_file"
    if rg -q "$fatal_pattern" "$log_file"; then
      rg -n "$fatal_pattern" "$log_file" | tail -n 30 >&2 || true
      fail "bank preview crashed during startup"
    fi
    rg -q 'CATAN_UI_PREVIEW ready mode=Bank' "$log_file" && preview_ready=1
    rg -q 'CATAN_BANK_LABELS rates=4,3,4,2,4' "$log_file" && labels_ready=1
    (( preview_ready && labels_ready )) && break
    sleep 1
  done
  (( preview_ready )) || fail "bank preview marker was not observed"
  (( labels_ready )) || fail "per-resource bank rate labels were not applied"
  assert_running_without_fatal "bank rate labels failed"
  adb exec-out screencap -p >"$output"
  [[ -s "$output" ]] || fail "bank rate screenshot is empty"
}

adb logcat -c
adb shell am force-stop "$package_name"
adb shell am start -n "$activity" >/dev/null || fail "activity launch failed"

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
assert_running_without_fatal "late Android/RHI failure detected"

adb exec-out screencap -p >"$screenshot"
[[ -s "$screenshot" ]] || fail "Android screenshot is empty"

test_combo_preview PlayerTrade 1180 490
test_combo_preview Development 1200 565
test_combo_preview Online 1200 435
test_combo_preview Bots 1200 390
test_bank_preview

print "PASS: Android startup, dynamic trade limits, bank labels and all dropdown families"
print "APK: $apk"
print "Artifacts: $log_dir"
if (( started_emulator )); then
  print "Emulator was started by this script and left running for inspection."
fi
