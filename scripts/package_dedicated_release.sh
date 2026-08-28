#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build-release}"
output_dir="${2:-$repo_root/dist/release}"
version="$(tr -d '[:space:]' < "$repo_root/VERSION")"

"$repo_root/scripts/validate_release_version.sh"

case "$(uname -s)" in
  Darwin) platform="macos" ;;
  Linux) platform="linux" ;;
  *) echo "Dedicated release packaging supports only macOS and Linux" >&2; exit 2 ;;
esac
architecture="$(uname -m)"
package_name="catan-dedicated-server-$version-$platform-$architecture"
archive="$output_dir/$package_name.tar.gz"
checksum="$archive.sha256"

mkdir -p "$output_dir"
if [[ -e "$archive" || -e "$checksum" ]]; then
  echo "Release artifact already exists: $archive" >&2
  exit 2
fi

cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
  -DCATAN_BUILD_CONSOLE=OFF -DCATAN_BUILD_TESTS=ON
cmake --build "$build_dir" --parallel
ctest --test-dir "$build_dir" --output-on-failure
"$repo_root/scripts/run_dedicated_server_smoke.sh" "$build_dir"

server_version="$($build_dir/catan-dedicated-server --version)"
probe_version="$($build_dir/catan-dedicated-probe --version)"
[[ "$server_version" == "catan-dedicated-server $version" ]] \
  || { echo "Unexpected server version: $server_version" >&2; exit 2; }
[[ "$probe_version" == "catan-dedicated-probe $version" ]] \
  || { echo "Unexpected probe version: $probe_version" >&2; exit 2; }

temporary_root="$(mktemp -d "${TMPDIR:-/tmp}/catan-release.XXXXXX")"
trap 'rm -rf "$temporary_root"' EXIT
install_root="$temporary_root/$package_name"
cmake --install "$build_dir" --prefix "$install_root"
tar -czf "$archive" -C "$temporary_root" "$package_name"

if command -v sha256sum >/dev/null 2>&1; then
  (cd "$output_dir" && sha256sum "$(basename "$archive")") > "$checksum"
else
  (cd "$output_dir" && shasum -a 256 "$(basename "$archive")") > "$checksum"
fi

echo "CATAN_RELEASE_PACKAGE_OK archive=$archive checksum=$checksum"
