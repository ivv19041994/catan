#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
repo_dir="${project_dir:h:h}"
apk="${1:-$project_dir/Builds/AndroidLatest/Catan-arm64.apk}"
package_name="${CATAN_ANDROID_PACKAGE:-com.ivv.catan}"
activity="$package_name/com.epicgames.unreal.SplashActivity"
avd="${CATAN_ANDROID_AVD:-UE_pixel_6_API_35}"
sdk_root="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-/opt/homebrew/share/android-commandlinetools}}"
emulator="$sdk_root/emulator/emulator"
build_dir="${CATAN_DEDICATED_BUILD:-/tmp/catan-android-dedicated-e2e-build}"
log_dir="$(mktemp -d /tmp/catan-android-dedicated-e2e.XXXXXX)"
server_pid=""
second_emulator_pid=""

cleanup() {
  [[ -n "$server_pid" ]] && kill "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

fail() {
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $log_dir"
  exit 1
}

[[ -f "$apk" ]] || fail "APK not found: $apk"
[[ -x "$emulator" ]] || fail "Android emulator not found: $emulator"

typeset -a devices
devices=( ${(f)"$(adb devices | awk 'NR > 1 && $2 == "device" && $1 ~ /^emulator-/ { print $1 }')"} )
if (( ${#devices} < 1 )); then
  "$emulator" -avd "$avd" -port 5554 -read-only -gpu host -feature Vulkan \
    -no-snapshot-save >"$log_dir/emulator-5554.log" 2>&1 &
fi
if (( ${#devices} < 2 )); then
  "$emulator" -avd "$avd" -port 5556 -read-only -gpu host -feature Vulkan \
    -no-snapshot-save >"$log_dir/emulator-5556.log" 2>&1 &
  second_emulator_pid="$!"
fi

wait_for_device() {
  local serial="$1"
  for _ in {1..180}; do
    if [[ "$(adb -s "$serial" get-state 2>/dev/null || true)" == "device" ]] \
        && [[ "$(adb -s "$serial" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" == "1" ]]; then
      return 0
    fi
    sleep 1
  done
  return 1
}

wait_for_device emulator-5554 || fail "first virtual phone did not boot"
wait_for_device emulator-5556 || fail "second virtual phone did not boot"
host_serial=emulator-5554
guest_serial=emulator-5556

for serial in "$host_serial" "$guest_serial"; do
  adb -s "$serial" install -r "$apk" >/dev/null || fail "APK install failed on $serial"
  adb -s "$serial" logcat -c
  adb -s "$serial" shell am force-stop "$package_name"
done

cmake -S "$repo_dir" -B "$build_dir" -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
cmake --build "$build_dir" --target catan-dedicated-server -j 4
"$build_dir/catan-dedicated-server" --bind 0.0.0.0 --port 0 >"$log_dir/server.log" 2>&1 &
server_pid="$!"
port=""
for _ in {1..100}; do
  port="$(sed -n 's/.* port=\([0-9][0-9]*\).*/\1/p' "$log_dir/server.log" | head -1 || true)"
  [[ -n "$port" ]] && break
  sleep 0.05
done
[[ -n "$port" ]] || fail "dedicated server did not start"
android_address="10.0.2.2:$port"

host_args="-CatanDedicatedAddress=$android_address -CatanAutoName=AndroidHost -CatanDedicatedLobbyName=AndroidE2E -CatanDedicatedAutoReady -CatanDedicatedAutoStart=2 -CatanDedicatedE2E -CatanMultiplayerE2E"
adb -s "$host_serial" shell am start -n "$activity" --es cmdline "'$host_args'" >/dev/null \
  || fail "host app did not launch"

lobby_token=""
for _ in {1..120}; do
  adb -s "$host_serial" logcat -d >"$log_dir/host.log"
  lobby_token="$(sed -n 's/.*CATAN_DEDICATED_CREATED lobby=\([^ ]*\).*/\1/p' "$log_dir/host.log" | tail -1 || true)"
  [[ -n "$lobby_token" ]] && break
  rg -q 'Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal' "$log_dir/host.log" && fail "host app crashed"
  sleep 1
done
[[ -n "$lobby_token" ]] || fail "Android host did not create a dedicated lobby"

guest_args="-CatanDedicatedAddress=$android_address -CatanDedicatedJoin=$lobby_token -CatanAutoName=AndroidGuest -CatanDedicatedAutoReady -CatanDedicatedE2E -CatanMultiplayerE2E"
adb -s "$guest_serial" shell am start -n "$activity" --es cmdline "'$guest_args'" >/dev/null \
  || fail "guest app did not launch"

connected=0
normal_turns=0
for _ in {1..180}; do
  adb -s "$host_serial" logcat -d >"$log_dir/host.log"
  adb -s "$guest_serial" logcat -d >"$log_dir/guest.log"
  if rg -q 'Assertion failed|Fatal error|FATAL EXCEPTION|Fatal signal' "$log_dir/host.log" "$log_dir/guest.log"; then
    fail "one of the Android clients crashed"
  fi
  connected="$(rg -l 'CATAN_DEDICATED_E2E CONNECTED' "$log_dir/host.log" "$log_dir/guest.log" | wc -l | tr -d ' ' || true)"
  normal_turns="$(rg --no-filename 'CATAN_MP_E2E action player=.* action=pass' "$log_dir/host.log" "$log_dir/guest.log" | wc -l | tr -d ' ' || true)"
  (( connected == 2 && normal_turns >= 2 )) && break
  sleep 1
done
(( connected == 2 )) || fail "both Android clients did not join the dedicated game"
(( normal_turns >= 2 )) || fail "Android clients did not complete setup and normal turns"

for serial in "$host_serial" "$guest_serial"; do
  top_activity="$(adb -s "$serial" shell dumpsys activity activities | rg -m1 topResumedActivity || true)"
  [[ "$top_activity" == *"$package_name/com.epicgames.unreal.GameActivity"* ]] \
    || fail "$serial no longer has Catan in the foreground"
done
adb -s "$host_serial" exec-out screencap -p >"$log_dir/host.png"
adb -s "$guest_serial" exec-out screencap -p >"$log_dir/guest.png"

print "PASS: two Android virtual phones created/joined lobby $lobby_token, completed setup and normal turns."
print "Server: $android_address"
print "APK: $apk"
print "Artifacts: $log_dir"
if [[ -n "$second_emulator_pid" ]]; then
  print "The second emulator was started for this test and left running for inspection."
fi
