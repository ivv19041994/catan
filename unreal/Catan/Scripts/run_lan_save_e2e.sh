#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
editor="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
log_dir="$(mktemp -d /tmp/catan-lan-save-e2e.XXXXXX)"
save_file="$log_dir/lan-host.catan"
host_address="${CATAN_E2E_ADDRESS:-127.0.0.1:7777}"
typeset -a pids
pids=()

cleanup() {
  for pid in $pids; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

fail() {
  print -u2 "FAIL: $1"
  print -u2 "Artifacts: $log_dir"
  exit 1
}

launch() {
  local log_file="$1"
  shift
  "$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
    -game -nullrhi -unattended -nosound -abslog="$log_file" \
    -CatanSaveFile="$save_file" "$@" &
  REPLY="$!"
  pids+=("$REPLY")
}

launch_catalog() {
  local log_file="$1"
  local catalog_dir="$2"
  shift 2
  "$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
    -game -nullrhi -unattended -nosound -abslog="$log_file" \
    -CatanSaveDirectory="$catalog_dir" "$@" &
  REPLY="$!"
  pids+=("$REPLY")
}

wait_for() {
  local log_file="$1"
  local pattern="$2"
  local timeout="$3"
  for (( attempt = 0; attempt < timeout; ++attempt )); do
    if [[ -f "$log_file" ]] && rg -q "$pattern" "$log_file"; then return 0; fi
    if [[ -f "$log_file" ]] && rg -q \
      'Assertion failed|Ensure condition failed|=== Critical error|SIGSEGV|Fatal error' "$log_file"; then
      fail "process crashed while waiting for $pattern"
    fi
    sleep 1
  done
  fail "timed out waiting for $pattern in $log_file"
}

stop_all() {
  for pid in $pids; do
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  done
  pids=()
}

print "Creating a three-player LAN game and waiting for an authoritative autosave..."
launch "$log_dir/first-host.log" -CatanAutoHostLobby -CatanAutoName=SaveHost \
  -CatanAutoReady -CatanAutoStart=3 -CatanMultiplayerE2E
wait_for "$log_dir/first-host.log" 'CATAN_E2E discovery host listening' 45
launch "$log_dir/first-client-a.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=SaveAlice -CatanAutoReady -CatanMultiplayerE2E
sleep 2
launch "$log_dir/first-client-b.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=SaveBob -CatanAutoReady -CatanMultiplayerE2E
wait_for "$log_dir/first-host.log" 'CATAN_SMOKE match started players=3 restored=0' 75
wait_for "$log_dir/first-host.log" 'CATAN_SAVE wrote .* players=3' 45
wait_for "$log_dir/first-host.log" 'CATAN_MP_E2E action player=.* action=(roll|pass)' 90
[[ -s "$save_file" ]] || fail "host did not create a non-empty save file"
stop_all

print "Opening the saved lobby and proving that missing/unknown names cannot start it..."
launch "$log_dir/restored-host.log" -CatanAutoHostSavedLobby -CatanAutoName=SaveHost \
  -CatanAutoReady -CatanAutoStart=3 -CatanMultiplayerE2E
wait_for "$log_dir/restored-host.log" 'CATAN_SAVE lobby expected=3 connected=1 waiting=' 60

launch "$log_dir/unknown-client.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=Intruder -CatanAutoReady
wait_for "$log_dir/unknown-client.log" 'CATAN_HUD_GRAPH connection-failure' 45
sleep 2
if rg -q 'CATAN_SAVE restored|CATAN_SMOKE match started.*restored=1' "$log_dir/restored-host.log"; then
  fail "restored match started while expected players were absent"
fi

launch "$log_dir/restored-client-a.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=SaveAlice -CatanAutoReady -CatanMultiplayerE2E
wait_for "$log_dir/restored-host.log" 'CATAN_SAVE lobby expected=3 connected=2 waiting=SaveBob' 60
if rg -q 'CATAN_SAVE restored|CATAN_SMOKE match started.*restored=1' "$log_dir/restored-host.log"; then
  fail "restored match started before the final expected player connected"
fi

launch "$log_dir/restored-client-b.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=SaveBob -CatanAutoReady -CatanMultiplayerE2E
wait_for "$log_dir/restored-host.log" 'CATAN_SAVE restored .* players=3' 75
wait_for "$log_dir/restored-host.log" 'CATAN_SMOKE match started players=3 restored=1' 45
wait_for "$log_dir/restored-host.log" 'CATAN_MP_E2E action player=.* action=(roll|pass|move-robber|discard)' 90

stop_all
print "Checking multi-slot metadata and damaged-save isolation..."
catalog_dir="$log_dir/catalog"
mkdir -p "$catalog_dir"
cp "$save_file" "$catalog_dir/family-game.catan"
cp "$save_file" "$catalog_dir/second-table.catan"
print -n 'damaged-save' >"$catalog_dir/broken.catan"
launch_catalog "$log_dir/catalog.log" "$catalog_dir" -CatanUIPreview=LocalNetwork
wait_for "$log_dir/catalog.log" 'CATAN_SAVE_CATALOG slots=3 valid=2 selectedValid=1' 60

print "PASS: host autosave, multi-slot catalog, damaged-save isolation, name-only rebinding, expected-name gate and resumed play succeeded"
print "Artifacts: $log_dir"
