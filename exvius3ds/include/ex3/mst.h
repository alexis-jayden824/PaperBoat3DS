/* Synthetic MST fixture loader for Exvius3DS (S3).
 *
 * Format (little-endian):
 *   magic[8]   = "EX3MST1\0"
 *   version    = u32 (1)
 *   flags      = u32 (bit0 = AES-CBC encrypted payload)
 *   key_token[8]
 *   record_count u32
 *   payload_len  u32
 *   payload bytes
 *
 * Each plaintext record:
 *   id   u32
 *   name char[32]  (NUL-padded)
 *
 * Encrypted payloads use the recovered FFBE wire key/IV scheme (key16 + IV).
 */
#ifndef EX3_MST_H
#define EX3_MST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EX3_MST_FLAG_ENCRYPTED = 1u,
    EX3_MST_NAME_LEN = 32,
    EX3_MST_MAX_RECORDS = 64
};

typedef struct {
    uint32_t id;
    char name[EX3_MST_NAME_LEN];
} Ex3MstRecord;

typedef struct {
    uint32_t version;
    uint32_t flags;
    char key_token[9];
    uint32_t record_count;
    Ex3MstRecord records[EX3_MST_MAX_RECORDS];
    bool loaded;
    char error[80];
} Ex3MstTable;

typedef enum {
    EX3_MST_OK = 0,
    EX3_MST_ERR_PARAM = 1,
    EX3_MST_ERR_FORMAT = 2,
    EX3_MST_ERR_CRYPTO = 3,
    EX3_MST_ERR_LIMIT = 4
} Ex3MstResult;

Ex3MstResult ex3_mst_parse(const uint8_t *data, size_t len, Ex3MstTable *out);
Ex3MstResult ex3_mst_load_path(const char *logical_path, Ex3MstTable *out);

#ifdef __cplusplus
}
#endif

#endif /* EX3_MST_H */
