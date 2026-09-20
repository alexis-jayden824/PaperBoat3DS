# M13 Overworld Integration

M13 is the point where the native 3DS shell begins consuming PaperBoat's real
overworld contract. This document describes the first integration checkpoint;
it does not mark the full milestone complete and does not replace PaperBoat's
game logic with a reimplementation.

## Pinned upstream anchors

The immutable PaperBoat `1.0.1` source at commit
`424c220f0863c29b9fe55cc674baceff88e9e14f` provides the choices used here:

| Upstream source | Contract used by the checkpoint |
|---|---|
| `src/state_demo.c` | A real world demo entry selects `mac_00`, entry 6. |
| `src/world/world.c` | `mac_00` uses the `nok_bg` background. |
| `src/state_world.c` | World stepping orders encounters, NPCs, player, items, effects, models, and camera. |
| `src/state_map_transitions.c` | Map entry returns control to `GAME_MODE_WORLD`. |
| `src/state_pause.c` | Pause returns to `GAME_MODE_WORLD`. |

CI cross-compiles those world/transition/pause sources plus camera math,
collision, the world table, and pause implementation for ARM11. That gate
detects target/compiler drift without vendoring thousands of upstream files.

## Bounded archive preflight

Confirming a file-select slot requests exactly these private O2R entries:

| Entry | Type and validation |
|---|---|
| `shapes/mac_00_shape` | Blob; name tables, tree offsets, node types, child/property spans, depth, and display-list references. |
| `collisions/mac_00_hit` | Blob; collision/zone headers, arrays, bounds, triangle spans, and vertex indices. |
| `shapes/mac_00_shape/vtx` | Vertex resource; exact 16-byte payload count. |
| `shapes/mac_00_shape/dlist_20` | F3DEX2 display-list sample; bounded commands and `G_ENDDL`. |
| `backgrounds/nok_bg` | Exact 296x200 CI8 texture. |
| `backgrounds/nok_bg_pal0` | Exact 256x1 RGBA16 palette. |

Every extraction has a fixed maximum size and is charged to the M6 scene
budget. Temporary archive bytes are released on every success/failure path.
The decoded 512x256 RGBA8 background remains only until synchronous GPU upload
and is then released. A missing or malformed entry leaves file select intact,
prints the precise failure, and permits A/START retry.

The owner-only archive acceptance run reports:

| Measurement | Result |
|---|---:|
| Shape nodes | 223 |
| Shape display-list references | 223 |
| Vertex resource | 3,211 |
| Collision groups / vertices / triangles | 110 / 727 / 873 |
| Zone groups / vertices / triangles | 18 / 76 / 69 |
| Sampled display-list commands | 19 |

These are structural checks over owner-generated resources. The archive and
its extracted proprietary bytes are never committed or uploaded.

## Native checkpoint flow

After successful preflight, a separate PICA200 texture receives `nok_bg` and
the state changes from loading to active. It is placed at LCD rectangle
`(52,20,296,200)`: the authentic 12-pixel inset inside the centered 320x240
Paper Mario canvas. Every New 3DS XL/LL still exposes the same 400x240 logical
top screen, so physical panel size requires no crop or alternate coordinates.

START toggles active/paused. Pause applies a visible translucent black overlay
without destroying the map texture; START resumes. L+R+START remains the
explicit checkpoint exit chord. The title and world textures are independent,
so a failed world allocation cannot invalidate the M12 title resources.

The M12.1 Folium result also exposed a prompt-combiner issue at `A:255`. M13
bakes PaperBoat's exact `(248,240,152)` RGB tint into a temporary PRESS START
upload buffer while preserving source alpha, then uses vertex alpha only for
the blink. This is covered by host tests and should be rechecked visually.

## Validation

Public and cross-compilation gates:

```sh
make m12-layout-test
make m10-graphics-test
make m13-world-test
make m13-core-check
make packages
```

Optional private validation:

```sh
sh tools/test_world_boot.sh build/m13-private /path/to/pm64.o2r
```

Folium should show the corrected title prompt, transition from a confirmed
slot to the centered `nok_bg`, report the exact private counts above, and dim/
restore the top screen across START pause/resume. Renderer rejects and frame
failures must remain zero.

## Remaining M13 work

This checkpoint does not yet render the map mesh or run gameplay. M13 remains
open until the pinned PaperBoat execution path drives:

- the complete map display-list/resource dependency set;
- camera state and projection;
- player, NPC, entity, item, and effect updates;
- collision queries and response;
- EVT/map scripts;
- pause UI and representative loading-zone transitions.

Until those gates have reproducible evidence, the project remains an
integration checkpoint rather than a playable native port.
