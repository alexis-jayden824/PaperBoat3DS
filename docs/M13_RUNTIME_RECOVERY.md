# M13 Upstream Runtime Integration Recovery

M13 is reopened. The custom `PBWorldScene` proved the 3DS archive, memory,
input, and PICA submission foundations, but it cannot provide Paper Mario
gameplay or authoritative rendering. It remains available only as a diagnostic
fallback while the pinned PaperBoat runtime becomes the sole gameplay path.

## Source of truth

The runtime source is HarbourMasters/PaperBoat 1.0.1 at pinned commit
`424c220f0863c29b9fe55cc674baceff88e9e14f`. The immutable revision is recorded
in `upstream/PAPERBOAT.lock`; CI fetches and verifies it before compilation.

The integration boundary follows the upstream port rather than inventing a
second game loop:

| Upstream function | 3DS responsibility |
|---|---|
| `boot_main` / port initialization | Initialize game globals and platform services without entering the N64 scheduler's infinite loop. |
| `step_game_loop` | Advance the real game modes, workers, triggers, EVT scripts, entities, player, collision, camera, messages, and UI. |
| `gfx_task_background` | Build the game's authentic background display list. |
| `gfx_draw_frame` | Build the authentic world, actor, effect, message, and UI display list. |
| `Graphics_ThreadUpdate` | Orchestrate one logic/render frame; on 3DS its audio hooks remain disabled until M14. |

The desktop port also bypasses a direct call to `boot_main` because that
function installs the original NuSystem callbacks and then spins forever. The
3DS entry must preserve its initialization semantics through `init_game_globals`
and the required platform shims, then call the same frame functions. This is a
platform adaptation of the upstream boot boundary, not a replacement gameplay
implementation.

## Recovery sequence

1. Link the actual runtime entry, world, player-input, player-physics, collision,
   and camera units for ARM11. A build gate must inspect one linked object for
   `boot_main`, `step_game_loop`, `gfx_draw_frame`, `state_step_world`,
   `update_player`, `update_player_input`, and `update_cameras`.
2. Implement the 3DS platform symbols used by that closure: controller input,
   timing, allocation, O2R resource lookup, NuSystem compatibility, port hooks,
   and frame submission. Unsupported future systems must fail explicitly, not
   silently substitute custom behavior.
3. Start `mac_00` through PaperBoat's own game-mode/map scripts. Remove the
   custom file-select-to-`PBWorldScene` handoff from the default path.
4. Feed the display lists emitted by `gfx_task_background` and `gfx_draw_frame`
   into the 3DS Fast3D interpreter. Implement commands from captured real-frame
   traces, including texture tiles and sizes, TLUT/palettes, combine and render
   modes, alpha compare/blending, matrices, geometry state, fog, rectangles,
   nested lists, and resource-path middleware.
5. Keep `PBWorldScene` behind an explicit diagnostic mode until it is no longer
   needed, then remove it from release builds.

## Acceptance gates

M13 remains open until all of these pass in one native build:

- the final ELF contains and calls the upstream runtime loop rather than only
  compiling it in a discarded check object;
- `mac_00` is entered through upstream game modes and EVT scripts;
- Mario's acceleration, deceleration, facing, action state, collision, and
  camera come from upstream state, with no `PBWorldScene` movement update;
- entities, interactions, pause, fades, and `mac_00`/`mac_01` transitions are
  upstream-driven;
- a real-frame command trace has zero unknown Fast3D opcodes and no unsupported
  render-state fallback for the accepted Toad Town route;
- side-by-side captures against PaperBoat/N64 reference footage agree on
  framing, layer order, texture/palette selection, transparency, fog, and the
  qualitative movement feel;
- renderer rejects, command overflows, allocation failures, and invalid resource
  lookups remain zero through repeated transitions and pause/resume.

Audio, saves, battles, and chapter coverage remain blocked. M14 does not begin
until Toad Town genuinely looks and feels correct on this upstream-driven path.

## Current evidence

The recovery candidate now builds a 379-source archive from the pinned
PaperBoat revision. The application activates it after file confirmation,
initializes the real game globals and engine data, enters `mac_00` entry 6
through `GAME_MODE_ENTER_DEMO_WORLD`, and advances frames through upstream
`Graphics_ThreadUpdate`. That path calls `step_game_loop`,
`gfx_task_background`, and `gfx_draw_frame`; `Graphics_PushFrame` sends their
display list to the 3DS interpreter. `PBWorldScene` is absent from the default
application loop.

The desktop port also avoids calling `boot_main` directly because it installs
NuSystem callbacks and never returns. The 3DS adapter preserves that boundary's
initialization and frame behavior without entering the original scheduler.
The build forces `boot_main` into the executable and checks the final ELF for
the entry, frame, player, camera, and 3DS handoff symbols.

### Native resource adapter

`source/runtime_resources.c` now provides the real `ResourceGetDataByName`,
`ResourceGetSizeByName`, texture dimension, and `GameEngine_Get*Exact` symbols.
It returns stable writable blob, vertex, texture, and F3DEX2 display-list
storage, with bounded allocations and explicit unsupported-format errors.
Vertices are converted to native endian; display-list word pairs expand to
native pointer-width `Gfx` packets. Texture/palette bytes retain their original
byte order. Blob storage includes upstream's 16-byte zero padding.

The cache never evicts behind live game pointers. Its owner must stop all game
and GPU users before clearing it. It is limited to 4,096 resources, a 2 MiB
serialized-entry limit, and the existing scene memory budget. Name and CRC
lookups both resolve against the archive and return the same stable cached
objects. Unsupported versions and types fail rather than returning serialized
bytes as if they were native objects.

`sh tools/test_runtime_resources.sh` links and executes the pinned upstream
`Shape_LoadFromRawData` against synthetic O2R data supplied by this adapter.
It verifies the resulting model's native display-list pointer and native vertex
fields, both endian modes, compressed/stored entries, extended command payloads,
stable mutable pointers, malformed inputs, memory rejection/retry, and teardown.
`SANITIZE=1` adds ASan/UBSan to that same integration test. The ARM11 check also
compiles the consumer's ABI assertions against the real pinned `Gfx` type.

### Host runtime trace

An owner-only host run against the generated `pm64.o2r` exercised 400 updates,
including directional input and pause/resume. After 21 transition-held updates,
379 frames were submitted from the real game loop. The trace recorded 54,093
draws, 344,591 triangles, 1,033,773 vertices, 609 runtime resources,
2,647,677 display-list commands, 151,115 nested lists, and a maximum call depth
of five. Player position, speed, and action changed under input and remained
fixed during pause.

The captured `mac_00` and pause stream has zero unknown opcodes, missing
resources, texture decode fallbacks, malformed lists, renderer rejects, or
frame failures. It covers matrices, geometry modes, vertices, triangles,
nested lists, texture images/tiles/sizes/loads, TLUT palettes, combine and
other modes, colors, alpha/blending state, fog, rectangles, scissor, depth,
and PaperBoat's resource-path/hash commands. The interpreter exposes these
counters on the bottom screen so the native run can detect a route-specific
gap immediately.

This evidence proves the host runtime and resource/display-list closure. Native
CI also links the final ARM11 ELF, verifies the entry, frame, player, camera,
and handoff symbols, passes the static memory budget, and packages `.3dsx`,
`.3ds`, and `.cia` candidates. It does not prove 3DS performance or
presentation fidelity. M13 remains open until side-by-side captures establish
Toad Town framing, layers, textures, transparency, fog, and movement feel.

### Native startup isolation

The `0.13.3-m13r3` device log proves that the complete 60,826-entry index is
built in about 3.45 seconds and `init_game_globals` returns. Both recorded runs
then stop inside upstream `load_engine_data`, before the first world frame or a
resource error is reported. This rules out the archive scan and file-select
event as the immediate failure boundary.

`0.13.4-m13r4` preserves the exact upstream initialization order but executes
its 38 operations one per application frame. The bottom screen and persistent
log name the operation before it runs, from save-flash and heap setup through
player sprites, fonts, HUD, and Toad Town activation. The application therefore
services the 3DS lifecycle and redraws between operations. Its port-owned
`is_debug_panic` also records the active stage and assertion message, marks the
runtime failed, and returns to the diagnostic screen instead of invoking the
desktop port's opaque `abort()` path. The build rejects an unnamespaced
upstream panic definition and verifies the staged-start symbols in the final
ELF.

This isolation does not by itself close M13: the native test must reach
`Upstream: active`, and movement and presentation must still pass the existing
side-by-side acceptance gates.

The first `0.13.4-m13r4` device log isolated the stop to
`clear_script_list()`: the first upstream virtual-entity allocation asserted.
The resource index and the preceding eight startup stages had all completed.
The linked ARM11 ELF then exposed the platform difference: PaperBoat's general
heap byte array was located at `0x0057364c`, but its allocator requires the
storage symbol itself to be 16-byte aligned. `_heap_create()` wrote its header
at the aligned address `0x00573650`; `_heap_malloc()` subsequently started at
the original symbol and therefore saw an empty heap. Desktop linkers happened
to satisfy the unstated alignment assumption, which is why host traces passed.

`0.13.5-m13r5` replaces only the four upstream heap-storage definitions with
equivalent 16-byte-aligned definitions for the 3DS runtime closure. Startup
also checks the invariant before creating the general heap, and every native
build rejects a final ELF whose general, sprite, collision, or battle heap is
misaligned or incorrectly sized. The check rejects the recorded r4 ELF, so
the regression is tied to the exact target binary failure rather than a host
approximation.

## r6 renderer and frame-time recovery

The r5 device recording proves that the linked upstream runtime now reaches
and updates Toad Town. It also exposes three independent M13 blockers rather
than an M14 content gap:

- The world updates only about three to four times per second. Display-list
  resource commands repeatedly scanned every loaded resource and recomputed a
  CRC64 for every name. The cost grew with the scene and again when pause
  resources loaded.
- The background disappears behind black while foreground models remain. The
  interpreter ignored `G_SETZIMG` and `G_SETCIMG`, so Paper Mario's normal
  color-image switch to the Z buffer turned its depth clear into a black color
  rectangle over the already-drawn background.
- Sprites, dialogue glyphs, model textures, and pause labels all have the same
  vertical inversion. N64 top-down texture rows were passed directly to the
  bottom-origin PICA texture coordinates.

`0.13.6-m13r6` adds a loaded-resource hash table with cached CRC64 values,
tracks color/depth image targets, implements depth-target fills, applies N64
copy-cycle rectangle stepping and inclusive edges, honors rectangle tile and
flip state, implements image rectangles, converts scissor coordinates, and
maps N64 T coordinates into the PICA texture orientation. A bounded display
list command budget prevents a malformed pause list from looking like an
unbounded freeze. Per-update timing, command-count, depth-clear, lookup-probe,
and pause-step diagnostics remain visible or logged for the next device test.

These corrections remain part of M13. M14 cannot begin until an r6-or-later
device recording shows a complete Toad Town background, correctly oriented
sprites and UI, responsive upstream movement, and repeatable pause/resume.

## r7 material, clipping, and submission recovery

The r6 device recording confirms that texture row orientation is corrected and
that upstream dialogue, actors, and map objects are being submitted. It also
isolates the remaining black-world and incomplete-object failures to the 3DS
interpreter rather than missing assets:

- Combiner operands were treated as an unordered set of color multipliers.
  Paper Mario frequently uses `(A - B) * C + D`; multiplying every referenced
  primitive or environment color can turn a valid texture/fog expression
  completely black. The runtime now decodes both color/alpha cycles, removes
  algebraically unused operands, and evaluates the non-texture portion in
  formula order before the PICA texture stage. The r8 correction keeps the
  first encoded cycle in one-cycle mode, matching the linked PaperBoat/Fast
  contract; r7's second-cycle selection caused valid world and menu materials
  to evaluate as transparent black.
- A triangle was discarded whenever any transformed vertex had non-positive
  `W`, and depth was clamped before clipping. Large building and ground
  polygons crossing the near plane therefore disappeared in whole pieces.
  Signed homogeneous coordinates and unclamped depth now reach PICA's clipper.
- Transparent sprite texels were blended while still participating in depth
  updates. The native pipeline now carries alpha-compare state, so zero-alpha
  texels are rejected before they can sever later Mario, NPC, glyph, or UI
  layers.
- `G_LOADTILE` now preserves source row stride, tile offsets, and loaded
  dimensions. This is required for sprites and UI assembled from subregions;
  `G_LOADBLOCK` remains contiguous even when its conventional image width is
  one.
- The renderer no longer linearly searches the runtime texture cache for every
  triangle or flushes the entire CPU cache for every draw. It retains the
  resolved texture per batch, deduplicates native viewport/pipeline/texture/
  sampler/combiner state, and flushes the used linear vertex range once before
  frame submission.

`0.13.8-m13r8` is a device-validation candidate, not an M13 acceptance claim.
It also preserves the last native color target during pause, suppresses the
dummy CPU framebuffer strips, and avoids their redundant texture uploads and
draws.
It must show the Toad Town background and building surfaces, whole sprites,
responsive 30 Hz-class movement after warm-up, zero renderer/resource error
counters, and stable pause/resume before M13 can close.

## r9 Fast3D semantic combiner boundary

The r8 device results show that repairing individual approximations in the
temporary display-list walker is not a renderer replacement. In particular,
its CPU path substituted white for every texture operand and then reduced the
result to one of three fixed PICA modes. An expression such as
`(TEXEL0 - ENVIRONMENT) * SHADE + ENVIRONMENT` therefore could not retain
Fast3D's meaning even when resource, texture, and command counters were clean.

`0.13.9-m13r9` starts the larger
PaperBoat -> Fast3D semantics -> GfxRenderingAPI3DS -> PICA200 migration:

- A target-independent module now reproduces the pinned libultraship
  `GenerateCC` shader IDs, normalization, one-cycle TEXEL1 remap, constant
  input mapping, shade-varying assignment, and cycle texture semantics.
- `GfxRenderingAPI3DS` compiles `(A - B) * C + D` into PICA200 TEV programs.
  Direct replace, multiply, multiply-add, and interpolate forms stay in one
  stage; general formulas and a pair of cycles can consume up to all six
  hardware stages. Saturation-sensitive multi-stage cases remain part of the
  device-validation boundary rather than being treated as proven equivalent.
  Primitive/environment constants are latched per draw instead of folded into
  a white-texture CPU approximation.
- The native stream and vertex shader now carry independent TEXEL0 and TEXEL1
  coordinates, so the backend contract no longer rejects a second texture
  merely because the temporary walker cannot bind it yet.
- The compatibility walker uses the semantic path for supported one-cycle
  batches. Fog, key/convert constants, and its still-incomplete second-tile
  path remain on the old evaluator and increment an explicit legacy fallback
  counter. This is a measured migration boundary, not a claim that the
  temporary walker has become Fast3D.

The bottom screen reports `CC:<semantic>/<legacy>` and shutdown logs preserve
both totals. The r9 playtest must compare the formerly black background and
building materials, record both counters before/pause/after resume, and keep
renderer rejects at zero. A nonzero legacy count is expected at this stage;
it identifies the next semantics to move rather than closing M13.

## r10 two-cycle texture routing

`0.13.10-m13r10` moves the compatibility walker's two-cycle batches across
the r9 semantic boundary. The TEV compiler already represented both RDP
cycles, including the physical TEXEL0/TEXEL1 swap in cycle two; r9 could not
exercise that program because it only uploaded the first render tile and
filled the second UV stream with zeroes.

The runtime now follows the pinned Fast3D texture contract for both units:

- texture unit 0 uses the selected render tile;
- texture unit 1 uses the following render tile when the base is tile 0 or 1;
- without the still-pending LOD path, base tiles 2 through 7 are sampled by
  both units, matching Fast3D's non-mipmap fallback;
- each unit independently resolves TMEM, palette, dimensions, wrapping,
  filtering, tile shift/origin, vertical orientation, and normalized UVs.

Shader-reported texture usage is treated as the final vertex-stream contract,
so a two-cycle program cannot request two UV pairs while the walker emits only
one. Host coverage submits two independent 8x8 TMEM loads through a real
two-cycle rectangle and requires two uploads, one semantic batch, one
two-cycle semantic batch, no legacy fallback, and no renderer rejection.

The bottom screen adds `2C`, the number of two-cycle batches accepted by the
semantic path; the shutdown log records `semantic_two_cycle_batches`. A
playtest should show that value increasing in affected world, sprite-shading,
or pause passes while `Reject` and `Fall` remain zero. Fog, key/convert
registers, LOD, and TEV saturation-sensitive programs still retain measured
fallback or device-validation boundaries. This checkpoint therefore advances
the renderer replacement but does not complete it or close M13.
