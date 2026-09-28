# Building PaperBoat3DS Refolded

## Current scope

M0–M12 established the native shell, pinned dependencies and CI, platform
layer, legal asset tooling, input, storage, timing, and the PICA graphics
foundation. M13 adds the Fast3D/texture/TEV path, staged upstream
initialization, and the bounded title → file select → `mac_00`/`mac_01`
runtime. Packages include `.3dsx`, `.3ds`, and `.cia`.

The normal build links the complete selected M13 PaperBoat closure. It does not
call `boot_main`; a staged initializer preserves the 3DS APT loop and calls the
upstream frame/update path directly. Game `.o2r` files are never committed or
included in CI artifacts.

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
- GNU Make, Git, and Python 3

```sh
sudo dkp-pacman -S 3ds-dev
make fetch-upstream
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

## Legal asset preparation

Supply your own unmodified big-endian US `.z64` dump. The tool accepts only the
SHA-1 pinned in `upstream/ASSET_CONTRACT.json`; it never downloads a ROM.
Python 3, CMake, a C++ compiler, and Git are required on the host.

```sh
python3 tools/pb3ds_assets.py prepare /path/to/papermario.us.z64 \
  --output-directory build/assets --jobs 4
```

This fetches the pinned PaperBoat/libultraship/Torch revisions, builds Torch,
generates `pm64.o2r`, creates `paperboat.o2r` from the pinned port inventory,
and verifies every OTR envelope plus the required gameplay assets. It also
writes `assets-manifest.json` with ROM and archive provenance.

Mount or insert the SD card, then stage without manually renaming files:

```sh
python3 tools/pb3ds_assets.py stage \
  --assets-directory build/assets --sd-root /path/to/sd-card
cp PaperBoat3DS-Refolded.3dsx \
  /path/to/sd-card/3ds/PaperBoat3DS/PaperBoat3DS-Refolded.3dsx
```

The resulting directory must contain:

```text
/3ds/PaperBoat3DS/PaperBoat3DS-Refolded.3dsx
/3ds/PaperBoat3DS/paperboat.o2r
/3ds/PaperBoat3DS/pm64.o2r
/3ds/PaperBoat3DS/assets-manifest.json
```

Do not use archives from a different PaperBoat/Torch release. On-device
startup rejects missing, wrong-version, malformed, or incomplete game assets
and records the exact resource in `PaperBoat3DS.log`.

## Host contracts (no 3DS toolchain)

Fetch the pinned source first, then run the maintained suite:

```sh
sh tools/fetch_upstream.sh
sh tools/test_asset_pipeline.sh
HOST_CC=cc sh tools/test_memory_policy.sh build/host-m6
HOST_CC=cc sh tools/test_input_backend.sh build/host-m8
HOST_CC=cc sh tools/test_renderer_contract.sh build/host-m9
HOST_CC=cc sh tools/test_gfx_bridge.sh build/host-m10-bridge
HOST_CC=cc HOST_CXX=c++ sh tools/test_gfx_api_contract.sh build/host-m10-api
HOST_CC=cc sh tools/test_first_frame.sh build/host-m11
HOST_CC=cc sh tools/test_title_flow.sh build/host-m12-flow
HOST_CC=cc sh tools/test_title_layout.sh build/host-m12-layout
python3 tests/test_m13_runtime_generation.py \
  tools/generate_m13_runtime.py .cache/upstream/PaperBoat
sh tools/test_runtime_startup.sh
HOST_CC=cc sh tools/test_runtime_flash.sh build/host-m13-flash
HOST_CC=cc sh tools/test_runtime_resources.sh build/host-m13-resources
HOST_CC=cc sh tools/test_world_boot.sh build/host-m13-world
HOST_CC=cc sh tools/test_world_scene.sh build/host-m13-scene
```

The asset tests use only synthetic fixtures. Runtime-resource tests exercise
STORE and DEFLATE archives, both envelope byte orders, malformed/missing
resources, sprites and palettes, recursive shape/GBI closure, viewports, OTR
hashes, and pointer lifetime. The graphics
contract covers the Fast3D, texture, combiner, framebuffer, depth, viewport,
and failure paths. The startup contract locks the production staged order,
soft-reset transition, 30 Hz cadence, and shutdown ownership order.

Set `PB3DS_VERIFY_UPSTREAM=1` when fetching to also HTTP-check that the pinned
commits exist on GitHub.

## CI

`.github/workflows/3ds-build.yml` runs the host contracts, cross-compiles the
full selected PaperBoat closure inside the pinned digest, verifies required
symbols and heap alignment in the final ELF, packages CCI/CIA, and uploads an
Actions artifact named `pb3ds` (a single `.zip`). Each push updates the
`ci-latest` GitHub Release so phones can download from Releases; the iOS app
does not download Actions artifacts.

https://github.com/alexis-jayden824/PaperBoat3DS/releases/latest/download/pb3ds.zip

```sh
# Desktop CLI
gh run download -R alexis-jayden824/PaperBoat3DS -n pb3ds
```
