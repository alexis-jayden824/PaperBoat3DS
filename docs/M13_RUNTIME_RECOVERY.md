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

The previous M13 check compiled nine upstream files independently and discarded
their objects. It did not prove runtime integration. The recovery check now
links the real entry loop, initialization, frame builder, world state,
player-input/physics, collision, EVT, and camera units into a single ARM11
relocatable object and verifies the authoritative symbols. This is the first
integration gate, not completion: the next gate is resolving the platform
closure and placing that object in the final ELF.

### Native resource adapter

`source/runtime_resources.c` now provides the real `ResourceGetDataByName`,
`ResourceGetSizeByName`, texture dimension, and `GameEngine_Get*Exact` symbols.
It returns stable writable blob, vertex, texture, and F3DEX2 display-list
storage, with bounded allocations and explicit unsupported-format errors.
Vertices are converted to native endian; display-list word pairs expand to
native pointer-width `Gfx` packets. Texture/palette bytes retain their original
byte order. Blob storage includes upstream's 16-byte zero padding.

The cache never evicts behind live game pointers. Its owner must stop all game
and GPU users before clearing it. It is limited to 1,024 resources, a 2 MiB
serialized-entry limit, and the existing scene memory budget. Archive lookup is
currently name-based and scans the directory on cache misses; CRC-name indexing,
engine/game archive routing, and an explicit map-lifetime policy remain needed
before full-runtime activation. Unsupported versions/types fail rather than
returning serialized bytes as if they were native objects.

`sh tools/test_runtime_resources.sh` links and executes the pinned upstream
`Shape_LoadFromRawData` against synthetic O2R data supplied by this adapter.
It verifies the resulting model's native display-list pointer and native vertex
fields, both endian modes, compressed/stored entries, extended command payloads,
stable mutable pointers, malformed inputs, memory rejection/retry, and teardown.
`SANITIZE=1` adds ASan/UBSan to that same integration test. The ARM11 check also
compiles the consumer's ABI assertions against the real pinned `Gfx` type.

This adapter is not yet bound by the default application loop. The shipped
application still uses the diagnostic scene; the new resource tests do not
establish a running world, renderer fidelity, or completion of M13.
