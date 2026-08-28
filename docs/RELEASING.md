# Release pipeline

`VERSION` is the single release version. It must match Unreal
`ProjectVersion` and Android `VersionDisplayName`; Android `StoreVersion` is a
separate monotonically increasing integer. Validate all four values from the
repository root:

```sh
scripts/validate_release_version.sh
```

## Dedicated server packages

Create a tested package for the current macOS or Linux host:

```sh
scripts/package_dedicated_release.sh build-release dist/release
```

The command builds in Release mode, runs CTest and the real TCP smoke, verifies
both CLI `--version` outputs, installs only the server, probe and documentation,
then writes a platform/architecture `.tar.gz` and adjacent `.sha256` file.

Pushing an annotated tag matching `v$(cat VERSION)` runs the same process on
GitHub-hosted Linux and macOS runners and publishes both archives plus a merged
`SHA256SUMS` file to the GitHub Release. `workflow_dispatch` performs a dry run
and retains the packages as workflow artifacts without publishing a release.

## Android Shipping APK

Unreal Engine is intentionally not downloaded by a public GitHub runner. Build
the Shipping APK on the validated UE workstation:

```sh
UE_ROOT="/Users/Shared/Epic Games/UE_5.8" \
  unreal/Catan/Scripts/build_android_release.sh dist/release
```

The script performs a full Shipping cook/package and writes a versioned APK and
checksum. Validate that exact Shipping build on an emulator or attached device
without relying on Development-only UE log markers:

```sh
unreal/Catan/Scripts/run_android_release_smoke.sh \
  dist/release/catan-"$(cat VERSION)"-android-arm64.apk
```

After inspecting the release created by the tag, upload both files with:

```sh
gh release upload "v$(cat VERSION)" \
  dist/release/catan-"$(cat VERSION)"-android-arm64.apk \
  dist/release/catan-"$(cat VERSION)"-android-arm64.apk.sha256
```

Never publish development APKs, dedicated state files, player tokens, `Saved/`,
`Intermediate/`, or local signing material.
