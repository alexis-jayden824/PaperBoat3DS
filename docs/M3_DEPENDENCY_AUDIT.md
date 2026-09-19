# M3 PaperBoat Dependency Audit

## Audited baseline

The port is bound to PaperBoat `1.0.1` (Mulberry Bravo), commit
`424c220f0863c29b9fe55cc674baceff88e9e14f`. Its two upstream submodules are
also pinned in `upstream/PAPERBOAT.lock`:

- libultraship `7aa03b6c830b059e3ddd6ad20d3f289c5f406161`
- Torch-LH `106f4e3056f07fe3a8758bb14a060f8423b6877f`

The audited PaperBoat tree contains 7,039 regular files. The largest source
groups are 3,181 world files, 973 battle files, 239 effect files, 1,428
headers, 113 port files, 235 US asset YAML files, and 57 files in the
PaperBoat runtime archive. These immutable references satisfy M3 without
copying thousands of upstream files before the M4 compatibility boundary is
ready.

## Dependency classification

### Game and generated core

Candidate ARM11 game-core inputs are the C sources under `src/` plus the
matching `include/` tree. PaperBoat's desktop build already excludes
`src/port/`, `src/os/`, `src/boot/`, effect display-list data replaced by OTR,
region-specific sources, and several include-only translation units. The 3DS
source selector must preserve those exclusions rather than compiling every C
file blindly.

The game loop enters through `boot_main`, `step_game_loop`, and
`gfx_draw_frame`. It still expects NuSystem/libultra types, global state,
display-list submission, audio hooks, controller state, save storage, and
resource lookup. Those interfaces form the M4 compatibility contract.

### Minimal compatibility candidates

The following existing PaperBoat files are useful starting points but require
3DS review rather than unconditional reuse:

- `src/port/init_globals.c`: initializes decomp global state.
- `src/port/libc_compat.c`: supplies N64-era libc compatibility helpers.
- `src/port/nusys_overrides.c`: documents the required NuSystem/libultra
  surface, but currently delegates graphics, input, saves, and timing to the
  desktop engine.
- `src/port/gfx_frame.c`: exposes the game-to-renderer frame boundary.
- `src/port/ld_addr.c`, `effects_shim.c`, `decode_yay0.c`, and
  `shape_loader.c`: bridge generated/decomp data into the port runtime.

M4 will implement the smallest 3DS-native versions of these interfaces. It
will not transplant the desktop engine wholesale.

### Desktop runtime to replace or isolate

`src/port/Game.cpp` enters through SDL and submits frames through
libultraship/Fast3D. `src/port/Engine.cpp` directly depends on SDL2, ImGui,
Fast3D, libultraship resource/window/controller/audio systems, filesystem,
threads, desktop crash handling, JSON, and UI configuration. The related
desktop UI, file-picker, web, Android, mod-loader, and updater paths are not
part of the initial 3DS runtime.

The full libultraship dependency graph is also too broad for the initial Old
3DS baseline. The 3DS port therefore targets its public behavioral contracts
with a minimal compatibility layer and dedicated PICA200, HID, ndsp, and SDMC
backends. Any reusable platform-neutral libultraship component will be added
individually only after its memory and dependency cost is measured.

### PC-side asset tooling

Torch-LH remains a host-side tool. It parses a legally supplied Paper Mario
ROM and produces `.o2r` resources; it never runs on the 3DS and is never
linked into the ARM11 application. PaperBoat's `assets/yaml/us/` recipes and
the pinned Torch PM64 factories define the legal extraction input. M7 will
wrap that flow with format/hash validation while keeping generated archives,
ROMs, and Nintendo assets out of version control.

## Platform boundary

| Concern | Reuse | 3DS implementation | Excluded initially |
|---|---|---|---|
| Game logic and scripts | PaperBoat C core | ABI/compiler fixes | Other regions |
| Global initialization | `init_globals.c` model | Bounded ARM11 startup | Desktop relaunch |
| Resource contracts | OTR/O2R names and schemas | Read-only archive/cache layer | On-device extraction |
| Rendering | N64 display-list intent | citro3d/PICA200 translator | SDL/OpenGL/Vulkan |
| Input | `OSContPad` contract | HID mapping | SDL controller backend |
| Audio | Game mixer contract | ndsp backend | Desktop audio devices |
| Saves/config | PaperBoat data model | Versioned SDMC storage | Native file dialogs |
| Menu | Configuration model | Bottom-screen UI at M16 | ImGui frontend |
| Mods | Resource/config subset later | Explicit bounded support | DLL/TCC loaders |

## Known ARM11 blockers

- Upstream runtime assumes C++20 facilities, `std::filesystem`, threads,
  desktop dynamic loading, and a much larger dependency/memory budget.
- Renderer entry points are coupled to Fast3D/libultraship rather than a small
  C display-list contract.
- NuSystem overrides contain desktop engine calls and placeholder N64 OS
  functions that need deliberate 3DS semantics.
- Thousands of generated translation units require batching and section
  garbage collection to keep command lines, link time, and memory manageable.
- O2R archive indexing/caching must be profiled on Old 3DS before the full
  resource manager is selected.
- Upstream stack-trace and crash paths use desktop APIs unavailable on 3DS.

## M4/M5 handoff

1. Define a C-only `libultraship-3ds` compatibility surface for logging,
   configuration, archive lookup, timing, controller state, and frame/audio
   submission.
2. Add a generated manifest that selects upstream game sources using the same
   exclusions as PaperBoat 1.0.1.
3. Compile a deliberately small core slice first: types/globals, heap helpers,
   event/runtime primitives, and a stubbed frame boundary.
4. Expand the slice only when unresolved symbols are classified and assigned
   to an existing milestone backend.
5. Keep Torch and all extraction dependencies on the host side.

M3 is complete when this pin and classification are committed. It does not
claim that the game core compiles; that is the M5 acceptance gate.
