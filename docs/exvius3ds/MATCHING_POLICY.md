# Matching / recovery policy

## Goals

1. **Behavioral fidelity** of recovered client logic on New 3DS XL.
2. **Provable** provenance for each recovered unit (binary address, contract section, or offline fixture).
3. **No proprietary blobs** in the repository.

## Matching levels

| Level | Meaning | When required |
| --- | --- | --- |
| **L0 — Spec recovery** | Behavior from public contract / offline fixtures; not instruction-matched | Boot crypto, stub responses (S0+) |
| **L1 — Semantic recovery** | Decompiled control flow + data structures; may differ in register allocation / inlining | Game logic ports (S3+) |
| **L2 — Matching** | Byte-identical (or asm-identical under documented toolchain) to a chosen ABI | Optional later; not a ship gate for n3DS |

Exvius3DS **ships on L0/L1**. L2 is research/optional and must not block dual-screen or offline play.

## Unit admission rules

A recovered function may land in `exvius3ds/source/` only if:

1. Provenance is cited (contract §, symbol name + ABI note, or fixture id).
2. A host unit test exists under `exvius3ds/tests/`.
3. No verbatim dump of copyrighted assets or large string tables is included.
4. Android/JNI edges are behind `ex3_plat` or eliminated.

## Prefer shim over rewrite

If a Cocos or Android API is only a platform edge, implement `ex3_plat_*` and keep the recovered caller. Do not rewrite the caller “for cleanliness” unless the shim cannot express the contract.
