# PaperBoat3DS M0-M24 Roadmap

This is the binding master milestone list. Older per-milestone docs under
`docs/M*.md` used a shifted numbering (M0 toolchain, M1 bootstrap, …). Those
files remain as historical evidence; **this document is the project map**.

## Binding technical direction

- Output `.3dsx` as the canonical Homebrew Launcher target. Produce `.3ds` for
  Folium/emulator QA and `.cia` as an optional CFW-installable target from the
  same ELF. Packaging must not fork the runtime or change gameplay behavior.
- Old Nintendo 3DS is the baseline. New Nintendo 3DS optimizations stay isolated.
- Gameplay renders at 400x240 on the top screen without stereoscopic 3D initially.
- The 320x240 bottom screen becomes a touch-friendly PaperBoat configuration menu sharing the desktop configuration API.
- SELECT is reserved for the future PaperBoat menu (M16). START is the game pause key.
- Platform stack: devkitARM, libctru, citro3d/citro2d, ndsp, HID, SDMC/RomFS, and a minimal libultraship-3DS compatibility layer.
- Torch remains a PC-side tool. It generates legal `pm64.o2r`/`paperboat.o2r` assets from a user-provided legal copy; asset extraction never runs on the 3DS.
- Desktop windows, updaters, native dialogs, and unrelated desktop backends are disabled or replaced.
- PaperBoat is the behavioral reference. Do not rebuild Paper Mario from scratch or fake gameplay geometry.
- M14+ stays gated behind M13 runtime/rendering acceptance.

## Milestones

### M0 - Native 3DS bootstrap — **complete**

devkitARM/libctru skeleton, `.3dsx` target, ARM11 lifecycle, top/bottom screens,
logging, clean shutdown. Evidence: packaged artifacts and Folium boot.

### M1 - Reproducible build and CI — **complete**

Pinned toolchain image (`devkitpro/devkitarm`), CI in
`.github/workflows/3ds-build.yml` and `m7-assets.yml`, version/build metadata,
`.3dsx`/`.3ds`/`.cia` artifacts, `docs/BUILDING.md`.

### M2 - Dependency graph / portability audit — **complete**

Pinned PaperBoat 1.0.1 / libultraship / Torch in `upstream/PAPERBOAT.lock`.
Classification lives in `docs/M3_DEPENDENCY_AUDIT.md` (legacy filename).

### M3 - 3DS platform abstraction — **complete**

Isolated headers under `include/pb3ds/` for graphics, input, filesystem, timing,
memory, logging, and lifecycle. Aggregate contract: `include/pb3ds/platform.h`.
Audio is declared (`pb_platform_audio_status` → `PB_AUDIO_DEFERRED_M14`) and is
not implemented until M14. The game is stepped from the APT main loop; extra OS
threads are not used.

### M4 - Memory, logging, and diagnostics — **complete** (hardware calibration open)

Heap/linear budgets, SDMC log sink, assertions/panic breadcrumb, bottom-screen
status, New 3DS detection. See `docs/M6_MEMORY.md` (legacy filename).

### M5 - PaperBoat source integration — **complete**

Pinned sources fetched by CI; ARM11 compile of the game-core slice; unsupported
desktop modules isolated. See `docs/M5_COMPATIBILITY.md`.

### M6 - libultraship / engine compatibility layer — **complete** (stubs documented)

Minimum engine surface: config, archives, resources, logging, timing, graphics
vtable, NuSystem/port hooks. Unsupported calls fail or increment
`platform_warnings` rather than silently substituting gameplay. Audio hooks stay
no-ops until M14.

### M7 - Legal asset pipeline — **complete**

`tools/pb3ds_assets.py`, synthetic CI, owner ROM workflow. ROM data is never
committed. See `docs/M7_ASSET_PIPELINE.md`.

### M8 - Input backend — **complete** (physical matrix still useful)

HID buttons, Circle Pad, D-Pad shift layer, touch, APT suspend/resume
neutralization, SELECT menu reservation. Host tests: `make m8-input-test`.
See `docs/M8_INPUT.md`.

### M9 - Filesystem and resource I/O — **complete**

SD paths `sdmc:/3ds/PaperBoat3DS/{config.ini,paperboat.o2r,pm64.o2r}`, bounded
O2R reader, archive/error/lifetime rules. See `docs/M11_FIRST_FRAME.md` and
`source/o2r.c`.

### M10 - Timing, game loop, and runtime services — **complete**

`osGetTime` monotonic clock, APT hooks, `Graphics_ThreadUpdate` /
`step_game_loop` stepping, `GameEngine_HoldFrame` sleep, no desktop window loop.

### M11 - Graphics backend foundation — **complete**

citro3d target, shaders, VBO stream, texture upload, depth, diagnostics.
See `docs/M9_RENDERER.md` and `docs/M10_GRAPHICS.md` (legacy filenames).

### M12 - Authentic title-screen integration — **complete**

Legal O2R title resources, title/file-select flow, runtime/resource bridge.
See `docs/M12_TITLE_FLOW.md`.

### M12.1 - Title framing / 3DS display correction — **complete**

400×240 presentation, 40 px pillars, upright UVs, PRESS START tint.
See `docs/M12_1_PRESENTATION.md`. Host: `make m12-layout-test`.

### M13 - Full PaperBoat runtime + renderer integration — **in progress**

Upstream `boot_main` / `step_game_loop` / `gfx_draw_frame` drive `mac_00`.
Fast3D interpretation, TEV combiners, TLUT, fog, texture retirement, START
pointer safety, and N64 homogeneous frustum clipping are in the native path.

Sub-gates:

| Gate | Status |
|---|---|
| M13-A instrumentation | Clip/huge/cull/invalid HUD; optional `PB3DS_DEBUG_*` |
| M13-B render state | Cached pipeline, viewport, scissor, combiner uniforms |
| M13-C textures | Decode, cache, UV, wrap, filter, lifetime |
| M13-D CI/TLUT | Staged 512-byte TLUT, pal16/pal256 |
| M13-E combiner | Semantic TEV + measured legacy fallback counters |
| M13-F viewport/scissor/clip | Canonical 320-in-400 + Fast3D invertY screen map; N64 frustum clip |
| M13-G framebuffer/depth | Color/Z image tracking; no black depth-clear quad |
| M13-H START menu | KSEG reject, HUD list spans, aux-cache size, skip badge tutorial |
| M13-I performance | Lookups hashed; no aggressive opt until visual sign-off |
| M13-J hardware | Folium/New 3DS XL playtest still required |

M13 is not accepted until Toad Town geometry, sprites, pause, and movement match
PaperBoat on hardware. `PBWorldScene` remains diagnostic-only.

### M14 - Audio backend

ndsp music/SFX. Blocked on M13.

### M15 - Save data and persistent configuration

Blocked on M13.

### M16 - Bottom-screen PaperBoat menu

Blocked on M13. SELECT is already reserved.

### M17 - Gameplay systems / battle validation

Blocked on M13.

### M18 - Graphics validation harness

Blocked on M13.

### M19 - Performance and memory optimization

Blocked on M13 correctness.

### M20 - Stability / long-session hardening

Blocked on M13.

### M21 - Compatibility and hardware validation matrix

Blocked on M13.

### M22 - Packaging: 3DSX / 3DS / CIA

Packaging exists today as part of M1; the M22 freeze is the release-format gate.

### M23 - Release candidate / full regression

Blocked.

### M24 - 1.0 release and maintenance baseline

Blocked.

## Global rules

1. PaperBoat is the behavioral reference.
2. Do not rebuild Paper Mario from scratch.
3. Do not fake final gameplay or hardcode scenes/textures to hide missing systems.
4. A successful compile is not runtime proof.
5. Renderer bugs must be traced to the first incorrect state transition.
6. Correctness comes before aggressive optimization.
7. Physical New 3DS hardware validation is required for hardware-sensitive behavior.
8. Keep 3DS-specific code isolated behind clean platform boundaries.
9. The top screen is for gameplay; the bottom screen is reserved for the PaperBoat configuration experience.
10. M14+ does not supersede unresolved M13 correctness blockers.
