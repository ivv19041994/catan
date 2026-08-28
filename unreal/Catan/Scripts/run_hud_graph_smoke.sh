#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
repo_dir="${project_dir:h:h}"
editor="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
build_dir="${CATAN_HUD_GRAPH_BUILD:-/tmp/catan-hud-graph-dedicated-build}"
log_dir="$(mktemp -d /tmp/catan-hud-graph-smoke.XXXXXX)"
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
    -game -nullrhi -unattended -nosound -abslog="$log_file" "$@" &
  REPLY="$!"
  pids+=("$REPLY")
}

wait_for() {
  local log_file="$1"
  local pattern="$2"
  local timeout="$3"
  for (( attempt = 0; attempt < timeout; ++attempt )); do
    if [[ -f "$log_file" ]] && rg -q "$pattern" "$log_file"; then return 0; fi
    if [[ -f "$log_file" ]] && rg -q 'Assertion failed|Ensure condition failed|Handled ensure|=== Critical error|SIGSEGV|Fatal error' "$log_file"; then
      fail "process crashed while waiting for $pattern"
    fi
    sleep 1
  done
  fail "timed out waiting for $pattern in $log_file"
}

stop_pid() {
  local pid="$1"
  kill "$pid" 2>/dev/null || true
  wait "$pid" 2>/dev/null || true
}

print "Checking every in-widget HUD navigation edge..."
launch "$log_dir/graph.log" -CatanHUDGraphSmoke
graph_pid="$REPLY"
wait_for "$log_dir/graph.log" 'CATAN_HUD_GRAPH PASS edges=38 failures=0' 45
stop_pid "$graph_pid"

print "Checking failed manual join stays out of the lobby..."
launch "$log_dir/invalid-join.log" -CatanAutoManualJoin=127.0.0.1:1 -CatanAutoName=InvalidJoin
invalid_pid="$REPLY"
wait_for "$log_dir/invalid-join.log" 'CATAN_HUD_GRAPH connection-failure' 150
wait_for "$log_dir/invalid-join.log" 'CATAN_HUD_GRAPH returned-main status=Connection failed:' 20
if rg -q 'CATAN_HUD_GRAPH leave-scheduled' "$log_dir/invalid-join.log"; then
  fail "failed connection incorrectly exposed the lobby"
fi
stop_pid "$invalid_pid"

print "Checking unavailable dedicated server stays on setup instead of entering lobby..."
launch "$log_dir/unavailable-dedicated.log" -CatanDedicatedAddress=127.0.0.1:1 \
  -CatanDedicatedJoin=INVALID -CatanAutoName=InvalidDedicated
unavailable_dedicated_pid="$REPLY"
wait_for "$log_dir/unavailable-dedicated.log" \
  'CATAN_HUD_GRAPH request-failure dedicatedActive=0 leaving=0' 45
if rg -q 'CATAN_HUD_GRAPH leave-scheduled' "$log_dir/unavailable-dedicated.log"; then
  fail "failed dedicated connection incorrectly exposed the lobby"
fi
stop_pid "$unavailable_dedicated_pid"

print "Checking LAN client leave updates the host and returns the client to main..."
launch "$log_dir/client-leave-host.log" -CatanAutoHostLobby -CatanAutoName=LeaveHost
client_leave_host_pid="$REPLY"
wait_for "$log_dir/client-leave-host.log" 'CATAN_E2E discovery host listening' 45
host_address="$(sed -n 's/.*LAN create session=.* address=\([^ ]*\).*/\1/p' "$log_dir/client-leave-host.log" | tail -1)"
[[ -n "$host_address" ]] || fail "LAN host address was not published"
launch "$log_dir/client-leave-client.log" -CatanAutoManualJoin="$host_address" \
  -CatanAutoName=LeavingClient -CatanAutoLeaveLobby -CatanAutoLeaveWhenPlayers=2
client_leave_client_pid="$REPLY"
wait_for "$log_dir/client-leave-client.log" 'CATAN_HUD_GRAPH leave-scheduled role=client players=2' 60
wait_for "$log_dir/client-leave-client.log" 'CATAN_HUD_GRAPH returned-main status=Returned to main menu' 30
wait_for "$log_dir/client-leave-host.log" 'CATAN_HUD_GRAPH logout total=1' 30
stop_pid "$client_leave_client_pid"
stop_pid "$client_leave_host_pid"

print "Checking LAN host leave closes the lobby and returns its client to main..."
launch "$log_dir/host-leave-host.log" -CatanAutoHostLobby -CatanAutoName=ClosingHost \
  -CatanAutoLeaveLobby -CatanAutoLeaveWhenPlayers=2
host_leave_host_pid="$REPLY"
wait_for "$log_dir/host-leave-host.log" 'CATAN_E2E discovery host listening' 45
host_address="$(sed -n 's/.*LAN create session=.* address=\([^ ]*\).*/\1/p' "$log_dir/host-leave-host.log" | tail -1)"
[[ -n "$host_address" ]] || fail "closing host address was not published"
launch "$log_dir/host-leave-client.log" -CatanAutoManualJoin="$host_address" -CatanAutoName=StrandedClient
host_leave_client_pid="$REPLY"
wait_for "$log_dir/host-leave-host.log" 'CATAN_HUD_GRAPH leave-scheduled role=host players=2' 60
wait_for "$log_dir/host-leave-host.log" 'CATAN_HUD_GRAPH returned-main status=Returned to main menu' 30
wait_for "$log_dir/host-leave-client.log" 'CATAN_HUD_GRAPH connection-failure' 45
wait_for "$log_dir/host-leave-client.log" 'CATAN_HUD_GRAPH returned-main status=Connection failed:' 30
stop_pid "$host_leave_client_pid"
stop_pid "$host_leave_host_pid"

print "Checking dedicated guest leave and host-close propagation..."
cmake -S "$repo_dir" -B "$build_dir" -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON >/dev/null
cmake --build "$build_dir" --target catan-dedicated-server -j 4 >/dev/null
"$build_dir/catan-dedicated-server" --bind 127.0.0.1 --port 0 --no-persistence >"$log_dir/server.log" 2>&1 &
server_pid="$!"
pids+=("$server_pid")
server_port=""
for _ in {1..100}; do
  server_port="$(sed -n 's/.* port=\([0-9][0-9]*\).*/\1/p' "$log_dir/server.log" | head -1 || true)"
  [[ -n "$server_port" ]] && break
  sleep 0.05
done
[[ -n "$server_port" ]] || fail "dedicated server did not start"
server_address="127.0.0.1:$server_port"

launch "$log_dir/invalid-dedicated-token.log" -CatanDedicatedAddress="$server_address" \
  -CatanDedicatedJoin=INVALID -CatanAutoName=InvalidToken
invalid_dedicated_token_pid="$REPLY"
wait_for "$log_dir/invalid-dedicated-token.log" \
  'CATAN_HUD_GRAPH request-failure dedicatedActive=0 leaving=0 error=Lobby token is invalid' 45
if rg -q 'CATAN_HUD_GRAPH leave-scheduled' "$log_dir/invalid-dedicated-token.log"; then
  fail "invalid dedicated token incorrectly exposed the lobby"
fi
stop_pid "$invalid_dedicated_token_pid"

launch "$log_dir/dedicated-guest-host.log" -CatanDedicatedAddress="$server_address" \
  -CatanAutoName=DedicatedHost -CatanDedicatedLobbyName=GuestLeave
dedicated_guest_host_pid="$REPLY"
wait_for "$log_dir/dedicated-guest-host.log" 'CATAN_DEDICATED_CREATED lobby=' 45
lobby_token="$(sed -n 's/.*CATAN_DEDICATED_CREATED lobby=\([^ ]*\).*/\1/p' "$log_dir/dedicated-guest-host.log" | tail -1)"
launch "$log_dir/dedicated-guest-client.log" -CatanDedicatedAddress="$server_address" \
  -CatanDedicatedJoin="$lobby_token" -CatanAutoName=DedicatedGuest \
  -CatanAutoLeaveLobby -CatanAutoLeaveWhenPlayers=2
dedicated_guest_client_pid="$REPLY"
wait_for "$log_dir/dedicated-guest-client.log" 'CATAN_HUD_GRAPH leave-scheduled role=client players=2' 60
wait_for "$log_dir/dedicated-guest-client.log" 'CATAN_HUD_GRAPH returned-main status=Returned to main menu' 30
wait_for "$log_dir/dedicated-guest-host.log" 'CATAN_HUD_GRAPH dedicated-lobby players=1' 30
stop_pid "$dedicated_guest_client_pid"
stop_pid "$dedicated_guest_host_pid"

launch "$log_dir/dedicated-host-leave.log" -CatanDedicatedAddress="$server_address" \
  -CatanAutoName=ClosingDedicatedHost -CatanDedicatedLobbyName=HostLeave \
  -CatanAutoLeaveLobby -CatanAutoLeaveWhenPlayers=2
dedicated_host_leave_pid="$REPLY"
wait_for "$log_dir/dedicated-host-leave.log" 'CATAN_DEDICATED_CREATED lobby=' 45
lobby_token="$(sed -n 's/.*CATAN_DEDICATED_CREATED lobby=\([^ ]*\).*/\1/p' "$log_dir/dedicated-host-leave.log" | tail -1)"
launch "$log_dir/dedicated-host-guest.log" -CatanDedicatedAddress="$server_address" \
  -CatanDedicatedJoin="$lobby_token" -CatanAutoName=ClosedLobbyGuest
dedicated_host_guest_pid="$REPLY"
wait_for "$log_dir/dedicated-host-leave.log" 'CATAN_HUD_GRAPH leave-scheduled role=host players=2' 60
wait_for "$log_dir/dedicated-host-leave.log" 'CATAN_HUD_GRAPH returned-main status=Returned to main menu' 30
wait_for "$log_dir/dedicated-host-guest.log" 'CATAN_HUD_GRAPH returned-main status=Dedicated lobby closed:' 45

print "PASS: complete HUD graph, invalid join, LAN host/client leave and dedicated host/client leave"
print "Artifacts: $log_dir"
