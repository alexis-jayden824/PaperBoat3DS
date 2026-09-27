# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0** and currently at **M11**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M11

M11 is the graphics backend foundation: citro3d command submission, N64
clip + invertY, 320×240 source with 40 px pillars, texture upload, and a
depth render-target. It does not interpret Fast3D and does not draw
Paper Mario. CI uploads `.3dsx`, `.3ds`, and `.cia` only.

M10 is the APT-side 30 Hz loop: monotonic time that freezes across HOME
sleep, no extra threads, no desktop window pump. `boot_main` is still not
linked.

M9 is SDMC resource I/O: paths under `sdmc:/3ds/PaperBoat3DS/`, 16-byte
aligned lookups, STORE `.o2r` entries, lifetime until shutdown. Nintendo
archives are not in git or CI.

**M0 acceptance still needs a Folium/hardware boot.** This is not Paper Mario.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Host contracts (no 3DS toolchain)
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

# Fetch pinned PaperBoat (gitignored cache), then native .3dsx
make fetch
make

# Folium/emulator .3ds and FBI-installable .cia (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). See
[docs/M5.md](docs/M5.md), [docs/M6.md](docs/M6.md), [docs/M7.md](docs/M7.md), and
[docs/M8.md](docs/M8.md), [docs/M9.md](docs/M9.md),
[docs/M10.md](docs/M10.md), and [docs/M11.md](docs/M11.md). No later milestone
will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
