#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
engine_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$engine_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
rounds="${1:-3}"
log_dir="$(mktemp -d /tmp/catan-bot-tournament.XXXXXX)"
typeset -A wins
total_actions=0
games=0

[[ -x "$editor" ]] || { print -u2 "UnrealEditor not found: $editor"; exit 2; }
[[ "$rounds" == <-> && "$rounds" -gt 0 ]] || { print -u2 "Rounds must be positive"; exit 2; }

for round in $(seq 1 "$rounds"); do
  for bot_count in 1 2 3; do
    log_file="$log_dir/round-${round}-players-$((bot_count + 1)).log"
    "$editor" "$project_dir/Catan.uproject" -game -nullrhi -unattended -nosound \
      -CatanAutoBots="$bot_count" -CatanPlayerName=Tournament -CatanBotAutoplay \
      -CatanBotMaxActions=12000 -abslog="$log_file"
    marker="$(rg 'CATAN_BOT_E2E (PASS|FAIL)' "$log_file" | tail -1 || true)"
    [[ "$marker" == *"CATAN_BOT_E2E PASS"* ]] \
      || { print -u2 "Tournament game failed: $log_file\n$marker"; exit 1; }
    winner="$(print -r -- "$marker" | sed -n 's/.*winner=\(.*\) actions=.*/\1/p')"
    actions="$(print -r -- "$marker" | sed -n 's/.*actions=\([0-9][0-9]*\).*/\1/p')"
    wins[$winner]=$(( ${wins[$winner]:-0} + 1 ))
    total_actions=$(( total_actions + actions ))
    games=$(( games + 1 ))
    rg -q 'CATAN_BOT plan=' "$log_file" || { print -u2 "No strategic plan marker: $log_file"; exit 1; }
  done
done

print "PASS: $games Bot AI tournament games completed"
print "Average actions: $(( total_actions / games ))"
for winner count in ${(kv)wins}; do print "Wins $winner: $count"; done
print "Artifacts: $log_dir"
