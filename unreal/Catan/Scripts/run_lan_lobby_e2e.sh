#!/bin/zsh
set -eu

project_file="${0:A:h:h}/Catan.uproject"
editor_binary="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
log_dir="$(mktemp -d /tmp/catan-lobby-e2e.XXXXXX)"
pids=()

cleanup() {
  for pid in $pids; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

launch() {
  "$editor_binary" "$project_file" "/Engine/Maps/Templates/Template_Default" \
    -game -windowed -ResX=800 -ResY=450 "$@" &
  pids+=("$!")
}

launch -abslog="$log_dir/host.log" -CatanAutoHostLobby -CatanAutoName=E2EHost
for attempt in {1..30}; do
  if grep -q "CATAN_E2E discovery host listening" "$log_dir/host.log" 2>/dev/null; then break; fi
  sleep 1
done
if ! grep -q "CATAN_E2E discovery host listening" "$log_dir/host.log" 2>/dev/null; then
  print -u2 "FAIL: host did not create a discoverable lobby; logs: $log_dir"
  exit 1
fi

host_address="$(sed -n 's/.*LAN create session=.* address=\([^ ]*\).*/\1/p' "$log_dir/host.log" | tail -1)"
launch -abslog="$log_dir/discovery.log" -CatanAutoFindJoin -CatanAutoName=DiscoveryClient
sleep 3
launch -abslog="$log_dir/manual.log" -CatanAutoManualJoin="$host_address" -CatanAutoName=ManualClient

for attempt in {1..45}; do
  if rg -q "Assertion failed|=== Critical error|SIGSEGV|Connection TIMED OUT" "$log_dir"/*.log 2>/dev/null; then
    print -u2 "FAIL: crash or connection timeout; logs: $log_dir"
    rg -n "Assertion failed|=== Critical error|SIGSEGV|Connection TIMED OUT" "$log_dir"/*.log
    exit 1
  fi
  if grep -q "CATAN_E2E post login.*total=3" "$log_dir/host.log" 2>/dev/null \
      && grep -q "LAN discovery success=1 results=1" "$log_dir/discovery.log" 2>/dev/null \
      && grep -q "Local LAN address mapped to loopback" "$log_dir/manual.log" 2>/dev/null; then
    print "PASS: UI-equivalent host, discovery join and manual-address join reached one lobby"
    print "Logs: $log_dir"
    exit 0
  fi
  sleep 1
done

print -u2 "FAIL: clients did not reach the host lobby; logs: $log_dir"
exit 1
