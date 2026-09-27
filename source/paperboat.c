#include "pb3ds/paperboat.h"

#if defined(__has_include)
#if __has_include("paperboat_config.h")
#include "paperboat_config.h"
#endif
#endif
#ifndef PB3DS_HAS_PAPERBOAT_SLICE
#define PB3DS_HAS_PAPERBOAT_SLICE 0
#endif

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

extern void decode_yay0(void *src, void *dst);
extern int _Printf(char *(*prout)(char *, const char *, size_t), char *arg,
                   const char *fmt, va_list args);

static char *slice_prout(char *dest, const char *src, size_t size) {
    if (dest == NULL || src == NULL) {
        return dest;
    }
    memcpy(dest, src, size);
    return dest + size;
}

bool pb_paperboat_slice_linked(void) {
    return PB3DS_HAS_PAPERBOAT_SLICE != 0;
}

const char *pb_paperboat_commit(void) {
    return PB3DS_PAPERBOAT_COMMIT;
}

const char *pb_paperboat_release(void) {
    return PB3DS_PAPERBOAT_RELEASE;
}

void pb_paperboat_decode_yay0(void *src, void *dst) {
    decode_yay0(src, dst);
}

int pb_paperboat_printf_shim(char *dst, size_t dst_size, const char *fmt, ...) {
    va_list args;
    int written;

    if (dst == NULL || dst_size == 0U || fmt == NULL) {
        return 0;
    }
    memset(dst, 0, dst_size);
    va_start(args, fmt);
    written = _Printf(slice_prout, dst, fmt, args);
    va_end(args);
    if (written < 0) {
        return 0;
    }
    if ((size_t)written >= dst_size) {
        dst[dst_size - 1U] = '\0';
    } else {
        dst[written] = '\0';
    }
    return written;
}
