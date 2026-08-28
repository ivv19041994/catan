#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
version="$(tr -d '[:space:]' < "$repo_root/VERSION")"
tag="${1:-}"

if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "VERSION is not a semantic version: $version" >&2
  exit 2
fi
if [[ -n "$tag" && "$tag" != "v$version" ]]; then
  echo "Release tag $tag does not match VERSION v$version" >&2
  exit 2
fi

grep -Fqx "ProjectVersion=$version" "$repo_root/unreal/Catan/Config/DefaultGame.ini" \
  || { echo "UE ProjectVersion does not match VERSION $version" >&2; exit 2; }
grep -Fqx "VersionDisplayName=$version" "$repo_root/unreal/Catan/Config/DefaultEngine.ini" \
  || { echo "Android VersionDisplayName does not match VERSION $version" >&2; exit 2; }

store_version="$(sed -n 's/^StoreVersion=//p' "$repo_root/unreal/Catan/Config/DefaultEngine.ini" | head -n 1)"
if [[ ! "$store_version" =~ ^[1-9][0-9]*$ ]]; then
  echo "Android StoreVersion must be a positive integer" >&2
  exit 2
fi

echo "CATAN_RELEASE_VERSION_OK version=$version androidStoreVersion=$store_version"
