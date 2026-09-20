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

Folium build `3c98a1458c01` established native target/shader/texture submission,
two draws per frame, stable command use, and zero frame failures. Its screenshot
did not make the intended sail overlay visually distinguishable. M10 removes
the texture modulation from that overlay and disables depth for the second draw
so the next runtime gate can verify it unambiguously.

## M10 audit result

The application now instantiates a concrete `PB3DS::GfxRenderingAPI3DS` against
the exact pinned header. Its diagnostic reaches PICA only through that API.
Host compilation proves every pure virtual method is present, while the
portable bridge independently validates packed combiner keys, texture and
shader bounds, exact vertex layouts, viewport/scissor ranges, streaming limits,
frame ordering, and suspend admission.

The first backend is intentionally not a claim of full Fast3D coverage. It
supports shade, texture 0, and texture-times-shade one-cycle programs. Complex
combiner options and offscreen/readback operations reject with telemetry. The
base-class transform/uniform state is retained. M11 proves an archive-backed
static image through this adapter; M12 adds a narrowly scoped native title/file-
select checkpoint. Full game matrix and PaperBoat display-list execution remain
outside that checkpoint and must precede gameplay claims.

Merge `4b5e6faef11ef469953a86d1ca84ecde32844d3a` passed both pull-request checks.
Post-merge devkitARM run 114 repeated the M6/M8/M9/M10 host contracts, linked
the native adapter, enforced memory budgets, and produced non-empty `.3dsx`,
`.3ds`, and `.cia` packages. This closes the M10 software build/package gate;
it does not substitute for emulator or physical-console evidence. Folium build
`bc0139a2f292` subsequently validated adapter counters and exposed the native
same-frame stream overwrite described in `docs/M10_GRAPHICS.md`.

## M11 audit result

M11 deliberately proves the smallest legal asset-to-PICA path before enabling
the live title loop. It does not load the 39 MiB game archive into RAM and does
not claim game-state or display-list execution.

- a 4 KiB cursor scans the ordinary central directory only until both fixed
  resource names are found;
- encrypted, multi-disk, unsupported-compression, ZIP64-central-directory,
  malformed, oversized, checksum-failing, and missing inputs fail closed;
- stored and raw-deflate entries are supported, including the ZIP64 size extra
  fields used by Torch's local headers while authoritative central sizes remain
  32-bit;
- compressed, inflater, extracted-resource, and decoded-texture allocations use
  the M6 archive/transient/scene classes and are released after GPU upload;
- the pinned Torch inflate sources are compiled directly, avoiding a mutable
  target package or whole libzip/libultraship desktop archive stack;
- the exact title CI8 image and 256-entry big-endian RGBA5551 palette decode to
  a 512x256 RGBA8 texture with transparent padding;
- a deterministic fallback preserves diagnostics and reports the precise
  archive/frame failure instead of crashing;
- the M10 native stream buffer now reserves non-overlapping spans per frame and
  exposes high-water and overflow counters.

Synthetic deflate/stored, local-ZIP64, missing-resource, invalid-resource, bad-
CRC, padding/color, and allocation-release tests pass. A private local run also
loaded the project owner's generated 60,826-entry archive successfully without
placing the archive or its contents in source control.

The first Folium render then showed the correct resource upside down. The
archive/decode evidence remains valid, but orientation did not satisfy M11's
visual gate. M12 fixes the mapping in one renderer helper and covers its exact
half-texel/top-bottom contract in the portable suite.

## M12 audit result

M12 extends the same bounded reader to three exact title-screen entries rather
than opening a general-purpose asset surface. RGBA32 and IA8 are the only new
decode formats. Each extraction has a fixed bound, decoded buffers are charged
to the M6 scene class, and all CPU pixels are released immediately after GPU
upload. Synthetic stored/deflate and private owner-archive runs pass with no
residual archive, transient, or scene allocation.

The interactive state machine mirrors the audited upstream input contract:
A/START enters file select, a latched stick or direction edge moves within a
2x2 grid, A/START confirms, and B returns. L+R+START is kept outside the mapped
N64 flow as the explicit checkpoint exit chord. Confirmation intentionally
stops at the M13 handoff. The rendered file panels do not claim save I/O,
message/font/window display lists, audio, or overworld execution.

## M13 checkpoint audit result

The first M13 slice follows pinned PaperBoat data rather than inventing a new
game path. PaperBoat's demo table selects `mac_00` entry 6, and its world table
associates that map with `nok_bg`. The 3DS loader therefore requests only six
bounded O2R entries: the map-shape blob, collision blob, vertex resource, one
representative F3DEX2 display list, background, and palette.

Before entering the world state it validates shape-tree offsets and depth,
display-list references, vertex payload sizing, collision and zone indices,
the sampled display-list terminator, exact CI8/RGBA16 background formats, and
all allocation releases. Synthetic stored/deflate and malformed fixtures pass;
the private owner archive reports 223 nodes, 223 display-list references,
3,211 vertices, collision `110/727/873`, zones `18/76/69`, and 19 sampled
commands. No proprietary bytes enter source control or CI.

The world background uses its own GPU texture, so a failed upload cannot erase
the working title path. Active and paused states are observable and retryable.
This is preflight and presentation evidence only: full map display-list
execution, camera, entities, collision response, scripts, and transitions are
still open M13 gates.

## Open gates

1. Pass the M13 native build/package and public synthetic CI gates.
2. Validate the prompt correction and overworld checkpoint in Folium.
3. Validate renderer lifecycle, input, and memory behavior on real hardware.
4. Preserve real-hardware gates for lifecycle, memory, controls, rendering,
   and Old 3DS performance.
