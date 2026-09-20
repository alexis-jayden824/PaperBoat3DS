# M9 PICA200 Renderer Foundation

M9 establishes the native rendering boundary that M10 will adapt to pinned
libultraship. It deliberately renders a deterministic diagnostic scene rather
than claiming a Paper Mario frame. The scene proves that the same ARM11 binary
can initialize citro3d, submit PICA200 commands, use a compiled vertex shader,
upload and sample a texture, draw a linear-memory vertex buffer, and present a
400x240 image on the top display.

## Implemented contract

`include/pb3ds/renderer.h` is portable C and contains no libctru or citro3d
types. `source/renderer.c` implements the deterministic portions so they can be
tested on a host compiler. `source/renderer_3ds.c` owns the platform objects and
is the only layer that includes citro3d.

- The logical gameplay viewport is 400x240 with a bottom-left origin. It maps
  to the PICA framebuffer's rotated 240x400 coordinates.
- All native PICA texture formats are enumerated with exact bits-per-pixel
  sizing. Dimensions must be powers of two from 8 through 1024, matching
  citro3d's texture allocation contract.
- Row-major RGBA8 uploads are converted to the PICA ABGR byte layout and
  Morton-swizzled in 8x8 tiles. Source and destination bounds are checked.
- Vertex-buffer size calculations reject zero-sized and overflowing requests.
  The diagnostic VBO is allocated from linear memory.
- Cull, depth, blend, filter, and wrap state use a validated, deduplicating
  cache. Invalid state is counted instead of silently reaching the GPU.
- `source/renderer.v.pica` is assembled by Picasso during the normal Makefile
  build. Its generated `.shbin` is linked into the application; no runtime
  desktop shader compiler is required.
- Runtime counters expose frames, draw calls, vertices, state changes/cache
  hits, rejected commands, resource sizes, frame failures, and peak citro3d
  command-buffer utilization.

The diagnostic top screen clears to dark navy and draws a repeated teal/light
checker panel plus a second translucent sail submission. This intentionally
exercises texture coordinates, repeat sampling, vertex color modulation,
depth, alpha blending, and two draws. Folium build `3c98a1458c01` confirmed the
checker, compiled shader, exactly two draws and nine vertices per frame, stable
command use, and zero failures. The intended sail was not visually distinct in
that capture even though its draw counter advanced. M10 resolves that ambiguity
by binding a shade-only TEV program and disabling depth for the overlay.

## Pinned libultraship handoff

The M9 boundary was checked against `Fast::GfxRenderingAPI` at the exact
libultraship commit in `upstream/PAPERBOAT.lock`. M10 will implement that C++
interface without pulling desktop window, OpenGL, Vulkan, DirectX, Metal, SDL,
or ImGui backends into the ARM11 link.

| Pinned graphics contract | M9 foundation | M10 work |
|---|---|---|
| `Init`, `StartFrame`, `EndFrame`, `FinishRender` | Owned citro3d lifecycle and top target | Attach frame calls and APT state |
| `GetMaxTextureSize`, `UploadTexture` | 1024 limit, checked RGBA8 conversion and upload | Texture-ID registry and eviction |
| `SetSamplerParameters` | Nearest/linear and all PICA wrap modes | Translate N64 `cms`/`cmt` flags |
| Depth, alpha, and cull setters | Validated PICA state model and cache | Translate interpreter state exactly |
| `SetViewport`, `SetScissor` | Tested logical-to-rotated viewport conversion | Add scissor application and edge cases |
| Shader create/load/lookup | Reproducible Picasso shader build and binding | Map combiner IDs to PICA vertex variants and TEV stages |
| `DrawTriangles` | Linear VBO and draw proof | Bounded streaming/ring-buffer uploads |
| Framebuffer operations | Native color/depth top target | Add only the offscreen operations PaperBoat actually uses |

PICA200 has a programmable vertex processor but no desktop-style fragment
shader. M10 must therefore translate supported Fast3D combiner programs to
PICA texture-environment stages and explicitly reject or fall back for an
unsupported program. It must not pretend that GLSL can be compiled on-device.

## Reproducible validation

Run the portable contract suite with:

```sh
sh tools/test_renderer_contract.sh
```

The suite validates every texture format and size, swizzle uniqueness and tile
boundaries, RGBA-to-ABGR conversion, viewport rotation, pipeline rejection,
state deduplication, buffer overflow handling, and diagnostic status strings.
CI runs the same suite before compiling the shader and all three packages.

Software acceptance requires:

1. all host renderer checks pass;
2. Picasso assembles `renderer.v.pica` and devkitARM links the backend with
   warnings treated as errors;
3. `.3dsx`, `.3ds`, and `.cia` packages are produced from the same ELF;
4. the M9 target, checker, and two submissions render in Folium with advancing
   counters and no frame failure.

Real hardware remains authoritative for PICA behavior, lifecycle recovery,
command-buffer headroom, linear-memory impact, and Old 3DS performance. Those
results must be recorded separately and are not inferred from CI or Folium.

## Primary platform references

- [devkitPro simple triangle example](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/simple_tri)
- [devkitPro textured cube example](https://github.com/devkitPro/3ds-examples/tree/master/graphics/gpu/textured_cube)
- [citro3d texture interface](https://github.com/devkitPro/citro3d/blob/master/include/c3d/texture.h)
- [Picasso shader assembler](https://github.com/devkitPro/picasso)
