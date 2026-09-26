# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M1

M0 is the Homebrew shell. M1 pins the toolchain and CI:

- ARM11 / libctru lifecycle
- top and bottom framebuffer init
- console logging on the bottom screen
- solid-color top screen (not Paper Mario)
- START requests a clean shutdown
- APT suspend/sleep/restore hooks
- pinned Docker digest + makerom SHA
- `.3dsx` and `.3ds` CI artifacts with `build-info.txt`

**M0 acceptance still needs a Folium/hardware boot.** M1 is the build contract
around that shell.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Host contracts (no 3DS toolchain)
sh tools/test_m1.sh
sh tools/test_bootstrap.sh

# Native .3dsx (requires DEVKITARM)
make

# Folium/emulator .3ds (requires makerom)
make packages
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). M1+ is not
started. No later milestone will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
