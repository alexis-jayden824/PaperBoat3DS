#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Canonical coordinates (applied once, not per-sprite):
 * - PaperBoat/N64 source: 320×240, origin top-left.
 * - Top LCD: 400×240; source is centered with 40 px pillars each side.
 * - Clip Y is inverted once (pb_gfx_invert_y). Texture T is upright once
 *   (pb_gfx_upright_t). Do not also flip geometry, UVs, or matrices.
 * - PICA render target is 240×400; citro3d display transfer presents it
 *   upright on the 400×240 LCD. CPU mapping uses 400×240 top pixels.
 */
#define PB_GFX_TOP_WIDTH 400U
#define PB_GFX_TOP_HEIGHT 240U
#define PB_GFX_BOTTOM_WIDTH 320U
#define PB_GFX_BOTTOM_HEIGHT 240U
#define PB_GFX_SOURCE_WIDTH 320U
#define PB_GFX_SOURCE_HEIGHT 240U
#define PB_GFX_PILLAR_PX 40U
#define PB_GFX_TEX_MAX 256U
#define PB_GFX_TEX_SLOTS 4U

#define PB_GFX_FMT_RGBA8888 0U

typedef struct {
    float x;
    float y;
    float z;
    float w;
    float s;
    float t;
    uint32_t color;
} PBGfxVertex;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t format;
    uint16_t slot;
} PBGfxTexDesc;

typedef struct {
    uint32_t frames;
    uint32_t dl_submits;
    uint32_t verts_in;
    uint32_t verts_clipped_out;
    uint32_t verts_kept;
    uint32_t tex_uploads;
    uint32_t tex_bytes;
    uint32_t cmd_begins;
    uint32_t last_clear_rgba;
    bool pica_ready;
    bool depth_enabled;
    bool invert_y;
    const void *last_dl;
    size_t last_dl_bytes;
} PBGfxDiag;

bool pb_gfx_ready(void);
bool pb_gfx_init(void);
void pb_gfx_shutdown(void);
void pb_gfx_clear_top(uint8_t red, uint8_t green, uint8_t blue);
void pb_gfx_present(void);

bool pb_gfx_invert_y_enabled(void);
float pb_gfx_invert_y(float clip_y);
float pb_gfx_upright_t(float t);
bool pb_gfx_vertex_in_clip(const PBGfxVertex *vertex);
void pb_gfx_map_source_to_top(float source_x, float source_y, float *top_x,
                              float *top_y);
bool pb_gfx_project_vertex(const PBGfxVertex *vertex, float *top_x,
                           float *top_y);

unsigned pb_gfx_submit_vertices(const PBGfxVertex *verts, unsigned count);
void pb_gfx_submit_dl(const void *display_list, size_t bytes);
bool pb_gfx_tex_upload(const PBGfxTexDesc *desc, const void *pixels,
                       size_t bytes);
void pb_gfx_set_depth_enabled(bool enabled);
void pb_gfx_set_scissor_source(int x0, int y0, int x1, int y1);
void pb_gfx_begin_frame(void);
void pb_gfx_query(PBGfxDiag *diag);

#ifdef __cplusplus
}
#endif
