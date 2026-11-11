/* Real New 3DS implementation of ex3_plat (libctru + citro2d). */
#include "plat_common.h"

#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    Ex3Plat base;
    C3D_RenderTarget *top;
    C3D_RenderTarget *bottom;
    aptHookCookie apt_cookie;
    bool gfx_ready;
} Ex3Plat3DS;

static u32 rgba8_to_c2d(uint32_t rgba)
{
    u8 r = (u8)((rgba >> 24) & 0xFF);
    u8 g = (u8)((rgba >> 16) & 0xFF);
    u8 b = (u8)((rgba >> 8) & 0xFF);
    u8 a = (u8)(rgba & 0xFF);
    return C2D_Color32(r, g, b, a);
}

static void apt_hook_cb(APT_HookType hook, void *param)
{
    Ex3Plat *plat = (Ex3Plat *)param;
    if (plat == NULL) {
        return;
    }
    switch (hook) {
        case APTHOOK_ONSUSPEND:
            plat->lifecycle = EX3_LIFECYCLE_SUSPENDED;
            break;
        case APTHOOK_ONSLEEP:
            plat->lifecycle = EX3_LIFECYCLE_SLEEPING;
            break;
        case APTHOOK_ONRESTORE:
        case APTHOOK_ONWAKEUP:
            plat->lifecycle = EX3_LIFECYCLE_ACTIVE;
            break;
        case APTHOOK_ONEXIT:
            plat->lifecycle = EX3_LIFECYCLE_EXITING;
            plat->exit_requested = true;
            break;
        default:
            break;
    }
}

Ex3Plat *ex3_plat_create(void)
{
    Ex3Plat3DS *impl = (Ex3Plat3DS *)calloc(1, sizeof(*impl));
    if (impl == NULL) {
        return NULL;
    }
    ex3_plat_common_reset(&impl->base);
    return &impl->base;
}

void ex3_plat_destroy(Ex3Plat *plat)
{
    if (plat == NULL) {
        return;
    }
    ex3_plat_shutdown(plat);
    free(plat);
}

Ex3PlatResult ex3_plat_init(Ex3Plat *plat)
{
    Ex3Plat3DS *impl;
    bool is_new = false;

    if (plat == NULL) {
        return EX3_PLAT_ERR_INIT;
    }
    if (plat->initialized) {
        return EX3_PLAT_OK;
    }

    impl = (Ex3Plat3DS *)plat;
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    impl->top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    impl->bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (impl->top == NULL || impl->bottom == NULL) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return EX3_PLAT_ERR_INIT;
    }
    impl->gfx_ready = true;

    if (R_SUCCEEDED(APT_CheckNew3DS(&is_new))) {
        plat->is_new_3ds = is_new;
    }

    aptHook(&impl->apt_cookie, apt_hook_cb, plat);

    /* Root SDMC path avoids mkdir dependency during S1. */
    snprintf(plat->log_path, sizeof(plat->log_path), "%s", "sdmc:/exvius3ds.log");
    plat->log_file = fopen(plat->log_path, "w");

    plat->initialized = true;
    plat->lifecycle = EX3_LIFECYCLE_ACTIVE;
    return EX3_PLAT_OK;
}

void ex3_plat_shutdown(Ex3Plat *plat)
{
    Ex3Plat3DS *impl;
    if (plat == NULL || !plat->initialized) {
        return;
    }
    impl = (Ex3Plat3DS *)plat;

    aptUnhook(&impl->apt_cookie);

    if (plat->log_file != NULL) {
        fclose(plat->log_file);
        plat->log_file = NULL;
    }

    if (impl->gfx_ready) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        impl->gfx_ready = false;
    }
    plat->initialized = false;
}

Ex3PlatResult ex3_plat_enable_new3ds_boost(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return EX3_PLAT_ERR_STATE;
    }
    if (!plat->is_new_3ds) {
        return EX3_PLAT_ERR_STATE;
    }
    osSetSpeedupEnable(true);
    plat->boost_enabled = true;
    return EX3_PLAT_OK;
}

void ex3_plat_poll(Ex3Plat *plat)
{
    if (plat == NULL) {
        return;
    }
    hidScanInput();
    if (hidKeysDown() & KEY_START) {
        plat->exit_requested = true;
        plat->lifecycle = EX3_LIFECYCLE_EXITING;
    }
    if (!aptMainLoop()) {
        plat->exit_requested = true;
        plat->lifecycle = EX3_LIFECYCLE_EXITING;
    }
}

void ex3_plat_begin_frame(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    plat->frame.screens_cleared = false;
    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void ex3_plat_clear_screens(Ex3Plat *plat, uint32_t top_rgba, uint32_t bottom_rgba)
{
    Ex3Plat3DS *impl;
    if (plat == NULL || !plat->initialized) {
        return;
    }
    impl = (Ex3Plat3DS *)plat;

    plat->frame.top_rgba = top_rgba;
    plat->frame.bottom_rgba = bottom_rgba;
    plat->frame.draw_rect_count = 0;
    plat->frame.last_draw_rgba = 0;

    /* TargetClear alone is not enough if a screen never gets SceneBegin/draw
     * this frame (S2 left bottom black on hardware). Clear, begin, and paint a
     * full-bleed rect on both LCDs so each screen is presented every frame. */
    C2D_TargetClear(impl->top, rgba8_to_c2d(top_rgba));
    C2D_SceneBegin(impl->top);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 400.0f, 240.0f, rgba8_to_c2d(top_rgba));

    C2D_TargetClear(impl->bottom, rgba8_to_c2d(bottom_rgba));
    C2D_SceneBegin(impl->bottom);
    C2D_DrawRectSolid(0.0f, 0.0f, 0.0f, 320.0f, 240.0f, rgba8_to_c2d(bottom_rgba));

    plat->frame.screens_cleared = true;
}

void ex3_plat_draw_rect(Ex3Plat *plat, Ex3Screen screen, float x, float y, float w,
                        float h, uint32_t rgba)
{
    Ex3Plat3DS *impl;
    C3D_RenderTarget *target;

    if (plat == NULL || !plat->initialized) {
        return;
    }
    if (w <= 0.0f || h <= 0.0f) {
        return;
    }

    impl = (Ex3Plat3DS *)plat;
    target = (screen == EX3_SCREEN_BOTTOM) ? impl->bottom : impl->top;
    C2D_SceneBegin(target);
    C2D_DrawRectSolid(x, y, 0.5f, w, h, rgba8_to_c2d(rgba));
    plat->frame.draw_rect_count++;
    plat->frame.last_draw_rgba = rgba;
}

void ex3_plat_end_frame(Ex3Plat *plat)
{
    if (plat == NULL || !plat->initialized) {
        return;
    }
    C3D_FrameEnd(0);
    if (plat->frame.screens_cleared) {
        plat->frame.frame_count++;
    }
}
