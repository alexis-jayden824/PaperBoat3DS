#include "pb3ds/gfx.h"
#include "pb3ds/log.h"

#include <math.h>
#include <string.h>

#ifdef __3DS__
bool pb_gfx_pica_init(void);
void pb_gfx_pica_shutdown(void);
bool pb_gfx_pica_begin(uint32_t clear_rgba);
void pb_gfx_pica_end(void);
bool pb_gfx_pica_upload_rgba(uint16_t slot, uint16_t width, uint16_t height,
                             const void *pixels, size_t bytes);
void pb_gfx_pica_cpu_clear(uint8_t red, uint8_t green, uint8_t blue);
void pb_gfx_pica_swap(void);
void pb_gfx_pica_set_depth(bool enabled);
void pb_gfx_pica_set_scissor(int x0, int y0, int x1, int y1);
void pb_gfx_pica_draw_tris(const PBGfxVertex *verts, unsigned count);
#endif

static bool g_ready;
static bool g_frame_open;
static PBGfxDiag g_diag;

static bool is_power_of_two(uint16_t value) {
    return value != 0U && (value & (uint16_t)(value - 1U)) == 0U;
}

static uint32_t pack_rgba(uint8_t red, uint8_t green, uint8_t blue) {
    return ((uint32_t)red << 24) | ((uint32_t)green << 16) |
           ((uint32_t)blue << 8) | 0xFFU;
}

bool pb_gfx_ready(void) {
    return g_ready;
}

bool pb_gfx_init(void) {
    memset(&g_diag, 0, sizeof(g_diag));
    g_diag.invert_y = true;
    g_diag.depth_enabled = true;
#ifdef __3DS__
    g_diag.pica_ready = pb_gfx_pica_init();
    g_ready = true;
    if (g_diag.pica_ready) {
        pb_log(PB_LOG_INFO, "gfx", "citro3d ready");
    } else {
        pb_log(PB_LOG_WARNING, "gfx", "citro3d init failed; CPU clear fallback");
    }
#else
    g_diag.pica_ready = false;
    g_ready = true;
    pb_log(PB_LOG_INFO, "gfx", "host gfx stub");
#endif
    return g_ready;
}

void pb_gfx_shutdown(void) {
#ifdef __3DS__
    pb_gfx_pica_shutdown();
#endif
    g_ready = false;
    memset(&g_diag, 0, sizeof(g_diag));
    g_diag.invert_y = true;
}

void pb_gfx_clear_top(uint8_t red, uint8_t green, uint8_t blue) {
    g_diag.last_clear_rgba = pack_rgba(red, green, blue);
#ifdef __3DS__
    if (!g_diag.pica_ready) {
        pb_gfx_pica_cpu_clear(red, green, blue);
    }
#else
    (void)red;
    (void)green;
    (void)blue;
#endif
}

void pb_gfx_begin_frame(void) {
#ifdef __3DS__
    if (g_diag.pica_ready) {
        g_diag.cmd_begins++;
        g_frame_open = pb_gfx_pica_begin(g_diag.last_clear_rgba);
        if (!g_frame_open) {
            pb_gfx_pica_cpu_clear((uint8_t)(g_diag.last_clear_rgba >> 24),
                                  (uint8_t)(g_diag.last_clear_rgba >> 16),
                                  (uint8_t)(g_diag.last_clear_rgba >> 8));
        }
        return;
    }
#endif
    g_frame_open = true;
}

void pb_gfx_present(void) {
#ifdef __3DS__
    if (g_diag.pica_ready) {
        if (g_frame_open) {
            pb_gfx_pica_end();
            g_frame_open = false;
        } else {
            g_diag.cmd_begins++;
            if (!pb_gfx_pica_begin(g_diag.last_clear_rgba)) {
                pb_gfx_pica_cpu_clear((uint8_t)(g_diag.last_clear_rgba >> 24),
                                      (uint8_t)(g_diag.last_clear_rgba >> 16),
                                      (uint8_t)(g_diag.last_clear_rgba >> 8));
            } else {
                pb_gfx_pica_end();
            }
        }
    } else {
        pb_gfx_pica_swap();
    }
#else
    g_frame_open = false;
#endif
    g_diag.frames++;
}

bool pb_gfx_invert_y_enabled(void) {
    return g_diag.invert_y;
}

float pb_gfx_invert_y(float clip_y) {
    return -clip_y;
}

float pb_gfx_upright_t(float t) {
    return 1.0f - t;
}

bool pb_gfx_vertex_in_clip(const PBGfxVertex *vertex) {
    if (vertex == NULL) {
        return false;
    }
    if (!(vertex->w > 0.0f)) {
        return false;
    }
    return fabsf(vertex->x) <= vertex->w && fabsf(vertex->y) <= vertex->w &&
           fabsf(vertex->z) <= vertex->w;
}

void pb_gfx_map_source_to_top(float source_x, float source_y, float *top_x,
                              float *top_y) {
    if (top_x != NULL) {
        *top_x = source_x + (float)PB_GFX_PILLAR_PX;
    }
    if (top_y != NULL) {
        *top_y = source_y;
    }
}

bool pb_gfx_project_vertex(const PBGfxVertex *vertex, float *top_x,
                           float *top_y) {
    float ndc_x;
    float ndc_y;
    float source_x;
    float source_y;

    if (!pb_gfx_vertex_in_clip(vertex)) {
        return false;
    }
    ndc_x = vertex->x / vertex->w;
    ndc_y = pb_gfx_invert_y(vertex->y / vertex->w);
    source_x = (ndc_x * 0.5f + 0.5f) * (float)PB_GFX_SOURCE_WIDTH;
    source_y = (ndc_y * 0.5f + 0.5f) * (float)PB_GFX_SOURCE_HEIGHT;
    pb_gfx_map_source_to_top(source_x, source_y, top_x, top_y);
    return true;
}

unsigned pb_gfx_submit_vertices(const PBGfxVertex *verts, unsigned count) {
    unsigned i;
    unsigned kept = 0U;

    if (verts == NULL) {
        return 0U;
    }
    for (i = 0U; i < count; i++) {
        g_diag.verts_in++;
        if (pb_gfx_vertex_in_clip(&verts[i])) {
            g_diag.verts_kept++;
            kept++;
        } else {
            g_diag.verts_clipped_out++;
        }
    }
#ifdef __3DS__
    if (g_frame_open && g_diag.pica_ready && kept == count && count >= 3U) {
        pb_gfx_pica_draw_tris(verts, count);
    }
#endif
    return kept;
}

void pb_gfx_submit_dl(const void *display_list, size_t bytes) {
    g_diag.dl_submits++;
    g_diag.last_dl = display_list;
    g_diag.last_dl_bytes = bytes;
}

bool pb_gfx_tex_upload(const PBGfxTexDesc *desc, const void *pixels,
                       size_t bytes) {
    size_t needed;

    if (desc == NULL || pixels == NULL) {
        return false;
    }
    if (desc->format != PB_GFX_FMT_RGBA8888) {
        return false;
    }
    if (desc->slot >= PB_GFX_TEX_SLOTS) {
        return false;
    }
    if (desc->width == 0U || desc->height == 0U ||
        desc->width > PB_GFX_TEX_MAX || desc->height > PB_GFX_TEX_MAX) {
        return false;
    }
    if (!is_power_of_two(desc->width) || !is_power_of_two(desc->height)) {
        return false;
    }
    needed = (size_t)desc->width * (size_t)desc->height * 4U;
    if (bytes < needed) {
        return false;
    }
#ifdef __3DS__
    if (g_diag.pica_ready) {
        if (!pb_gfx_pica_upload_rgba(desc->slot, desc->width, desc->height,
                                     pixels, bytes)) {
            return false;
        }
    }
#endif
    g_diag.tex_uploads++;
    g_diag.tex_bytes += (uint32_t)needed;
    return true;
}

void pb_gfx_set_depth_enabled(bool enabled) {
    g_diag.depth_enabled = enabled;
#ifdef __3DS__
    if (g_diag.pica_ready) {
        pb_gfx_pica_set_depth(enabled);
    }
#endif
}

void pb_gfx_set_scissor_source(int x0, int y0, int x1, int y1) {
#ifdef __3DS__
    if (g_diag.pica_ready) {
        pb_gfx_pica_set_scissor(x0, y0, x1, y1);
    }
#else
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
#endif
}

void pb_gfx_query(PBGfxDiag *diag) {
    if (diag == NULL) {
        return;
    }
    *diag = g_diag;
    diag->pica_ready = g_ready && g_diag.pica_ready;
}
