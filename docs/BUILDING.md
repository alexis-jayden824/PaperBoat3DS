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

## Host contract (no toolchain)

```sh
sh tools/test_bootstrap.sh
```

This compiles `source/bootstrap.c` with a host C11 compiler and checks
lifecycle, logging, START-exit, and that the shell does not claim PaperBoat
is running.

## CI

`.github/workflows/3ds-build.yml` runs the host contract and cross-compiles
the `.3dsx` in `devkitpro/devkitarm:latest`. That image pin is a convenience
for M0; M1 will document and freeze the toolchain contract.
