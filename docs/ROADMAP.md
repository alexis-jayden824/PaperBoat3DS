# PaperBoat3DS Refolded — Master Milestone Roadmap (M0–M24)

**Project goal:** Produce a clean, maintainable native Nintendo 3DS port of
PaperBoat / Paper Mario 64, using PaperBoat as the behavioral reference and a
dedicated 3DS platform layer for graphics, audio, input, filesystem, timing,
memory, threading, and hardware services.

This repository starts at **M0**. Later milestones are listed so the rebuild
does not drift from the original plan. Nothing past M0 is implemented.

M14+ stays gated behind M13 runtime/rendering acceptance.

## Binding rules

- `.3dsx` is the canonical Homebrew Launcher target.
- Old Nintendo 3DS is the performance/memory baseline.
- Do not rebuild Paper Mario from scratch or fake gameplay geometry.
- PaperBoat is the behavioral reference once source integration begins (M5).
- Do not mark a milestone complete without reproducible evidence.

## M0 — Native 3DS Bootstrap — **in progress**

Smallest legitimate native 3DS application: devkitARM/libctru skeleton,
`.3dsx` target, ARM11 lifecycle, top/bottom framebuffers, logging, clean
shutdown.

**Acceptance:** boots on a 3DS-compatible runtime and exits cleanly without
pretending PaperBoat is running.

## M1 — Reproducible Build & CI — **not started**

Pin toolchain assumptions, CI, deterministic version/build metadata, artifact
generation, document local build requirements.

## M2 — Dependency Graph / Portability Audit — **not started**

Map PaperBoat, libultraship, Torch, and 3DS dependencies. Classify portable
vs platform-specific code. Define PaperBoat → compatibility layer → 3DS.

## M3 — 3DS Platform Abstraction — **not started**

Graphics, input, audio, filesystem, timing, threads, memory, system/lifecycle
interfaces. PaperBoat-facing code must not scatter 3DS calls.

## M4 — Memory, Logging & Diagnostics Foundation — **not started**

Heap/linear awareness, logging, assertions, crash breadcrumbs, runtime
status, New 3DS detection.

## M5 — PaperBoat Source Integration — **not started**

Pinned upstream source in the 3DS build graph. Isolate unsupported desktop
modules. Preserve upstream game logic.

## M6 — libultraship / Engine Compatibility Layer — **not started**

Minimum engine surface. Stubs only where temporarily necessary. Document
unsupported calls.

## M7 — Legal Asset Pipeline — **not started**

PC-side ROM/resource preparation. Never distribute copyrighted ROM data.

## M8 — Input Backend — **not started**

Native 3DS HID. Reserve SELECT for a future PaperBoat menu.

## M9 — Filesystem & Resource I/O — **not started**

SD/resource paths, lookup, alignment/lifetime.

## M10 — Timing, Game Loop & Runtime Services — **not started**

Monotonic timing, frame stepping, suspend/resume, no desktop window loop.

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
