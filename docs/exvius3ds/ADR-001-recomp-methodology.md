# ADR-001 — Exvius3DS recomp methodology

- **Status:** Accepted (S0)
- **Date:** 2026-11-11
- **Context:** Deliver FFBE Offline on New 3DS XL without shipping an Android runtime

## Decision

Exvius3DS is a **decomp → recompile → platform shim** project.

1. **Authority** — Global FFBE v9.0.0 client logic recovered from `libgame.so` (Cocos2d-x).
2. **Host** — Recompile recovered C/C++ for **ARM11 / devkitARM** (New 3DS XL exclusive).
3. **Shim** — Replace Android / JNI / GLES / Java crypto / HTTPS with `ex3_plat_*` (libctru, citro3d/citro2d, ndsp, SDMC).
4. **Offline** — Retarget the client’s request transport to **in-process stubs** whose behavior is proven against [exvius-offline](https://github.com/Elvlin/exvius-offline) fixtures and `FFBE_SERVER_CONTRACT.md`.
5. **Enhancements** — Dual-screen presenter and QoL are **host mods** on the recomp (SoH-style), not a second game.

## Consequences

| We will | We will not |
| --- | --- |
| Recover and recompile client call chains needed for boot → Home → battle → story | Run the APK, ART, Chaquopy, or Frida on 3DS |
| Prefer fidelity of recovered code over clean-room rewrites of the same systems | Treat static ARM64→ARM11 binary translation as the primary path |
| Keep SE binaries and assets out of git/CI | Commit `libgame.so`, APK, MST, CPK, or artwork |
| Use synthetic fixtures + public contracts in CI | Claim milestones without Folium/hardware evidence |

## Prefer `armeabi-v7a` when available

v9 analysis in exvius-offline centers on **ARM64** `libgame.so`. If the APK also ships **armeabi-v7a**, prefer that binary for early recovery (closer register width / calling conventions to ARM11 bring-up). ARM64 remains valid for semantic recovery when v7a is absent.

## Related

- [`../EXVIUS3DS_ROADMAP.md`](../EXVIUS3DS_ROADMAP.md)
- [`S0_BINARY_INVENTORY.md`](S0_BINARY_INVENTORY.md)
- [`GHIDRA_WORKFLOW.md`](GHIDRA_WORKFLOW.md)
- [`MATCHING_POLICY.md`](MATCHING_POLICY.md)
