# Building PaperBoat3DS Refolded

## Current scope

M0 provides the native Homebrew shell. M1 pins the toolchain and CI. M2 pins
PaperBoat 1.0.1 / libultraship / Torch. M3 wraps libctru behind
`include/pb3ds/platform.h`. M4 adds diagnostics. M5 fetches PaperBoat 1.0.1
and compiles a two-file slice (`libc_compat`, `decode_yay0`). M6 is the C
engine compatibility layer. M7 is the host-only legal asset wrapper (Torch
never runs on ARM11). M8 is native HID with SELECT reserved for M16. M9 is
SDMC resource lookup (STORE zip / register, 16-byte aligned). M10 is the
30 Hz APT game loop (monotonic clock, suspend/resume). M11 is the citro3d
graphics foundation (command submit, clip/invertY/pillars, tex, depth).
M12 binds PaperBoat title-screen OTR names (no raster). M13 is the Fast3D
interpreter, texture/TEV path, and runtime gate (`step_game_loop` when
`PB3DS_GAME_OBJECTS` and both `.o2r` files are present).
Packages include `.3dsx`, `.3ds`, and `.cia`.

The application still does not run `boot_main`. Game `.o2r` files are
optional on SD and are never committed.

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
sh tools/test_m8.sh
sh tools/test_m9.sh
sh tools/test_m10.sh
sh tools/test_m11.sh
sh tools/test_m12.sh
sh tools/test_m13.sh
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

`test_m8.sh` maps HID bits to `OSContPad` and asserts SELECT never reaches
the game pad.

`test_m9.sh` builds a synthetic STORE zip (not a game dump), checks path
sanitizing, alignment, and lookup lifetime.

`test_m10.sh` checks 30 Hz stepping and HOME-pause clock freeze.

`test_m11.sh` checks N64 clip, invertY, 40 px pillars, DL submit without
Fast3D, texture upload rules, and that citro3d stays in `gfx_pica.c`.

`test_m12.sh` binds PaperBoat title OTR names/sizes, upright T, and scene
phases without claiming the logo is drawn.

`test_m13.sh` runs a TRI1 display list, CI decode (including missing TLUT),
scissor clamp, combiner→TEV, and the runtime gate (game objects off).

Set `PB3DS_VERIFY_UPSTREAM=1` to also HTTP-check that the three commits exist
on GitHub.

## CI

`.github/workflows/3ds-build.yml` runs both host contracts, cross-compiles
inside the pinned digest, packages CCI and CIA, and uploads `.3dsx`, `.3ds`,
`.cia`, `.elf`, `.smdh`, and `build-info.txt`.
