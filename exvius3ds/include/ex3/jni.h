/* Native replacements for LapisJNI / JniHelper Android edges (S4). */
#ifndef EX3_JNI_H
#define EX3_JNI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LapisJNI.encodeCStringForBase64WithNewCrypto stand-in.
 * Takes plaintext UTF-8, EncodeKey token; writes base64 ciphertext (double-pad).
 * Returns 0 on success, -1 on error.
 */
int ex3_jni_encode_b64_aes(const char *encode_key, const char *plain_utf8,
                           char *out_b64, size_t out_cap);

/* Decode path used by response parsing. */
int ex3_jni_decode_b64_aes(const char *encode_key, const char *b64,
                           uint8_t *out_plain, size_t out_cap, size_t *out_len);

void ex3_jni_rlog(const char *msg);
void ex3_jni_rlogs(const char *msg);
void ex3_jni_purchase_reset_flags(void);
int ex3_jni_purchase_pending(void);

/* Minimal JniHelper stand-ins so recovered callers have a home. */
int ex3_jni_helper_get_static_method(const char *class_name, const char *method_name);
void ex3_jni_helper_call_void(int method_id);

#ifdef __cplusplus
}
#endif

#endif /* EX3_JNI_H */
