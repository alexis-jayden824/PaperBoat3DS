# M7 Legal PC-Side Asset Pipeline

PaperBoat3DS contains no Paper Mario ROM or extracted Nintendo data. M7 keeps
extraction on the user's computer, rejects unsupported inputs before Torch is
built or run, and creates archives that can be copied to the SD card. Nothing
in this workflow uploads the ROM or `pm64.o2r` to GitHub.

## Archive boundary

| Archive | Contents | Source | Distribution rule |
|---|---|---|---|
| `paperboat.o2r` | PaperBoat fonts, shaders, and interface textures | Pinned PaperBoat `port/` tree | Generated locally from non-ROM upstream files |
| `pm64.o2r` | Extracted Paper Mario resources | User-supplied supported ROM through pinned Torch | Never committed, uploaded, or distributed |

The accepted ROM contract comes from PaperBoat 1.0.1's `config.yml` and is
mirrored in `upstream/ASSET_CONTRACT.json` so the verifier needs no YAML
dependency. The only supported input currently has SHA-1:

```text
3837f44cda784b466c9a2d99df70d77c322b97a0
```

The tool also requires the big-endian `.z64` header. Renamed, byte-swapped,
modified, truncated, or different-region images are rejected.

## Prerequisites

- Python 3.10 or newer
- Git
- CMake and a C/C++ compiler
- Internet access for the pinned source/dependency fetch
- A legally dumped supported Paper Mario ROM

Linux and macOS use `python3` in the examples. On Windows, use `py -3` from a
Developer PowerShell with CMake and a C++ compiler available.

## Prepare both archives

From the PaperBoat3DS repository root:

```sh
python3 tools/pb3ds_assets.py prepare /path/to/baserom.us.z64
```

The command performs this sequence:

1. Fetches PaperBoat, libultraship, and Torch at `upstream/PAPERBOAT.lock`.
2. Verifies the checkouts against `upstream/ASSET_CONTRACT.json`.
3. Hashes the ROM and stops unless it is the supported big-endian image.
4. Builds a PM64-only standalone Torch executable on the host.
5. Reproducibly packs `paperboat.o2r` from the pinned `port/` directory.
6. Runs Torch to generate `pm64.o2r`, then normalizes ZIP ordering, timestamps,
   and permissions.
7. Validates every archive member and writes `assets-manifest.json`.

Outputs are written under `build/assets/`, which is ignored by Git:

```text
build/assets/
  assets-manifest.json
  paperboat.o2r
  pm64.o2r
```

The command refuses to overwrite existing outputs. Use `--force` only when an
intentional rebuild should replace all three. `--jobs N` controls the Torch
build, `--no-fetch` uses already-fetched locked checkouts, and
`--torch /path/to/torch` uses a separately built executable.

## Stage an SD card

After preparation, copy the validated outputs with:

```sh
python3 tools/pb3ds_assets.py stage --sd-root /path/to/3ds-sd-card
```

This creates or uses `/3ds/PaperBoat3DS/` beneath the supplied SD root. It also
refuses to replace existing files unless `--force` is explicitly provided.
Before copying anything, staging revalidates both archives and requires every
archive hash and inventory digest to match `assets-manifest.json`. The ROM is
never copied to the SD card and extraction never runs on the 3DS.

## Validation performed

- Pinned PaperBoat, libultraship, and Torch revisions match the lock and asset
  contract.
- The pinned PaperBoat recipe set still contains the supported SHA-1 and emits
  `pm64.o2r`.
- O2R files are valid ZIP archives with passing per-entry CRCs.
- Absolute paths, parent traversal, case-colliding duplicates, encryption,
  unsupported compression, oversized entries, and excessive expansion are
  rejected.
- `paperboat.o2r` matches the pinned `port/` file inventory byte for byte.
- `pm64.o2r` contains `version` and `portVersion`; those records must match the
  supplied ROM header and PaperBoat 1.0.1.
- The manifest records archive SHA-256, content-inventory SHA-256, sizes, entry
  counts, the accepted ROM SHA-1, and exact upstream commits without recording
  a local ROM path.

## CI boundary

CI executes synthetic failure/success tests, audits all 235 pinned extraction
recipes, reproduces the 57-file non-ROM engine archive twice, validates it, and
builds the pinned PM64-only Torch executable. CI never receives a ROM and never
generates or uploads `pm64.o2r`. The only published M7 artifact is a small
non-proprietary validation manifest.

M7's implementation is complete when these checks pass. Final acceptance also
requires one user-run extraction from a legally dumped supported ROM and the
resulting manifest; the ROM and archives themselves must stay local.
