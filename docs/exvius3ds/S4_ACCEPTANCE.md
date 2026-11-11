# S4 acceptance record

**Sprint:** S4 — JNI / Java surface elimination  
**Date:** 2026-11-11 (America/New_York)  
**Status:** Host software gate **met**. Device retest of S3 RomFS fix is owner-gated.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| Catalog of Android/JNI edges | [`S4_JNI_CATALOG.md`](S4_JNI_CATALOG.md) |
| AES-CBC + base64 LapisJNI stand-in | `ex3_jni_encode_b64_aes` / `ex3_jni_decode_b64_aes` |
| Prefs → file (SDMC path on device) | `ex3_prefs_*` |
| HTTP offline dispatcher hook | `ex3_http_request` (S6 fills real handlers) |
| Login offline guest | `ex3_login_guest` |
| Link proof, zero JNI symbols | `test_jni_link` + `recovered_client_refs.c` |

## Host proof

```bash
make -C exvius3ds test
```

Must build and pass `test_jni_link`.

## Hardware note (S3 follow-up)

Red bottom bars meant MST/tex load failed because RomFS was never initialized.  
`plat_3ds.c` now calls `romfsInit()` / `romfsExit()`. Expected after reflash:

- Top: gold diag sprite (unchanged)
- Bottom: green status strip, light-blue name bars, gold tex chip (not red)
