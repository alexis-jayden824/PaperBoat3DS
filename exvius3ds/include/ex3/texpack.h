/* Tiny texture-pack fixture for S3 (not full CPK). */
#ifndef EX3_TEXPACK_H
#define EX3_TEXPACK_H

#include "ex3/texture.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EX3_TEXPACK_MAX_PIXELS = 64 * 64 * 4
};

typedef struct {
    uint32_t width;
    uint32_t height;
    Ex3TextureInfo plan;
    uint8_t rgba[EX3_TEXPACK_MAX_PIXELS];
    size_t rgba_len;
    bool loaded;
    char error[80];
} Ex3TexPack;

typedef enum {
    EX3_TEXPACK_OK = 0,
    EX3_TEXPACK_ERR_PARAM = 1,
    EX3_TEXPACK_ERR_FORMAT = 2,
    EX3_TEXPACK_ERR_LIMIT = 3,
    EX3_TEXPACK_ERR_PLAN = 4
} Ex3TexPackResult;

/* magic "EX3TEX1\0", width u32, height u32, raw RGBA8 pixels */
Ex3TexPackResult ex3_texpack_parse(const uint8_t *data, size_t len, Ex3TexPack *out);
Ex3TexPackResult ex3_texpack_load_path(const char *logical_path, Ex3TexPack *out);

#ifdef __cplusplus
}
#endif

#endif /* EX3_TEXPACK_H */
