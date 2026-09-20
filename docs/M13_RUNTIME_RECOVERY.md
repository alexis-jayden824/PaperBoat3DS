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
draws, 344,591 triangles, 1,033,773 vertices, 1,306 runtime resources,
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
