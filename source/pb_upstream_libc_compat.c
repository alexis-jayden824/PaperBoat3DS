#if defined(__has_include)
#if __has_include("paperboat_config.h")
#include "paperboat_config.h"
#endif
#endif
#ifndef PB3DS_HAS_PAPERBOAT_SLICE
#define PB3DS_HAS_PAPERBOAT_SLICE 0
#endif

#if PB3DS_HAS_PAPERBOAT_SLICE
#include PB3DS_PAPERBOAT_SLICE_LIBC
#else
#include <stdarg.h>
#include <stddef.h>

int _Printf(char *(*prout)(char *, const char *, size_t), char *arg, const char *fmt,
            va_list args) {
    (void)prout;
    (void)arg;
    (void)fmt;
    (void)args;
    return 0;
}
#endif
