#include "pb3ds/assets.h"
#include "pb3ds/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks_run;

#define CHECK(expression)                                                      \
    do {                                                                       \
        checks_run++;                                                          \
        if (!(expression)) {                                                   \
            fprintf(stderr, "M7 assets check failed at %s:%d: %s\n",           \
                    __FILE__, __LINE__, #expression);                          \
            return false;                                                      \
        }                                                                      \
    } while (0)

static bool test_host_only_identity(void) {
    CHECK(strcmp(PB3DS_VERSION, "0.7.0-m7") == 0);
    CHECK(strstr(PB3DS_ROADMAP_STAGE, "M7") != NULL);
    CHECK(pb_assets_status() == PB_ASSETS_HOST_ONLY);
    CHECK(!pb_assets_extraction_on_device());
    CHECK(strcmp(pb_assets_baserom_name(), "baserom.us.z64") == 0);
    CHECK(strcmp(pb_assets_us_sha1(),
                 "3837f44cda784b466c9a2d99df70d77c322b97a0") == 0);
    CHECK(strcmp(PB_ASSETS_PM64_O2R, "pm64.o2r") == 0);
    CHECK(strcmp(PB_ASSETS_PAPERBOAT_O2R, "paperboat.o2r") == 0);
    return true;
}

int main(void) {
    if (!test_host_only_identity()) {
        return EXIT_FAILURE;
    }
    printf("M7 assets contract: %u checks passed\n", checks_run);
    return EXIT_SUCCESS;
}
