#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
repo_dir="${project_dir:h:h}"
editor="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
build_dir="${CATAN_DEDICATED_BUILD:-/tmp/catan-dedicated-e2e-build}"
log_dir="$(mktemp -d /tmp/catan-dedicated-e2e.XXXXXX)"
state_dir="$(mktemp -d /tmp/catan-dedicated-e2e-state.XXXXXX)"
state_file="$state_dir/server.state"
server_pid=""
host_pid=""
guest_pid=""

cleanup() {
  for pid in "$host_pid" "$guest_pid" "$server_pid"; do
    [[ -n "$pid" ]] && kill "$pid" 2>/dev/null || true
  done
  rm -f "$state_file" "$state_file.tmp"
  rmdir "$state_dir" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

if [[ ! -x "$editor" ]]; then
  print -u2 "UnrealEditor not found: $editor"
  exit 2
fi

cmake -S "$repo_dir" -B "$build_dir" -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
cmake --build "$build_dir" --target catan-dedicated-server catan-dedicated-probe \
  catan_dedicated_server_tests -j 4

"$build_dir/catan-dedicated-server" --bind 127.0.0.1 --port 0 \
  --state-file "$state_file" --drop-response-once CREATE2 >"$log_dir/server.log" 2>&1 &
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
rg -q 'CATAN_DEDICATED_DEBUG dropped_response operation=CREATE2' "$log_dir/server.log" \
  || { print -u2 "UE client did not exercise dropped CREATE2 response; logs: $log_dir"; exit 1; }

print "Restarting the dedicated server while both UE clients remain alive..."
kill -TERM "$server_pid"
wait "$server_pid"
server_pid=""
for _ in {1..30}; do
  waiting="$(rg -l 'CATAN_DEDICATED_RECONNECT waiting' "$log_dir/host.log" "$log_dir/guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  (( waiting == 2 )) && break
  sleep 1
done
(( ${waiting:-0} == 2 )) || { print -u2 "Both clients did not observe outage; logs: $log_dir"; exit 1; }

"$build_dir/catan-dedicated-server" --bind 127.0.0.1 --port "$port" \
  --state-file "$state_file" >"$log_dir/server-restarted.log" 2>&1 &
server_pid="$!"
for _ in {1..10}; do
  rg -q 'CATAN_DEDICATED_READY.*lobbies=1' "$log_dir/server-restarted.log" 2>/dev/null && break
  sleep 1
done
rg -q 'CATAN_DEDICATED_READY.*lobbies=1' "$log_dir/server-restarted.log" \
  || { print -u2 "Persistent server did not restore the lobby; logs: $log_dir"; exit 1; }
for _ in {1..45}; do
  reconnected="$(rg -l 'CATAN_DEDICATED_RECONNECTED' "$log_dir/host.log" "$log_dir/guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  (( reconnected == 2 )) && break
  sleep 1
done
(( ${reconnected:-0} == 2 )) || { print -u2 "Both clients did not reconnect after restart; logs: $log_dir"; exit 1; }

print "Checking process-level RESUME with original private credentials..."
kill "$host_pid" "$guest_pid" 2>/dev/null || true
wait "$host_pid" 2>/dev/null || true; host_pid=""
wait "$guest_pid" 2>/dev/null || true; guest_pid=""

request() {
  "$build_dir/catan-dedicated-probe" --host 127.0.0.1 --port "$port" \
    --timeout-ms 3000 --request "$1"
}
resume_created="$(request $'CREATE2\tresume-create-request-001\t526573756d65486f7374\t526573756d6520453245')"
IFS=$'\t' read -r _ _ resume_lobby resume_host_token _ <<<"$resume_created"
resume_joined="$(request $'JOIN2\tresume-join-request-00001\t'"$resume_lobby"$'\t526573756d654775657374')"
IFS=$'\t' read -r _ _ _ resume_guest_token _ <<<"$resume_joined"
request $'READY2\t'"$resume_lobby"$'\t'"$resume_host_token"$'\tresume-ready-host-0001\t1' >/dev/null
request $'READY2\t'"$resume_lobby"$'\t'"$resume_guest_token"$'\tresume-ready-guest-001\t1' >/dev/null
request $'START2\t'"$resume_lobby"$'\t'"$resume_host_token"$'\tresume-start-request-001' >/dev/null

"$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
  -game -nullrhi -unattended -nosound -abslog="$log_dir/resume-host.log" \
  -CatanDedicatedAddress="$address" -CatanDedicatedResumeLobby="$resume_lobby" \
  -CatanDedicatedPlayerToken="$resume_host_token" -CatanAutoName=ResumeHost \
  -CatanDedicatedE2E -CatanMultiplayerE2E &
host_pid="$!"
"$editor" "$project_dir/Catan.uproject" "/Engine/Maps/Templates/Template_Default" \
  -game -nullrhi -unattended -nosound -abslog="$log_dir/resume-guest.log" \
  -CatanDedicatedAddress="$address" -CatanDedicatedResumeLobby="$resume_lobby" \
  -CatanDedicatedPlayerToken="$resume_guest_token" -CatanAutoName=ResumeGuest \
  -CatanDedicatedE2E -CatanMultiplayerE2E &
guest_pid="$!"
for _ in {1..120}; do
  resumed="$(rg -l 'CATAN_DEDICATED_RESUMED' "$log_dir/resume-host.log" "$log_dir/resume-guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  resumed_connected="$(rg -l 'CATAN_DEDICATED_E2E CONNECTED' "$log_dir/resume-host.log" "$log_dir/resume-guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  resume_actions="$(rg --no-filename 'CATAN_MP_E2E action player=.* action=' "$log_dir/resume-host.log" "$log_dir/resume-guest.log" 2>/dev/null | wc -l | tr -d ' ' || true)"
  (( resumed == 2 && resumed_connected == 2 && resume_actions >= 2 )) && break
  if rg -q 'Assertion failed|=== Critical error|SIGSEGV|CATAN_DEDICATED_E2E FAIL' \
      "$log_dir/resume-host.log" "$log_dir/resume-guest.log" 2>/dev/null; then
    print -u2 "Resumed UE clients crashed or failed; logs: $log_dir"; exit 1
  fi
  sleep 1
done
if (( ${resumed:-0} != 2 || ${resumed_connected:-0} != 2 || ${resume_actions:-0} < 2 )); then
  print -u2 "Resumed UE clients did not continue the game; logs: $log_dir"
  exit 1
fi

kill "$host_pid" "$guest_pid" 2>/dev/null || true
wait "$host_pid" 2>/dev/null || true; host_pid=""
wait "$guest_pid" 2>/dev/null || true; guest_pid=""
for private_log in "$log_dir/resume-host.log" "$log_dir/resume-guest.log"; do
  sed -i '' "s/$resume_host_token/[REDACTED_PLAYER_TOKEN]/g; s/$resume_guest_token/[REDACTED_PLAYER_TOKEN]/g" \
    "$private_log"
done
ctest --test-dir "$build_dir" -R catan_dedicated_server_tests --output-on-failure
print "PASS: dropped-response retry, server restart reconnect, process RESUME, and two UE proxies succeeded."
print "Lobby token: $lobby_token"
print "Logs: $log_dir"
