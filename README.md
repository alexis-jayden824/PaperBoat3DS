# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0** and currently at **M6**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M6

M5 links a two-file PaperBoat slice. M6 is the C compatibility surface:
resource lookup, CVars, controller, gfx submit, and audio frames are
**explicitly deferred**. Logging is ready (M4). Desktop libultraship is not
linked. CI uploads `.3dsx`, `.3ds`, and `.cia`.

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

# Fetch pinned PaperBoat (gitignored cache), then native .3dsx
make fetch
make

# Folium/emulator .3ds and FBI-installable .cia (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). See
[docs/M5.md](docs/M5.md), and [docs/M6.md](docs/M6.md). No later milestone
will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
