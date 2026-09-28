#include "pb3ds/runtime.h"

#include "pb3ds/f3d.h"
#include "pb3ds/fs.h"
#include "pb3ds/input.h"
#include "pb3ds/log.h"
#include "pb3ds/tex.h"
#include "pb3ds/title.h"

#include <string.h>

#ifdef PB3DS_GAME_OBJECTS
void step_game_loop(void);
void gfx_draw_frame(void);
void gfx_task_background(void);
#endif

static PBRuntimePlay g_play;
static bool g_ready;

static bool archives_ready(void) {
    PBFsMountInfo mount;
    pb_fs_query_mount(&mount);
    return mount.paperboat_present && mount.pm64_present;
}

void pb_runtime_init(void) {
    memset(&g_play, 0, sizeof(g_play));
    g_play.game_linked = pb_runtime_game_linked();
    g_play.assets_ready = archives_ready();
    g_play.playing = g_play.game_linked && g_play.assets_ready;
    g_ready = true;
    pb_f3d_reset();
    if (g_play.playing) {
        pb_log(PB_LOG_INFO, "runtime", "PaperBoat step/draw path");
    } else if (g_play.game_linked) {
        pb_log(PB_LOG_INFO, "runtime", "game linked; waiting for o2r");
    } else {
        pb_log(PB_LOG_INFO, "runtime", "game objects not linked");
    }
}

void pb_runtime_shutdown(void) {
    memset(&g_play, 0, sizeof(g_play));
    g_ready = false;
    pb_tex_shutdown();
}

void pb_runtime_frame(unsigned game_ticks) {
    unsigned i;

    if (!g_ready) {
        pb_runtime_init();
    }
    g_play.assets_ready = archives_ready();
    g_play.game_linked = pb_runtime_game_linked();
    g_play.playing = g_play.game_linked && g_play.assets_ready;

    if (!g_play.playing) {
        for (i = 0U; i < game_ticks; i++) {
            pb_title_step();
        }
        return;
    }

    g_play.started = true;
#ifdef PB3DS_GAME_OBJECTS
    for (i = 0U; i < game_ticks; i++) {
        step_game_loop();
        g_play.steps++;
    }
    gfx_task_background();
    gfx_draw_frame();
    g_play.draws++;
#else
    (void)i;
#endif
}

void pb_runtime_play_query(PBRuntimePlay *state) {
    if (state == NULL) {
        return;
    }
    if (!g_ready) {
        pb_runtime_init();
    }
    *state = g_play;
}

bool pb_runtime_playing(void) {
    if (!g_ready) {
        pb_runtime_init();
    }
    return g_play.playing;
}

bool pb_runtime_game_linked(void) {
#if defined(PB3DS_GAME_OBJECTS) && defined(PB3DS_GAME_RUNTIME_READY)
    return true;
#else
    return false;
#endif
}

bool pb_runtime_quit_combo(uint32_t held) {
    return (held & (PB_KEY_L | PB_KEY_R | PB_KEY_START)) ==
           (PB_KEY_L | PB_KEY_R | PB_KEY_START);
}
