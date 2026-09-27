#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define PB_FS_SDMC_ROOT "sdmc:/3ds/PaperBoat3DS/"
#define PB_FS_PAPERBOAT_O2R PB_FS_SDMC_ROOT "paperboat.o2r"
#define PB_FS_PM64_O2R PB_FS_SDMC_ROOT "pm64.o2r"

typedef enum {
    PB_FS_DEFERRED_M9 = 0,
    PB_FS_READY,
} PBFsStatus;

PBFsStatus pb_fs_status(void);
const char *pb_fs_sdmc_root(void);

#ifdef __cplusplus
}
#endif
