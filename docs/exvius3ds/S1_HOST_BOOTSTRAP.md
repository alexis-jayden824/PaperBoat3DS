# S1 — New 3DS host bootstrap

**Status:** Software complete for host proof + CI package path (2026-11-11).  
Folium / real n3DS XL visual confirmation is an **owner** acceptance step.

## Deliverables

| Item | Location |
| --- | --- |
| `ex3_plat` ABI | `exvius3ds/include/ex3/plat.h` |
| Host implementation | `exvius3ds/source/plat_host.c` |
| New 3DS implementation | `exvius3ds/source/plat_3ds.c` (citro2d dual clear, APT hooks, SDMC log) |
| Bootstrap frame loop | `exvius3ds/source/bootstrap.c`, `main_3ds.c` |
| New 3DS exclusive RSF | `exvius3ds/packaging/Exvius3DS.rsf` — 96MB, 804 MHz, L2, core2 |
| Host tests | `make -C exvius3ds test` |
| CI `.3dsx/.3ds/.cia` | `.github/workflows/exvius3ds-3ds-build.yml` |

## Dual-screen diagnostic

| Screen | Clear color (RGBA8) | Role |
| --- | --- | --- |
| Top 400×240 | `0x0B1E3AFF` | Stage navy |
| Bottom 320×240 | `0x163A6BFF` | Touch chrome blue |

START requests clean exit. APT suspend/sleep pauses drawing; restore resumes.

## New 3DS profile

- RSF: `SystemMode 96MB`, `CpuSpeed 804MHz`, `EnableL2Cache true`, `CanAccessCore2 true`
- Runtime: `osSetSpeedupEnable(true)` via `ex3_plat_enable_new3ds_boost` when `APT_CheckNew3DS` is true

## Proof commands

```bash
# Always (this environment)
make -C exvius3ds test

# With DEVKITARM / CI container
make -C exvius3ds 3ds
make -C exvius3ds packages MAKEROM=/path/to/makerom
```

## Owner hardware / Folium checklist

1. Install CI artifact `Exvius3DS.3dsx` (or `.cia` under CFW).
2. Confirm top navy + bottom panel blue clears.
3. Confirm START exits; home/sleep suspend does not crash.
4. Confirm `sdmc:/exvius3ds.log` contains boot banner with sha/utc.
5. Attach capture + log excerpt to close S1 hardware gate.
