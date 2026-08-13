#!/bin/zsh
set -eu

project_file="${0:A:h:h}/Catan.uproject"
editor_binary="${UE_EDITOR:-/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor}"
player_count="${1:-3}"
smoke_port="${CATAN_SMOKE_PORT:-17777}"

if (( player_count < 2 || player_count > 4 )); then
  print -u2 "player count must be between 2 and 4"
  exit 2
fi
if [[ ! -x "$editor_binary" ]]; then
  print -u2 "UnrealEditor not found: $editor_binary (set UE_EDITOR to override)"
  exit 2
fi

log_dir="$(mktemp -d /tmp/catan-lan-smoke.XXXXXX)"
pids=()
cleanup() {
  for pid in $pids; do kill "$pid" 2>/dev/null || true; done
}
trap cleanup EXIT INT TERM

"$editor_binary" "$project_file" "/Engine/Maps/Templates/Template_Default?listen?Name=Host?Port=$smoke_port" \
  -game -windowed -ResX=960 -ResY=540 -abslog="$log_dir/host.log" \
  -CatanAutoReady -CatanAutoStart="$player_count" &
pids+=("$!")
sleep 4

for index in {$((player_count - 1))..1}; do
  "$editor_binary" "$project_file" "127.0.0.1:$smoke_port?Name=Client${index}" \
    -game -windowed -ResX=960 -ResY=540 -abslog="$log_dir/client${index}.log" \
    -CatanAutoReady &
  pids+=("$!")
  sleep 4
done

print "LAN smoke running with $player_count instances"
print "Logs: $log_dir"
print "Waiting up to 45 seconds for an automatically started match..."
for attempt in {1..45}; do
  failed=false
  if rg -q "Assertion failed|=== Critical error|SIGSEGV" "$log_dir"/*.log 2>/dev/null; then failed=true; fi
  if [[ "$failed" == true ]]; then
    print -u2 "FAIL: an Unreal instance crashed or exited; inspect $log_dir"
    rg -n "Assertion failed|=== Critical error|SIGSEGV" "$log_dir"/*.log 2>/dev/null || true
    exit 1
  fi

  boards_ready=0
  for log_file in "$log_dir"/*.log; do
    if grep -q "CATAN_SMOKE client board ready" "$log_file" 2>/dev/null; then
      (( boards_ready += 1 ))
    fi
  done
  if grep -q "CATAN_SMOKE match started players=$player_count" "$log_dir/host.log" 2>/dev/null \
      && (( boards_ready == player_count )); then
    print "PASS: lobby started and all $player_count instances built their replicated board"
    exit 0
  fi
  sleep 1
done

print -u2 "FAIL: match did not start; inspect $log_dir"
tail -80 "$log_dir/host.log" 2>/dev/null || true
exit 1
