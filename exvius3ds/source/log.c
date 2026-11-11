#include "ex3/log.h"

#include <stdarg.h>
#include <stdio.h>

static Ex3Plat *g_log_plat;

void ex3_log_init(Ex3Plat *plat)
{
    g_log_plat = plat;
}

void ex3_logf(Ex3LogLevel level, const char *fmt, ...)
{
    char buf[512];
    const char *tag = "I";
    va_list args;
    int n;

    if (fmt == NULL) {
        return;
    }
    switch (level) {
        case EX3_LOG_WARN:
            tag = "W";
            break;
        case EX3_LOG_ERROR:
            tag = "E";
            break;
        case EX3_LOG_INFO:
        default:
            tag = "I";
            break;
    }

    n = snprintf(buf, sizeof(buf), "[%s] ", tag);
    if (n < 0) {
        return;
    }
    if ((size_t)n >= sizeof(buf)) {
        n = (int)sizeof(buf) - 1;
    }

    va_start(args, fmt);
    vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, args);
    va_end(args);

    if (g_log_plat != NULL) {
        (void)ex3_plat_log_line(g_log_plat, buf);
    }
}
