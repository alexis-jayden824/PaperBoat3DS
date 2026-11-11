# Exvius3DS

Native **recomp host** for Final Fantasy Brave Exvius Offline on New Nintendo 3DS XL.

See [`docs/EXVIUS3DS_ROADMAP.md`](../docs/EXVIUS3DS_ROADMAP.md) and [`docs/exvius3ds/`](../docs/exvius3ds/).

## S0 (current)

Public-contract recovery of the wire crypto envelope + host unit tests.

```bash
make -C exvius3ds test
```

Do **not** place APKs or `libgame.so` in git. Use `exvius3ds/private/` (gitignored) for local analysis.
