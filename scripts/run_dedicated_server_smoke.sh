#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-/tmp/catan-dedicated-smoke-build}"
server="$build_dir/catan-dedicated-server"
probe="$build_dir/catan-dedicated-probe"
log_file="$(mktemp -t catan-dedicated-server.XXXXXX)"
state_dir="$(mktemp -d -t catan-dedicated-state.XXXXXX)"
state_file="$state_dir/server.state"

if [[ ! -x "$server" || ! -x "$probe" ]]; then
  cmake -S "$(cd "$(dirname "$0")/.." && pwd)" -B "$build_dir" \
    -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
  cmake --build "$build_dir" --target catan-dedicated-server catan-dedicated-probe \
    catan_dedicated_server_tests -j 4
fi

cleanup() {
  if [[ -n "${server_pid:-}" ]]; then
    kill "$server_pid" 2>/dev/null || true
    for _ in {1..50}; do
      kill -0 "$server_pid" 2>/dev/null || break
      sleep 0.05
    done
    kill -KILL "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -f "$log_file"
  rm -f "$state_file" "$state_file.tmp"
  rmdir "$state_dir" 2>/dev/null || true
}
trap cleanup EXIT

start_server() {
  : >"$log_file"
  "$server" --bind 127.0.0.1 --port 0 --state-file "$state_file" >"$log_file" 2>&1 &
  server_pid=$!
  port=""
  for _ in {1..100}; do
    port="$(sed -n 's/.* port=\([0-9][0-9]*\).*/\1/p' "$log_file" | head -1)"
    [[ -n "$port" ]] && break
    sleep 0.05
  done
  if [[ -z "$port" ]]; then
    echo "Dedicated server did not become ready" >&2
    sed -n '1,80p' "$log_file" >&2
    exit 1
  fi
}

stop_server() {
  kill -TERM "$server_pid"
  for _ in {1..100}; do
    kill -0 "$server_pid" 2>/dev/null || break
    sleep 0.05
  done
  if kill -0 "$server_pid" 2>/dev/null; then
    echo "Dedicated server did not stop cleanly" >&2
    exit 1
  fi
  wait "$server_pid"
  server_pid=""
}

start_server

request() {
  "$probe" --host 127.0.0.1 --port "$port" --timeout-ms 3000 --request "$1"
}

[[ "$(request PING)" == $'OK\tPONG' ]]

first_request=$'CREATE2\tsmoke-create-first-0001\t416c696365\t466972737420726f6f6d'
second_request=$'CREATE2\tsmoke-create-second-001\t4361726f6c\t5365636f6e6420726f6f6d'
first="$(request "$first_request")"
second="$(request "$second_request")"
IFS=$'\t' read -r _ _ first_lobby first_host _ <<<"$first"
IFS=$'\t' read -r _ _ second_lobby second_host _ <<<"$second"
[[ "$first_lobby" != "$second_lobby" ]]

first_join_request=$'JOIN2\tsmoke-join-first-000001\t'"$first_lobby"$'\t426f62'
second_join_request=$'JOIN2\tsmoke-join-second-00001\t'"$second_lobby"$'\t44617665'
first_join="$(request "$first_join_request")"
second_join="$(request "$second_join_request")"
IFS=$'\t' read -r _ _ _ first_guest _ <<<"$first_join"
IFS=$'\t' read -r _ _ _ second_guest _ <<<"$second_join"

if request $'SNAPSHOT\t'"$first_lobby"$'\t'"$second_host" >/dev/null 2>&1; then
  echo "A token from another lobby was incorrectly accepted" >&2
  exit 1
fi

ready_index=0
for item in "$first_lobby:$first_host" "$first_lobby:$first_guest" \
            "$second_lobby:$second_host" "$second_lobby:$second_guest"; do
  lobby="${item%%:*}"; player="${item#*:}"
  (( ready_index += 1 ))
  request $'READY2\t'"$lobby"$'\t'"$player"$'\tsmoke-ready-000'"$ready_index"$'\t1' >/dev/null
done
first_start_request=$'START2\t'"$first_lobby"$'\t'"$first_host"$'\tsmoke-start-first-0001'
second_start_request=$'START2\t'"$second_lobby"$'\t'"$second_host"$'\tsmoke-start-second-001'
first_start="$(request "$first_start_request")"
second_start="$(request "$second_start_request")"

first_snapshot="$(request $'SNAPSHOT\t'"$first_lobby"$'\t'"$first_guest")"
second_snapshot="$(request $'SNAPSHOT\t'"$second_lobby"$'\t'"$second_guest")"
[[ "$first_snapshot" == $'OK\tSNAPSHOT\t'* ]]
[[ "$second_snapshot" == $'OK\tSNAPSHOT\t'* ]]
[[ "$first_snapshot" != "$second_snapshot" ]]

stop_server
[[ -s "$state_file" ]]
start_server
grep -q 'lobbies=2' "$log_file"

[[ "$(request "$first_request")" == "$first" ]]
[[ "$(request "$second_request")" == "$second" ]]
[[ "$(request "$first_join_request")" == "$first_join" ]]
[[ "$(request "$second_join_request")" == "$second_join" ]]
[[ "$(request "$first_start_request")" == "$first_start" ]]
[[ "$(request "$second_start_request")" == "$second_start" ]]

restored_first="$(request $'SNAPSHOT\t'"$first_lobby"$'\t'"$first_guest")"
restored_second="$(request $'SNAPSHOT\t'"$second_lobby"$'\t'"$second_host")"
[[ "$restored_first" == $'OK\tSNAPSHOT\t'* ]]
[[ "$restored_second" == $'OK\tSNAPSHOT\t'* ]]
if request $'SNAPSHOT\t'"$first_lobby"$'\t'"$second_guest" >/dev/null 2>&1; then
  echo "Cross-lobby token was accepted after restart" >&2
  exit 1
fi

stop_server
permissions="$(stat -f '%Lp' "$state_file" 2>/dev/null || stat -c '%a' "$state_file")"
[[ "$permissions" == "600" ]]
printf 'corrupt' >>"$state_file"
if "$server" --bind 127.0.0.1 --port 0 --state-file "$state_file" >"$log_file" 2>&1; then
  echo "Dedicated server accepted a corrupted state file" >&2
  exit 1
fi
grep -Eq 'trailing data|Could not restore' "$log_file"

ctest --test-dir "$build_dir" -R catan_dedicated_server_tests --output-on-failure
echo "Dedicated server persistence smoke passed (two restored lobbies, private file, corruption rejected)."
