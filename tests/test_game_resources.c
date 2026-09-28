#include "pb3ds/fs.h"
#include "pb3ds/log.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void *ResourceGetDataByName(const char *name);
size_t ResourceGetSizeByName(const char *name);
uint16_t GameEngine_GetTexWidthExact(const char *name);
uint16_t GameEngine_GetTexHeightExact(const char *name);
uint8_t GameEngine_OTRSigCheck(const char *name);
void pb_game_resources_shutdown(void);

void pb_log(PBLogLevel level, const char *component, const char *message) {
    (void)level;
    (void)component;
    (void)message;
}

#define CHECK(value) do { if (!(value)) { \
    fprintf(stderr, "game resource check failed: %s:%d: %s\n", \
            __FILE__, __LINE__, #value); return 1; } } while (0)

int main(int argc, char **argv) {
    const uint8_t *blob;
    const uint8_t *texture;
    const uint32_t *dl;
    if (argc != 2) return 2;
    pb_fs_host_set_root(argv[1]);
    pb_fs_init();
    CHECK(GameEngine_OTRSigCheck("__OTR__textures/one") == 1U);
    CHECK(GameEngine_OTRSigCheck((const char *)(uintptr_t)0x80001000U) == 0U);
    CHECK(GameEngine_OTRSigCheck((const char *)(uintptr_t)0x40U) == 0U);
    blob = ResourceGetDataByName("__OTR__data/one");
    CHECK(blob != NULL && ResourceGetSizeByName("data/one") == 4U);
    CHECK(memcmp(blob, "ABCD", 4U) == 0);
    texture = ResourceGetDataByName("__OTR__textures/one");
    CHECK(texture != NULL && texture[0] == 0xF8U && texture[1] == 0x01U);
    CHECK(GameEngine_GetTexWidthExact("textures/one") == 1U);
    CHECK(GameEngine_GetTexHeightExact("textures/one") == 1U);
    dl = ResourceGetDataByName("lists/one");
    CHECK(dl != NULL && dl[0] == 0xDF000000U && dl[1] == 0U);
    CHECK(ResourceGetDataByName("missing") == NULL);
    CHECK(ResourceGetDataByName("invalid/one") == NULL);
    pb_game_resources_shutdown();
    pb_fs_shutdown();
    puts("M13 game resource decode passed");
    return 0;
}
