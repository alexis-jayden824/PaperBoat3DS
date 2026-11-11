# S3 acceptance record

**Sprint:** S3 — File system, MST, texpack bring-up  
**Date:** 2026-11-11 (America/New_York)  
**Status:** Host software gate **met**. Hardware confirm owner-gated.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| Bottom black bugfix | `plat_3ds.c` full-bleed clear on both screens |
| RomFS/SDMC FS abstraction | `ex3/fs.h` |
| MST load (+ encrypted) | `diag.mst` / `diag_enc.mst` via `ex3_mst_*` |
| Texpack load | `diag.tex` via `ex3_texpack_*` |
| Host proof | `make -C exvius3ds test` (includes `test_mst`, AES roundtrip) |

## Owner hardware check

Bottom should be blue with status bars (not black). Top still shows gold diag scene.
