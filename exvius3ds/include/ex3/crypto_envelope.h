/* Recovered wire-crypto envelope helpers for Exvius3DS (S0).
 *
 * Provenance: Elvlin/exvius-offline FFBE_SERVER_CONTRACT.md §1e and
 * source/server/ffbe_crypto.py (key16, IV, double-pad rules).
 * Algorithm recovery only — no Square Enix binary is vendored.
 */
#ifndef EX3_CRYPTO_ENVELOPE_H
#define EX3_CRYPTO_ENVELOPE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EX3_AES_BLOCK = 16,
    EX3_AES_KEY_LEN = 16,
    EX3_AES_IV_LEN = 16
};

/* Fixed CBC IV recovered from libgame.so .rodata / LapisJNI path. */
const uint8_t *ex3_crypto_iv(void);

/* 8-char EncodeKey → 16-byte AES key (UTF-8 bytes, NUL-padded).
 * Returns 0 on success, -1 if token is NULL.
 */
int ex3_crypto_key16(const char *token, uint8_t out[EX3_AES_KEY_LEN]);

/* Manual PKCS7 pad into dst. dst_cap must be >= len + 16.
 * Returns padded length, or 0 on error.
 */
size_t ex3_crypto_pkcs7_pad(const uint8_t *src, size_t len,
                            uint8_t *dst, size_t dst_cap);

/* Client request path: manual PKCS7 then an extra full 0x10 block
 * (LapisJNI double-pad). Returns padded length, or 0 on error.
 */
size_t ex3_crypto_double_pad(const uint8_t *src, size_t len,
                             uint8_t *dst, size_t dst_cap);

/* Validate and strip a single PKCS7 trailer. Returns unpadded length
 * via *out_len; returns 0 on success, -1 on bad padding.
 */
int ex3_crypto_pkcs7_unpad(uint8_t *buf, size_t len, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* EX3_CRYPTO_ENVELOPE_H */
