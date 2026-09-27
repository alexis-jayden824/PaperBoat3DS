# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0** and currently at **M4**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M4

M0 is the Homebrew shell. M1 pins the toolchain. M2 pins PaperBoat 1.0.1.
M3 is the C platform boundary. M4 adds memory pressure, a RAM log ring,
assertions, crash breadcrumbs, runtime status, and New 3DS detection
without enabling New 3DS extras.

PaperBoat-facing code must not include `3ds.h`. Audio stays deferred until
M14. SDMC I/O is M9. Full HID mapping is M8. citro3d is M11.

**M0 acceptance still needs a Folium/hardware boot.**

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Host contracts (no 3DS toolchain)
sh tools/test_m1.sh
sh tools/test_m2.sh
sh tools/test_bootstrap.sh
sh tools/test_platform.sh
sh tools/test_m4.sh

# Native .3dsx (requires DEVKITARM)
make

# Folium/emulator .3ds (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). See
[docs/M2.md](docs/M2.md), [docs/M3.md](docs/M3.md), and
[docs/M4.md](docs/M4.md). No later milestone will be marked complete
without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
