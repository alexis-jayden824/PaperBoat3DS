#include "pb3ds/assets.h"

#include <stdbool.h>

PBAssetsStatus pb_assets_status(void) {
    return PB_ASSETS_HOST_ONLY;
}

const char *pb_assets_us_sha1(void) {
    return PB_ASSETS_US_SHA1;
}

const char *pb_assets_baserom_name(void) {
    return PB_ASSETS_BASEROM_NAME;
}

bool pb_assets_extraction_on_device(void) {
    return false;
}
