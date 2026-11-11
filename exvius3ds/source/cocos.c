#include "ex3/cocos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Ex3Node *alloc_node(Ex3NodeKind kind)
{
    Ex3Node *node = (Ex3Node *)calloc(1, sizeof(Ex3Node));
    if (node == NULL) {
        return NULL;
    }
    node->kind = kind;
    node->visible = true;
    node->color_rgba = 0xFFFFFFFFU;
    return node;
}

Ex3Node *ex3_node_create(void)
{
    return alloc_node(EX3_NODE_KIND_NODE);
}

Ex3Sprite *ex3_sprite_create_color(float width, float height, uint32_t rgba)
{
    Ex3Node *node = alloc_node(EX3_NODE_KIND_SPRITE);
    if (node == NULL) {
        return NULL;
    }
    node->width = width;
    node->height = height;
    node->color_rgba = rgba;
    return (Ex3Sprite *)node;
}

Ex3Label *ex3_label_create(const char *text, float width, float height, uint32_t rgba)
{
    Ex3Node *node = alloc_node(EX3_NODE_KIND_LABEL);
    if (node == NULL) {
        return NULL;
    }
    node->width = width;
    node->height = height;
    node->color_rgba = rgba;
    if (text != NULL) {
        snprintf(node->label_text, sizeof(node->label_text), "%s", text);
    }
    return (Ex3Label *)node;
}

Ex3Scene *ex3_scene_create(void)
{
    return (Ex3Scene *)alloc_node(EX3_NODE_KIND_SCENE);
}

struct Ex3Director {
    Ex3Scene *scene;
};

Ex3Director *ex3_director_create(void)
{
    return (Ex3Director *)calloc(1, sizeof(Ex3Director));
}

void ex3_node_destroy(Ex3Node *node)
{
    int i;
    if (node == NULL) {
        return;
    }
    for (i = 0; i < node->child_count; i++) {
        ex3_node_destroy(node->children[i]);
        node->children[i] = NULL;
    }
    free(node);
}

void ex3_director_destroy(Ex3Director *director)
{
    if (director == NULL) {
        return;
    }
    if (director->scene != NULL) {
        ex3_node_destroy((Ex3Node *)director->scene);
        director->scene = NULL;
    }
    free(director);
}

void ex3_node_set_position(Ex3Node *node, float x, float y)
{
    if (node == NULL) {
        return;
    }
    node->x = x;
    node->y = y;
}

void ex3_node_set_visible(Ex3Node *node, bool visible)
{
    if (node == NULL) {
        return;
    }
    node->visible = visible;
}

bool ex3_node_add_child(Ex3Node *parent, Ex3Node *child)
{
    if (parent == NULL || child == NULL) {
        return false;
    }
    if (parent->child_count >= EX3_NODE_MAX_CHILDREN) {
        return false;
    }
    if (child->parent != NULL) {
        return false;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
    return true;
}

void ex3_director_run_with_scene(Ex3Director *director, Ex3Scene *scene)
{
    if (director == NULL) {
        return;
    }
    if (director->scene != NULL && director->scene != scene) {
        ex3_node_destroy((Ex3Node *)director->scene);
    }
    director->scene = scene;
}

Ex3Scene *ex3_director_running_scene(Ex3Director *director)
{
    if (director == NULL) {
        return NULL;
    }
    return director->scene;
}

static void visit_node(Ex3Node *node, Ex3Plat *plat, float ox, float oy)
{
    float ax;
    float ay;
    int i;

    if (node == NULL || plat == NULL || !node->visible) {
        return;
    }

    ax = ox + node->x;
    ay = oy + node->y;

    if (node->kind == EX3_NODE_KIND_SPRITE || node->kind == EX3_NODE_KIND_LABEL) {
        if (node->width > 0.0f && node->height > 0.0f) {
            ex3_plat_draw_rect(plat, EX3_SCREEN_TOP, ax, ay, node->width, node->height,
                               node->color_rgba);
        }
    }

    for (i = 0; i < node->child_count; i++) {
        visit_node(node->children[i], plat, ax, ay);
    }
}

void ex3_director_draw(Ex3Director *director, Ex3Plat *plat)
{
    Ex3Node *root;
    if (director == NULL || plat == NULL || director->scene == NULL) {
        return;
    }
    root = (Ex3Node *)director->scene;
    visit_node(root, plat, 0.0f, 0.0f);
}

Ex3Scene *ex3_diag_scene_create(void)
{
    Ex3Scene *scene = ex3_scene_create();
    Ex3Sprite *sprite;
    Ex3Label *label;

    if (scene == NULL) {
        return NULL;
    }

    /* Centered gold panel on 400x240 top screen. */
    sprite = ex3_sprite_create_color(160.0f, 80.0f, EX3_DIAG_SPRITE_RGBA);
    label = ex3_label_create("Exvius3DS S2", 140.0f, 18.0f, EX3_DIAG_LABEL_RGBA);
    if (sprite == NULL || label == NULL) {
        ex3_node_destroy((Ex3Node *)scene);
        ex3_node_destroy((Ex3Node *)sprite);
        ex3_node_destroy((Ex3Node *)label);
        return NULL;
    }

    ex3_node_set_position((Ex3Node *)sprite, 120.0f, 80.0f);
    ex3_node_set_position((Ex3Node *)label, 10.0f, 31.0f);
    if (!ex3_node_add_child((Ex3Node *)scene, (Ex3Node *)sprite) ||
        !ex3_node_add_child((Ex3Node *)sprite, (Ex3Node *)label)) {
        ex3_node_destroy((Ex3Node *)scene);
        return NULL;
    }
    return scene;
}
