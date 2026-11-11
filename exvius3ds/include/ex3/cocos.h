/* Minimal cocos2d-x-shaped scene graph for the Exvius3DS recomp shim (S2).
 *
 * Not a full engine — just Director/Scene/Node/Sprite/Label enough for recovered
 * client bring-up and a diagnostic top-screen draw path.
 */
#ifndef EX3_COCOS_H
#define EX3_COCOS_H

#include "ex3/plat.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    EX3_NODE_MAX_CHILDREN = 16
};

typedef struct Ex3Node Ex3Node;
typedef struct Ex3Sprite Ex3Sprite;
typedef struct Ex3Label Ex3Label;
typedef struct Ex3Scene Ex3Scene;
typedef struct Ex3Director Ex3Director;

typedef enum {
    EX3_NODE_KIND_NODE = 0,
    EX3_NODE_KIND_SPRITE,
    EX3_NODE_KIND_LABEL,
    EX3_NODE_KIND_SCENE
} Ex3NodeKind;

struct Ex3Node {
    Ex3NodeKind kind;
    float x;
    float y;
    float width;
    float height;
    uint32_t color_rgba;
    bool visible;
    Ex3Node *parent;
    Ex3Node *children[EX3_NODE_MAX_CHILDREN];
    int child_count;
    char label_text[64];
};

Ex3Node *ex3_node_create(void);
Ex3Sprite *ex3_sprite_create_color(float width, float height, uint32_t rgba);
Ex3Label *ex3_label_create(const char *text, float width, float height, uint32_t rgba);
Ex3Scene *ex3_scene_create(void);
Ex3Director *ex3_director_create(void);

void ex3_node_destroy(Ex3Node *node);
void ex3_director_destroy(Ex3Director *director);

void ex3_node_set_position(Ex3Node *node, float x, float y);
void ex3_node_set_visible(Ex3Node *node, bool visible);
bool ex3_node_add_child(Ex3Node *parent, Ex3Node *child);

void ex3_director_run_with_scene(Ex3Director *director, Ex3Scene *scene);
Ex3Scene *ex3_director_running_scene(Ex3Director *director);

/* Visit scene graph and issue plat draw_rect calls on the top screen. */
void ex3_director_draw(Ex3Director *director, Ex3Plat *plat);

/* Build the S2 hardware diagnostic scene (gold sprite + pale label on navy). */
Ex3Scene *ex3_diag_scene_create(void);

#ifdef __cplusplus
}
#endif

#endif /* EX3_COCOS_H */
