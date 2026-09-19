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

### M6 - Memory strategy
Measure static, linear, heap, stack, archive, and scene costs on Old 3DS. Introduce budgets, bounded caches, streaming, and allocation-failure behavior.

### M7 - Legal PC-side asset pipeline
Provide a separate PC workflow using Torch to produce required `.o2r` archives from a legally obtained copy. Validate hashes/formats without distributing copyrighted content.

### M8 - Input backend
Map Circle Pad, D-pad, face/shoulder buttons, touch, and system lifecycle behavior. Audit conflicts before choosing the PaperBoat-menu physical keybind.

### M9 - PICA200 renderer foundation
Implement the citro3d translation layer, shader conversion path, texture formats, buffers, render states, and top-screen viewport.

### M10 - libultraship graphics integration
Connect the renderer to libultraship's graphics contract and replace desktop window/context behavior with 3DS lifecycle handling.

### M11 - First rendered game frame
Load legal archives and display a deterministic Paper Mario frame on the top screen with diagnostic fallback on failure.

### M12 - Title and file-select flow
Reach title/file-select, validate transitions and input, and document remaining graphical defects.

### M13 - Core overworld gameplay
Stabilize map loading, camera, entities, collision, scripts, pause flow, and representative transitions.

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
