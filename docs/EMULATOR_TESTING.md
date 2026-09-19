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
