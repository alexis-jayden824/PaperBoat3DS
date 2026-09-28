#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_FS_SDMC_ROOT "sdmc:/3ds/PaperBoat3DS/"
#define PB_FS_PAPERBOAT_O2R PB_FS_SDMC_ROOT "paperboat.o2r"
#define PB_FS_PM64_O2R PB_FS_SDMC_ROOT "pm64.o2r"
#define PB_FS_ALIGN 16U
#define PB_FS_NAME_MAX 96U
#define PB_FS_MAX_BLOB PB_KIB_FS(512)

/* Keep fs.h free of memory.h; 512 KiB cap is the Old 3DS per-lookup budget. */
#define PB_KIB_FS(value) ((size_t)(value) * 1024U)

typedef enum {
    PB_FS_DEFERRED_M9 = 0,
    PB_FS_READY,
} PBFsStatus;

typedef struct {
    bool paperboat_present;
    bool pm64_present;
    bool paperboat_zip;
    bool pm64_zip;
    uint32_t paperboat_bytes;
    uint32_t pm64_bytes;
} PBFsMountInfo;

void pb_fs_init(void);
void pb_fs_shutdown(void);
PBFsStatus pb_fs_status(void);
const char *pb_fs_sdmc_root(void);
void pb_fs_query_mount(PBFsMountInfo *info);
bool pb_fs_exists(const char *logical_path);
int pb_fs_join(char *dst, size_t dst_size, const char *leaf);
int pb_fs_register(const char *name, const void *data, size_t size);
void *pb_fs_lookup(const char *name, size_t *size_out);
/* Caller owns *data_out. Avoids the fixed title/diagnostic cache for game assets. */
bool pb_fs_load_raw(const char *name, uint8_t **data_out, size_t *size_out);
bool pb_fs_is_aligned(const void *pointer);

#ifndef __3DS__
void pb_fs_host_set_root(const char *native_root);
#endif

#ifdef __cplusplus
}
#endif
