#ifndef EX3_PREFS_H
#define EX3_PREFS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* SDMC-backed prefs replacing SharedPreferences / SaveUtils JNI path. */
int ex3_prefs_init(const char *sdmc_path);
void ex3_prefs_shutdown(void);
int ex3_prefs_set_string(const char *key, const char *value);
int ex3_prefs_get_string(const char *key, char *out, size_t out_cap);
int ex3_prefs_flush(void);

#ifdef __cplusplus
}
#endif

#endif /* EX3_PREFS_H */
