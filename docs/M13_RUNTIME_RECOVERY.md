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
initializes the real game globals and engine data, enters `mac_00` entry 1
through `GAME_MODE_ENTER_DEMO_WORLD`, and advances frames through upstream
`Graphics_ThreadUpdate`. That path calls `step_game_loop`,
`gfx_task_background`, and `gfx_draw_frame`; `Graphics_PushFrame` sends their
display list to the 3DS interpreter. `PBWorldScene` is absent from the default
application loop. The generated M13 map units bind only the accepted
`mac_00`/`mac_01` exits; transitions to maps outside this two-map slice remain
disabled until their runtime closure is deliberately added.

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

## r11 bounded-map and fog-semantic recovery

The r10 device log did not show a pause-system failure. It captured
`Map not found: kmr_20`: startup entry 6 placed Mario on the coordinates shared
by the Toad Town sewer-pipe trigger, and the map script bound that trigger even
though the deliberately bounded M13 registry contains only `mac_00` and
`mac_01`. Pressing START happened near the same update; it was not the failing
operation.

`0.13.11-m13r11` starts at authentic `mac_00` entry 1 and generates bounded
copies of both map main units. `mac_00` retains only its walk exit to `mac_01`,
and `mac_01` retains only its walk exit to `mac_00`. Generation fails if the
pinned upstream binding markers drift, and a host regression verifies that no
other exit event is bound. This preserves upstream movement, map scripts, and
the accepted transition while preventing an unsupported map request from
becoming a runtime panic.

r11 also moves standard depth fog and constant fog from the compatibility
evaluator into the semantic backend. The runtime classifies the pinned Fast3D
blender source, selects fog or blend RGB as required, applies Fast3D's standard
shade-alpha rule, builds a PICA200 fog visibility LUT from the N64 fog
multiplier/offset, and binds native fog state per batch. Vertex-alpha-driven
fog remains an explicit legacy fallback. The bottom screen and shutdown log
now report `Fog semantic/legacy` independently from the general `CC` and `2C`
counters.

Host tests prove the map boundary, fog-LUT endpoints and monotonicity, and a
real fogged display-list route with one semantic fog batch and no legacy or
renderer rejection. This directly targets the observed crash and black fogged
materials, but it is not visual acceptance. The r11 Folium/device run must
still show a complete Toad Town, intact sprites and UI, stable repeated pause,
working `mac_00`/`mac_01` transitions, no `kmr_20` panic, increasing semantic
fog where fogged materials render, and zero error counters before M13 can
close. Key/convert constants, LOD, vertex-alpha fog, saturation-sensitive TEV
programs, and broader maps remain measured migration boundaries.

## r12 key/convert semantic inputs

`0.13.12-m13r12` removes another CPU-combiner approximation from the accepted
route. The display-list walker now decodes `G_SETKEYR`, `G_SETKEYGB`, and
`G_SETCONVERT`, including signed nine-bit K coefficients. Fast3D `CENTER`,
`SCALE`, `K4`, and `K5` operands are normalized exactly like the pinned
interpreter and supplied to the semantic TEV compiler as per-draw constants.
The legacy evaluator consumes the same state if a different boundary still
forces a fallback.

A real two-cycle display-list regression exercises
`(SHADE - CENTER) * SCALE + ENVIRONMENT` followed by
`(COMBINED - K4) * K5 + PRIMITIVE`, including a negative K5 value. It must
produce one semantic two-cycle key/convert batch, no unknown command, and no
legacy fallback. The bottom screen and shutdown log report
`Key/conv semantic/legacy`; the previously overwritten fog diagnostic is also
kept visible.

This closes the key/convert constant-input fallback, not the complete RDP
chroma-key or YUV conversion pipeline: key-width thresholding and K0-K3 texture
conversion are not claimed. LOD, vertex-alpha fog, saturation-sensitive TEV
programs, and broader maps remain explicit migration boundaries. M13 still
requires a side-by-side r12 playtest showing complete Toad Town materials,
sprites, UI, pause/resume, and zero error counters.

## r13 bounded TLUT staging

`0.13.13-m13r13` fixes a CI texture defect found in the end-to-end renderer
audit. The old runtime kept raw palette-source pointers and assumed that a CI8
palette beginning at bank zero always had 512 contiguous bytes. PaperBoat may
populate RDP TLUT memory with independent loads, so upper palette indices could
read beyond the first source buffer and decode as black or corrupt texels.

The runtime now models the 512-byte TLUT as bounded persistent staging, tracks
validity for all 256 entries, and only decodes CI4/CI8 textures when the selected
palette range is complete. Texture-cache identity also includes the staged
palette content and the full decoded source descriptor, preventing a later TLUT
load or a 32-bit key collision from silently reusing stale RGBA data. A split
CI8 regression selects an upper-half palette entry and runs under ASan/UBSan in
CI.

This is a concrete palette/sprite correctness fix, not visual acceptance of all
Toad Town materials. The r13 playtest must repeat the r12 route and specifically
inspect the background, Mario/NPC sprites, pause world map, glyphs, and any
palette-swapped surfaces. LOD, vertex-alpha fog, saturation-sensitive TEV
programs, key-width/K0-K3 conversion, broader maps, and full device validation
remain open M13 boundaries.

## r14 frame-safe texture churn and clipped state

Real-hardware testing of r13 reproduced the black Toad Town backdrop, visually
cut sprites, and a system crash while opening Paper Mario's pause menu. That
rules out a Folium-only presentation issue. The follow-up audit found that the
runtime LRU cache deleted `C3D_Tex` allocations immediately during display-list
translation even though already-recorded PICA commands in the same frame still
referenced those allocations. A texture-heavy frame could therefore free and
reuse the backdrop or an early sprite before the GPU consumed its draw, while
pause-menu texture churn could escalate the same use-after-retire defect into a
GPU/system failure.

`0.13.14-m13r14` detaches evicted or replaced textures from the logical cache
but retires their native allocations as a batch. The renderer submits the
frame, synchronizes PICA once when a retirement batch exists, and only then
calls `C3D_TexDelete`. A failed retirement-node allocation leaves the live
texture intact instead of desynchronizing the native and Fast3D registries.
The bottom screen reports runtime eviction attempts as `Ev` and native
retirements as `Rt total/peak/fail`; the failure component must remain zero. A
192-source display-list regression exercises intra-frame LRU eviction under
ASan/UBSan without renderer rejection.

The supplied clipping/legacy-combiner patch is included in the same candidate.
Partially off-screen viewport and scissor requests are intersected with the
400x240 target so an animated pause clip cannot leave unrelated stale state;
fully off-screen rectangles still reject. The duplicate per-frame framebuffer
clear is removed. For legacy CPU-combiner batches, the GPU samples a texture
only when the fallback can reconstruct a pure texel modulation. Unsafe
subtractive/additive formulas now keep the CPU approximation rather than
multiplying it by a real texel a second time and crushing the result toward
black; `Unsafe` counts those measured fallbacks.

These are targeted corrections for all three observed symptoms, not a claim
that M13 or the TEV migration is complete. Hardware acceptance still requires
an intact backdrop and sprites, repeated pause/resume without a crash, zero
retirement failures/rejections, and comparison against PaperBoat/N64 output.

## r15 partial-height viewport origin correction

`0.13.15-m13r15` corrects the Y origin used when `G_MOVEMEM` installs a Fast3D
camera viewport. N64 viewport translation is expressed from the framebuffer's
top edge, while the PICA200 projection used by this renderer is bottom-left
origin. Full-screen viewports hid the mismatch, but asymmetric partial-height
camera shots could be placed in the opposite vertical region and cut sprites
or geometry off at the screen edge. Direct and hash-backed viewport resources
now perform the same explicit vertical conversion used by rectangles and
scissors.

The runtime HUD and shutdown log also report the active game viewport as `Vp`
and scissor as `Sc`. A regression display list uses a 320x100 viewport and
requires the converted Y origin to be 140. This is a focused clipping fix on
top of r14's texture-lifetime and combiner work; M13 remains open pending
side-by-side and repeated-pause validation on real hardware.

## r16 pal16 CI8 TLUT validity

`0.13.16-m13r16` stopped requiring all 256 CI8 TLUT validity flags before
decoding. Paper Mario menus and sprites often load a 16-entry pal16 into one
bank; treating unloaded entries as a miss made those surfaces fallback. That
fix is retained. The r16 hardware capture of `mac_00` still showed a black
painted backdrop with intact buildings and sprites and `Fall:0`, so the
remaining sky hole was not a missing pal256.

## r17 COPY-cycle backdrop opacity and pause START hang

The r16 Folium/hardware photo of `mac_00` (`Mode:5`, `Fall:0`, `Reject:0`,
viewport `40,0 320x240`) shows 3D props and Toad/Mario sprites against a
black clear. PaperBoat draws that sky as `G_CYC_COPY` `TEXRECT_WIDE` strips
from `nok_bg` CI8 + pal256. Two interpreter choices discarded those pixels:

- every textured batch enabled alpha test, so RGBA5551 palette entries with a
  clear LSB (common for backgrounds) were rejected before blending;
- world geometry mode keeps `G_CULL_BACK`, and screen-space quads after the
  PICA Y flip do not match that cull convention.

COPY/FILL batches now disable alpha test, alpha blend, depth, and culling;
other texrects are screen-space and also keep culling off. Alpha test follows
the RDP compare bits instead of "any textured draw".

START freeze/crash had two separate causes on the pause overlay path:

- `gbi_resolve_vtx_in_static_dl` walked one `Gfx` at a time and never
  skipped TEXRECT extra words, so a payload byte that was not `G_ENDDL`
  could loop forever when `pause_init` resolved HUD lists;
- `hud_element_set_aux_cache(D_80200000, 0x38000)` wrote 224 KiB through a
  16 KiB overlay placeholder in `heaps.c`. The generated heap unit now sizes
  `D_80200000` to 0x38000 so START cannot smash adjacent BSS.

A COPY `TEXRECT_WIDE` regression uses a zero-alpha pal256 plus `G_CULL_BACK`
and still records the copy rectangle. Generation checks the aux-cache array.
M13 remains open pending a hardware playtest of the painted Toad Town sky
and repeated START pause/resume.

## r18 sprite punchthrough, HUD lists, and START resolve

r17 made COPY backdrops visible, but PaperBoat sprites and pause HUD never
set `G_AC_THRESHOLD`. They punch holes with `CVG_X_ALPHA` plus `G_CC_DECALRGBA`.
With alpha test gated only on the compare bits, transparent texels kept their
black RGB and drew as full quads under Mario/Toads, and HUD glyphs stacked as
solid rectangles.

1-cycle batches now enable alpha test when `CVG_X_ALPHA` is set. COPY/FILL stay
opaque so `nok_bg` pal256 entries with a clear LSB still paint the sky.

`gbi_resolve_vtx_in_static_dl` now follows PaperBoat `GBIMiddleware.cpp`: skip
odd `G_VTX` pointers, recurse `G_DL` (push vs `G_DL_NOPUSH` branch), and keep
the TEXRECT command span so `pause_init` cannot hang. `GameEngine_OTRSigCheck`
rejects NULL, low, and odd addresses before `strncmp`. The interpreter resolves
leftover `__OTR__` `G_VTX`/`G_DL` pointers instead of treating path strings as
vertex memory. `GameEngine_HoldFrame` sleeps during `DISABLE_DRAW_FRAME`.

A 1-cycle `CVG_X_ALPHA` rectangle still submits, and a nested-list resolve
rewrites even OTR vertex paths without touching TEXRECT payloads.

## r19 near-plane building clip and START KSEG abort

The r18 Folium photo of `mac_01` shows punchthrough sprites, but Mario/Toad
hats are sheared off and the Dojo's near walls are missing while the roof
remains. Screen-space vertices carry N64 depth in 0..1 into
`Mtx_OrthoTilt(..., 0, 1)`. PICA clips fragments on those planes, so close
walls and sprite heads (depth ~1) and far walls (depth ~0) disappear. The
ortho volume now has slack (`-0.5` .. `1.5`).

START still hung because pause static lists can carry leftover N64 KSEG
`G_DL`/`G_VTX` pointers (`0x8xxxxxxx`). Recursing or `strncmp` there
data-aborts on ARM11. The walker and `OTRSigCheck` now refuse that range
before touching memory.
