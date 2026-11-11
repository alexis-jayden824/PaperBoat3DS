/* Filesystem abstraction for RomFS / SDMC / host paths (S3). */
#ifndef EX3_FS_H
#define EX3_FS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EX3_FS_OK = 0,
    EX3_FS_ERR_PARAM = 1,
    EX3_FS_ERR_OPEN = 2,
    EX3_FS_ERR_READ = 3,
    EX3_FS_ERR_NOMEM = 4
} Ex3FsResult;

/* Resolve logical paths:
 *   romfs:/...  -> RomFS on device, ./romfs/... on host
 *   sdmc:/...   -> SDMC on device, ./sdmc/... on host (optional)
 *   relative    -> as-is from CWD / configured root
 */
Ex3FsResult ex3_fs_read_all(const char *logical_path, uint8_t **out_buf, size_t *out_len);

void ex3_fs_free(uint8_t *buf);

/* Optional host/test root override (prefix for relative and romfs mapping). */
void ex3_fs_set_host_root(const char *root);

#ifdef __cplusplus
}
#endif

#endif /* EX3_FS_H */
