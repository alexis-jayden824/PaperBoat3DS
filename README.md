# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0** and currently at **M3**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M3

M0 is the Homebrew shell. M1 pins the toolchain. M2 pins PaperBoat 1.0.1.
M3 is the C platform boundary (`include/pb3ds/platform.h`): gfx, input, fs,
time, memory, log, audio (deferred), threads (no extra workers), lifecycle.

PaperBoat-facing code must not include `3ds.h`. Audio stays deferred until
M14. SDMC I/O is M9. Full HID mapping is M8. citro3d is M11.

**M0 acceptance still needs a Folium/hardware boot.** M1 is the build contract.
M2 is the dependency contract. M3 is the platform API contract.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Host contracts (no 3DS toolchain)
sh tools/test_m1.sh
sh tools/test_m2.sh
sh tools/test_bootstrap.sh
sh tools/test_platform.sh

# Native .3dsx (requires DEVKITARM)
make

# Folium/emulator .3ds (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). The M2
audit is [docs/M2.md](docs/M2.md). The M3 API is [docs/M3.md](docs/M3.md).
No later milestone will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
