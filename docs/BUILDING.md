# Building PaperBoat3DS

## Current scope

This branch contains the native application shell, the completed M5 compilation
gate, M6 memory guardrails, the completed M7 legal host-side asset workflow,
and the host-tested M8 native input layer; it is not yet a playable PaperBoat
port. A successful package build proves that one ARM11 ELF can produce `.3dsx`,
`.3ds`, and `.cia` artifacts.

## Prerequisites

- A current devkitPro installation
- The `3ds-dev` package group
- `DEVKITPRO` and `DEVKITARM` exported by the devkitPro environment
- GNU Make

The Makefile is derived from the maintained `devkitPro/3ds-examples` structure rather than a desktop CMake target.

## Pinned upstream sources

PaperBoat, libultraship, and Torch are fetched at the immutable commits recorded
in `upstream/PAPERBOAT.lock`. They are stored under `.cache/upstream` and are not
vendored into this repository.

```sh
make fetch-upstream
```

The fetch is safe to repeat. It verifies that each checkout resolves to the
locked commit and fails rather than silently building a moving branch.

Cross-compile the M5 representative upstream source matrix without linking it into the
application shell:

```sh
make m5-core-check
```

CI runs both operations on every branch build.

Run the native input policy tests on any host with a C11 compiler:

```sh
sh tools/test_input_backend.sh
```

The 3DS build job also exposes the same check as `make m8-input-test`.

## Host-side asset preparation

M7 provides one cross-platform Python entry point that verifies a legally
dumped ROM, builds pinned Torch, creates `paperboat.o2r` and `pm64.o2r`, and
validates both archives:

```sh
python3 tools/pb3ds_assets.py prepare /path/to/baserom.us.z64
```

Do not upload the ROM or generated game archive to GitHub Actions. See
`docs/M7_ASSET_PIPELINE.md` for prerequisites, staging, validation, Windows
usage, and the exact legal boundary.

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

ROMs, extracted Nintendo assets, generated `.o2r` packages, encryption keys,
and proprietary files must never be committed. The M7 workflow runs locally
against a legally obtained copy and only CI-tests non-proprietary and synthetic
inputs.
