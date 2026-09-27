#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void pb_assert_fail(const char *file, int line, const char *expression);
bool pb_assert_failed(void);
const char *pb_assert_expression(void);
int pb_assert_line(void);

#define PB_ASSERT(expression)                                                  \
    do {                                                                       \
        if (!(expression)) {                                                   \
            pb_assert_fail(__FILE__, __LINE__, #expression);                   \
        }                                                                      \
    } while (0)

#ifdef __cplusplus
}
#endif
