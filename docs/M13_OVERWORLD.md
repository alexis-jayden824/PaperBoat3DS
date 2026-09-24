# M13 Diagnostic Overworld Scaffold

This document records the custom `PBWorldScene` checkpoint that exposed the
archive, memory, input, and renderer defects found during early M13 testing. It
consumes PaperBoat/Torch resources from the owner's legal O2R archive; no ROM or
extracted asset is embedded in the executable or repository.

It is **diagnostic scaffolding, not the M13 gameplay implementation**. Folium
testing showed that its hand-written movement, collision, camera, actors, and
partial display-list interpretation do not reproduce Paper Mario faithfully.
Those systems must not be expanded into a parallel game engine. The recovery
plan in `M13_RUNTIME_RECOVERY.md` replaces them with the pinned upstream runtime.

## Pinned upstream contract

The immutable PaperBoat `1.0.1` source at commit
`424c220f0863c29b9fe55cc674baceff88e9e14f` anchors the entry and world-state
choices:

| Upstream source | M13 contract |
|---|---|
| `src/state_demo.c` | Begin in `mac_00`, entry 6. |
| `src/world/world.c` | Use `nok_bg` behind the Toad Town maps. |
| `src/state_world.c` | Keep player, entity/script, collision, camera, and render updates ordered. |
| `src/state_map_transitions.c` | Fade and return to world state after a map entry. |
| `src/state_pause.c` | Pause freezes world simulation and resumes in place. |

CI formerly cross-compiled a small selection of world sources but did not link
or call them. That check was insufficient and has been replaced by a linked
ARM11 runtime slice containing the actual entry loop, player update/input,
world state, and camera symbols.

## Authentic map and texture path

The loader scans the complete `mac_00` and `mac_01` shape namespaces, validates
their trees, resolves every referenced display list and vertex resource, and
translates the required F3DEX2 matrix, geometry, vertex, nested-display-list,
and triangle commands into bounded native triangles. It discovers the maps'
`mac_tex` texture set, decodes the used RGBA, CI, intensity, and
intensity/alpha formats, and uploads all pixels through the existing PICA200
graphics adapter. Unsupported or malformed data fails closed.

Owner-only validation reports:

| Map | Nodes / leaves / display lists | Source vertices | Native triangles | Textures | Collision groups / vertices / triangles |
|---|---:|---:|---:|---:|---:|
| `mac_00` | 223 / 174 / 223 | 3,211 | 2,040 | 41 | 110 / 727 / 873 |
| `mac_01` | 206 / 148 / 206 | 3,624 | 2,210 | 48 | 98 / 567 / 684 |

The decoded background and actor pixels are freed immediately after synchronous
GPU upload. Geometry and collision remain scene-owned. Measured owner-archive
scene peaks are about 1,379 KiB for `mac_00` and 1,369 KiB for `mac_01`, within
the M6 scene policy.

## Diagnostic coverage

The scaffold provides:

- authentic `mac_00` and `mac_01` geometry, textures, and `nok_bg`;
- Mario raster frames, Circle Pad/D-pad movement, facing, and collision slide;
- floor sampling, wall rejection, and a smooth follow camera;
- the `mac_00` sign interaction on A and a collectible Star Piece;
- 30-frame fades, automatic entry walking, and bidirectional
  `mac_00`/`mac_01` loading-zone transitions;
- START pause/resume with frozen simulation and a visible dim overlay;
- live map, position, floor, script, transition, renderer, and memory telemetry.

The sign and Star Piece are test doubles, not upstream entity/script coverage.
They are retained only to exercise resource lifetime, input, pause, and draw
submission while the real runtime path is connected.

## Bounded resources and failure behavior

The world has fixed limits for display lists, source vertices, native triangles,
textures, colliders, and collision triangles. Archive extraction is bounded and
charged to the M6 memory monitor. A missing entry, unsupported command, invalid
index, capacity overflow, allocation failure, or GPU-upload failure reports a
precise error and remains retryable from file select. The title and world own
independent GPU resources.

The renderer arena is expanded for this milestone to 4,096 triangles and
576 KiB of streamed source data per frame. The texture registry is 64 entries.
Both remain fixed allocations with overflow/rejection telemetry.

## Validation

Public and native gates:

```sh
make m12-layout-test
make m10-graphics-test
make m13-world-test
make m13-core-check
make packages
```

Owner-only archive validation:

```sh
sh tools/test_world_boot.sh build/m13-private /path/to/pm64.o2r
sh tools/test_world_scene.sh build/m13-scene-private /path/to/pm64.o2r
```

The scene suite covers both maps, both directions of entry protection, archive
formats, textures, collision, sign interaction, Star Piece collection, fades,
transitions, pixel release, and complete scene release. The private run passes
845 checks; the focused graphics adapter passes 103 checks. Proprietary input
never enters CI or source control.

The first completed Folium candidate exposed an ARM11-only stack fault at file
confirmation: the target reserves a 32 KiB main-thread stack, while the original
scene loader placed 138,704 bytes of temporary index/shape state in one frame.
Those workspaces now use the tracked transient heap and return to zero after
every load. Host stack-usage output measures the corrected loader frame at
2,416 bytes. This is a functional correction, not a larger stack reservation.

The next Folium run reached a stable world and confirmed advancing frame/draw
counters, zero renderer failures, and working pause. It also exposed three
render/input defects hidden by host-only checks: world vertices discarded
camera-space homogeneous W, so the GPU interpolated textures affinely; actor
billboards could lose their complete footprint to the contact floor's depth;
and only Circle Pad axes drove the slice even though the UI advertised D-pad
movement. World vertices now retain distance as clip W for perspective-correct
interpolation, the actor pass draws readably over its contact floor, and a
neutral Circle Pad falls back to the physical D-pad. Portable coverage includes
the D-pad movement path; the corrected native presentation remains a Folium
acceptance gate.

## Folium acceptance

With `pm64.o2r` and `paperboat.o2r` installed in Folium's virtual SD:

1. Confirm a file slot and verify the view fades into textured `mac_00` with
   Mario visible, rather than the earlier background-only checkpoint.
2. Move with the Circle Pad or D-pad. Verify Mario stays on the map, collides
   with solid geometry, and the camera follows smoothly.
3. Approach the sign and press A. Verify the interaction panel opens; close it
   with A or B.
4. Collect the Star Piece near `(-420, 20, 410)` and verify the bottom-screen
   `Star` field changes from `live` to `got`.
5. Reach the east loading zone in `mac_00`. Verify fade-out, `mac_01` entry 0,
   automatic walk-in, and fade-in. Return through the west loading zone and
   verify `mac_00` entry 1 without an immediate bounce-back.
6. Pause and resume with START in each map. Position, collection state, and
   transition counters must remain stable while paused.
7. Confirm zero renderer rejects, frame failures, unsupported commands, stream
   overflows, and memory allocation failures. Preserve both-screen captures and
   `PaperBoat3DS.log`, then exit with L+R+START.

These steps remain useful regression checks for the scaffold, but they no
longer accept or close M13. The upstream-runtime gates in
`M13_RUNTIME_RECOVERY.md` are authoritative.
