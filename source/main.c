#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <string.h>

#define TOP_FILL_R 26U
#define TOP_FILL_G 51U
#define TOP_FILL_B 68U

static void draw_bottom_status(const PBBootstrap *bootstrap) {
    char line[96];
    PBRuntimeStatus runtime;
    PBCompatState compat;
    size_t index;
    size_t start;

    pb_runtime_query(&runtime, bootstrap->frames, bootstrap->gfx_ready);
    pb_compat_query(&compat);
    pb_console_clear();
    snprintf(line, sizeof(line), "%s\n", PB3DS_PROJECT_NAME);
    pb_console_print(line);
    snprintf(line, sizeof(line), "%s  %s\n", PB3DS_VERSION, PB3DS_ROADMAP_STAGE);
    pb_console_print(line);
    snprintf(line, sizeof(line), "sha %s\n", PB3DS_BUILD_SHA);
    pb_console_print(line);
    snprintf(line, sizeof(line), "%s  hw %s  n3ds=%d\n",
             pb_bootstrap_status_line(bootstrap),
             pb_hw_model_name(runtime.hardware),
             runtime.new_3ds_features_enabled ? 1 : 0);
    pb_console_print(line);
    snprintf(line, sizeof(line), "mem %s app=%lu lin=%lu\n",
             pb_memory_pressure_name(runtime.memory.pressure),
             (unsigned long)runtime.memory.application_free,
             (unsigned long)runtime.memory.linear_free);
    pb_console_print(line);
    snprintf(line, sizeof(line), "frames %lu  assert=%d\n",
             (unsigned long)bootstrap->frames, runtime.assert_failed ? 1 : 0);
    pb_console_print(line);
    snprintf(line, sizeof(line), "pb %s slice=%d gfx=%s\n",
             pb_paperboat_release(), pb_paperboat_slice_linked() ? 1 : 0,
             pb_compat_status_name(compat.gfx));
    pb_console_print(line);
    snprintf(line, sizeof(line), "assets host-only extract3ds=%d\n",
             pb_assets_extraction_on_device() ? 1 : 0);
    pb_console_print(line);
    snprintf(line, sizeof(line), "us %s\n", pb_assets_us_sha1());
    pb_console_print(line);
    pb_console_print("START exits. This is not PaperBoat.\ncrumbs:\n");
    start = runtime.breadcrumb_count > 4U ? runtime.breadcrumb_count - 4U : 0U;
    for (index = start; index < runtime.breadcrumb_count; index++) {
        const PBBreadcrumb *crumb = pb_breadcrumb_at(index);
        if (crumb != NULL) {
            snprintf(line, sizeof(line), "  %s\n", crumb->text);
            pb_console_print(line);
        }
    }
}

int main(int argc, char **argv) {
    PBBootstrap bootstrap;
    PBInputSample input;
    PBMemoryStatus memory;

    (void)argc;
    (void)argv;
    pb_bootstrap_init(&bootstrap);
    if (!pb_system_init(&bootstrap)) {
        pb_log(PB_LOG_ERROR, "main", "system init failed");
        return 1;
    }
    pb_memory_query(&memory);
    PB_ASSERT(pb_gfx_ready());
    pb_compat_init();
    pb_bootstrap_log(&bootstrap, "compat layer (M6)");
    pb_bootstrap_log(&bootstrap, "asset extract host-only (M7)");
    pb_bootstrap_log(&bootstrap, "press START to exit");
    pb_log(PB_LOG_INFO, "main", pb_paperboat_commit());
    if (pb_paperboat_slice_linked()) {
        unsigned char yay0_src[16];
        unsigned char yay0_dst[8];
        memset(yay0_src, 0, sizeof(yay0_src));
        pb_paperboat_decode_yay0(yay0_src, yay0_dst);
    }

    while (pb_system_pump() && pb_bootstrap_is_running(&bootstrap)) {
        pb_input_poll(&input);
        if ((input.down & PB_KEY_START) != 0U) {
            pb_breadcrumb("START exit");
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
