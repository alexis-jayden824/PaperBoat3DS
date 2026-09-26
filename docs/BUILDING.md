# Building PaperBoat3DS Refolded

## Current scope

M0 only: a native 3DS Homebrew shell that initializes graphics, logs on the
bottom screen, paints a solid color on the top screen, and exits cleanly.
It does not load PaperBoat, libultraship, or game assets.

## Prerequisites for `.3dsx`

- A current [devkitPro](https://devkitpro.org/) installation
- The `3ds-dev` package group
- `DEVKITPRO` and `DEVKITARM` exported (the installer does this)
- GNU Make

```sh
sudo dkp-pacman -S 3ds-dev
make
```

The output is `PaperBoat3DS-Refolded.3dsx` for the Homebrew Launcher.

Build the Folium/emulator CCI image with:

```sh
make packages
```

`packages` requires `makerom` on `PATH`. CI builds a pinned copy from
[Project_CTR](https://github.com/3DSGuy/Project_CTR) (`e8f5f529`).

Expected outputs:

- `PaperBoat3DS-Refolded.elf`
- `PaperBoat3DS-Refolded.3dsx`
- `PaperBoat3DS-Refolded.3ds`
- `PaperBoat3DS-Refolded.smdh`

## Host contract (no toolchain)

```sh
sh tools/test_bootstrap.sh
```

This compiles `source/bootstrap.c` with a host C11 compiler and checks
lifecycle, logging, START-exit, and that the shell does not claim PaperBoat
is running.

## CI

`.github/workflows/3ds-build.yml` runs the host contract, cross-compiles the
`.3dsx`, packages a `.3ds` CCI with pinned `makerom`, and uploads both
artifacts from `devkitpro/devkitarm:latest`. That image pin is a convenience
for M0; M1 will document and freeze the toolchain contract.
