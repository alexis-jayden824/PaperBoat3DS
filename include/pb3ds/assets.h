#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * M7 asset identity. Extraction is host-only (Torch-LH). The 3DS never runs
 * Torch and never ships Nintendo ROM data.
 */
#define PB_ASSETS_US_SHA1 "3837f44cda784b466c9a2d99df70d77c322b97a0"
#define PB_ASSETS_BASEROM_NAME "baserom.us.z64"
#define PB_ASSETS_PM64_O2R "pm64.o2r"
#define PB_ASSETS_PAPERBOAT_O2R "paperboat.o2r"

typedef enum {
    PB_ASSETS_HOST_ONLY = 0,
} PBAssetsStatus;

PBAssetsStatus pb_assets_status(void);
const char *pb_assets_us_sha1(void);
const char *pb_assets_baserom_name(void);
bool pb_assets_extraction_on_device(void);

#ifdef __cplusplus
}
#endif
