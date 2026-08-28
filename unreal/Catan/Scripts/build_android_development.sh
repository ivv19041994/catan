#!/bin/bash

set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ue_root="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
output_dir="${1:-$project_dir/Builds/AndroidPerformance}"
run_uat="$ue_root/Engine/Build/BatchFiles/RunUAT.sh"

[[ -x "$run_uat" ]] || { echo "RunUAT.sh not found under UE_ROOT=$ue_root" >&2; exit 2; }
temporary_archive="$(mktemp -d "${TMPDIR:-/tmp}/catan-android-development.XXXXXX")"
trap 'rm -rf "$temporary_archive"' EXIT
"$run_uat" BuildCookRun -project="$project_dir/Catan.uproject" -nop4 \
  -platform=Android -clientconfig=Development -build -cook -stage -pak -package \
  -archive -archivedirectory="$temporary_archive" -utf8output

source_apk="$temporary_archive/Android/Catan-arm64.apk"
[[ -f "$source_apk" ]] || { echo "Development APK was not produced: $source_apk" >&2; exit 1; }
mkdir -p "$output_dir"
cp "$source_apk" "$output_dir/Catan-arm64.apk"
echo "CATAN_ANDROID_DEVELOPMENT_OK apk=$output_dir/Catan-arm64.apk"
