#ifndef EX3_LOG_H
#define EX3_LOG_H

#include "ex3/plat.h"

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EX3_LOG_INFO = 0,
    EX3_LOG_WARN,
    EX3_LOG_ERROR
} Ex3LogLevel;

void ex3_log_init(Ex3Plat *plat);
void ex3_logf(Ex3LogLevel level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* EX3_LOG_H */
