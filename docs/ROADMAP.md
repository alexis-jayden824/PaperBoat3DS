# PaperBoat3DS Refolded — Master Milestone Roadmap (M0–M24)

**Project goal:** Produce a clean, maintainable native Nintendo 3DS port of
PaperBoat / Paper Mario 64, using PaperBoat as the behavioral reference and a
dedicated 3DS platform layer for graphics, audio, input, filesystem, timing,
memory, threading, and hardware services.

This repository is at **M10**. M0–M9 are shell through SDMC I/O. M10 is
the APT 30 Hz loop. Nothing past M10 is implemented.

M14+ stays gated behind M13 runtime/rendering acceptance.

## Binding rules

- `.3dsx` is the canonical Homebrew Launcher target.
- Old Nintendo 3DS is the performance/memory baseline.
- Do not rebuild Paper Mario from scratch or fake gameplay geometry.
- PaperBoat is the behavioral reference once source integration begins (M5).
- Do not mark a milestone complete without reproducible evidence.

## M0 — Native 3DS Bootstrap — **implemented (hardware boot still required)**

Smallest legitimate native 3DS application: devkitARM/libctru skeleton,
`.3dsx` and `.3ds` targets, ARM11 lifecycle, top/bottom framebuffers, logging,
clean shutdown. Host contract is green; Folium/hardware boot is owner evidence.

## M1 — Reproducible Build & CI — **implemented (CI evidence)**

Pinned `devkitpro/devkitarm` digest, pinned makerom, `toolchain/TOOLCHAINS.lock`,
build SHA/UTC metadata, documented local rebuild, CI artifacts.

## M2 — Dependency Graph / Portability Audit — **implemented**

`upstream/PAPERBOAT.lock` pins PaperBoat 1.0.1, libultraship, and Torch.
`docs/M2.md` classifies portable game core, the minimum compatibility surface,
desktop-only systems, 3DS backends, and the host-only asset pipeline. No game
source is compiled yet.

## M3 — 3DS Platform Abstraction — **implemented**

C APIs in `include/pb3ds/` for gfx, input, fs, time, memory, log, audio
(deferred M14), threads (no extra workers), and APT lifecycle. Only
`source/platform.c` includes `3ds.h`. Full HID mapping is M8, SDMC I/O is
M9, citro3d is M11.

## M4 — Memory, Logging & Diagnostics Foundation — **implemented**

Heap/linear pressure against Old 3DS reserves, RAM log ring, `PB_ASSERT`,
crash breadcrumbs, `pb_runtime_query`, New 3DS detection with extras off.

## M5 — PaperBoat Source Integration — **implemented**

Fetch PaperBoat 1.0.1 into `.cache/upstream`. Compile `libc_compat.c` and
`decode_yay0.c` with PaperBoat CMake exclusions recorded in
`upstream/PAPERBOAT.exclusions`. Desktop engine/SDL/Torch stay out.
`boot_main` is not linked.

## M6 — libultraship / Engine Compatibility Layer — **implemented**

C ABI in `include/pb3ds/compat.h`: logging is ready; resources, HID, timing
hold, gfx submit, audio, and CVars are deferred with explicit status.
Desktop libultraship is not linked. CI also emits `.cia`.

## M7 — Legal Asset Pipeline — **implemented**

Host Torch-LH wrapper and US SHA-1 gate (`upstream/ASSETS.lock`). `pm64.o2r`
comes from `./torch otr` plus `assets/yaml/us`; `paperboat.o2r` comes from
desktop `GeneratePortO2R`. Neither archive is committed or uploaded. The
3DS never extracts.

## M8 — Input Backend — **implemented**

Native 3DS HID to `OSContPad`. Circle pad scaled to N64 ±80. SELECT is
reserved for M16 and is not a game button. START still exits the shell.

## M9 — Filesystem & Resource I/O — **implemented**

SDMC paths, STORE `.o2r` lookup, 16-byte aligned copies, lifetime until
`pb_fs_shutdown`. Missing archives are expected until the owner copies them.

## M10 — Timing, Game Loop & Runtime Services — **implemented**

30 Hz ticks from the APT loop. Monotonic `pb_time_ms` freezes across HOME
suspend/sleep. No extra threads. No desktop window loop. `boot_main` is
not linked.

## M11 — Graphics Backend Foundation — **not started**

PICA200/citro3d target, command submission, vertex path, texture upload,
depth, diagnostics.

## M12 — Authentic Title-Screen Integration — **not started**

Real prepared resources. Title scene. Correct texture orientation.

## M12.1 — Title Framing / 3DS Display Correction — **not started**

400×240 top framebuffer, 320×240 source with 40 px pillars, upright UVs.

## M13 — Full PaperBoat Runtime + Renderer Integration — **not started**

Authentic gameplay through a correct 3DS architecture. Sub-gates M13-A
through M13-J as in the master plan. This is the major engineering gate.

## M14 — Audio Backend — blocked on M13

## M15 — Save Data / Persistent Configuration — blocked on M13

## M16 — Bottom-Screen PaperBoat Menu — blocked on M13

## M17 — Gameplay Systems / Battle Validation — blocked on M13

## M18 — Graphics Validation Harness — blocked on M13

## M19 — Performance and Memory Optimization — blocked on M13

## M20 — Stability / Long-Session Hardening — blocked on M13

## M21 — Compatibility and Hardware Validation Matrix — blocked on M13

## M22 — Packaging: 3DSX / 3DS / CIA — blocked on M13

## M23 — Release Candidate / Documentation Freeze — blocked on M13

## M24 — First Playable Public Homebrew Release — blocked on M13
