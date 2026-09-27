#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>

#define TOP_FILL_R 26U
#define TOP_FILL_G 51U
#define TOP_FILL_B 68U

static void draw_bottom_status(const PBBootstrap *bootstrap) {
    char line[96];
    size_t index;
    size_t start;

    pb_console_clear();
    snprintf(line, sizeof(line), "%s\n", PB3DS_PROJECT_NAME);
    pb_console_print(line);
    snprintf(line, sizeof(line), "%s  %s\n", PB3DS_VERSION, PB3DS_ROADMAP_STAGE);
    pb_console_print(line);
    snprintf(line, sizeof(line), "sha %s\n", PB3DS_BUILD_SHA);
    pb_console_print(line);
    snprintf(line, sizeof(line), "%s\n\n", pb_bootstrap_status_line(bootstrap));
    pb_console_print(line);
    snprintf(line, sizeof(line), "Top %ux%u  Bottom %ux%u\n",
             PB_GFX_TOP_WIDTH, PB_GFX_TOP_HEIGHT, PB_GFX_BOTTOM_WIDTH,
             PB_GFX_BOTTOM_HEIGHT);
    pb_console_print(line);
    snprintf(line, sizeof(line), "frames %lu  audio=%d  fs=%d\n",
             (unsigned long)bootstrap->frames, (int)pb_audio_status(),
             (int)pb_fs_status());
    pb_console_print(line);
    pb_console_print("START exits. This is not PaperBoat.\n\nlog:\n");
    start = bootstrap->log.count < PB_BOOTSTRAP_LOG_CAPACITY
                ? 0U
                : bootstrap->log.next;
    for (index = 0; index < bootstrap->log.count; index++) {
        const size_t slot = (start + index) % PB_BOOTSTRAP_LOG_CAPACITY;
        snprintf(line, sizeof(line), "  %s\n", bootstrap->log.lines[slot]);
        pb_console_print(line);
    }
}

int main(int argc, char **argv) {
    PBBootstrap bootstrap;
    PBInputSample input;

    (void)argc;
    (void)argv;
    pb_bootstrap_init(&bootstrap);
    if (!pb_system_init(&bootstrap)) {
        pb_log(PB_LOG_ERROR, "main", "system init failed");
        return 1;
    }
    pb_bootstrap_log(&bootstrap, "platform ready (M3)");
    pb_bootstrap_log(&bootstrap, "press START to exit");
    pb_log(PB_LOG_INFO, "main", pb_fs_sdmc_root());

    while (pb_system_pump() && pb_bootstrap_is_running(&bootstrap)) {
        pb_input_poll(&input);
        if ((input.down & PB_KEY_START) != 0U) {
            pb_bootstrap_on_start(&bootstrap);
            break;
        }
        pb_bootstrap_tick(&bootstrap);
        pb_gfx_clear_top(TOP_FILL_R, TOP_FILL_G, TOP_FILL_B);
        draw_bottom_status(&bootstrap);
        pb_gfx_present();
    }

    pb_bootstrap_log(&bootstrap, "shutdown");
    pb_system_shutdown(&bootstrap);
    return 0;
}
