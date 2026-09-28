# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0** and currently at **M13**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M13 playtest candidate

The normal CI `.3dsx` now links the pinned PaperBoat 1.0.1 title,
file-select, pause, and bounded Toad Town runtime closure. With a validated
`pm64.o2r` on SD it enters a staged, non-blocking initialization sequence,
activates PaperBoat's real title mode, accepts its real file menu, and routes a
selected file into `mac_00`/`mac_01`. The runtime calls the upstream
`Graphics_ThreadUpdate` at 30 Hz and submits its Fast3D display lists to the
PICA200 renderer. `boot_main` is linked as a closure check but is deliberately
not called because its NuSystem loop never returns to `aptMainLoop`.

Resource blobs, sprites/palettes, vertices, matrices, lights, textures, and
display lists are decoded from the pinned Torch archive formats. OTR names and
CRC64 references resolve through a lifetime-stable cache. Missing or malformed
assets, unsupported display-list commands, and unsupported game modes stop the
runtime with a named diagnostic instead of rendering a fake scene as gameplay.

Audio is intentionally silent for M13. Save-file flash is persistent at
`sdmc:/3ds/PaperBoat3DS/m13-runtime.sav`. SELECT remains reserved for the later
bottom-screen menu. L+R+START exits cleanly.

CI and host verification are required but do not constitute New 3DS XL proof.
M13 remains a playtest candidate until the hardware checklist in
[docs/M13.md](docs/M13.md) is returned with visible title → file select →
overworld evidence. Nintendo assets are never stored in git or CI.

## Download (iPhone)

The GitHub iOS app cannot download Actions artifacts (the long SHA-named
row under a workflow run). Use **Releases** instead:

https://github.com/alexis-jayden824/PaperBoat3DS/releases/latest

Download **`pb3ds.zip`**, open it in Files, then share `.3ds` into Folium
or `.cia` into FBI. Direct file:

https://github.com/alexis-jayden824/PaperBoat3DS/releases/latest/download/pb3ds.zip

If Safari asks you to sign in, that is expected for this private repo.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Maintained host contracts (no 3DS toolchain)
sh tools/fetch_upstream.sh
sh tools/test_asset_pipeline.sh
sh tools/test_memory_policy.sh
sh tools/test_input_backend.sh
sh tools/test_renderer_contract.sh
sh tools/test_gfx_bridge.sh
sh tools/test_gfx_api_contract.sh
sh tools/test_first_frame.sh
sh tools/test_title_flow.sh
sh tools/test_title_layout.sh
sh tools/test_runtime_flash.sh
sh tools/test_runtime_startup.sh
sh tools/test_runtime_resources.sh
sh tools/test_world_boot.sh
sh tools/test_world_scene.sh

# Build the full M13 .3dsx with the already-fetched pinned tree
make

# Folium/emulator .3ds and FBI-installable .cia (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). See
[docs/M5.md](docs/M5.md), [docs/M6.md](docs/M6.md), [docs/M7.md](docs/M7.md), and
[docs/M8.md](docs/M8.md), [docs/M9.md](docs/M9.md),
[docs/M10.md](docs/M10.md), [docs/M11.md](docs/M11.md),
[docs/M12.md](docs/M12.md), and [docs/M13.md](docs/M13.md). No later
milestone will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
