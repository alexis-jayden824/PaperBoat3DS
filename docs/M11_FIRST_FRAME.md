# M11 First Archive-Backed Frame

M11 proves one complete legal asset path from the user's generated
`pm64.o2r` to PICA200. It displays Paper Mario's 296x200 title background
centered on the 400x240 top screen. This is a deterministic static frame, not a
claim that the title state, display-list interpreter, input flow, audio, or game
loop is running.

## Fixed resource contract

The loader requests exactly these two entries:

| Entry | Expected resource | Bound |
|---|---|---:|
| `backgrounds/title_bg` | 296x200 `Palette8bpp` indices | 64 KiB |
| `backgrounds/title_bg_pal0` | 256x1 `RGBA16bpp` palette | 1 KiB |

Both resources must have a valid 64-byte OTR header, binary texture version 0,
matching body length, and the exact dimensions/formats above. RGBA16 palette
words retain the N64 big-endian RGBA5551 representation. Each CI8 texel is
expanded to RGBA8; unused space in the 512x256 power-of-two texture is cleared
transparent before upload.

The original ROM, extracted resources, and generated O2R archives are never
compiled into the executable, committed, or uploaded to public CI. The private
test bundle places user-generated archives beside the `.3dsx`; Folium must place
the same payload in its virtual SD.

## Bounded O2R reader

O2R is a ZIP container. Pulling libzip and its desktop transitive stack into the
ARM11 shell would exceed this checkpoint's needs, so M11 implements a narrow
reader with the following fail-closed policy:

- scan at most the ZIP comment tail for EOCD, then traverse the central
  directory with one 4 KiB cursor;
- stop as soon as both exact names are found rather than retaining the 4.6 MiB
  central directory or scanning all 60,826 entries;
- accept stored or raw-deflate data only and reject encryption and multi-disk
  archives;
- reject ZIP64 central-directory sizes/offsets, while accepting Torch's local
  ZIP64 size extra fields because the matching central records contain verified
  32-bit sizes;
- validate all offsets/additions against the archive size, enforce per-resource
  bounds, compare the local filename, require exact decompressed length, and
  verify CRC-32 before parsing;
- compile only the inflate subset from the immutable Torch commit already
  pinned by `upstream/PAPERBOAT.lock`.

Archive input, inflate state, extracted resources, and decoded pixels are
charged to the M6 archive, transient, and scene allocation classes. The CPU
RGBA buffer is released immediately after the renderer copies it into the
native texture. The persistent GPU allocation is 512 KiB; the 8x8 fallback
checker remains resident so failure behavior is always available.

## Renderer lifetime correction

The M10 Folium capture showed the sail and correct counters but only one checker
triangle. Both submissions converted vertices into the beginning of one linear
buffer before PICA consumed the queued first draw. M11 treats the buffer as a
per-frame arena: `StartFrame` resets a cursor, every draw receives a unique
span, the written cache range is flushed, and capacity exhaustion rejects the
draw. Diagnostics expose arena high-water and overflow counts.

The M11 success frame is one texture-times-shade draw: two triangles and six
vertices per presented frame. The fallback remains two draws, three triangles,
and nine vertices and now exercises the corrected same-frame arena.

## Failure behavior

Any missing archive/resource, invalid header or texture, unsupported ZIP
feature, size/offset violation, allocation failure, inflate error, CRC mismatch,
or GPU upload failure keeps the checker/sail diagnostic. The bottom screen and
SD log state the reason. No partially decoded texture is presented and all
temporary allocations are released.

## Reproducible validation

Run:

```sh
make fetch-upstream
make m9-renderer-test
make m10-graphics-test
make m11-frame-test
make packages
```

The public M11 suite generates legal synthetic stored and deflated archives
with the same local ZIP64-header shape as Torch output. It checks exact color
conversion, transparent padding, central-directory early exit, CRC rejection,
missing/invalid resources, bounded memory accounting, and complete release.
An optional local invocation of the resulting test binary can validate a
private `pm64.o2r`; that archive must never be attached to CI or a public issue.

M11 software completion requires all host checks plus native `.3dsx`, `.3ds`,
and `.cia` packages. Folium must then show the correctly oriented title frame
and zero failure/overflow telemetry. Real-hardware `.3dsx` testing remains
authoritative for PICA correctness, lifecycle recovery, and memory headroom.
