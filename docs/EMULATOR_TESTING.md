# Emulator Testing Policy

## Supported artifacts

- `PaperBoat3DS.3dsx` is the canonical Homebrew Launcher and hardware artifact.
- `PaperBoat3DS.3ds` is a secondary emulator QA artifact built from the same ELF.
- `PaperBoat3DS.cia` is an optional CFW-installable QA artifact built from the same ELF.

The `.3ds` variant exists because the Folium iOS frontend does not directly
import `.3dsx` files. It must remain a packaging difference only: source,
features, configuration, save formats, and platform behavior stay shared.

The `.cia` target is for controlled CFW testing. Installing a CIA modifies the
console's title database, so it is never required when `.3dsx` is sufficient.

## Confirmed Folium observations

The project owner confirmed the following on iOS:

- A `.3ds` image imported after selecting Folium's Nintendo 3DS core.
- Direct `.3dsx` import was not offered by the Folium frontend.
- DSP audio became operational after `dspfirm.cdc` was installed.

The DSP result validates the emulator environment, not the future PaperBoat ndsp
backend. `dspfirm.cdc` is user-supplied system firmware and must never be
committed, bundled, or distributed by this repository.

## Validation boundary

Folium is useful for rapid boot, display, input, filesystem, and later audio
regression checks when physical hardware is unavailable. It cannot establish
Old 3DS performance, memory ceilings, lifecycle behavior, timing accuracy, or
hardware compatibility. Roadmap milestones that require hardware remain blocked
until the project owner tests the matching `.3dsx` build on a real console.

Every emulator report should record the build SHA, Folium version, selected
core, iOS device, artifact format, DSP-firmware presence, steps, and outcome.

## Validation log

### 2026-09-19 - M0 package / M1 bootstrap

- Build commit: `6d0838eea40741b388c2bd3f5177fb8c22b032e2`
- Workflow run: `35456310434`
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- DSP firmware: present in the previously validated Folium environment
- Result: booted successfully and rendered the expected bootstrap text on both displays
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: START exit behavior, physical controls, suspend/resume, or real-hardware compatibility

The exact Folium version and iOS device model were not reported, so they remain
unknown rather than inferred from the screenshot.

### 2026-09-19 - M4 compatibility core

- Build commit: `e2ba067e4147276f29a15771043501a0ebe80b17`
- Workflow run: `35457581873`
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- Result: booted successfully; GFX, APT, and HID initialization reported OK
- Runtime observations: New 3DS detected, lifecycle active, SD logging active,
  configuration defaults loaded, and the optional archive correctly reported absent
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: START exit behavior, lifecycle transitions,
  real-SD archive access, or real-hardware compatibility

Folium reported application heap free space as `0 KiB` while linear memory was
available. The port treats this isolated emulator value as unavailable telemetry,
not proof of heap exhaustion; real-hardware logging remains authoritative.

### 2026-09-19 - M7 legal asset pipeline shell

- Build commit: `04db69223b32baf6fdddc0822886e8c7825c23a9`
- Workflow run: `35478862029`
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- Result: booted successfully; GFX, APT, and HID reported OK, lifecycle was
  active, the memory budget reported no pressure and zero failures, SD logging
  was active, and linear free space was 31,193 KiB
- Archive observation: engine and game archives reported missing because the
  external `SD_ROOT/3ds/PaperBoat3DS` payload was not installed into Folium's
  virtual SD; this is not an extraction-validation failure
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: virtual-SD archive discovery, physical input,
  touch accuracy, suspend/resume, or real-hardware compatibility

### 2026-09-20 - M8 input backend

- Build commit: `0735cd7bde2127b8559061a1b563b0f84ca7a6db`
- Workflow run: `35481886518`
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- Result: booted successfully as `0.8.0-m8`; detected the New 3DS profile,
  reported an active lifecycle and input gate, retained healthy memory-budget
  and SD-log diagnostics, and latched `Menu SELECT: REQUESTED`
- Archive observation: generated archives were not installed in Folium's
  virtual SD and were correctly reported missing
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: the complete button map, Circle Pad range,
  touch coordinates, lid lifecycle, clean START exit, or hardware behavior

### 2026-09-20 - M9 PICA200 renderer

- Runtime build: `3c98a1458c01` (`0.9.0-m9`)
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- Result: initialized citro3d and the compiled PICA shader, displayed the
  repeated RGBA8 checker, advanced at exactly two draws and nine vertices per
  frame, held command-buffer use near 0.4%, and reported zero frame failures
- Visual qualification: the second sail draw was counted but was not visibly
  distinguishable in the supplied capture; M10 changes that draw to a
  shade-only TEV program with depth disabled
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: scissor behavior, lifecycle restoration,
  physical PICA correctness, or Old 3DS performance

### 2026-09-20 - M10 libultraship graphics adapter

- Runtime build: `bc0139a2f292` (`0.10.0-m10`)
- Artifact: `PaperBoat3DS.3ds`
- Environment: Folium Nintendo 3DS core on iOS
- Result: reported the exact `Fast::GfxRenderingAPI` contract, two supported
  TEV shaders, one 256-byte texture, 4,449 presented frames, 8,898 draws,
  13,347 triangles, 40,041 vertices, zero rejects, and zero frame failures
- Visual result: the bright translucent sail was clearly visible; only one of
  the checker's two rectangle triangles survived in the capture
- Root cause: both same-frame draws reused vertex zero in one asynchronous
  streaming buffer, so the sail conversion overwrote the first panel triangle
  before PICA consumed it
- M11 correction: each draw reserves a non-overlapping span in a reset-on-frame
  bounded arena, flushes that written span, and reports high-water/overflow
  telemetry
- Archive observation: external archives were not installed in Folium's
  virtual SD and were correctly reported missing
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: corrected multi-draw output, O2R frame load,
  lifecycle restoration, physical PICA correctness, or Old 3DS performance

### 2026-09-20 - M11 archive-backed frame

- Runtime build: `d5a3b09c7eba` (`0.11.0-m11`)
- Artifact: private `PaperBoat3DS.3ds` plus owner-generated O2R payload
- Environment: Folium Nintendo 3DS core on iOS
- Pipeline result: `title_bg` resolved as 296x200 CI8, decoded to a 512x256
  RGBA8 texture, both archives reported available, and the renderer advanced
  with zero rejects, failures, and stream overflows
- Visual result: the complete expected title image appeared but was vertically
  inverted; M11's visual acceptance therefore remained open
- M12 correction: all imported-image quads now share a host-tested PICA V
  mapping instead of the inverted one-off M11 vertices
- Evidence: project-owner screenshot supplied in the development conversation
- Not established by this result: corrected orientation, interactive title
  flow, lifecycle restoration, physical PICA correctness, or Old 3DS performance

## M8 input check

Use the newest build marked `0.8.0-m8`. Exercise A, B, X, Y, L, R, D-Pad,
Circle Pad, SELECT, and touch while watching the live masks and coordinates.
SELECT must latch `Menu SELECT: REQUESTED` without adding an N64 bit. START
exits the diagnostic shell. Record any Folium overlay control that does not
reach the expected input; emulator mapping defects must not be mistaken for
libctru hardware behavior.

## M9 renderer check

Use the newest build marked `0.9.0-m9`. The top display should show a dark navy
background, a repeated teal/light checker panel, and a translucent triangular
sail. The bottom display must report `C3D: ready`, `Shader: PICA shbin OK`, an
8x8 RGBA8 texture, and a 324-byte VBO. Frame, draw, vertex, and cached-state
counters must increase; `fail` must remain zero. Capture both displays and the
exact build SHA. A Folium pass establishes an emulator rendering smoke test,
not physical PICA correctness or Old 3DS performance.

## M10 graphics-adapter check

Use the newest build marked `0.10.0-m10`. The top display should retain the
navy checker scene and show a clearly visible bright translucent triangular
sail. The bottom display must report `API: ready`, the exact
`Fast::GfxRenderingAPI` contract, two live TEV shaders, one texture, advancing
frames, two draws and three triangles per frame, zero unsupported shaders, and
zero frame failures. `Reject` must remain zero during the normal diagnostic.
Capture both displays and the exact build SHA. This verifies the bounded M10
adapter path in Folium; APT suspend/resume and hardware behavior remain real-
console gates.

## M11 first-frame check

The recorded `0.11.0-m11` private-bundle run found and decoded the expected
296x200 CI8 title background, reported both archives available, advanced the
one-draw/two-triangle path with zero rejects/failures, and displayed no fallback.
The image was vertically inverted, so the visual acceptance gate did not pass.
That result isolated the defect to textured-quad V mapping rather than archive
import or decoding; M12 centralizes and reverses the native mapping.

The bottom display must report `Frame: title_bg 296x200 CI8`, `O2R: ready`, a
512x256 RGBA8 GPU texture, advancing frames at one draw/two triangles/six
vertices per frame, zero unsupported shaders, zero rejects, zero failures, and
zero stream overflows. If the virtual-SD payload is deliberately removed, the
checker and the complete two-triangle panel plus sail must appear with
`Fallback: pm64.o2r missing`. Capture both success and fallback displays with
the exact build SHA. Folium does not close the physical lifecycle or Old 3DS
performance gates.

## M12 title/file-select check

Use the newest private bundle marked `0.12.1-m12.1`, with its legal O2R payload
installed at `3ds/PaperBoat3DS/` in Folium's virtual SD.

1. Confirm the background, logo, prompt, and copyright are all upright,
   centered, and neither mirrored nor cropped. The bottom display must report
   `Safe:320@x40`; expect equal black pillars around the centered 320x240
   fixed-art canvas. Folium's phone surround is not part of the 400x240 target.
2. Capture the title when the live prompt diagnostic reports `A:255`; PRESS
   START must be visibly pale yellow. A capture at `A:0` is the normal off
   phase.
3. Press A or START and confirm the display changes to four file-slot panels.
4. Move through all four slots with the Circle Pad or direction controls. The
   highlight must move once per press/deflection and remain inside the 2x2 grid.
5. Press A or START; the chosen border must turn green without a crash. Press B
   and confirm the title returns.
6. Confirm the bottom display reports `M12.1 Title Presentation`, title assets
   ready, the safe-area/UV line, zero rejects/failures, and zero stream
   overflows.
7. Exit with L+R+START. Preserve the exact build SHA and both-screen captures.

The simple panels are expected at M12. Missing save text/window decoration,
audio, and an overworld transition are documented milestone boundaries rather
than emulator failures. Folium still cannot establish physical lifecycle,
input, or Old 3DS performance behavior.
