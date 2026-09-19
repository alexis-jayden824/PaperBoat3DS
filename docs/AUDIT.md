# Repository and port audit

## Baseline

- Repository began empty on 2026-09-19.
- `main` contains only the project policy/readme bootstrap.
- Active development branch: `port/3ds`.
- No PaperBoat or libultraship source has been imported yet.
- No devkitARM build, emulator boot, or hardware boot has been performed yet.

## Known upstream shape

The target PaperBoat 1.0.1 codebase relies on Torch-LH and libultraship submodules. The exact upstream commit and complete submodule graph must be pinned during M3 before platform implementation is allowed to spread across the tree.

## Current bootstrap boundary

The M0 scaffold deliberately contains only:

- devkitARM/libctru Makefile structure;
- explicit citro3d/citro2d linkage for the intended platform stack;
- a minimal ARM11 entry point using both displays;
- legal asset exclusions and build instructions;
- the binding M0-M24 roadmap.

It does not claim that PaperBoat compiles, links, boots, renders, or runs on Nintendo 3DS.

## Next audit actions

1. Resolve and pin the authoritative PaperBoat 1.0.1 upstream source.
2. Record all submodule URLs and commits.
3. Produce a dependency inventory with one of: portable, replace, stub, disable, or investigate.
4. Establish the minimum libultraship subset needed by the game core.
5. Keep desktop UI, updater, dialog, and unrelated backend code out of the first ARM11 link target.

