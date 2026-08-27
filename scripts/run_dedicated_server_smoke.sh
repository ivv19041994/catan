#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-/tmp/catan-dedicated-smoke-build}"
server="$build_dir/catan-dedicated-server"
probe="$build_dir/catan-dedicated-probe"
log_file="$(mktemp -t catan-dedicated-server.XXXXXX)"

if [[ ! -x "$server" || ! -x "$probe" ]]; then
  cmake -S "$(cd "$(dirname "$0")/.." && pwd)" -B "$build_dir" \
    -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
  cmake --build "$build_dir" --target catan-dedicated-server catan-dedicated-probe \
    catan_dedicated_server_tests -j 4
fi

"$server" --bind 127.0.0.1 --port 0 >"$log_file" 2>&1 &
server_pid=$!
cleanup() {
  kill "$server_pid" 2>/dev/null || true
  for _ in {1..50}; do
    kill -0 "$server_pid" 2>/dev/null || break
    sleep 0.05
  done
  kill -KILL "$server_pid" 2>/dev/null || true
  wait "$server_pid" 2>/dev/null || true
  rm -f "$log_file"
}
trap cleanup EXIT

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

request() {
  "$probe" --host 127.0.0.1 --port "$port" --timeout-ms 3000 --request "$1"
}

[[ "$(request PING)" == $'OK\tPONG' ]]

first="$(request $'CREATE\t416c696365\t466972737420726f6f6d')"
second="$(request $'CREATE\t4361726f6c\t5365636f6e6420726f6f6d')"
IFS=$'\t' read -r _ _ first_lobby first_host _ <<<"$first"
IFS=$'\t' read -r _ _ second_lobby second_host _ <<<"$second"
[[ "$first_lobby" != "$second_lobby" ]]

first_join="$(request $'JOIN\t'"$first_lobby"$'\t426f62')"
second_join="$(request $'JOIN\t'"$second_lobby"$'\t44617665')"
IFS=$'\t' read -r _ _ _ first_guest _ <<<"$first_join"
IFS=$'\t' read -r _ _ _ second_guest _ <<<"$second_join"

if request $'SNAPSHOT\t'"$first_lobby"$'\t'"$second_host" >/dev/null 2>&1; then
  echo "A token from another lobby was incorrectly accepted" >&2
  exit 1
fi

for item in "$first_lobby:$first_host" "$first_lobby:$first_guest" \
            "$second_lobby:$second_host" "$second_lobby:$second_guest"; do
  lobby="${item%%:*}"; player="${item#*:}"
  request $'READY\t'"$lobby"$'\t'"$player"$'\t1' >/dev/null
done
request $'START\t'"$first_lobby"$'\t'"$first_host" >/dev/null
request $'START\t'"$second_lobby"$'\t'"$second_host" >/dev/null

first_snapshot="$(request $'SNAPSHOT\t'"$first_lobby"$'\t'"$first_guest")"
second_snapshot="$(request $'SNAPSHOT\t'"$second_lobby"$'\t'"$second_guest")"
[[ "$first_snapshot" == $'OK\tSNAPSHOT\t'* ]]
[[ "$second_snapshot" == $'OK\tSNAPSHOT\t'* ]]
[[ "$first_snapshot" != "$second_snapshot" ]]

ctest --test-dir "$build_dir" -R catan_dedicated_server_tests --output-on-failure
echo "Dedicated server smoke passed on 127.0.0.1:$port (two independent lobbies)."
