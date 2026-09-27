/*
 * PICA200 / citro3d owner (M11–M13). This is the only GPU translation unit
 * allowed to include citro3d.h. Fast3D interpretation lives in f3d.c.
 */

#include "pb3ds/gfx.h"
#include "pb3ds/tev.h"

#ifdef __3DS__
#include <3ds.h>
#include <citro3d.h>
#include <stdlib.h>
#include <string.h>

#include "default_shbin.h"

#define PB_GFX_DISPLAY_TRANSFER                                                      \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |                         \
     GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |      \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                                \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static C3D_RenderTarget *g_top;
static bool g_pica_ready;
static bool g_frame_open;
static bool g_shader_ready;
static C3D_Tex g_tex_slots[PB_GFX_TEX_SLOTS];
static bool g_tex_used[PB_GFX_TEX_SLOTS];
static DVLB_s *g_dvlb;
static shaderProgram_s g_program;
static int g_bound_slot = -1;

static unsigned morton8(unsigned x, unsigned y) {
    return ((x & 1U) << 0) | ((y & 1U) << 1) | ((x & 2U) << 1) |
           ((y & 2U) << 2) | ((x & 4U) << 2) | ((y & 4U) << 3);
}

static void tile_rgba8(uint8_t *dst, const uint8_t *src, unsigned width,
                       unsigned height) {
    unsigned x;
    unsigned y;
    unsigned tiles_x = width / 8U;

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            unsigned tile = (y / 8U) * tiles_x + (x / 8U);
            unsigned dst_i = tile * 64U + morton8(x & 7U, y & 7U);
            const uint8_t *s = src + (y * width + x) * 4U;
            uint8_t *d = dst + dst_i * 4U;
            /* GPU_RGBA8 native byte order is A B G R. */
            d[0] = s[3];
            d[1] = s[2];
            d[2] = s[1];
            d[3] = s[0];
        }
    }
}

static bool load_shader(void) {
    C3D_AttrInfo *attr;

    g_dvlb = DVLB_ParseFile((u32 *)(void *)(uintptr_t)default_shbin,
                           default_shbin_size);
    if (g_dvlb == NULL) {
        return false;
    }
    shaderProgramInit(&g_program);
    shaderProgramSetVsh(&g_program, &g_dvlb->DVLE[0]);
    C3D_BindProgram(&g_program);
    attr = C3D_GetAttrInfo();
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attr, 2, GPU_FLOAT, 2);
    g_shader_ready = true;
    return true;
}

bool pb_gfx_pica_init(void) {
    unsigned i;

    if (g_pica_ready) {
        return true;
    }
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        return false;
    }
    /* 240×400 is the PICA render-target orientation for the 400×240 LCD. */
    g_top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (g_top == NULL) {
        C3D_Fini();
        return false;
    }
    C3D_RenderTargetSetOutput(g_top, GFX_TOP, GFX_LEFT, PB_GFX_DISPLAY_TRANSFER);
    for (i = 0; i < PB_GFX_TEX_SLOTS; i++) {
        g_tex_used[i] = false;
        memset(&g_tex_slots[i], 0, sizeof(g_tex_slots[i]));
    }
    g_bound_slot = -1;
    g_pica_ready = true;
    (void)load_shader();
    C3D_CullFace(GPU_CULL_NONE);
    C3D_DepthTest(true, GPU_LEQUAL, GPU_WRITE_ALL);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA,
                   GPU_ONE_MINUS_SRC_ALPHA);
    C3D_AlphaTest(true, GPU_GREATER, 0);
    return true;
}

void pb_gfx_pica_shutdown(void) {
    unsigned i;

    if (!g_pica_ready) {
        return;
    }
    if (g_frame_open) {
        C3D_FrameEnd(0);
        g_frame_open = false;
    }
    for (i = 0; i < PB_GFX_TEX_SLOTS; i++) {
        if (g_tex_used[i]) {
            C3D_TexDelete(&g_tex_slots[i]);
            g_tex_used[i] = false;
        }
    }
    if (g_shader_ready) {
        shaderProgramFree(&g_program);
        DVLB_Free(g_dvlb);
        g_dvlb = NULL;
        g_shader_ready = false;
    }
    if (g_top != NULL) {
        C3D_RenderTargetDelete(g_top);
        g_top = NULL;
    }
    C3D_Fini();
    g_pica_ready = false;
}

bool pb_gfx_pica_begin(uint32_t clear_rgba) {
    if (!g_pica_ready || g_top == NULL) {
        return false;
    }
    if (g_frame_open) {
        C3D_FrameEnd(0);
        g_frame_open = false;
    }
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        return false;
    }
    C3D_FrameDrawOn(g_top);
    C3D_RenderTargetClear(g_top, C3D_CLEAR_ALL, clear_rgba, 0);
    if (g_shader_ready) {
        C3D_BindProgram(&g_program);
    }
    g_frame_open = true;
    return true;
}

void pb_gfx_pica_end(void) {
    if (!g_pica_ready || !g_frame_open) {
        return;
    }
    C3D_FrameEnd(0);
    g_frame_open = false;
}

bool pb_gfx_pica_frame(uint32_t clear_rgba) {
    if (!pb_gfx_pica_begin(clear_rgba)) {
        return false;
    }
    pb_gfx_pica_end();
    return true;
}

void pb_gfx_pica_cpu_clear(uint8_t red, uint8_t green, uint8_t blue) {
    u16 width = 0;
    u16 height = 0;
    u8 *framebuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, &width, &height);
    size_t pixels;
    size_t index;

    if (framebuffer == NULL || width == 0 || height == 0) {
        return;
    }
    pixels = (size_t)width * (size_t)height;
    for (index = 0; index < pixels; index++) {
        framebuffer[index * 3U + 0U] = blue;
        framebuffer[index * 3U + 1U] = green;
        framebuffer[index * 3U + 2U] = red;
    }
}

void pb_gfx_pica_swap(void) {
    gfxFlushBuffers();
    gfxSwapBuffers();
    gspWaitForVBlank();
}

void pb_gfx_pica_set_depth(bool enabled) {
    if (!g_pica_ready) {
        return;
    }
    C3D_DepthTest(enabled, GPU_LEQUAL, enabled ? GPU_WRITE_ALL : GPU_WRITE_COLOR);
}

void pb_gfx_pica_set_scissor(int x0, int y0, int x1, int y1) {
    int px0;
    int py0;
    int px1;
    int py1;

    if (!g_pica_ready) {
        return;
    }
    /* Source 320×240 → top 400×240 (+40 pillars) → PICA 240×400. */
    px0 = y0;
    px1 = y1;
    py0 = x0 + (int)PB_GFX_PILLAR_PX;
    py1 = x1 + (int)PB_GFX_PILLAR_PX;
    if (px0 < 0) {
        px0 = 0;
    }
    if (py0 < 0) {
        py0 = 0;
    }
    if (px1 > 240) {
        px1 = 240;
    }
    if (py1 > 400) {
        py1 = 400;
    }
    if (px1 <= px0 || py1 <= py0) {
        return;
    }
    C3D_SetScissor(GPU_SCISSOR_NORMAL, px0, py0, px1, py1);
}

void pb_gfx_pica_set_tev(uint8_t mode) {
    C3D_TexEnv *env;

    if (!g_pica_ready) {
        return;
    }
    env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    switch (mode) {
        case PB_TEV_MODE_REPLACE:
            C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_TEXTURE0, GPU_TEXTURE0);
            C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
            break;
        case PB_TEV_MODE_PRIMITIVE:
        case PB_TEV_MODE_SHADE:
            C3D_TexEnvSrc(env, C3D_Both, GPU_PRIMARY_COLOR, GPU_PRIMARY_COLOR,
                          GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
            break;
        case PB_TEV_MODE_MODULATE:
        case PB_TEV_MODE_FALLBACK_MODULATE:
        default:
            C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, GPU_PRIMARY_COLOR,
                          GPU_PRIMARY_COLOR);
            C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
            break;
    }
}

void pb_gfx_pica_draw_tris(const PBGfxVertex *verts, unsigned count) {
    unsigned i;

    if (!g_pica_ready || !g_frame_open || !g_shader_ready || verts == NULL ||
        count < 3U) {
        return;
    }
    if (g_bound_slot >= 0 && g_tex_used[g_bound_slot]) {
        C3D_TexBind(0, &g_tex_slots[g_bound_slot]);
    }
    pb_gfx_pica_set_tev(pb_tev_mode());
    C3D_ImmDrawBegin(GPU_TRIANGLES);
    for (i = 0; i < count; i++) {
        float r = (float)((verts[i].color >> 24) & 0xFFU) / 255.0f;
        float g = (float)((verts[i].color >> 16) & 0xFFU) / 255.0f;
        float b = (float)((verts[i].color >> 8) & 0xFFU) / 255.0f;
        float a = (float)(verts[i].color & 0xFFU) / 255.0f;
        C3D_ImmSendAttrib(verts[i].x, pb_gfx_invert_y(verts[i].y), verts[i].z,
                          verts[i].w);
        C3D_ImmSendAttrib(r, g, b, a);
        C3D_ImmSendAttrib(verts[i].s, verts[i].t, 0.0f, 1.0f);
    }
    C3D_ImmDrawEnd();
}

bool pb_gfx_pica_upload_rgba(uint16_t slot, uint16_t width, uint16_t height,
                             const void *pixels, size_t bytes) {
    C3D_Tex *tex;
    uint8_t *tiled;
    size_t needed;

    if (!g_pica_ready || pixels == NULL || slot >= PB_GFX_TEX_SLOTS) {
        return false;
    }
    needed = (size_t)width * (size_t)height * 4U;
    if (bytes < needed) {
        return false;
    }
    tex = &g_tex_slots[slot];
    if (g_tex_used[slot]) {
        C3D_TexDelete(tex);
        g_tex_used[slot] = false;
    }
    if (!C3D_TexInit(tex, width, height, GPU_RGBA8)) {
        return false;
    }
    tiled = (uint8_t *)linearAlloc(needed);
    if (tiled == NULL) {
        C3D_TexDelete(tex);
        return false;
    }
    tile_rgba8(tiled, (const uint8_t *)pixels, width, height);
    GSPGPU_FlushDataCache(tiled, needed);
    C3D_TexUpload(tex, tiled);
    C3D_TexFlush(tex);
    C3D_TexSetFilter(tex, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(tex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    linearFree(tiled);
    g_tex_used[slot] = true;
    g_bound_slot = (int)slot;
    return true;
}

#else

bool pb_gfx_pica_init(void) {
    return false;
}

void pb_gfx_pica_shutdown(void) {
}

bool pb_gfx_pica_begin(uint32_t clear_rgba) {
    (void)clear_rgba;
    return false;
}

void pb_gfx_pica_end(void) {
}

bool pb_gfx_pica_frame(uint32_t clear_rgba) {
    (void)clear_rgba;
    return false;
}

bool pb_gfx_pica_upload_rgba(uint16_t slot, uint16_t width, uint16_t height,
                             const void *pixels, size_t bytes) {
    (void)slot;
    (void)width;
    (void)height;
    (void)pixels;
    (void)bytes;
    return false;
}

void pb_gfx_pica_cpu_clear(uint8_t red, uint8_t green, uint8_t blue) {
    (void)red;
    (void)green;
    (void)blue;
}

void pb_gfx_pica_swap(void) {
}

void pb_gfx_pica_set_depth(bool enabled) {
    (void)enabled;
}

void pb_gfx_pica_set_scissor(int x0, int y0, int x1, int y1) {
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}

void pb_gfx_pica_set_tev(uint8_t mode) {
    (void)mode;
}

void pb_gfx_pica_draw_tris(const PBGfxVertex *verts, unsigned count) {
    (void)verts;
    (void)count;
}

#endif
