# M12 Title and File-Select Flow

M12 turns the legal M11 title-background path into the first interactive game-
facing checkpoint. It intentionally validates title resources, transitions,
and input without claiming that PaperBoat's full game loop, save system, or
display-list compositor is running.

## Legal resource contract

The bounded O2R reader requests three additional fixed entries from the user's
locally generated `pm64.o2r`:

| Entry | Expected resource | Extraction bound | Padded GPU texture |
|---|---|---:|---:|
| `title_screen/title_logo_img` | 200x112 RGBA32 | 96 KiB | 256x128 RGBA8 |
| `title_screen/title_press_start_img` | 128x32 IA8 | 8 KiB | 128x32 RGBA8 |
| `title_screen/title_copyright_img` | 144x32 IA8 | 8 KiB | 256x32 RGBA8 |

RGBA32 bytes retain their channel order. IA8 expands its high intensity nibble
and low alpha nibble to eight bits. Every decoded image uses transparent power-
of-two padding required by PICA200. A missing, malformed, oversized, checksum-
failing, or allocation-rejected resource fails closed and preserves the M11
background-only or checker/sail fallback.

The ROM, extracted assets, and O2R archives remain private local inputs. Public
tests generate synthetic textures with distinct first and last rows so decoder
orientation can be checked without copyrighted data.

## M11 orientation correction

The first M11 Folium capture proved that the expected 296x200 CI8 resource was
found, decoded, uploaded, and drawn, but showed it vertically inverted. The
resource decoder was retaining row order; the defect was the quad's V mapping
at the PICA boundary.

M12 centralizes textured rectangles in `pb_renderer_textured_quad`. With the
renderer’s bottom-left logical coordinate system, the lower edge receives
minimum V and the upper edge receives maximum V. Background, logo, prompt, and
copyright quads all use that helper. The renderer contract locks the exact
half-texel inset and the corrected bottom/top mapping, preventing future scenes
from restoring the one-off M11 inversion.

## Interaction contract

| Screen | Input | Result |
|---|---|---|
| Title | A or START | Enter file select |
| File select | Circle Pad or C/D directions | Move within the 2x2 slot grid |
| File select | A or START | Confirm the highlighted slot |
| File select | B | Return to title |
| Either | L+R+START | Exit the checkpoint shell |

Circle Pad movement is edge-triggered with a neutral-release latch, so holding
one direction cannot race through slots every frame. Direction buttons already
arrive through the audited M8 N64 mapping. Prompt alpha follows a deterministic
32-frame cadence. Transition, selection, and confirmation events are recorded
in the SD log.

Confirming a slot remains on file select and marks it green. That is the M13
handoff point, not a failed world load.

## Rendering boundary and known defects

The title screen uses authentic archive textures and the M10 texture-times-
shade program. File select presently overlays four native bounded panels at
the upstream 2x2 slot coordinates. This validates transition, focus, confirm,
and return behavior while these visible differences remain explicit:

- no save-file reads, writes, names, progress, or recovery until M15;
- no upstream font, message, window-frame, or full display-list compositor;
- no title/file-select audio until M14;
- no confirmed-slot transition into the overworld until M13;
- no bottom-screen configuration frontend until M16;
- physical PICA, suspend/resume, and memory behavior still require hardware.

## Reproducible validation

Run:

```sh
make fetch-upstream
make m9-renderer-test
make m10-graphics-test
make m11-frame-test
make m12-flow-test
make packages
```

The M12 host suite covers RGBA32 and IA8 expansion, transparent padding,
first/last-row preservation, deflate and stored archives, missing/invalid/CRC
failure, allocation release, prompt timing, navigation boundaries, stick
latching, confirm/reset, and B return. The exact graphics-interface test also
submits both title and file-select scenes and checks draw/triangle accounting.

Folium must next show an upright background and upright title textures, accept
the transition/input matrix above, retain zero rejects/failures/stream
overflows, and preserve the documented fallback. Real New 3DS XL/LL testing
remains authoritative for lifecycle and physical input; Old 3DS evidence is
still required for the performance baseline.
