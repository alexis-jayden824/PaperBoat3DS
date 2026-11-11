#ifndef EX3_AES128_H
#define EX3_AES128_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AES-128-CBC, no padding (caller supplies block-aligned buffers). */
int ex3_aes128_cbc_encrypt(const uint8_t key[16], const uint8_t iv[16],
                           const uint8_t *in, uint8_t *out, size_t len);
int ex3_aes128_cbc_decrypt(const uint8_t key[16], const uint8_t iv[16],
                           const uint8_t *in, uint8_t *out, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* EX3_AES128_H */
