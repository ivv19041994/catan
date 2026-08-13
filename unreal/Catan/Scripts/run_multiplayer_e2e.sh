#!/bin/zsh

set -euo pipefail

project_file="${0:A:h:h}/Catan.uproject"
editor_binary="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
host_address="${CATAN_E2E_ADDRESS:-127.0.0.1:7777}"
log_dir="$(mktemp -d /tmp/catan-multiplayer-e2e.XXXXXX)"
host_pid=""
client_one_pid=""
client_two_pid=""
reconnect_pid=""

cleanup() {
  for pid in "$host_pid" "$client_one_pid" "$client_two_pid" "$reconnect_pid"; do
    if [[ -n "$pid" ]]; then kill "$pid" 2>/dev/null || true; fi
  done
}
trap cleanup EXIT INT TERM

if [[ ! -x "$editor_binary" ]]; then
  print -u2 "UnrealEditor not found: $editor_binary (set UE_EDITOR to override)"
  exit 2
fi

crashed() {
  local logs=("$log_dir"/*.log(N))
  (( ${#logs} > 0 )) && rg -q "Assertion failed|=== Critical error|SIGSEGV|Connection TIMED OUT" $logs 2>/dev/null
}

wait_for() {
  local pattern="$1"
  local timeout="$2"
  local elapsed=0
  while (( elapsed < timeout )); do
    if crashed; then
      print -u2 "FAIL: crash or connection timeout; logs: $log_dir"
      rg -n "Assertion failed|=== Critical error|SIGSEGV|Connection TIMED OUT" "$log_dir"/*.log 2>/dev/null || true
      return 1
    fi
    local logs=("$log_dir"/*.log(N))
    if (( ${#logs} > 0 )) && rg -q "$pattern" $logs 2>/dev/null; then return 0; fi
    sleep 1
    (( elapsed += 1 ))
  done
  print -u2 "FAIL: timed out waiting for '$pattern'; logs: $log_dir"
  return 1
}

launch_client() {
  local name="$1"
  local log_file="$2"
  shift 2
  "$editor_binary" "$project_file" "/Engine/Maps/Templates/Template_Default" \
    -game -nullrhi -unattended -nosound -abslog="$log_file" \
    -CatanAutoManualJoin="$host_address" -CatanAutoName="$name" \
    -CatanAutoReady -CatanMultiplayerE2E "$@" &
  REPLY="$!"
}

print "Starting host and two LAN clients..."
"$editor_binary" "$project_file" "/Engine/Maps/Templates/Template_Default" \
  -game -nullrhi -unattended -nosound -abslog="$log_dir/host.log" \
  -CatanAutoHostLobby -CatanAutoName=E2EHost -CatanAutoReady \
  -CatanAutoStart=3 -CatanMultiplayerE2E &
host_pid="$!"

wait_for "CATAN_E2E discovery host listening" 45
launch_client E2EClient1 "$log_dir/client1.log"
client_one_pid="$REPLY"
sleep 2
launch_client E2EClient2 "$log_dir/client2.log"
client_two_pid="$REPLY"

wait_for "CATAN_SMOKE match started players=3" 60
wait_for "CATAN_MP_E2E action player=.* action=pass" 75

for attempt in {1..90}; do
  if crashed; then
    print -u2 "FAIL: crash or connection timeout; logs: $log_dir"
    exit 1
  fi
  logs=("$log_dir"/*.log(N))
  pass_count="$(rg --no-filename "CATAN_MP_E2E action player=.* action=pass" $logs 2>/dev/null | wc -l | tr -d ' ')"
  if (( pass_count >= 6 )); then break; fi
  sleep 1
done
if (( ${pass_count:-0} < 6 )); then
  print -u2 "FAIL: fewer than six normal turns completed; logs: $log_dir"
  exit 1
fi

print "Normal turns completed; disconnecting and reconnecting E2EClient2..."
kill "$client_two_pid" 2>/dev/null || true
wait "$client_two_pid" 2>/dev/null || true
client_two_pid=""
sleep 2
launch_client E2EClient2 "$log_dir/reconnect.log" -CatanExpectReconnect
reconnect_pid="$REPLY"

wait_for "CATAN_MP_E2E reconnect restored player=E2EClient2" 45
wait_for "CATAN_MP_E2E reconnect snapshot player=E2EClient2" 45
wait_for "CATAN_MP_E2E action player=E2EClient2 action=(roll|pass|move-robber|discard)" 60

print "PASS: lobby, ready/start, distributed setup, normal turns and reconnect succeeded"
print "Logs: $log_dir"
