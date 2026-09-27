#include "pb3ds/tex.h"
#include "pb3ds/f3d.h"
#include "pb3ds/log.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    PBTexKey key;
    uint8_t *rgba;
    size_t bytes;
    uint32_t lru;
    bool used;
} PBTexSlot;

static PBTexSlot g_slots[PB_TEX_CACHE_SLOTS];
static PBTexCacheDiag g_diag;
static uint32_t g_lru;
static bool g_ready;

static uint16_t crc16(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFFU;
    size_t i;

    if (p == NULL) {
        return 0U;
    }
    for (i = 0; i < size; i++) {
        crc = (uint16_t)((crc >> 8) ^ (uint16_t)(p[i] << 8) ^ crc);
    }
    return crc;
}

static uint8_t expand_4(uint8_t nibble) {
    nibble &= 0x0FU;
    return (uint8_t)((nibble << 4) | nibble);
}

static void rgba16_to_8888(uint16_t pixel, uint8_t *out) {
    uint8_t r = (uint8_t)((pixel >> 11) & 0x1FU);
    uint8_t g = (uint8_t)((pixel >> 6) & 0x1FU);
    uint8_t b = (uint8_t)((pixel >> 1) & 0x1FU);
    uint8_t a = (uint8_t)(pixel & 1U);
    out[0] = (uint8_t)((r << 3) | (r >> 2));
    out[1] = (uint8_t)((g << 3) | (g >> 2));
    out[2] = (uint8_t)((b << 3) | (b >> 2));
    out[3] = a ? 0xFFU : 0U;
}

static uint16_t u16be(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

void pb_tex_init(void) {
    memset(g_slots, 0, sizeof(g_slots));
    memset(&g_diag, 0, sizeof(g_diag));
    g_lru = 1U;
    g_ready = true;
}

void pb_tex_shutdown(void) {
    unsigned i;

    for (i = 0; i < PB_TEX_CACHE_SLOTS; i++) {
        free(g_slots[i].rgba);
        memset(&g_slots[i], 0, sizeof(g_slots[i]));
    }
    memset(&g_diag, 0, sizeof(g_diag));
    g_ready = false;
}

void pb_tex_query(PBTexCacheDiag *diag) {
    if (diag == NULL) {
        return;
    }
    *diag = g_diag;
}

size_t pb_tex_src_bytes(uint8_t fmt, uint8_t siz, uint16_t width,
                        uint16_t height) {
    size_t pixels = (size_t)width * (size_t)height;

    if (siz == PB_F3D_SIZ_32B) {
        return pixels * 4U;
    }
    if (siz == PB_F3D_SIZ_16B) {
        return pixels * 2U;
    }
    if (siz == PB_F3D_SIZ_8B) {
        return pixels;
    }
    if (siz == PB_F3D_SIZ_4B) {
        return (pixels + 1U) / 2U;
    }
    (void)fmt;
    return pixels;
}

bool pb_tex_decode_rgba8888(uint8_t fmt, uint8_t siz, uint16_t width,
                            uint16_t height, const void *src, size_t src_bytes,
                            const uint16_t *tlut, unsigned tlut_count,
                            uint8_t *dst, size_t dst_bytes) {
    size_t pixels;
    size_t i;
    const uint8_t *in;

    if (src == NULL || dst == NULL || width == 0U || height == 0U ||
        width > PB_TEX_DIM_MAX || height > PB_TEX_DIM_MAX) {
        return false;
    }
    pixels = (size_t)width * (size_t)height;
    if (dst_bytes < pixels * 4U) {
        return false;
    }
    in = (const uint8_t *)src;

    if (fmt == PB_F3D_FMT_CI) {
        if (tlut == NULL || tlut_count == 0U) {
            pb_log(PB_LOG_WARNING, "tlut", "CI without palette");
            memset(dst, 0x40, pixels * 4U);
            for (i = 0; i < pixels; i++) {
                dst[i * 4U + 3U] = 0xFFU;
            }
            g_diag.black_prevented++;
            return true;
        }
        if (siz == PB_F3D_SIZ_8B) {
            if (src_bytes < pixels) {
                return false;
            }
            for (i = 0; i < pixels; i++) {
                unsigned idx = in[i];
                if (idx >= tlut_count) {
                    idx = 0U;
                }
                rgba16_to_8888(tlut[idx], dst + i * 4U);
            }
            return true;
        }
        if (siz == PB_F3D_SIZ_4B) {
            if (src_bytes < (pixels + 1U) / 2U) {
                return false;
            }
            for (i = 0; i < pixels; i++) {
                unsigned packed = in[i / 2U];
                unsigned idx = (i & 1U) ? (packed & 0x0FU) : (packed >> 4);
                if (idx >= tlut_count) {
                    idx = 0U;
                }
                rgba16_to_8888(tlut[idx], dst + i * 4U);
            }
            return true;
        }
        return false;
    }

    if (fmt == PB_F3D_FMT_RGBA && siz == PB_F3D_SIZ_16B) {
        if (src_bytes < pixels * 2U) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            rgba16_to_8888(u16be(in + i * 2U), dst + i * 4U);
        }
        return true;
    }
    if (fmt == PB_F3D_FMT_RGBA && siz == PB_F3D_SIZ_32B) {
        if (src_bytes < pixels * 4U) {
            return false;
        }
        memcpy(dst, in, pixels * 4U);
        return true;
    }
    if (fmt == PB_F3D_FMT_IA && siz == PB_F3D_SIZ_8B) {
        if (src_bytes < pixels) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            uint8_t v = in[i];
            uint8_t intensity = expand_4((uint8_t)(v >> 4));
            uint8_t alpha = expand_4((uint8_t)(v & 0x0FU));
            dst[i * 4U + 0U] = intensity;
            dst[i * 4U + 1U] = intensity;
            dst[i * 4U + 2U] = intensity;
            dst[i * 4U + 3U] = alpha;
        }
        return true;
    }
    if (fmt == PB_F3D_FMT_IA && siz == PB_F3D_SIZ_16B) {
        if (src_bytes < pixels * 2U) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            dst[i * 4U + 0U] = in[i * 2U];
            dst[i * 4U + 1U] = in[i * 2U];
            dst[i * 4U + 2U] = in[i * 2U];
            dst[i * 4U + 3U] = in[i * 2U + 1U];
        }
        return true;
    }
    if (fmt == PB_F3D_FMT_I && siz == PB_F3D_SIZ_8B) {
        if (src_bytes < pixels) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            dst[i * 4U + 0U] = in[i];
            dst[i * 4U + 1U] = in[i];
            dst[i * 4U + 2U] = in[i];
            dst[i * 4U + 3U] = 0xFFU;
        }
        return true;
    }
    if (fmt == PB_F3D_FMT_IA && siz == PB_F3D_SIZ_4B) {
        if (src_bytes < (pixels + 1U) / 2U) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            unsigned packed = in[i / 2U];
            uint8_t nibble =
                (uint8_t)((i & 1U) ? (packed & 0x0FU) : (packed >> 4));
            uint8_t intensity = expand_4((uint8_t)(nibble >> 1));
            uint8_t alpha = (nibble & 1U) ? 0xFFU : 0U;
            dst[i * 4U + 0U] = intensity;
            dst[i * 4U + 1U] = intensity;
            dst[i * 4U + 2U] = intensity;
            dst[i * 4U + 3U] = alpha;
        }
        return true;
    }
    if (fmt == PB_F3D_FMT_I && siz == PB_F3D_SIZ_4B) {
        if (src_bytes < (pixels + 1U) / 2U) {
            return false;
        }
        for (i = 0; i < pixels; i++) {
            unsigned packed = in[i / 2U];
            uint8_t intensity =
                expand_4((uint8_t)((i & 1U) ? (packed & 0x0FU) : (packed >> 4)));
            dst[i * 4U + 0U] = intensity;
            dst[i * 4U + 1U] = intensity;
            dst[i * 4U + 2U] = intensity;
            dst[i * 4U + 3U] = 0xFFU;
        }
        return true;
    }
    pb_log(PB_LOG_WARNING, "tex", "unsupported format");
    return false;
}

static int find_slot(const PBTexKey *key) {
    unsigned i;

    for (i = 0; i < PB_TEX_CACHE_SLOTS; i++) {
        if (g_slots[i].used && memcmp(&g_slots[i].key, key, sizeof(*key)) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static unsigned evict_slot(void) {
    unsigned i;
    unsigned victim = 0;
    uint32_t oldest = 0xFFFFFFFFU;

    for (i = 0; i < PB_TEX_CACHE_SLOTS; i++) {
        if (!g_slots[i].used) {
            return i;
        }
        if (g_slots[i].lru < oldest) {
            oldest = g_slots[i].lru;
            victim = i;
        }
    }
    g_diag.allocated -= (uint32_t)g_slots[victim].bytes;
    free(g_slots[victim].rgba);
    memset(&g_slots[victim], 0, sizeof(g_slots[victim]));
    g_diag.evictions++;
    return victim;
}

const uint8_t *pb_tex_cache_get(const PBTexKey *key, const void *src,
                                size_t src_bytes, const uint16_t *tlut,
                                unsigned tlut_count) {
    int hit;
    unsigned slot;
    size_t bytes;
    uint8_t *rgba;
    PBTexKey keyed;

    if (!g_ready) {
        pb_tex_init();
    }
    if (key == NULL || src == NULL) {
        return NULL;
    }
    keyed = *key;
    keyed.tlut_crc = crc16(tlut, (size_t)tlut_count * sizeof(uint16_t));
    hit = find_slot(&keyed);
    if (hit >= 0) {
        g_slots[hit].lru = ++g_lru;
        g_diag.hits++;
        return g_slots[hit].rgba;
    }
    g_diag.misses++;
    bytes = (size_t)keyed.width * (size_t)keyed.height * 4U;
    if (g_diag.allocated + (uint32_t)bytes > PB_TEX_CACHE_BYTES_MAX) {
        evict_slot();
    }
    slot = evict_slot();
    rgba = (uint8_t *)malloc(bytes);
    if (rgba == NULL) {
        return NULL;
    }
    if (!pb_tex_decode_rgba8888(keyed.fmt, keyed.siz, keyed.width, keyed.height,
                                src, src_bytes, tlut, tlut_count, rgba, bytes)) {
        free(rgba);
        return NULL;
    }
    g_slots[slot].key = keyed;
    g_slots[slot].rgba = rgba;
    g_slots[slot].bytes = bytes;
    g_slots[slot].used = true;
    g_slots[slot].lru = ++g_lru;
    g_diag.allocated += (uint32_t)bytes;
    g_diag.uploads++;
    return rgba;
}
