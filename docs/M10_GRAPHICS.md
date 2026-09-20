# M10 libultraship Graphics Integration

M10 connects the M9 citro3d renderer to the exact
`Fast::GfxRenderingAPI` interface from the libultraship commit pinned in
`upstream/PAPERBOAT.lock`. The 3DS application now drives its diagnostic scene
through that interface rather than calling a private smoke-render function.
No desktop window, SDL, OpenGL, Vulkan, Direct3D, Metal, or runtime shader
compiler is linked into the ARM11 executable.

## Exact contract and ownership

`PB3DS::GfxRenderingAPI3DS` is a concrete final subclass of the pinned C++
interface. A host compile-time assertion fails if any pure virtual method is
missing. A narrow C boundary lets the existing native application shell own
the adapter without converting the platform bootstrap to C++.

The responsibilities are intentionally separated:

| Layer | Responsibility |
|---|---|
| `gfx_bridge` | Portable combiner decoding, vertex-layout validation, bounded texture IDs, lifecycle state, and telemetry |
| `GfxRenderingAPI3DS` | Exact libultraship method surface and deterministic capability policy |
| `renderer_3ds` | citro3d target, PICA state, TEV setup, texture upload, rotated viewport/scissor, and linear streaming VBO |
| APT hook | Stops frame admission while suspended/asleep and reopens it only after restore/wakeup |

The adapter reports a 1024-pixel maximum texture dimension, a 0..1 clip-depth
range, a logical 400x240 framebuffer, and one default screen framebuffer.
Viewport and scissor rectangles are validated in logical coordinates and then
rotated to the native 240x400 PICA target.

## Bounded resources

- Texture IDs come from a 32-entry registry. Uploads require power-of-two RGBA8
  dimensions from 8 through 1024, are Morton-swizzled, and replace an existing
  native texture without leaking the old allocation.
- Shader plans use a fixed 64-entry cache; there is no runtime allocation or
  source compilation for a combiner.
- Each draw is limited to 384 triangles and 64 KiB of source vertex data. The
  native conversion buffer is one fixed linear allocation sized for 1,152
  vertices. M11 resets it once per frame and gives each draw a non-overlapping
  span; the earlier per-draw overwrite was unsafe while PICA consumed queued
  commands asynchronously.
- Draws are rejected unless their float count exactly matches libultraship's
  active layout: `position4 + matrix-slot1`, optional UV pairs, and optional
  RGB/RGBA shade.

## PICA combiner baseline

PICA200 has a programmable vertex processor but no desktop fragment shader.
M10 therefore decodes libultraship's two 64-bit shader keys and translates a
small, explicit one-cycle baseline to texture-environment stages:

- vertex shade replace;
- texture 0 replace;
- texture 0 multiplied by vertex shade;
- optional vertex alpha and texture-0 clamp flags.

The adapter identifies texture use, shade use, input count, alpha, cycle count,
and exact vertex stride from the same packed values consumed by the pinned
interpreter. Two-cycle combiners, texture 1, fog, noise, thresholds, grayscale,
mask/blend replacement textures, primitive depth, LOD/mip LOD, lighting,
texgen, palettes, and custom Prism shaders are explicitly unsupported at this
checkpoint. They increment rejection/unsupported telemetry instead of being
silently approximated. Subsequent milestones can add a feature only with a
matching TEV or shader implementation and tests.

The diagnostic uses two accepted combiner plans. Its checker panel is rendered
with texture-times-shade, then its bright translucent sail is rendered with a
shade-only TEV stage and depth disabled. This makes the second shader binding
and draw visually distinct, fixing the ambiguous M9 Folium overlay while still
remaining independent of copyrighted game assets.

## Framebuffers and transforms

The default top-screen color/depth target is implemented. Offscreen framebuffer
creation/copy/readback, framebuffer-as-texture, MSAA resolve, depth readback,
and partial depth clear are rejected and counted because M10 has no evidence
that PaperBoat's first frame needs them. They must be implemented before use,
not represented as successful no-ops.

The pinned base class accepts and retains transform, lighting, combiner, and
custom uniform blocks. M10's diagnostic supplies already projected screen
coordinates. M11 establishes the first legal archive-backed image through the
same texture/draw path. M12 uses that bounded path for a native title/file-
select checkpoint; applying the game matrix palette and expanding the combiner
subset for full live display lists remains later integration work. This
limitation is recorded rather than hidden behind a gameplay claim.

## Validation

Run the portable bridge and exact-interface suites after fetching the pinned
sources:

```sh
make fetch-upstream
make m10-graphics-test
```

The first suite tests shader-key decoding, unsupported-feature reasons, exact
vertex sizes, lifecycle gates, viewport/scissor bounds, the fixed texture
registry, upload accounting, and integrated draw telemetry. The second compiles
the real adapter against the pinned header, proves it is non-abstract, and
exercises the diagnostic through both its C++ and C entry points.

The software gate passed at merge
`4b5e6faef11ef469953a86d1ca84ecde32844d3a`: post-merge CI run 114 compiled and
linked the adapter with devkitARM, passed both M10 host suites, and produced
non-empty `.3dsx`, `.3ds`, and `.cia` packages from the same ELF. The archived
build has SHA-256
`c378d54cb66bddd0c00676843b316e87c8e172fe9a127e1b3e2a38b0713c5517`.

Folium build `bc0139a2f292` showed the clearly visible sail, exact two-draw and
three-triangle ratios, zero failures, zero rejects, and zero unsupported
shaders. It also showed only half of the checker rectangle. That evidence
identified a native streaming lifetime defect rather than an adapter-contract
failure; M11 fixes it with non-overlapping same-frame spans and adds overflow
telemetry. Real hardware remains authoritative for APT lifecycle recovery,
PICA behavior, memory headroom, and Old 3DS performance.
