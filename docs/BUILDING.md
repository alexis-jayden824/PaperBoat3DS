# Building PaperBoat3DS Refolded

## Current scope

M0 provides the native Homebrew shell. M1 pins the toolchain and CI. M2 pins
PaperBoat 1.0.1 / libultraship / Torch. M3 wraps libctru behind
`include/pb3ds/platform.h`. M4 adds diagnostics. M5 fetches PaperBoat 1.0.1
and compiles a two-file slice (`libc_compat`, `decode_yay0`). M6 is the C
engine compatibility layer. M7 is the host-only legal asset wrapper (Torch
never runs on ARM11). Packages include `.3dsx`, `.3ds`, and `.cia`.

The application still does not run `boot_main` or load game assets.

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

## Local `.3ds` / `.cia` (Folium / FBI)

```sh
make packages
```

`packages` requires `makerom` on `PATH`. CI builds the pinned Project_CTR
binary. Expected outputs:

- `PaperBoat3DS-Refolded.elf`
- `PaperBoat3DS-Refolded.3dsx`
- `PaperBoat3DS-Refolded.3ds`
- `PaperBoat3DS-Refolded.cia`
- `PaperBoat3DS-Refolded.smdh`
- `build/build-info.txt` (CI)

## Host contracts (no 3DS toolchain)

```sh
sh tools/test_m1.sh
sh tools/test_m2.sh
sh tools/test_bootstrap.sh
sh tools/test_platform.sh
sh tools/test_m4.sh
sh tools/test_m5.sh
sh tools/test_m6.sh
sh tools/test_m7.sh
```

`test_m5.sh` fetches the PaperBoat pin, checks CMake exclusions, and
compiles the slice on the host. `make fetch` is required before a 3DS
link that includes the real `_Printf` / `decode_yay0` objects.

`test_m1.sh` checks that the GitHub workflow still matches
`toolchain/TOOLCHAINS.lock`. `test_m2.sh` checks `upstream/PAPERBOAT.lock`
against `docs/M2.md`. `test_platform.sh` compiles the M3 host stubs and
asserts that PaperBoat-facing sources do not include `3ds.h`.
`test_m4.sh` covers memory pressure, asserts, breadcrumbs, and New 3DS
detection-without-enable.

`test_m7.sh` checks `upstream/ASSETS.lock`, refuses a fake ROM, and fetches
pinned Torch-LH into `.cache/` (never linked). `make fetch` still fetches
PaperBoat only; use `make fetch-torch` or `sh tools/prepare_assets.sh` on
a PC that already has a legal US dump.

Set `PB3DS_VERIFY_UPSTREAM=1` to also HTTP-check that the three commits exist
on GitHub.

## CI

`.github/workflows/3ds-build.yml` runs both host contracts, cross-compiles
inside the pinned digest, packages CCI and CIA, and uploads `.3dsx`, `.3ds`,
`.cia`, `.elf`, `.smdh`, and `build-info.txt`.
