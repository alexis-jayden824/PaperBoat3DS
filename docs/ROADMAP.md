# PaperBoat3DS M0-M24 Roadmap

## Binding technical direction

- Output `.3dsx` as the canonical Homebrew Launcher target. Produce `.3ds` for
  Folium/emulator QA and `.cia` as an optional CFW-installable target from the
  same ELF. Packaging must not fork the runtime or change gameplay behavior.
- Old Nintendo 3DS is the baseline. New Nintendo 3DS optimizations stay isolated.
- Gameplay renders at 400x240 on the top screen without stereoscopic 3D initially.
- The 320x240 bottom screen becomes a touch-friendly PaperBoat configuration menu sharing the desktop configuration API.
- The physical menu keybind is selected after the M8 input audit.
- Platform stack: devkitARM, libctru, citro3d/citro2d, ndsp, HID, SDMC/RomFS, and a minimal libultraship-3DS compatibility layer.
- Torch remains a PC-side tool. It generates legal `pm64.o2r`/`paperboat.o2r` assets from a user-provided legal copy; asset extraction never runs on the 3DS.
- Desktop windows, updaters, native dialogs, and unrelated desktop backends are disabled or replaced.
- No compile, link, boot, frame, gameplay, audio, performance, or hardware milestone is claimed without reproducible evidence.

## Milestones

### M0 - Toolchain contract
Pin the platform contract, repository policy, devkitARM Makefile, required libraries, output formats, and reproducible build instructions. Acceptance: the same bootstrap ELF packages successfully as `.3dsx`, `.3ds`, and `.cia` in a real devkitARM environment.

### M1 - Native application bootstrap
Boot a minimal ARM11 application, initialize services/screens, show diagnostics on both displays, and exit cleanly. Acceptance: test `.3ds` in Folium and optionally test `.cia` under CFW; a `.3dsx` test on real hardware remains the authority.

### M2 - Diagnostics foundation
Add structured logging, fatal-error presentation, toolchain/build metadata, memory counters, and an SDMC log sink with graceful failure.

### M3 - PaperBoat dependency audit
Import or reference the exact PaperBoat 1.0.1 source and submodules. Classify game/core/platform dependencies and document every desktop-only edge before porting.

### M4 - libultraship-3DS core
Create the smallest 3DS-aware libultraship layer: configuration, archives, resource loading, logging, timing, and platform interfaces without desktop UI/render/audio backends.

### M5 - ARM11 game-core compilation
Cross-compile PaperBoat's game core and generated code for ARM11. Eliminate unsupported compiler, ABI, threading, filesystem, and endian assumptions.

Status: **complete**. CI fetches PaperBoat, libultraship, and Torch at the
commits in `upstream/PAPERBOAT.lock`, then cross-compiles representative
foundation, game-state, event, battle, entity, and generated map/script units
as ARM11 objects. The scoped ABI/compiler shims and the final proof run are
recorded in `docs/M5_COMPATIBILITY.md`. Desktop threading/filesystem backends
remain excluded at the platform boundary; runtime memory and asset-endian
validation continue in M6 and M7 rather than being hidden inside this gate.

### M6 - Memory strategy
Measure static, linear, heap, stack, archive, and scene costs on Old 3DS. Introduce budgets, bounded caches, streaming, and allocation-failure behavior.

Status: **in progress**. Runtime peak/failure telemetry, allocation-class
budgets, bounded 64 KiB archive reads, deterministic policy tests, and CI
ELF-size enforcement are in place. Final calibration requires real-hardware
logs; see `docs/M6_MEMORY.md`.

### M7 - Legal PC-side asset pipeline
Provide a separate PC workflow using Torch to produce required `.o2r` archives from a legally obtained copy. Validate hashes/formats without distributing copyrighted content.

Status: **complete**. The pinned cross-platform host workflow rejects
unsupported ROMs, builds PM64-only Torch, creates deterministic engine/game
archives, performs ZIP/resource metadata validation, writes a privacy-safe
manifest, and stages only generated archives to SD. The project-owner input
passed the supported-ROM contract and produced a verified 57-entry
`paperboat.o2r` plus 60,826-entry `pm64.o2r`; no ROM or generated archive was
committed or uploaded to CI. See `docs/M7_ASSET_PIPELINE.md`.

### M8 - Input backend
Map Circle Pad, D-pad, face/shoulder buttons, touch, and system lifecycle behavior. Audit conflicts before choosing the PaperBoat-menu physical keybind.

Status: **in progress**. The native input boundary, complete Old 3DS mapping,
optional New 3DS duplicates, touch state, edge semantics, lifecycle neutral
gate, SELECT menu reservation, live diagnostics, and deterministic host tests
are implemented. The three packages built successfully at merge
`0735cd7bde2127b8559061a1b563b0f84ca7a6db`, and Folium confirmed the New 3DS
profile, active input gate, and SELECT menu request. Full physical-control,
touch, and lifecycle validation remains; see `docs/M8_INPUT.md`.

### M9 - PICA200 renderer foundation
Implement the citro3d translation layer, shader conversion path, texture formats, buffers, render states, and top-screen viewport.

Status: **software complete; hardware pending**. A portable PICA contract, texture sizing/swizzle,
viewport rotation, render-state cache, Picasso shader build, citro3d target,
linear VBO, sampled texture, deterministic diagnostic scene, telemetry, and
host tests are implemented. CI produced `.3dsx`, `.3ds`, and `.cia` packages,
and Folium build `3c98a1458c01` confirmed the target, compiled shader, sampled
checker, two draw submissions, advancing counters, and zero failures. The sail
overlay was not visually distinct in that capture; M10 changes it to a
shade-only TEV draw. Physical PICA and lifecycle evidence remains; see
`docs/M9_RENDERER.md`.

### M10 - libultraship graphics integration
Connect the renderer to libultraship's graphics contract and replace desktop window/context behavior with 3DS lifecycle handling.

Status: **software and emulator complete; hardware pending**. A concrete adapter
implements the exact pinned `Fast::GfxRenderingAPI` vtable with bounded shader,
texture, streaming, state, frame, and APT-lifecycle behavior. The supported
one-cycle TEV baseline and all rejected features are explicit. Post-merge CI
run 114 built `.3dsx`, `.3ds`, and `.cia` from merge
`4b5e6faef11ef469953a86d1ca84ecde32844d3a`. Folium build
`bc0139a2f292` confirmed the two TEV programs, advancing two-draw/three-triangle
counters, visible sail, zero rejects, and zero frame failures. The capture also
exposed same-frame VBO reuse in the native layer: the sail overwrote one checker
triangle before GPU consumption. M11 replaces per-draw overwrite with a bounded
per-frame streaming arena. Physical lifecycle and PICA validation remain
authoritative. See `docs/M10_GRAPHICS.md`.

### M11 - First rendered game frame
Load legal archives and display a deterministic Paper Mario frame on the top screen with diagnostic fallback on failure.

Status: **archive path validated; orientation correction moved into M12**. A bounded O2R reader locates and
CRC-checks the legal `backgrounds/title_bg` CI8 image and RGBA16 palette,
inflates only those fixed entries, decodes them into a 512x256 RGBA8 GPU
texture, and renders the 296x200 frame centered on the top screen through the
M10 adapter. Missing, malformed, unsupported, oversized, or memory-rejected
inputs retain the checker/sail fallback with an explicit reason. Synthetic
ZIP64-local-header tests and a private local test against the project owner's
generated archive pass. The first Folium capture proved the archive, decode,
upload, and draw path but exposed a vertically inverted image; the shared M12
quad mapping contains the correction and regression test.
See `docs/M11_FIRST_FRAME.md`.

### M12 - Title and file-select flow
Reach title/file-select, validate transitions and input, and document remaining graphical defects.

Status: **functional Folium path validated; M12.1 framing accepted and prompt
correction carried into M13**. The checkpoint loads the authentic
RGBA32 logo plus IA8 prompt/copyright resources through the bounded O2R path,
renders them over the corrected title background, and mirrors PaperBoat's
A/START, 2x2 slot navigation, confirm, and B-return contract. Host fixtures and
the owner's private archive pass. Folium evidence confirms upright assets and
the title/file-select interaction path, while also exposing the diagnostic
navy surround and the later M12.1 capture validated the corrected framing. Its
remaining prompt-combiner defect is corrected and retested in M13. The
file-select panels are a bounded native
checkpoint compositor; save data, text/message/window display lists, and the
overworld handoff remain explicit later-milestone work. See
`docs/M12_TITLE_FLOW.md`.

#### M12.1 - Title presentation correction

Preserve Paper Mario's fixed 320x240 title composition at integer scale on the
400x240 top LCD, with equal 40-pixel side pillars on all retail 3DS models.
Replace the bootstrap navy clear with black, derive every title rectangle from
one tested safe-area transform, restore PaperBoat's pale-yellow PRESS START
tint, and expose its live alpha for visual evidence. Do not stretch or crop the
title art. See `docs/M12_1_PRESENTATION.md`.

### M13 - Core overworld gameplay
Stabilize map loading, camera, entities, collision, scripts, pause flow, and representative transitions.

Status: **in progress; first authentic overworld integration checkpoint
implemented**. Confirming a file slot now preflights the pinned PaperBoat
`mac_00`/entry 6 contract against the private O2R, validates its real shape,
vertex, representative F3DEX2 display list, collision, zone, and `nok_bg`
resources, uploads the background without replacing the title texture, and
enters an observable active/paused world state. The pinned upstream world,
transition, pause, camera-math, and collision sources are cross-compiled in CI.
Live map display-list execution, camera control, entities, collision response,
scripts, and representative map transitions remain required before M13 can be
closed. See `docs/M13_OVERWORLD.md`.

### M14 - ndsp audio backend
Implement initialization, mixing, streaming, buffering, sample conversion, latency control, suspend/resume, and clean shutdown. Validate Folium with its required DSP firmware as a secondary check, while treating real-hardware ndsp results as authoritative.

### M15 - Saves and configuration persistence
Use an explicit SDMC layout, atomic writes, validation, recovery, versioning, and migration. Never overwrite desktop data implicitly.

### M16 - Bottom-screen PaperBoat menu
Build the touch-friendly configuration frontend on the bottom screen, sharing the desktop configuration model. Preserve top-screen gameplay and add the audited physical keybind.

### M17 - Battle-system validation
Validate representative normal, partner, boss, timed-input, reward, and transition paths.

### M18 - Chapter and content coverage
Run structured coverage across chapters, partners, menus, minigames, cutscenes, loading zones, and ending flow.

### M19 - Performance and memory optimization
Profile Old 3DS first, enforce frame/memory budgets, remove stalls, tune caches, and add isolated New 3DS enhancements only when safe.

### M20 - Supported mod boundary
Define which PaperBoat resource/configuration mods can be supported within 3DS memory, storage, UI, and CPU constraints.

### M21 - Platform polish
Finalize lifecycle behavior, error UX, icon/metadata, accessibility/readability, configuration defaults, and recovery paths.

### M22 - Hardware validation matrix
Project owner tests supported Old/New 3DS models and firmware/homebrew environments. Every result records build SHA, model, steps, logs, and outcome.

### M23 - Full validation and release candidate
Run clean builds, asset-pipeline tests, playthrough coverage, suspend/resume, save integrity, performance checks, and known-issue triage.

### M24 - Reproducible release
Tag a source-only release with build instructions, checksums, licenses/notices, compatibility notes, and no copyrighted game data.
