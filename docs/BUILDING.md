# Building PaperBoat3DS

## Current scope

This branch contains an M0 bootstrap target, not a playable PaperBoat port. A successful package build proves that one ARM11 ELF can produce `.3dsx`, `.3ds`, and `.cia` artifacts.

## Prerequisites

- A current devkitPro installation
- The `3ds-dev` package group
- `DEVKITPRO` and `DEVKITARM` exported by the devkitPro environment
- GNU Make

The Makefile is derived from the maintained `devkitPro/3ds-examples` structure rather than a desktop CMake target.

## Build

```sh
make
```

Build the Folium/emulator and optional CFW-installable packages with:

```sh
make packages
```

Expected outputs after a successful build:

- `PaperBoat3DS.elf`
- `PaperBoat3DS.3dsx`
- `PaperBoat3DS.3ds`
- `PaperBoat3DS.cia`
- `PaperBoat3DS.smdh`
- `PaperBoat3DS.map`

Clean generated output with:

```sh
make clean
```

## Validation rules

1. Do not claim the expanded M0 packaging contract complete until all three formats succeed in a real devkitARM environment.
2. Do not claim M1 complete until the `.3dsx` boots and exits cleanly through START.
3. Record toolchain versions and the complete compiler/linker error when a build fails.
4. Hardware verification is performed by the project owner on a real Nintendo 3DS.

The `.cia` uses the provisional homebrew unique ID `0xF0B42`. Install it only on a CFW-enabled test console. The ID may change before save compatibility is frozen.

## Asset policy

ROMs, extracted Nintendo assets, generated `.o2r` packages, encryption keys, and proprietary files must never be committed. Later asset-generation milestones run on the user's PC against a legally obtained game copy.
