# S1 acceptance record

**Sprint:** S1 — New 3DS host bootstrap  
**Date:** 2026-11-11 (America/New_York)  
**Status:** Host software gate **met**. Package gate via CI workflow. Hardware/Folium **owner pending**.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| `ex3_plat` ABI skeleton | `exvius3ds/include/ex3/plat.h` |
| Dual-screen clear frame loop | `bootstrap.c` + `plat_3ds.c` / host tests |
| Diagnostics + SDMC/host log | `log.c`, `ex3_plat_log_line`, path `sdmc:/exvius3ds.log` |
| APT lifecycle | `plat_3ds.c` apt hooks; host injectors in tests |
| New 3DS 804 MHz + L2 + 96MB | `packaging/Exvius3DS.rsf` + `ex3_plat_enable_new3ds_boost` |
| Host proof | `make -C exvius3ds test` |
| `.3dsx` / `.3ds` / `.cia` path | `.github/workflows/exvius3ds-3ds-build.yml` |
| Folium + n3DS XL paint | Owner checklist in `S1_HOST_BOOTSTRAP.md` |

## Host test run

```text
$ make -C exvius3ds test
ok: … (crypto S0)
ok: plat create / init / boost / dual clear / lifecycle
ok: bootstrap three frames / suspend / exit
all tests passed
```

## Remaining

- CI green upload of `Exvius3DS.3dsx`
- Owner Folium + hardware captures for both-screen colors
