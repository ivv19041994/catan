#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
repo_dir="${project_dir:h:h}"
editor="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
build_dir="${CATAN_DEDICATED_BUILD:-/tmp/catan-dedicated-e2e-build}"
log_dir="$(mktemp -d /tmp/catan-dedicated-e2e.XXXXXX)"
server_pid=""
host_pid=""
guest_pid=""

cleanup() {
  for pid in "$host_pid" "$guest_pid" "$server_pid"; do
    [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
  done
}
trap cleanup EXIT INT TERM

if [[ ! -x "$editor" ]]; then
  print -u2 "UnrealEditor not found: $editor"
  exit 2
fi

cmake -S "$repo_dir" -B "$build_dir" -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
cmake --build "$build_dir" --target catan-dedicated-server catan-dedicated-probe \
  catan_dedicated_server_tests -j 4

"$build_dir/catan-dedicated-server" --bind 127.0.0.1 --port 0 >"$log_dir/server.log" 2>&1 &
server_pid="$!"
port=""
for _ in {1..100}; do
  port="$(sed -n 's/.* port=\([0-9][0-9]*\).*/\1/p' "$log_dir/server.log" | head -1)"
  [[ -n "$port" ]] && break
  sleep 0.05
done
[[ -n "$port" ]] || { print -u2 "Dedicated server failed to start; logs: $log_dir"; exit 1; }
address="127.0.0.1:$port"

"$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
  -game -nullrhi -unattended -nosound -abslog="$log_dir/host.log" \
  -CatanDedicatedAddress="$address" -CatanAutoName=DedicatedHost \
  -CatanDedicatedLobbyName="Dedicated E2E" -CatanDedicatedAutoReady \
  -CatanDedicatedAutoStart=2 -CatanDedicatedE2E -CatanMultiplayerE2E &
host_pid="$!"

lobby_token=""
for _ in {1..90}; do
  lobby_token="$(sed -n 's/.*CATAN_DEDICATED_CREATED lobby=\([^ ]*\).*/\1/p' "$log_dir/host.log" 2>/dev/null | tail -1 || true)"
  [[ -n "$lobby_token" ]] && break
  if rg -q "Assertion failed|=== Critical error|SIGSEGV" "$log_dir/host.log" 2>/dev/null; then
    print -u2 "Host crashed; logs: $log_dir"; exit 1
  fi
  sleep 1
done
[[ -n "$lobby_token" ]] || { print -u2 "Host did not create a lobby; logs: $log_dir"; exit 1; }

"$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
  -game -nullrhi -unattended -nosound -abslog="$log_dir/guest.log" \
  -CatanDedicatedAddress="$address" -CatanDedicatedJoin="$lobby_token" \
  -CatanAutoName=DedicatedGuest -CatanDedicatedAutoReady -CatanDedicatedE2E -CatanMultiplayerE2E &
guest_pid="$!"

for _ in {1..120}; do
  connected="$(rg -l "CATAN_DEDICATED_E2E CONNECTED" "$log_dir/host.log" "$log_dir/guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  normal_turns="$(rg --no-filename "CATAN_MP_E2E action player=.* action=pass" "$log_dir/host.log" "$log_dir/guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  (( connected == 2 && normal_turns >= 2 )) && break
  if rg -q "Assertion failed|=== Critical error|SIGSEGV|CATAN_DEDICATED_E2E FAIL" \
      "$log_dir/host.log" "$log_dir/guest.log" 2>/dev/null; then
    print -u2 "Dedicated UE E2E crashed or failed; logs: $log_dir"; exit 1
  fi
  sleep 1
done
if (( ${connected:-0} != 2 || ${normal_turns:-0} < 2 )); then
  print -u2 "Dedicated UE clients did not complete setup and normal turns; logs: $log_dir"
  exit 1
fi

kill "$host_pid" "$guest_pid" 2>/dev/null || true
wait "$host_pid" 2>/dev/null || true; host_pid=""
wait "$guest_pid" 2>/dev/null || true; guest_pid=""
ctest --test-dir "$build_dir" -R catan_dedicated_server_tests --output-on-failure
print "PASS: dedicated CLI server, create/token join, ready/start, and two UE proxies succeeded."
print "Lobby token: $lobby_token"
print "Logs: $log_dir"
