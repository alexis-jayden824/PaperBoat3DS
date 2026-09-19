#include <3ds.h>
#include <stdio.h>

#include "pb3ds/version.h"

static void print_top_screen(void) {
    consoleInit(GFX_TOP, NULL);
    printf("\x1b[2;2H%s\n", PB3DS_PROJECT_NAME);
    printf("\x1b[4;2HVersion: %s\n", PB3DS_VERSION);
    printf("\x1b[5;2HStage:   %s\n", PB3DS_ROADMAP_STAGE);
    printf("\x1b[8;2HNative 3DS bootstrap shell\n");
    printf("\x1b[10;2HNo game assets are bundled.\n");
    printf("\x1b[13;2HPress START to exit.\n");
}

static void print_bottom_screen(void) {
    consoleInit(GFX_BOTTOM, NULL);
    printf("\x1b[2;2HPaperBoat Configuration\n");
    printf("\x1b[4;2HBottom-screen UI reserved\n");
    printf("\x1b[5;2Hfor roadmap milestone M16.\n");
    printf("\x1b[8;2HInput keybind: deferred until\n");
    printf("\x1b[9;2Hthe M8 input audit.\n");
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    gfxInitDefault();
    print_top_screen();
    print_bottom_screen();

    while (aptMainLoop()) {
        hidScanInput();
        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    gfxExit();
    return 0;
}

