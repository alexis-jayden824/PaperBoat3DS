# S1 acceptance record

**Sprint:** S1 — New 3DS host bootstrap  
**Date:** 2026-11-11 (America/New_York)  
**Status:** **Complete** — host, CI packages, and real-hardware dual-screen paint.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| `ex3_plat` ABI skeleton | `exvius3ds/include/ex3/plat.h` |
| Dual-screen clear frame loop | `bootstrap.c` + `plat_3ds.c` / host tests |
| Diagnostics + SDMC/host log | `log.c`, `ex3_plat_log_line`, path `sdmc:/exvius3ds.log` |
| APT lifecycle | `plat_3ds.c` apt hooks; host injectors in tests |
| New 3DS 804 MHz + L2 + 96MB | `packaging/Exvius3DS.rsf` + `ex3_plat_enable_new3ds_boost` |
| Host proof | `make -C exvius3ds test` |
| `.3dsx` / `.3ds` / `.cia` path | CI `build-3ds` green on `4523b1e` |
| n3DS XL hardware paint | Owner report 2026-11-11: top + bottom navy/blue clears on real hardware |

## Hardware note

Owner opened the CI `.3dsx` on New 3DS XL and observed two navy blue screens (top `0x0B1E3A`, bottom `0x163A6B`). That matches the S1 diagnostic clear contract — **S1 pass**.
