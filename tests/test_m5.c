#include "pb3ds/paperboat.h"
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
            fprintf(stderr, "M5 PaperBoat slice check failed at %s:%d: %s\n",  \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_pin_and_printf(void) {
    char text[32];

    CHECK(strcmp(PB3DS_VERSION, "0.8.0-m8") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M8") != NULL);
    CHECK(strcmp(pb_paperboat_release(), "1.0.1") == 0);
    CHECK(strcmp(pb_paperboat_commit(),
                 "424c220f0863c29b9fe55cc674baceff88e9e14f") == 0);
    CHECK(pb_paperboat_slice_linked());
    CHECK(pb_paperboat_printf_shim(text, sizeof(text), "m%d", 5) > 0);
    CHECK(strcmp(text, "m5") == 0);
    return true;
}

static bool test_yay0_fallback_and_literals(void) {
    unsigned char fallback_src[16];
    unsigned char fallback_dst[8];
    unsigned char yay0[24];
    unsigned char decoded[8];

    memset(fallback_src, 0, sizeof(fallback_src));
    fallback_src[0] = 'A';
    fallback_src[1] = 'B';
    fallback_src[2] = 'C';
    fallback_src[3] = 'D';
    fallback_src[7] = 4U;
    memset(fallback_dst, 0xFF, sizeof(fallback_dst));
    pb_paperboat_decode_yay0(fallback_src, fallback_dst);
    CHECK(fallback_dst[0] == 'A');
    CHECK(fallback_dst[1] == 'B');
    CHECK(fallback_dst[2] == 'C');
    CHECK(fallback_dst[3] == 'D');

    memset(yay0, 0, sizeof(yay0));
    yay0[0] = 'Y';
    yay0[1] = 'a';
    yay0[2] = 'y';
    yay0[3] = '0';
    yay0[7] = 4U;
    yay0[11] = 20U;
    yay0[15] = 17U;
    yay0[16] = 0xF0U;
    yay0[17] = 'P';
    yay0[18] = 'B';
    yay0[19] = '3';
    yay0[20] = 'D';
    memset(decoded, 0, sizeof(decoded));
    pb_paperboat_decode_yay0(yay0, decoded);
    CHECK(decoded[0] == 'P');
    CHECK(decoded[1] == 'B');
    CHECK(decoded[2] == '3');
    CHECK(decoded[3] == 'D');
    return true;
}

int main(void) {
    if (!test_pin_and_printf() || !test_yay0_fallback_and_literals()) {
        return EXIT_FAILURE;
    }
    printf("M5 PaperBoat slice contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
