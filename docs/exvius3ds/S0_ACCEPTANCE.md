# S0 acceptance record

**Sprint:** S0 — Recomp methodology & binary inventory  
**Date:** 2026-11-11 (America/New_York)  
**Status:** Software acceptance **met** for public-contract recoveries; owner binary fill-in remains local.

## Criteria → evidence

| Criterion | Evidence |
| --- | --- |
| ADR: decomp→recompile | [`ADR-001-recomp-methodology.md`](ADR-001-recomp-methodology.md) |
| ABI / partition inventory | [`S0_BINARY_INVENTORY.md`](S0_BINARY_INVENTORY.md) (seeded from public contract; local hashes TBD by owner) |
| Ghidra / recovery workflow | [`GHIDRA_WORKFLOW.md`](GHIDRA_WORKFLOW.md) |
| Matching policy | [`MATCHING_POLICY.md`](MATCHING_POLICY.md) |
| No SE binaries in git | [`.gitignore`](../../.gitignore); policy in ADR + workflow |
| One recovered function + host unit test | `exvius3ds/source/crypto_envelope.c` · `make -C exvius3ds test` |

## Host test run

```text
$ make -C exvius3ds test
ok: iv non-null
ok: iv literal
ok: key16 rc
ok: key16 GameSettingRequest vector
ok: key16 null token
ok: pkcs7 515→528
ok: pkcs7 trailer 0x0D
ok: double-pad 515→544
ok: double-pad trailer 0x10
ok: unpad rc
ok: unpad len
all tests passed
```

Recovered unit provenance: `FFBE_SERVER_CONTRACT.md` §1e live vectors  
(`EncodeKey` `4foXVwWd`, IV `dZMjkk8gFDzKHlsx`, 515→528→544 double-pad).

## Remaining owner-only work (does not block S0 software gate)

- Place user-owned APK under `exvius3ds/private/` and record `libgame.so` SHA-256s.
- Confirm `armeabi-v7a` presence.
- Capture Cocos version string via `strings`.

## Next sprint

**S1** — New 3DS host bootstrap (`.3dsx`, dual-screen clear, `ex3_plat` skeleton).
