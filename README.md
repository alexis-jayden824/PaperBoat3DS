# PaperBoat3DS Refolded

A clean native Nintendo 3DS port of PaperBoat / Paper Mario 64, started
from milestone **M0**.

This is a from-scratch rebuild. It is **not** a continuation of the previous
PaperBoat3DS renderer/runtime tree. PaperBoat remains the behavioral
reference; this repository does not rebuild Paper Mario, does not fake
gameplay geometry, and does not pretend the game is running until that
milestone is actually reached.

## Current status: M0

M0 is a legitimate Homebrew application shell:

- ARM11 / libctru lifecycle
- top and bottom framebuffer init
- console logging on the bottom screen
- solid-color top screen (not Paper Mario)
- START requests a clean shutdown
- APT suspend/sleep/restore hooks

**Acceptance:** the `.3dsx` boots on a 3DS-compatible runtime and exits
cleanly without claiming PaperBoat is running.

## Build

See [docs/BUILDING.md](docs/BUILDING.md).

```sh
# Host contract (no 3DS toolchain)
sh tools/test_bootstrap.sh

# Native .3dsx (requires DEVKITARM)
make
```

## Roadmap

The binding milestone list is [docs/ROADMAP.md](docs/ROADMAP.md). M1+ is not
started. No later milestone will be marked complete without evidence.

## Legal

This repository must not contain Nintendo ROMs, copyrighted game assets,
extracted `.o2r` packages, encryption keys, or other proprietary material.
