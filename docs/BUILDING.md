# Building PaperBoat3DS Refolded

## Current scope

M0 provides the native Homebrew shell. M1 pins the toolchain and CI. M2 pins
PaperBoat 1.0.1 / libultraship / Torch and classifies what may later enter the
ARM11 binary.

The application still does not load PaperBoat, libultraship, or game assets.

## Pinned CI environment (authoritative)

Recorded in `toolchain/TOOLCHAINS.lock`:

- Image: `devkitpro/devkitarm@sha256:15b79ce75822c289538d8153da5fa7aafe5e6adc32ad8a575a197beca0f0761b`
- makerom: Project_CTR commit `e8f5f529c54ff9b22a2491a480ffa69206bf7b19`
- Packages: `3ds-dev`
- Arch: `armv6k` / `mpcore` / hard-float

Do not build release artifacts against an unpinned `:latest` tag.

## Local `.3dsx`

- A current [devkitPro](https://devkitpro.org/) installation
- The `3ds-dev` package group
- `DEVKITPRO` and `DEVKITARM` exported
- GNU Make

```sh
sudo dkp-pacman -S 3ds-dev
make PB3DS_BUILD_SHA="$(git rev-parse HEAD)" \
     PB3DS_BUILD_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"
```

Output: `PaperBoat3DS-Refolded.3dsx`

## Local `.3ds` (Folium / emulator)

```sh
make packages
```

`packages` requires `makerom` on `PATH`. CI builds the pinned Project_CTR
binary. Expected outputs:

- `PaperBoat3DS-Refolded.elf`
- `PaperBoat3DS-Refolded.3dsx`
- `PaperBoat3DS-Refolded.3ds`
- `PaperBoat3DS-Refolded.smdh`
- `build/build-info.txt` (CI)

## Host contracts (no 3DS toolchain)

```sh
sh tools/test_m1.sh
sh tools/test_m2.sh
sh tools/test_bootstrap.sh
```

`test_m1.sh` checks that the GitHub workflow still matches
`toolchain/TOOLCHAINS.lock`. `test_m2.sh` checks `upstream/PAPERBOAT.lock`
against `docs/M2.md`.

Set `PB3DS_VERIFY_UPSTREAM=1` to also HTTP-check that the three commits exist
on GitHub.

## CI

`.github/workflows/3ds-build.yml` runs both host contracts, cross-compiles
inside the pinned digest, packages the CCI, and uploads `.3dsx`, `.3ds`,
`.elf`, `.smdh`, and `build-info.txt`.
