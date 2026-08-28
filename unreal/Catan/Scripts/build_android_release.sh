#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_root="$(cd "$project_dir/../.." && pwd)"
version="$(tr -d '[:space:]' < "$repo_root/VERSION")"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
output_dir="${1:-$repo_root/dist/release}"
run_uat="$ue_root/Engine/Build/BatchFiles/RunUAT.sh"

"$repo_root/scripts/validate_release_version.sh"
[[ -x "$run_uat" ]] || { echo "RunUAT.sh not found under UE_ROOT=$ue_root" >&2; exit 2; }

temporary_archive="$(mktemp -d "${TMPDIR:-/tmp}/catan-android-release.XXXXXX")"
trap 'rm -rf "$temporary_archive"' EXIT
"$run_uat" BuildCookRun -project="$project_dir/Catan.uproject" -nop4 \
  -platform=Android -clientconfig=Shipping -build -cook -stage -pak -package \
  -archive -archivedirectory="$temporary_archive" -utf8output

source_apk="$temporary_archive/Android/Catan-Android-Shipping-arm64.apk"
[[ -f "$source_apk" ]] || { echo "Shipping APK was not produced: $source_apk" >&2; exit 1; }
mkdir -p "$output_dir"
artifact="$output_dir/catan-$version-android-arm64.apk"
checksum="$artifact.sha256"
[[ ! -e "$artifact" && ! -e "$checksum" ]] \
  || { echo "Release artifact already exists: $artifact" >&2; exit 2; }
cp "$source_apk" "$artifact"
if command -v sha256sum >/dev/null 2>&1; then
  (cd "$output_dir" && sha256sum "$(basename "$artifact")") > "$checksum"
else
  (cd "$output_dir" && shasum -a 256 "$(basename "$artifact")") > "$checksum"
fi
echo "CATAN_ANDROID_RELEASE_OK apk=$artifact checksum=$checksum"
