#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PB_TEX_CACHE_SLOTS 16U
#define PB_TEX_CACHE_BYTES_MAX (256U * 1024U)
#define PB_TEX_DIM_MAX 256U

typedef struct {
    const void *img;
    uint8_t fmt;
    uint8_t siz;
    uint16_t width;
    uint16_t height;
    uint16_t tlut_crc;
} PBTexKey;

typedef struct {
    uint32_t hits;
    uint32_t misses;
    uint32_t evictions;
    uint32_t allocated;
    uint32_t uploads;
    uint32_t black_prevented;
} PBTexCacheDiag;

void pb_tex_init(void);
void pb_tex_shutdown(void);
void pb_tex_query(PBTexCacheDiag *diag);

/* Decode N64 texels to RGBA8888. tlut is RGBA5551 words (may be NULL). */
size_t pb_tex_src_bytes(uint8_t fmt, uint8_t siz, uint16_t width,
                        uint16_t height);

bool pb_tex_decode_rgba8888(uint8_t fmt, uint8_t siz, uint16_t width,
                            uint16_t height, const void *src, size_t src_bytes,
                            const uint16_t *tlut, unsigned tlut_count,
                            uint8_t *dst, size_t dst_bytes);

const uint8_t *pb_tex_cache_get(const PBTexKey *key, const void *src,
                                size_t src_bytes, const uint16_t *tlut,
                                unsigned tlut_count);

#ifdef __cplusplus
}
#endif
