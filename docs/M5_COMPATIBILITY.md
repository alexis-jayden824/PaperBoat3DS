# M5 ARM11 Compatibility Ledger

M5 cross-compiles pinned PaperBoat sources as ARM11 objects before they are
linked into the application. Compatibility changes live in this repository;
the fetched upstream trees remain detached at the audited commits.

## ABI shims

### `OSIntMask`

The pinned libultraship headers declare `OSIntMask` as both `u32` and
`uint32_t`. On devkitARM/newlib those are distinct C types (`unsigned int` and
`unsigned long`) even though both are 32 bits, so GCC correctly reports a
conflicting typedef.

`include/libultraship/libultra/interrupt.h` shadows only that upstream header
for 3DS builds and uses `u32`, matching libultraship's `exception.h` and the
intended 32-bit N64 ABI. No structure size or calling convention changes.

## Validation history

- Run `35458786218`: pinned fetch succeeded; compilation stopped at the missing
  libultraship include root.
- Run `35458920570`: include discovery succeeded; compilation exposed the
  `OSIntMask` newlib typedef conflict documented above.
