# M5 ARM11 Compatibility Ledger


M5 cross-compiles pinned PaperBoat sources as ARM11 objects before they are
linked into the application. Compatibility changes live in this repository;
the fetched upstream trees remain detached at the audited commits.

**Status: complete.** The final matrix passed in GitHub Actions run
`35474089074` (#63).


## ABI shims


### `OSIntMask`


The pinned libultraship headers declare `OSIntMask` as both `u32` and
`uint32_t`. On devkitARM/newlib those are distinct C types (`unsigned int` and
`unsigned long`) even though both are 32 bits, so GCC correctly reports a
conflicting typedef.


`include/libultraship/libultra/interrupt.h` shadows only that upstream header
for 3DS builds and uses `u32`, matching libultraship's `exception.h` and the
intended 32-bit N64 ABI. No structure size or calling convention changes.


### EEPROM declarations


The aggregate libultraship headers also redeclare the EEPROM entry points with
`int32_t`, while `os.h` uses the N64 aliases (`s32`/`u8`) and `int`. Those are
the same width on supported desktop hosts but not the same C type under
devkitARM/newlib. `include/libultraship/libultra/eeprom.h` preserves the
existing `os.h` signatures; `osWritebackDCache` deliberately retains its
upstream `int32_t` parameter because that declaration is already consistent.


The scoped `libultraship.h` and `libultra.h` facades route aggregate includes
through these two overlays. The pinned checkout is never patched.


### Upstream macro warnings


The full `common.h` graph redefines `aPoleFilter`, `ALIGN8`, and `ALIGNED8`
with equivalent port-side forms. The `main_pre.c` gate keeps all warnings
enabled but does not promote warnings to errors for this upstream-only unit;
the repository's native 3DS sources and foundation port slices remain under
`-Werror`.


`src/43F0.c` also uses the legacy `sins`/`coss` implicit-declaration path.
The pinned upstream CMake explicitly disables that diagnostic, so this one
object mirrors `-Wno-implicit-function-declaration`. The exception is not
applied to native 3DS sources.


## Validation history


- Run `35458786218`: pinned fetch succeeded; compilation stopped at the missing
  libultraship include root.
- Run `35458920570`: include discovery succeeded; compilation exposed the
  `OSIntMask` newlib typedef conflict documented above.
- Run `35459089023`: the scoped header shim passed the M5 ARM11 object gate and
  the complete `.3dsx`, `.3ds`, and `.cia` packaging workflow.
- Run `35473434192` (#57): `src/main_pre.c` passed through the complete
  `common.h` dependency graph, followed by successful `.3dsx`, `.3ds`, and
  `.cia` packaging.
- Run `35473665849` (#60): `src/43F0.c` passed as ARM11 code and all three
  package formats were produced successfully.
- Run `35474089074` (#63): the completed representative matrix passed, then
  the same ELF packaged successfully as `.3dsx`, `.3ds`, and `.cia`.


## Compile slices


- Foundation: `src/port/decode_yay0.c` and `src/port/libc_compat.c`.
- Game core: `src/main_pre.c` traverses PaperBoat's full `common.h` graph;
  `src/43F0.c` adds heap allocation, math/trigonometry helpers, string/number
  conversion, and static display-list construction. These compile without
  linking desktop backends into the 3DS shell.
- Runtime breadth: `src/evt/evt.c`, `src/state_title_screen.c`,
  `src/battle/camera.c`, and `src/entity/Switch.c` cover the event VM, game
  state, battle logic, and entity interfaces.
- Generated content: `src/world/area_mac/mac_00/settings.c` and `main.c`
  exercise generated map data, entry tables, scripts, and included common
  event fragments.

## M5 exit decision

The representative matrix covers every compile-time category named by the M5
gate. Desktop-only threading and filesystem implementations remain isolated by
the M3/M4 platform boundary. Runtime allocation budgets move to M6, legal O2R
generation and byte-order validation move to M7, and backend linkage proceeds
in the later renderer/input/audio milestones. This keeps M5 reproducible and
prevents object compilation from being mistaken for a playable game build.
