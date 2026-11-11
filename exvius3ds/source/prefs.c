#include "ex3/prefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { EX3_PREFS_MAX = 64, EX3_PREFS_KEY = 64, EX3_PREFS_VAL = 256 };

typedef struct {
    char key[EX3_PREFS_KEY];
    char value[EX3_PREFS_VAL];
    int used;
} Ex3PrefEntry;

static Ex3PrefEntry g_entries[EX3_PREFS_MAX];
static char g_path[256];
static int g_ready;

int ex3_prefs_init(const char *sdmc_path)
{
    memset(g_entries, 0, sizeof(g_entries));
    g_ready = 0;
    if (sdmc_path == NULL || sdmc_path[0] == '\0') {
        return -1;
    }
    snprintf(g_path, sizeof(g_path), "%s", sdmc_path);
    g_ready = 1;
    /* Best-effort load */
    {
        FILE *fp = fopen(g_path, "rb");
        char line[EX3_PREFS_KEY + EX3_PREFS_VAL + 8];
        if (fp == NULL) {
            return 0;
        }
        while (fgets(line, sizeof(line), fp) != NULL) {
            char *eq = strchr(line, '=');
            char *nl;
            if (eq == NULL) {
                continue;
            }
            *eq = '\0';
            nl = strchr(eq + 1, '\n');
            if (nl != NULL) {
                *nl = '\0';
            }
            (void)ex3_prefs_set_string(line, eq + 1);
        }
        fclose(fp);
    }
    return 0;
}

void ex3_prefs_shutdown(void)
{
    g_ready = 0;
    g_path[0] = '\0';
}

int ex3_prefs_set_string(const char *key, const char *value)
{
    int i;
    int free_slot = -1;
    if (!g_ready || key == NULL || value == NULL) {
        return -1;
    }
    for (i = 0; i < EX3_PREFS_MAX; i++) {
        if (g_entries[i].used && strcmp(g_entries[i].key, key) == 0) {
            snprintf(g_entries[i].value, sizeof(g_entries[i].value), "%s", value);
            return 0;
        }
        if (!g_entries[i].used && free_slot < 0) {
            free_slot = i;
        }
    }
    if (free_slot < 0) {
        return -1;
    }
    g_entries[free_slot].used = 1;
    snprintf(g_entries[free_slot].key, sizeof(g_entries[free_slot].key), "%s", key);
    snprintf(g_entries[free_slot].value, sizeof(g_entries[free_slot].value), "%s", value);
    return 0;
}

int ex3_prefs_get_string(const char *key, char *out, size_t out_cap)
{
    int i;
    if (!g_ready || key == NULL || out == NULL || out_cap == 0) {
        return -1;
    }
    for (i = 0; i < EX3_PREFS_MAX; i++) {
        if (g_entries[i].used && strcmp(g_entries[i].key, key) == 0) {
            snprintf(out, out_cap, "%s", g_entries[i].value);
            return 0;
        }
    }
    return -1;
}

int ex3_prefs_flush(void)
{
    FILE *fp;
    int i;
    if (!g_ready) {
        return -1;
    }
    fp = fopen(g_path, "wb");
    if (fp == NULL) {
        return -1;
    }
    for (i = 0; i < EX3_PREFS_MAX; i++) {
        if (g_entries[i].used) {
            fprintf(fp, "%s=%s\n", g_entries[i].key, g_entries[i].value);
        }
    }
    fclose(fp);
    return 0;
}
