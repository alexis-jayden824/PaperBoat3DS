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
