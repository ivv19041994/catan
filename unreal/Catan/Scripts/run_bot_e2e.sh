#!/bin/zsh

set -euo pipefail

project_dir="${0:A:h:h}"
engine_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
editor="$engine_root/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor"
log_file="$HOME/Library/Logs/Catan/Catan.log"

if [[ ! -x "$editor" ]]; then
    print -u2 "UnrealEditor not found: $editor"
    print -u2 "Set UE_ROOT to the Unreal Engine installation directory."
    exit 2
fi

for bot_count in 1 2 3; do
    print "Running Catan bot E2E with $((bot_count + 1)) players..."
    "$editor" "$project_dir/Catan.uproject" -game -nullrhi -unattended -nosound \
        -CatanAutoBots="$bot_count" -CatanPlayerName=E2E -CatanBotAutoplay \
        -CatanBotMaxActions=12000 -log
    result="$(grep 'CATAN_BOT_E2E \(PASS\|FAIL\)' "$log_file" | tail -1 || true)"
    if [[ "$result" != *"CATAN_BOT_E2E PASS"* ]]; then
        print -u2 "Bot E2E failed for $((bot_count + 1)) players: ${result:-no result marker}"
        exit 1
    fi
    print "$result"
done

print "All Catan bot E2E scenarios passed."
