#ifndef EX3_BASE64_H
#define EX3_BASE64_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int ex3_base64_encode(const uint8_t *in, size_t in_len, char *out, size_t out_cap);
int ex3_base64_decode(const char *in, uint8_t *out, size_t out_cap, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* EX3_BASE64_H */
