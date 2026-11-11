# Exvius3DS

Native **recomp host** for Final Fantasy Brave Exvius Offline on New Nintendo 3DS XL.

See [`docs/EXVIUS3DS_ROADMAP.md`](../docs/EXVIUS3DS_ROADMAP.md) and [`docs/exvius3ds/`](../docs/exvius3ds/).

## Host tests (S0 + S1)

```bash
make -C exvius3ds test
```

## 3DS packages (requires DEVKITARM)

```bash
make -C exvius3ds 3ds
make -C exvius3ds packages MAKEROM=/path/to/makerom
```

CI image: `devkitpro/devkitarm` (see `.github/workflows/exvius3ds-3ds-build.yml`).

Do **not** place APKs or `libgame.so` in git. Use `exvius3ds/private/` (gitignored) for local analysis.
