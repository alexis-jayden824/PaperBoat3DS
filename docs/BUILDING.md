# Building PaperBoat3DS

## Current scope

This branch contains the native application shell, the completed M5 compilation
gate, M6 memory guardrails, the completed M7 legal host-side asset workflow,
the host-tested M8 native input layer, the M9 citro3d renderer foundation,
the M10 pinned-libultraship graphics adapter, the M11 bounded legal-archive
frame path, and the M12 title/file-select checkpoint. M13 adds a bounded
playable overworld slice using authentic `mac_00` and `mac_01` geometry,
textures, collision, Mario sprites, camera, representative entities/scripts,
pause, and bidirectional transitions. It is not yet a complete PaperBoat port.
A successful package build proves that one ARM11 ELF can produce `.3dsx`,
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

Run the portable renderer contract tests with:

```sh
sh tools/test_renderer_contract.sh
```

The 3DS build job exposes the same check as `make m9-renderer-test`. The normal
build discovers `source/*.v.pica`, assembles it with Picasso, converts the
resulting `.shbin` into a linked object/header pair, and then compiles the
citro3d backend. Generated shader files stay under `build/`.

Run the portable M10 bridge plus the exact C++ interface check with:

```sh
make fetch-upstream
make m10-graphics-test
```

The C++ check includes the `GfxRenderingAPI` header from the exact locked
libultraship checkout. The normal build therefore fetches/verifies the pinned
sources before compiling; no upstream source is vendored into this repository.

Run the M11 O2R scanner, stored/raw-deflate extraction, CRC, resource parser,
CI8/RGBA16 decoder, padding, failure, and memory-release checks with:

```sh
make fetch-upstream
make m11-frame-test
```

The test creates synthetic archives only. The ARM11 build compiles the bounded
inflate subset from the exact Torch checkout pinned in
`upstream/PAPERBOAT.lock`; it does not depend on a mutable system zlib package.

Run the M12 RGBA32/IA8 texture decoder, title-asset loader, memory-release,
prompt timing, and title/file-select transition checks with:

```sh
make fetch-upstream
make m12-flow-test
make m12-layout-test
```

These tests use generated public fixtures. A private local path can be passed
directly to `tools/test_title_flow.sh` for owner-generated `pm64.o2r`
validation; never add that path to CI.

The M12.1 layout test is asset-free. It locks the New 3DS XL/LL 400x240
presentation to a centered 320x240 safe canvas with equal 40-pixel pillars and
checks every title rectangle against the upstream Paper Mario coordinates.

Run the public M13 O2R fixture, complete map-resource translation, texture,
collision/gameplay, transition, release, retry, and pause-flow checks with:

```sh
make fetch-upstream
make m13-world-test
make m13-core-check
```

`m13-core-check` cross-compiles the pinned PaperBoat world mode, demo entry,
map transitions, pause mode, camera math, collision, world table, and pause
implementation for ARM11. For an owner-only acceptance run, pass the generated
private archive as a second argument to both structural and playable-scene
tests:

```sh
sh tools/test_world_boot.sh build/m13-private /path/to/pm64.o2r
sh tools/test_world_scene.sh build/m13-scene-private /path/to/pm64.o2r
```

Never commit or upload that private archive. The pinned archive reports 2,040
native triangles and 41 map textures for `mac_00`, and 2,210 triangles and 48
textures for `mac_01`. The private playable-scene suite covers both maps and
both return-entry guards.

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

M11 through M13 read these exact SD paths at runtime:

- `sdmc:/3ds/PaperBoat3DS/paperboat.o2r`
- `sdmc:/3ds/PaperBoat3DS/pm64.o2r`

Only `pm64.o2r` supplies the current title-screen textures. Neither archive is
embedded in public packages or CI artifacts.

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
