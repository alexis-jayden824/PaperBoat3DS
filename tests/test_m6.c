#include "pb3ds/compat.h"
#include "pb3ds/platform.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M6 compat check failed at %s:%d: %s\n",           \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_deferred_contracts(void) {
    PBCompatState state;
    PBOSContPad pad;

    CHECK(strcmp(PB3DS_VERSION, "0.13.0-m13") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M13") != NULL);
    pb_compat_init();
    pb_compat_query(&state);
    CHECK(state.logging == PB_COMPAT_READY);
    CHECK(state.resources == PB_COMPAT_READY);
    CHECK(state.controller == PB_COMPAT_READY);
    CHECK(state.time == PB_COMPAT_READY);
    CHECK(state.gfx == PB_COMPAT_READY);
    CHECK(state.audio == PB_COMPAT_DEFERRED_M14);
    CHECK(state.config == PB_COMPAT_DEFERRED_M15);
    CHECK(!pb_compat_desktop_engine_allowed());
    CHECK(ResourceGetDataByName("shapes/mac_00") == NULL);
    CHECK(GameEngine_GetDataExact("missing") == NULL);
    CHECK(CVarGetInteger("any", 7) == 7);
    {
        float cvar = CVarGetFloat("any", 1.5f);
        CHECK(cvar > 1.49f && cvar < 1.51f);
    }
    CVarSetInteger("any", 1);
    Graphics_PushFrame(NULL);
    GameEngine_StartAudioFrame();
    GameEngine_EndAudioFrame();
    GameEngine_HoldFrame();
    pb_compat_poll_controller(&pad);
    CHECK(pad.button == 0U);
    CHECK(pb_compat_unsupported("SDL_CreateWindow") == PB_COMPAT_UNSUPPORTED);
    pb_compat_query(&state);
    CHECK(strcmp(state.last_unsupported, "SDL_CreateWindow") == 0);
    CHECK(strcmp(pb_compat_status_name(PB_COMPAT_DEFERRED_M11), "deferred-m11") ==
          0);
    GameEngine_LogInfo("m%d", 6);
    return true;
}

int main(void) {
    if (!test_deferred_contracts()) {
        return EXIT_FAILURE;
    }
    printf("M6 compatibility contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
