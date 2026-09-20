# Repository and port audit

## Current baseline

- The repository was initialized on 2026-09-19 and active development occurs
  on `port/3ds`.
- PaperBoat, libultraship, and Torch are fetched rather than vendored. Their
  immutable repositories and commits are recorded in
  `upstream/PAPERBOAT.lock`.
- The source tree contains no ROM, generated `.o2r`, keys, or extracted
  proprietary assets. CI rejects those extensions.
- One devkitARM ELF is packaged as canonical `.3dsx`, emulator `.3ds`, and
  optional CFW `.cia` outputs.
- The application is still a diagnostic shell, not a playable port.

## Evidence through M8

- M0/M1 established reproducible package output and Folium bootstrap evidence.
- M2 added structured SD logging, build metadata, and graceful diagnostics.
- M3 classified the pinned dependency graph and excluded desktop-only edges.
- M4 supplies the current 3DS configuration/archive/log/time compatibility
  surface.
- M5 cross-compiles representative pinned PaperBoat game-core units for ARM11.
- M6 enforces provisional Old 3DS memory budgets and records runtime telemetry;
  hardware calibration is pending.
- M7 completed a private supported-ROM extraction run without committing or
  uploading copyrighted input/output.
- M8 implements and host-tests native input. Build
  `0735cd7bde2127b8559061a1b563b0f84ca7a6db` packaged successfully, and Folium
  confirmed the New 3DS profile plus SELECT menu request. Complete physical
  input and lifecycle validation is pending.

## M9 audit result

The existing M8 shell linked citro3d but did not initialize it; the top display
was a software console. It had no shader build rule, render target, GPU buffer,
texture translation, render-state model, or libultraship graphics handoff.

M9 adds those foundations while keeping the dependency boundary narrow:

- portable, host-tested format/layout/state/viewport logic;
- a single citro3d implementation unit and opaque public handle;
- automatic Picasso assembly for repository-owned `.v.pica` shaders;
- a 240x400 native render target presenting a logical 400x240 top screen;
- a real linear VBO, Morton-swizzled RGBA8 texture, shader, TEV stage, depth,
  blending, and runtime telemetry;
- a deterministic diagnostic scene that does not require or bundle game data.

The pinned `Fast::GfxRenderingAPI` is substantially larger than the M9 smoke
backend. In particular, dynamic Fast3D combiner IDs, texture lifetime,
streaming triangles, scissor state, and framebuffer operations remain M10
work. Desktop shader/window implementations remain excluded.

## Open gates

1. Prove the M9 shader/backend and all package formats in devkitARM CI.
2. Validate the M9 scene and counters in Folium.
3. Implement the pinned libultraship graphics API in M10, with explicit
   PICA/TEV fallbacks for unsupported combiner behavior.
4. Load the private legal archives and render the first deterministic game
   frame in M11.
5. Preserve real-hardware gates for lifecycle, memory, controls, rendering,
   and Old 3DS performance.
