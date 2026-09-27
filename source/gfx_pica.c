/*
 * PICA200 / citro3d owner (M11). This is the only GPU translation unit
 * allowed to include citro3d.h. It does not interpret Fast3D, and it does
 * not emit gameplay meshes.
 */

#include "pb3ds/gfx.h"

#ifdef __3DS__
#include <3ds.h>
#include <citro3d.h>
#include <string.h>

#define PB_GFX_DISPLAY_TRANSFER                                                      \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) |                         \
     GX_TRANSFER_RAW_COPY(0) | GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) |      \
     GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |                                \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

static C3D_RenderTarget *g_top;
static bool g_pica_ready;
static C3D_Tex g_tex_slots[PB_GFX_TEX_SLOTS];
static bool g_tex_used[PB_GFX_TEX_SLOTS];

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
    g_pica_ready = true;
    return true;
}

void pb_gfx_pica_shutdown(void) {
    unsigned i;

    if (!g_pica_ready) {
        return;
    }
    for (i = 0; i < PB_GFX_TEX_SLOTS; i++) {
        if (g_tex_used[i]) {
            C3D_TexDelete(&g_tex_slots[i]);
            g_tex_used[i] = false;
        }
    }
    if (g_top != NULL) {
        C3D_RenderTargetDelete(g_top);
        g_top = NULL;
    }
    C3D_Fini();
    g_pica_ready = false;
}

bool pb_gfx_pica_frame(uint32_t clear_rgba) {
    if (!g_pica_ready || g_top == NULL) {
        return false;
    }
    if (!C3D_FrameBegin(C3D_FRAME_SYNCDRAW)) {
        return false;
    }
    C3D_FrameDrawOn(g_top);
    C3D_RenderTargetClear(g_top, C3D_CLEAR_ALL, clear_rgba, 0);
    C3D_FrameEnd(0);
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

bool pb_gfx_pica_upload_rgba(uint16_t slot, uint16_t width, uint16_t height,
                             const void *pixels, size_t bytes) {
    C3D_Tex *tex;

    if (!g_pica_ready || pixels == NULL || slot >= PB_GFX_TEX_SLOTS) {
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
    if (bytes < tex->size) {
        C3D_TexDelete(tex);
        return false;
    }
    C3D_TexUpload(tex, pixels);
    C3D_TexFlush(tex);
    g_tex_used[slot] = true;
    return true;
}

#else

bool pb_gfx_pica_init(void) {
    return false;
}

void pb_gfx_pica_shutdown(void) {
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

#endif
