#include "pb3ds/world_scene.h"

#include <3ds.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PB_OTR_HEADER_SIZE 64U
#define PB_OTR_BLOB_TYPE 0x4F424C42U
#define PB_OTR_DISPLAY_LIST_TYPE 0x4F444C54U
#define PB_OTR_VERTEX_TYPE 0x4F565458U
#define PB_SHAPE_MAX_BYTES (256U * 1024U)
#define PB_COLLISION_MAX_BYTES (64U * 1024U)
#define PB_VERTEX_MAX_BYTES (128U * 1024U)
#define PB_DISPLAY_LIST_MAX_BYTES (32U * 1024U)
#define PB_TEXTURE_MAX_BYTES (512U * 1024U)
#define PB_WORLD_MAX_LEAVES 512U
#define PB_WORLD_MAX_SHAPE_NODES 1024U
#define PB_WORLD_TEXTURE_INDEX_CAPACITY 256U
#define PB_WORLD_MAX_RECURSION 32U
#define PB_WORLD_FADE_FRAMES 30U
#define PB_WORLD_EXIT_COOLDOWN 90U
#define PB_WORLD_ENTRY_WALK_FRAMES 45U
#define PB_WORLD_PLAYER_RADIUS 12.0f
#define PB_WORLD_PLAYER_HEIGHT 20.0f
#define PB_WORLD_MAX_STEP 42.0f
#define PB_WORLD_MAX_FLOOR_RAY 320.0f
#define PB_WORLD_PI 3.14159265358979323846f

#define PB_G_DL_OTR_HASH 0x31U
#define PB_G_VTX_OTR_HASH 0x32U
#define PB_G_MARKER 0x33U
#define PB_G_TRI1 0x05U
#define PB_G_TRI2 0x06U
#define PB_G_END_DL 0xDFU
#define PB_G_LIGHTING 0x00020000U

typedef struct {
    const uint8_t *body;
    size_t body_size;
    bool big_endian;
} PBResourceView;

typedef struct {
    const uint8_t *data;
    size_t size;
} PBBlobView;

typedef struct {
    uint8_t *allocation;
    size_t allocation_size;
    PBResourceView resource;
    uint64_t hash;
} PBDisplayListResource;

typedef struct {
    uint32_t display_list_offset;
    char texture_name[PB_WORLD_TEXTURE_NAME_CAPACITY];
    uint32_t render_mode;
    float transform[4][4];
    int16_t texture_index;
} PBShapeLeaf;

typedef struct {
    const uint8_t *shape;
    size_t shape_size;
    PBShapeLeaf *leaves;
    size_t leaf_count;
    uint32_t *visited;
    size_t visited_capacity;
    size_t visited_count;
} PBShapeWalk;

typedef struct {
    PBO2REntry display_list_entries[PB_WORLD_MAX_DISPLAY_LISTS];
    PBDisplayListResource display_lists[PB_WORLD_MAX_DISPLAY_LISTS];
    PBShapeLeaf leaves[PB_WORLD_MAX_LEAVES];
    PBO2REntry texture_entries[PB_WORLD_TEXTURE_INDEX_CAPACITY];
    uint32_t visited[PB_WORLD_MAX_SHAPE_NODES];
} PBWorldLoadScratch;

_Static_assert(sizeof(PBWorldLoadScratch) <= PB_MEMORY_TRANSIENT_LIMIT,
               "world loader scratch exceeds transient budget");

typedef struct {
    const char *map_id;
    const char *texture_archive;
    const char *exit_collider;
    const char *exit_map;
    uint8_t exit_entry;
    const char *sign_collider;
    bool has_star_piece;
} PBMapContract;

typedef struct {
    const char *map_id;
    uint8_t entry_id;
    PBWorldVec3 position;
    float yaw;
} PBEntryContract;

typedef struct {
    PBWorldScene *scene;
    const PBResourceView *vertices;
    uint32_t vertex_count;
    const PBDisplayListResource *display_lists;
    size_t display_list_count;
    const PBShapeLeaf *leaf;
    uint16_t slots[64];
    bool slot_loaded[64];
    uint32_t geometry_mode;
} PBDisplayListInterpreter;

static const PBMapContract kMapContracts[] = {
    {
        "mac_00", "mac_tex", "deilie", "mac_01", 0U, "sign", true,
    },
    {
        "mac_01", "mac_tex", "deiliw", "mac_00", 1U, NULL, false,
    },
};

/* PaperBoat 1.0.1 map entry tables, kept explicit for the M13 slice. */
static const PBEntryContract kEntryContracts[] = {
    { "mac_00", 1U, { 600.0f, 0.0f, 0.0f }, 270.0f },
    { "mac_00", 6U, { -100.0f, 30.0f, -370.0f }, 135.0f },
    { "mac_01", 0U, { -600.0f, 0.0f, 0.0f }, 90.0f },
};

static uint16_t read_u16(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return (uint16_t)(((uint16_t)bytes[0] << 8U) | bytes[1]);
    }
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8U));
}

static int16_t read_s16(const uint8_t *bytes, bool big_endian) {
    return (int16_t)read_u16(bytes, big_endian);
}

static uint32_t read_u32(const uint8_t *bytes, bool big_endian) {
    if (big_endian) {
        return ((uint32_t)bytes[0] << 24U) |
               ((uint32_t)bytes[1] << 16U) |
               ((uint32_t)bytes[2] << 8U) | bytes[3];
    }
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8U) |
           ((uint32_t)bytes[2] << 16U) | ((uint32_t)bytes[3] << 24U);
}

static int32_t read_s32(const uint8_t *bytes, bool big_endian) {
    return (int32_t)read_u32(bytes, big_endian);
}

static float read_f32(const uint8_t *bytes, bool big_endian) {
    const uint32_t bits = read_u32(bytes, big_endian);
    float value = 0.0f;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static bool span_is_valid(size_t size, uint32_t offset, size_t length) {
    return (size_t)offset <= size && length <= size - (size_t)offset;
}

static bool parse_resource(const uint8_t *data, size_t size,
                           uint32_t expected_type,
                           PBResourceView *resource) {
    if (data == NULL || resource == NULL || size < PB_OTR_HEADER_SIZE ||
        data[0] > 1U) {
        return false;
    }
    const bool big_endian = data[0] == 1U;
    if (read_u32(&data[4], big_endian) != expected_type ||
        read_u32(&data[8], big_endian) != 0U) {
        return false;
    }
    resource->body = &data[PB_OTR_HEADER_SIZE];
    resource->body_size = size - PB_OTR_HEADER_SIZE;
    resource->big_endian = big_endian;
    return true;
}

static bool parse_blob(const uint8_t *data, size_t size, PBBlobView *blob) {
    PBResourceView resource;
    if (blob == NULL ||
        !parse_resource(data, size, PB_OTR_BLOB_TYPE, &resource) ||
        resource.body_size < sizeof(uint32_t)) {
        return false;
    }
    const uint32_t payload_size =
        read_u32(resource.body, resource.big_endian);
    if (payload_size != resource.body_size - sizeof(uint32_t)) {
        return false;
    }
    blob->data = &resource.body[sizeof(uint32_t)];
    blob->size = payload_size;
    return true;
}

static bool opcode_is_expanded(uint8_t opcode) {
    return opcode == 0x20U || opcode == 0x31U || opcode == 0x32U ||
           opcode == 0x33U || opcode == 0x35U || opcode == 0x36U ||
           opcode == 0x42U;
}

static const PBMapContract *find_map_contract(const char *map_id) {
    if (map_id == NULL) {
        return NULL;
    }
    for (size_t index = 0U;
         index < sizeof(kMapContracts) / sizeof(kMapContracts[0]); index++) {
        if (strcmp(kMapContracts[index].map_id, map_id) == 0) {
            return &kMapContracts[index];
        }
    }
    return NULL;
}

static const PBEntryContract *find_entry_contract(const char *map_id,
                                                   uint8_t entry_id) {
    for (size_t index = 0U;
         index < sizeof(kEntryContracts) / sizeof(kEntryContracts[0]);
         index++) {
        if (kEntryContracts[index].entry_id == entry_id &&
            strcmp(kEntryContracts[index].map_id, map_id) == 0) {
            return &kEntryContracts[index];
        }
    }
    return NULL;
}

static PBWorldSceneResult map_archive_failure(PBO2RResult result) {
    if (result == PB_O2R_ENTRY_NOT_FOUND) {
        return PB_WORLD_SCENE_RESOURCE_MISSING;
    }
    if (result == PB_O2R_OUT_OF_MEMORY) {
        return PB_WORLD_SCENE_OUT_OF_MEMORY;
    }
    if (result == PB_O2R_CAPACITY_EXCEEDED) {
        return PB_WORLD_SCENE_CAPACITY;
    }
    return PB_WORLD_SCENE_ARCHIVE_ERROR;
}

static bool extract_entry(PBWorldScene *scene, PBArchive *archive,
                          const PBO2REntry *entry, size_t maximum_size,
                          PBMemoryMonitor *memory, uint8_t **data,
                          size_t *size) {
    scene->archive_result = pb_o2r_extract_entry(
        archive, entry, maximum_size, memory, PB_MEMORY_SCENE, data, size,
        &scene->archive_stats);
    return scene->archive_result == PB_O2R_OK;
}

static const PBO2REntry *find_indexed_entry(const PBO2REntry *entries,
                                            size_t entry_count,
                                            const char *name) {
    for (size_t index = 0U; index < entry_count; index++) {
        if (strcmp(entries[index].name, name) == 0) {
            return &entries[index];
        }
    }
    return NULL;
}

static void matrix_identity(float matrix[4][4]) {
    memset(matrix, 0, sizeof(float) * 16U);
    for (size_t index = 0U; index < 4U; index++) {
        matrix[index][index] = 1.0f;
    }
}

static void matrix_multiply(float output[4][4], const float left[4][4],
                            const float right[4][4]) {
    float result[4][4];
    for (size_t row = 0U; row < 4U; row++) {
        for (size_t column = 0U; column < 4U; column++) {
            result[row][column] =
                left[row][0] * right[0][column] +
                left[row][1] * right[1][column] +
                left[row][2] * right[2][column] +
                left[row][3] * right[3][column];
        }
    }
    memcpy(output, result, sizeof(result));
}

static bool read_fixed_matrix(const uint8_t *shape, size_t shape_size,
                              uint32_t offset, float output[4][4]) {
    if (!span_is_valid(shape_size, offset, 64U)) {
        return false;
    }
    for (size_t row = 0U; row < 4U; row++) {
        for (size_t pair = 0U; pair < 2U; pair++) {
            const size_t index = row * 2U + pair;
            const uint32_t integer =
                read_u32(&shape[offset + index * 4U], false);
            const uint32_t fraction =
                read_u32(&shape[offset + 32U + index * 4U], false);
            const int32_t first = (int32_t)(
                (integer & 0xFFFF0000U) | (fraction >> 16U));
            const int32_t second = (int32_t)(
                (integer << 16U) | (fraction & 0x0000FFFFU));
            output[row][pair * 2U] = (float)first / 65536.0f;
            output[row][pair * 2U + 1U] =
                (float)second / 65536.0f;
        }
    }
    return true;
}

static PBWorldVec3 transform_point(const PBWorldVec3 *point,
                                   const float matrix[4][4]) {
    PBWorldVec3 result;
    result.x = point->x * matrix[0][0] + point->y * matrix[1][0] +
               point->z * matrix[2][0] + matrix[3][0];
    result.y = point->x * matrix[0][1] + point->y * matrix[1][1] +
               point->z * matrix[2][1] + matrix[3][1];
    result.z = point->x * matrix[0][2] + point->y * matrix[1][2] +
               point->z * matrix[2][2] + matrix[3][2];
    return result;
}

static bool shape_offset_seen(const PBShapeWalk *walk, uint32_t offset) {
    for (size_t index = 0U; index < walk->visited_count; index++) {
        if (walk->visited[index] == offset) {
            return true;
        }
    }
    return false;
}

static bool copy_shape_string(char *destination, size_t capacity,
                              const uint8_t *shape, size_t shape_size,
                              uint32_t offset) {
    if (destination == NULL || capacity == 0U || offset >= shape_size) {
        return false;
    }
    size_t length = 0U;
    while ((size_t)offset + length < shape_size &&
           shape[(size_t)offset + length] != 0U) {
        const uint8_t value = shape[(size_t)offset + length];
        if (value < 0x20U || value > 0x7EU || length + 1U >= capacity) {
            destination[0] = '\0';
            return false;
        }
        destination[length++] = (char)value;
    }
    if ((size_t)offset + length >= shape_size) {
        destination[0] = '\0';
        return false;
    }
    destination[length] = '\0';
    return length > 0U;
}

static bool walk_shape_node(PBShapeWalk *walk, uint32_t node_offset,
                            uint32_t depth,
                            const float parent_transform[4][4]) {
    if (walk == NULL || depth > PB_WORLD_MAX_RECURSION ||
        walk->visited == NULL ||
        walk->visited_count >= walk->visited_capacity ||
        shape_offset_seen(walk, node_offset) ||
        !span_is_valid(walk->shape_size, node_offset, 20U)) {
        return false;
    }
    walk->visited[walk->visited_count++] = node_offset;
    const uint8_t *node = &walk->shape[node_offset];
    const int32_t type = read_s32(&node[0], false);
    const uint32_t display_data = read_u32(&node[4], false);
    const int32_t property_count = read_s32(&node[8], false);
    const uint32_t property_list = read_u32(&node[12], false);
    const uint32_t group_data = read_u32(&node[16], false);
    if ((type != 2 && type != 5 && type != 7 && type != 10) ||
        property_count < 0 || property_count > 1024 ||
        (property_count > 0 &&
         !span_is_valid(walk->shape_size, property_list,
                        (size_t)property_count * 12U))) {
        return false;
    }

    char texture_name[PB_WORLD_TEXTURE_NAME_CAPACITY] = { 0 };
    uint32_t render_mode = 1U;
    for (int32_t index = 0; index < property_count; index++) {
        const uint8_t *property =
            &walk->shape[(size_t)property_list + (size_t)index * 12U];
        const uint32_t key = read_u32(&property[0], false);
        const uint32_t value = read_u32(&property[8], false);
        if (key == 0x5EU) {
            (void)copy_shape_string(texture_name, sizeof(texture_name),
                                    walk->shape, walk->shape_size, value);
        } else if (key == 0x5CU) {
            render_mode = value;
        }
    }

    float world[4][4];
    memcpy(world, parent_transform, sizeof(world));
    int32_t child_count = 0;
    uint32_t child_list = 0U;
    if (group_data != 0U) {
        if (!span_is_valid(walk->shape_size, group_data, 20U)) {
            return false;
        }
        const uint8_t *group = &walk->shape[group_data];
        const uint32_t matrix_offset = read_u32(&group[0], false);
        child_count = read_s32(&group[12], false);
        child_list = read_u32(&group[16], false);
        if (child_count < 0 || child_count > 1024 ||
            (child_count > 0 &&
             !span_is_valid(walk->shape_size, child_list,
                            (size_t)child_count * sizeof(uint32_t)))) {
            return false;
        }
        if (matrix_offset != 0U) {
            float local[4][4];
            if (!read_fixed_matrix(walk->shape, walk->shape_size,
                                   matrix_offset, local)) {
                return false;
            }
            matrix_multiply(world, local, parent_transform);
        }
    }

    if (type == 2 && display_data != 0U) {
        if (walk->leaf_count >= PB_WORLD_MAX_LEAVES ||
            !span_is_valid(walk->shape_size, display_data, 8U)) {
            return false;
        }
        const uint32_t display_list =
            read_u32(&walk->shape[display_data], false);
        if (display_list == 0U) {
            return false;
        }
        PBShapeLeaf *leaf = &walk->leaves[walk->leaf_count++];
        memset(leaf, 0, sizeof(*leaf));
        leaf->display_list_offset = display_list;
        leaf->render_mode = render_mode;
        leaf->texture_index = -1;
        memcpy(leaf->texture_name, texture_name,
               sizeof(leaf->texture_name));
        memcpy(leaf->transform, world, sizeof(leaf->transform));
    }

    for (int32_t index = 0; index < child_count; index++) {
        const uint32_t child = read_u32(
            &walk->shape[(size_t)child_list + (size_t)index * 4U], false);
        if (child == 0U ||
            !walk_shape_node(walk, child, depth + 1U, world)) {
            return false;
        }
    }
    return true;
}

static bool collect_shape_leaves(const PBBlobView *shape_blob,
                                 PBShapeLeaf *leaves, uint32_t *visited,
                                 size_t visited_capacity, size_t *leaf_count,
                                 uint32_t *node_count) {
    if (shape_blob == NULL || leaves == NULL || visited == NULL ||
        visited_capacity < PB_WORLD_MAX_SHAPE_NODES ||
        leaf_count == NULL || node_count == NULL ||
        shape_blob->size < 32U) {
        return false;
    }
    const uint32_t root = read_u32(&shape_blob->data[0], false);
    float identity[4][4];
    matrix_identity(identity);
    PBShapeWalk walk = {
        .shape = shape_blob->data,
        .shape_size = shape_blob->size,
        .leaves = leaves,
        .visited = visited,
        .visited_capacity = visited_capacity,
    };
    if (root == 0U || !walk_shape_node(&walk, root, 0U, identity) ||
        walk.leaf_count == 0U) {
        return false;
    }
    *leaf_count = walk.leaf_count;
    *node_count = (uint32_t)walk.visited_count;
    return true;
}

static bool parse_display_list_resource(uint8_t *data, size_t size,
                                        PBDisplayListResource *display_list) {
    PBResourceView resource;
    if (display_list == NULL ||
        !parse_resource(data, size, PB_OTR_DISPLAY_LIST_TYPE, &resource) ||
        resource.body_size < 24U || resource.body[0] != 4U) {
        return false;
    }
    const uint32_t marker = read_u32(&resource.body[8],
                                     resource.big_endian);
    if ((uint8_t)(marker >> 24U) != PB_G_MARKER) {
        return false;
    }
    const uint32_t high = read_u32(&resource.body[16],
                                   resource.big_endian);
    const uint32_t low = read_u32(&resource.body[20],
                                  resource.big_endian);
    display_list->allocation = data;
    display_list->allocation_size = size;
    display_list->resource = resource;
    display_list->hash = ((uint64_t)high << 32U) | low;
    return true;
}

static const PBDisplayListResource *find_display_list_hash(
    const PBDisplayListResource *display_lists, size_t display_list_count,
    uint64_t hash) {
    for (size_t index = 0U; index < display_list_count; index++) {
        if (display_lists[index].hash == hash) {
            return &display_lists[index];
        }
    }
    return NULL;
}

static const PBDisplayListResource *find_display_list_name(
    const PBDisplayListResource *display_lists,
    const PBO2REntry *entries, size_t display_list_count,
    const char *name) {
    for (size_t index = 0U; index < display_list_count; index++) {
        if (strcmp(entries[index].name, name) == 0) {
            return &display_lists[index];
        }
    }
    return NULL;
}

static void release_display_lists(PBDisplayListResource *display_lists,
                                  size_t display_list_count,
                                  PBMemoryMonitor *memory) {
    for (size_t index = 0U; index < display_list_count; index++) {
        if (display_lists[index].allocation != NULL) {
            pb_memory_free(memory, PB_MEMORY_SCENE,
                           display_lists[index].allocation,
                           display_lists[index].allocation_size);
            memset(&display_lists[index], 0, sizeof(display_lists[index]));
        }
    }
}

static bool parse_vertex_resource(const uint8_t *data, size_t size,
                                  PBResourceView *vertices,
                                  uint32_t *vertex_count) {
    if (!parse_resource(data, size, PB_OTR_VERTEX_TYPE, vertices) ||
        vertices->body_size < 4U) {
        return false;
    }
    const uint32_t count =
        read_u32(vertices->body, vertices->big_endian);
    if (count == 0U || count > PB_WORLD_MAX_SOURCE_VERTICES ||
        vertices->body_size - 4U != (size_t)count * 16U) {
        return false;
    }
    *vertex_count = count;
    return true;
}

static bool render_mode_is_translucent(uint32_t mode) {
    return (mode >= 0x11U && mode <= 0x27U) || mode == 0x29U ||
           (mode >= 0x2CU && mode <= 0x2FU);
}

static bool render_mode_is_cutout(uint32_t mode) {
    return mode == 0x0DU || mode == 0x0FU || mode == 0x10U ||
           mode == 0x2BU;
}

static bool read_source_vertex(const PBResourceView *resource,
                               uint32_t vertex_count, uint16_t index,
                               PBWorldVertex *vertex,
                               const PBShapeLeaf *leaf,
                               const PBWorldTexture *texture,
                               bool lighting) {
    if (resource == NULL || vertex == NULL || leaf == NULL ||
        index >= vertex_count) {
        return false;
    }
    const uint8_t *source = &resource->body[4U + (size_t)index * 16U];
    PBWorldVec3 position = {
        (float)read_s16(&source[0], resource->big_endian),
        (float)read_s16(&source[2], resource->big_endian),
        (float)read_s16(&source[4], resource->big_endian),
    };
    vertex->position = transform_point(&position, leaf->transform);
    const int16_t texture_s = read_s16(&source[8], resource->big_endian);
    const int16_t texture_t = read_s16(&source[10], resource->big_endian);
    if (texture != NULL && texture->decoded.texture_width != 0U &&
        texture->decoded.texture_height != 0U) {
        vertex->u = (float)texture_s /
                    (32.0f * (float)texture->decoded.texture_width);
        vertex->v = (float)texture_t /
                    (32.0f * (float)texture->decoded.texture_height);
    } else {
        vertex->u = 0.0f;
        vertex->v = 0.0f;
    }
    if (lighting) {
        PBWorldVec3 normal = {
            (float)(int8_t)source[12] / 127.0f,
            (float)(int8_t)source[13] / 127.0f,
            (float)(int8_t)source[14] / 127.0f,
        };
        PBWorldVec3 transformed = {
            normal.x * leaf->transform[0][0] +
                normal.y * leaf->transform[1][0] +
                normal.z * leaf->transform[2][0],
            normal.x * leaf->transform[0][1] +
                normal.y * leaf->transform[1][1] +
                normal.z * leaf->transform[2][1],
            normal.x * leaf->transform[0][2] +
                normal.y * leaf->transform[1][2] +
                normal.z * leaf->transform[2][2],
        };
        const float length = sqrtf(transformed.x * transformed.x +
                                   transformed.y * transformed.y +
                                   transformed.z * transformed.z);
        float diffuse = 0.0f;
        if (length > 0.0001f) {
            diffuse = (-0.36f * transformed.x +
                       0.86f * transformed.y +
                       0.36f * transformed.z) /
                      length;
        }
        if (diffuse < 0.0f) {
            diffuse = 0.0f;
        }
        const uint8_t shade = (uint8_t)(150.0f + diffuse * 105.0f);
        vertex->red = shade;
        vertex->green = shade;
        vertex->blue = shade;
    } else {
        vertex->red = source[12];
        vertex->green = source[13];
        vertex->blue = source[14];
    }
    vertex->alpha = source[15];
    return true;
}

static bool emit_triangle(PBDisplayListInterpreter *interpreter,
                          uint8_t first, uint8_t second, uint8_t third) {
    if (first >= 64U || second >= 64U || third >= 64U ||
        !interpreter->slot_loaded[first] ||
        !interpreter->slot_loaded[second] ||
        !interpreter->slot_loaded[third] ||
        interpreter->scene->stats.triangles >= PB_WORLD_MAX_TRIANGLES) {
        return false;
    }
    PBWorldTriangle *triangle =
        &interpreter->scene->triangles[interpreter->scene->stats.triangles];
    memset(triangle, 0, sizeof(*triangle));
    triangle->texture_index = interpreter->leaf->texture_index;
    if (render_mode_is_translucent(interpreter->leaf->render_mode)) {
        triangle->render_class = PB_WORLD_RENDER_TRANSLUCENT;
    } else if (render_mode_is_cutout(interpreter->leaf->render_mode)) {
        triangle->render_class = PB_WORLD_RENDER_CUTOUT;
    } else {
        triangle->render_class = PB_WORLD_RENDER_OPAQUE;
    }
    const PBWorldTexture *texture =
        triangle->texture_index >= 0
            ? &interpreter->scene->textures[triangle->texture_index]
            : NULL;
    const uint8_t slots[3] = { first, second, third };
    for (size_t index = 0U; index < 3U; index++) {
        if (!read_source_vertex(
                interpreter->vertices, interpreter->vertex_count,
                interpreter->slots[slots[index]], &triangle->vertices[index],
                interpreter->leaf, texture,
                (interpreter->geometry_mode & PB_G_LIGHTING) != 0U)) {
            return false;
        }
    }
    interpreter->scene->stats.triangles++;
    if (texture != NULL) {
        interpreter->scene->stats.textured_triangles++;
    }
    if ((interpreter->geometry_mode & PB_G_LIGHTING) != 0U) {
        interpreter->scene->stats.lit_triangles++;
    }
    return true;
}

static bool interpret_display_list(PBDisplayListInterpreter *interpreter,
                                   const PBDisplayListResource *display_list,
                                   uint32_t depth) {
    if (interpreter == NULL || display_list == NULL ||
        depth > PB_WORLD_MAX_RECURSION) {
        return false;
    }
    const PBResourceView *resource = &display_list->resource;
    size_t offset = 8U;
    uint32_t safety = 0U;
    while (offset <= resource->body_size - 8U && safety++ < 4096U) {
        const uint32_t word0 =
            read_u32(&resource->body[offset], resource->big_endian);
        const uint32_t word1 =
            read_u32(&resource->body[offset + 4U], resource->big_endian);
        const uint8_t opcode = (uint8_t)(word0 >> 24U);
        offset += 8U;
        uint64_t extra = 0U;
        if (opcode_is_expanded(opcode)) {
            if (offset > resource->body_size - 8U) {
                return false;
            }
            const uint32_t high =
                read_u32(&resource->body[offset], resource->big_endian);
            const uint32_t low = read_u32(&resource->body[offset + 4U],
                                          resource->big_endian);
            extra = ((uint64_t)high << 32U) | low;
            offset += 8U;
        }
        interpreter->scene->stats.display_list_commands++;

        if (opcode == 0xD9U) {
            interpreter->geometry_mode &= word0 & 0x00FFFFFFU;
            interpreter->geometry_mode |= word1;
            continue;
        }
        if (opcode == PB_G_MARKER || opcode == 0x00U || opcode == 0xE7U) {
            continue;
        }
        if (opcode == PB_G_VTX_OTR_HASH) {
            const uint32_t count = (word0 >> 12U) & 0xFFU;
            const uint32_t end = (word0 >> 1U) & 0x7FU;
            if (count == 0U || end < count || end > 64U ||
                word1 % 16U != 0U ||
                word1 / 16U > interpreter->vertex_count - count) {
                return false;
            }
            const uint32_t first_slot = end - count;
            for (uint32_t index = 0U; index < count; index++) {
                interpreter->slots[first_slot + index] =
                    (uint16_t)(word1 / 16U + index);
                interpreter->slot_loaded[first_slot + index] = true;
            }
            continue;
        }
        if (opcode == PB_G_DL_OTR_HASH) {
            const PBDisplayListResource *nested = find_display_list_hash(
                interpreter->display_lists,
                interpreter->display_list_count, extra);
            if (nested == NULL ||
                !interpret_display_list(interpreter, nested, depth + 1U)) {
                return false;
            }
            continue;
        }
        if (opcode == PB_G_TRI1 || opcode == PB_G_TRI2) {
            const uint32_t words[2] = { word0, word1 };
            const size_t count = opcode == PB_G_TRI2 ? 2U : 1U;
            for (size_t index = 0U; index < count; index++) {
                const uint32_t packed = words[index];
                if (!emit_triangle(
                        interpreter,
                        (uint8_t)(((packed >> 16U) & 0xFFU) / 2U),
                        (uint8_t)(((packed >> 8U) & 0xFFU) / 2U),
                        (uint8_t)((packed & 0xFFU) / 2U))) {
                    return false;
                }
            }
            continue;
        }
        if (opcode == PB_G_END_DL) {
            return true;
        }
        interpreter->scene->stats.unsupported_commands++;
        return false;
    }
    return false;
}

static int16_t find_scene_texture(const PBWorldScene *scene,
                                  const char *name) {
    for (uint16_t index = 0U; index < scene->texture_count; index++) {
        if (strcmp(scene->textures[index].name, name) == 0) {
            return (int16_t)index;
        }
    }
    return -1;
}

static bool decode_texture_entry(PBWorldScene *scene, PBArchive *archive,
                                 const PBO2REntry *texture_entry,
                                 const PBO2REntry *palette_entry,
                                 PBMemoryMonitor *memory,
                                 PBDecodedTexture *decoded) {
    uint8_t *texture_data = NULL;
    size_t texture_size = 0U;
    uint8_t *palette_data = NULL;
    size_t palette_size = 0U;
    PBTextureResourceView texture;
    PBTextureResourceView palette;
    memset(&palette, 0, sizeof(palette));
    bool success = false;
    if (texture_entry == NULL ||
        !extract_entry(scene, archive, texture_entry, PB_TEXTURE_MAX_BYTES,
                       memory, &texture_data, &texture_size) ||
        !pb_texture_resource_parse(texture_data, texture_size, &texture)) {
        goto finish;
    }
    if (texture.type == PB_RESOURCE_TEXTURE_CI4 ||
        texture.type == PB_RESOURCE_TEXTURE_CI8) {
        if (palette_entry == NULL ||
            !extract_entry(scene, archive, palette_entry,
                           PB_TEXTURE_MAX_BYTES, memory, &palette_data,
                           &palette_size) ||
            !pb_texture_resource_parse(palette_data, palette_size,
                                       &palette)) {
            goto finish;
        }
    }
    const PBTextureDecodeResult result = pb_texture_decode_rgba8(
        decoded, &texture,
        (texture.type == PB_RESOURCE_TEXTURE_CI4 ||
         texture.type == PB_RESOURCE_TEXTURE_CI8)
            ? &palette
            : NULL,
        memory);
    if (result == PB_TEXTURE_DECODE_OUT_OF_MEMORY) {
        scene->archive_result = PB_O2R_OUT_OF_MEMORY;
    }
    success = result == PB_TEXTURE_DECODE_OK;

finish:
    if (palette_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, palette_data, palette_size);
    }
    if (texture_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, texture_data, texture_size);
    }
    return success;
}

static bool load_map_textures(PBWorldScene *scene, PBArchive *archive,
                              const PBMapContract *contract,
                              PBShapeLeaf *leaves, size_t leaf_count,
                              PBO2REntry *entries, size_t entry_capacity,
                              PBMemoryMonitor *memory) {
    char prefix[PB_O2R_NAME_CAPACITY];
    const int prefix_length = snprintf(prefix, sizeof(prefix), "textures/%s/",
                                       contract->texture_archive);
    if (prefix_length <= 0 || (size_t)prefix_length >= sizeof(prefix)) {
        return false;
    }
    if (entries == NULL ||
        entry_capacity < PB_WORLD_TEXTURE_INDEX_CAPACITY) {
        scene->archive_result = PB_O2R_INVALID_ARGUMENT;
        return false;
    }
    size_t entry_count = 0U;
    scene->archive_result = pb_o2r_find_entries_with_prefix(
        archive, prefix, entries, entry_capacity, &entry_count,
        &scene->archive_stats);
    if (scene->archive_result != PB_O2R_OK) {
        return false;
    }

    for (size_t leaf_index = 0U; leaf_index < leaf_count; leaf_index++) {
        PBShapeLeaf *leaf = &leaves[leaf_index];
        if (leaf->texture_name[0] == '\0') {
            continue;
        }
        const int16_t existing =
            find_scene_texture(scene, leaf->texture_name);
        if (existing >= 0) {
            leaf->texture_index = existing;
            continue;
        }
        if (scene->texture_count >= PB_WORLD_MAX_TEXTURES) {
            scene->archive_result = PB_O2R_CAPACITY_EXCEEDED;
            return false;
        }
        char resource_name[PB_O2R_NAME_CAPACITY];
        const int resource_length = snprintf(
            resource_name, sizeof(resource_name), "%s%s", prefix,
            leaf->texture_name);
        if (resource_length <= 0 ||
            (size_t)resource_length >= sizeof(resource_name)) {
            leaf->texture_name[0] = '\0';
            continue;
        }
        const PBO2REntry *texture_entry =
            find_indexed_entry(entries, entry_count, resource_name);
        if (texture_entry == NULL) {
            /* One retail shape property is not a printable texture name. */
            leaf->texture_name[0] = '\0';
            continue;
        }
        char palette_name[PB_O2R_NAME_CAPACITY];
        const int palette_length = snprintf(
            palette_name, sizeof(palette_name), "%s_tlut", resource_name);
        const PBO2REntry *palette_entry =
            palette_length > 0 && (size_t)palette_length < sizeof(palette_name)
                ? find_indexed_entry(entries, entry_count, palette_name)
                : NULL;
        PBWorldTexture *texture =
            &scene->textures[scene->texture_count];
        memset(texture, 0, sizeof(*texture));
        if (!decode_texture_entry(scene, archive, texture_entry,
                                  palette_entry, memory,
                                  &texture->decoded)) {
            return false;
        }
        memcpy(texture->name, leaf->texture_name,
               sizeof(texture->name));
        texture->wrap_s = 0U;
        texture->wrap_t = 0U;
        leaf->texture_index = (int16_t)scene->texture_count;
        scene->texture_count++;
    }
    scene->stats.textures = scene->texture_count;
    return scene->texture_count > 0U;
}

static PBWorldVec3 subtract_vec3(PBWorldVec3 left, PBWorldVec3 right) {
    PBWorldVec3 result = {
        left.x - right.x,
        left.y - right.y,
        left.z - right.z,
    };
    return result;
}

static PBWorldVec3 cross_vec3(PBWorldVec3 left, PBWorldVec3 right) {
    PBWorldVec3 result = {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
    return result;
}

static PBWorldVec3 normalize_vec3(PBWorldVec3 value) {
    const float length = sqrtf(value.x * value.x + value.y * value.y +
                               value.z * value.z);
    if (length > 0.0001f) {
        value.x /= length;
        value.y /= length;
        value.z /= length;
    } else {
        value.x = 0.0f;
        value.y = 0.0f;
        value.z = 0.0f;
    }
    return value;
}

static bool copy_collider_name(char *destination, size_t capacity,
                               const PBBlobView *shape_blob,
                               uint32_t names_offset, uint16_t index) {
    if (!span_is_valid(shape_blob->size, names_offset,
                       ((size_t)index + 1U) * sizeof(uint32_t))) {
        return false;
    }
    const uint32_t string_offset = read_u32(
        &shape_blob->data[(size_t)names_offset + (size_t)index * 4U], false);
    return copy_shape_string(destination, capacity, shape_blob->data,
                             shape_blob->size, string_offset);
}

static bool load_collision(PBWorldScene *scene,
                           const PBBlobView *collision_blob,
                           const PBBlobView *shape_blob,
                           PBMemoryMonitor *memory) {
    if (collision_blob == NULL || shape_blob == NULL ||
        collision_blob->size < 8U || shape_blob->size < 16U) {
        return false;
    }
    const uint32_t section_offset =
        read_u32(&collision_blob->data[0], false);
    const uint32_t names_offset = read_u32(&shape_blob->data[12], false);
    if (!span_is_valid(collision_blob->size, section_offset, 24U)) {
        return false;
    }
    const uint8_t *header = &collision_blob->data[section_offset];
    const uint16_t collider_count = read_u16(&header[0], false);
    const uint32_t collider_offset = read_u32(&header[4], false);
    const uint16_t vertex_count = read_u16(&header[8], false);
    const uint32_t vertex_offset = read_u32(&header[12], false);
    const uint16_t bounds_words = read_u16(&header[16], false);
    const uint32_t bounds_offset = read_u32(&header[20], false);
    if (collider_count == 0U || collider_count > PB_WORLD_MAX_COLLIDERS ||
        vertex_count == 0U ||
        vertex_count > PB_WORLD_MAX_COLLISION_VERTICES ||
        !span_is_valid(collision_blob->size, collider_offset,
                       (size_t)collider_count * 12U) ||
        !span_is_valid(collision_blob->size, vertex_offset,
                       (size_t)vertex_count * 6U) ||
        !span_is_valid(collision_blob->size, bounds_offset,
                       (size_t)bounds_words * 4U)) {
        return false;
    }

    uint32_t triangle_count = 0U;
    for (uint16_t index = 0U; index < collider_count; index++) {
        const uint8_t *asset =
            &collision_blob->data[(size_t)collider_offset +
                                  (size_t)index * 12U];
        const uint16_t count = read_u16(&asset[6], false);
        const uint32_t triangles = read_u32(&asset[8], false);
        if (count > PB_WORLD_MAX_COLLISION_TRIANGLES - triangle_count ||
            (count > 0U &&
             !span_is_valid(collision_blob->size, triangles,
                            (size_t)count * 4U))) {
            return false;
        }
        triangle_count += count;
    }

    scene->collision_vertices_allocation =
        (size_t)vertex_count * sizeof(*scene->collision_vertices);
    scene->collision_triangles_allocation =
        (size_t)triangle_count * sizeof(*scene->collision_triangles);
    scene->colliders_allocation =
        (size_t)collider_count * sizeof(*scene->colliders);
    scene->collision_vertices = pb_memory_alloc(
        memory, PB_MEMORY_SCENE, scene->collision_vertices_allocation);
    scene->collision_triangles = pb_memory_alloc(
        memory, PB_MEMORY_SCENE, scene->collision_triangles_allocation);
    scene->colliders = pb_memory_alloc(memory, PB_MEMORY_SCENE,
                                       scene->colliders_allocation);
    if (scene->collision_vertices == NULL ||
        scene->collision_triangles == NULL || scene->colliders == NULL) {
        return false;
    }
    memset(scene->colliders, 0, scene->colliders_allocation);
    for (uint16_t index = 0U; index < vertex_count; index++) {
        const uint8_t *vertex =
            &collision_blob->data[(size_t)vertex_offset +
                                  (size_t)index * 6U];
        scene->collision_vertices[index].x =
            (float)read_s16(&vertex[0], false);
        scene->collision_vertices[index].y =
            (float)read_s16(&vertex[2], false);
        scene->collision_vertices[index].z =
            (float)read_s16(&vertex[4], false);
    }

    uint32_t output_triangle = 0U;
    for (uint16_t index = 0U; index < collider_count; index++) {
        const uint8_t *asset =
            &collision_blob->data[(size_t)collider_offset +
                                  (size_t)index * 12U];
        PBWorldCollider *collider = &scene->colliders[index];
        const int16_t bounds_word = read_s16(&asset[0], false);
        collider->next_sibling = read_s16(&asset[2], false);
        collider->first_child = read_s16(&asset[4], false);
        collider->triangle_count = read_u16(&asset[6], false);
        collider->first_triangle = output_triangle;
        (void)copy_collider_name(collider->name, sizeof(collider->name),
                                 shape_blob, names_offset, index);
        if (bounds_word >= 0 && bounds_words >= 7U &&
            (uint32_t)bounds_word <= bounds_words - 7U) {
            const uint8_t *bounds =
                &collision_blob->data[(size_t)bounds_offset +
                                      (size_t)bounds_word * 4U];
            collider->minimum.x = read_f32(&bounds[0], false) - 1.0f;
            collider->minimum.y = read_f32(&bounds[4], false) - 1.0f;
            collider->minimum.z = read_f32(&bounds[8], false) - 1.0f;
            collider->maximum.x = read_f32(&bounds[12], false) + 1.0f;
            collider->maximum.y = read_f32(&bounds[16], false) + 1.0f;
            collider->maximum.z = read_f32(&bounds[20], false) + 1.0f;
            collider->flags = read_u32(&bounds[24], false);
            collider->has_bounds = true;
        }
        const uint32_t packed_offset = read_u32(&asset[8], false);
        for (uint16_t triangle_index = 0U;
             triangle_index < collider->triangle_count; triangle_index++) {
            const uint32_t packed = read_u32(
                &collision_blob->data[(size_t)packed_offset +
                                      (size_t)triangle_index * 4U],
                false);
            PBWorldCollisionTriangle *triangle =
                &scene->collision_triangles[output_triangle++];
            triangle->vertex[0] = (uint16_t)(packed & 0x3FFU);
            triangle->vertex[1] =
                (uint16_t)((packed >> 10U) & 0x3FFU);
            triangle->vertex[2] =
                (uint16_t)((packed >> 20U) & 0x3FFU);
            triangle->collider = index;
            triangle->one_sided = ((packed >> 30U) & 1U) != 0U;
            if (triangle->vertex[0] >= vertex_count ||
                triangle->vertex[1] >= vertex_count ||
                triangle->vertex[2] >= vertex_count) {
                return false;
            }
            const PBWorldVec3 first =
                scene->collision_vertices[triangle->vertex[0]];
            const PBWorldVec3 second =
                scene->collision_vertices[triangle->vertex[1]];
            const PBWorldVec3 third =
                scene->collision_vertices[triangle->vertex[2]];
            /* Matches PaperBoat collision.c: (v3-v1) x (v1-v2). */
            triangle->normal = normalize_vec3(cross_vec3(
                subtract_vec3(third, first),
                subtract_vec3(first, second)));
        }
    }
    scene->collider_count = collider_count;
    scene->stats.colliders = collider_count;
    scene->stats.collision_vertices = vertex_count;
    scene->stats.collision_triangles = triangle_count;
    return true;
}

static int16_t find_collider(const PBWorldScene *scene, const char *name) {
    if (name == NULL) {
        return -1;
    }
    for (uint16_t index = 0U; index < scene->collider_count; index++) {
        if (strcmp(scene->colliders[index].name, name) == 0) {
            return (int16_t)index;
        }
    }
    return -1;
}

static bool point_in_triangle_xz(float x, float z, PBWorldVec3 first,
                                 PBWorldVec3 second, PBWorldVec3 third,
                                 float *height) {
    const float denominator =
        (second.z - third.z) * (first.x - third.x) +
        (third.x - second.x) * (first.z - third.z);
    if (fabsf(denominator) < 0.0001f) {
        return false;
    }
    const float a = ((second.z - third.z) * (x - third.x) +
                     (third.x - second.x) * (z - third.z)) /
                    denominator;
    const float b = ((third.z - first.z) * (x - third.x) +
                     (first.x - third.x) * (z - third.z)) /
                    denominator;
    const float c = 1.0f - a - b;
    if (a < -0.001f || b < -0.001f || c < -0.001f) {
        return false;
    }
    *height = a * first.y + b * second.y + c * third.y;
    return true;
}

static bool sample_floor(PBWorldScene *scene, float x, float start_y,
                         float z, float *height, int16_t *collider_id) {
    bool found = false;
    float best = start_y - PB_WORLD_MAX_FLOOR_RAY;
    int16_t best_collider = -1;
    for (uint32_t index = 0U;
         index < scene->stats.collision_triangles; index++) {
        const PBWorldCollisionTriangle *triangle =
            &scene->collision_triangles[index];
        if (fabsf(triangle->normal.y) < 0.35f) {
            continue;
        }
        const PBWorldVec3 first =
            scene->collision_vertices[triangle->vertex[0]];
        const PBWorldVec3 second =
            scene->collision_vertices[triangle->vertex[1]];
        const PBWorldVec3 third =
            scene->collision_vertices[triangle->vertex[2]];
        float candidate = 0.0f;
        if (point_in_triangle_xz(x, z, first, second, third, &candidate) &&
            candidate <= start_y + PB_WORLD_MAX_STEP) {
            const bool exit_tie =
                found && fabsf(candidate - best) <= 0.01f &&
                (int16_t)triangle->collider == scene->exit_collider;
            if (candidate > best || exit_tie) {
                best = candidate;
                best_collider = (int16_t)triangle->collider;
                found = true;
            }
        }
    }
    scene->floor_samples++;
    if (found) {
        *height = best;
        *collider_id = best_collider;
    }
    return found;
}

static bool segment_triangle_hit(PBWorldVec3 origin, PBWorldVec3 direction,
                                 float maximum,
                                 const PBWorldVec3 *first,
                                 const PBWorldVec3 *second,
                                 const PBWorldVec3 *third, float *distance) {
    const PBWorldVec3 edge1 = subtract_vec3(*second, *first);
    const PBWorldVec3 edge2 = subtract_vec3(*third, *first);
    const PBWorldVec3 p = cross_vec3(direction, edge2);
    const float determinant =
        edge1.x * p.x + edge1.y * p.y + edge1.z * p.z;
    if (fabsf(determinant) < 0.0001f) {
        return false;
    }
    const float inverse = 1.0f / determinant;
    const PBWorldVec3 t = subtract_vec3(origin, *first);
    const float u = (t.x * p.x + t.y * p.y + t.z * p.z) * inverse;
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    const PBWorldVec3 q = cross_vec3(t, edge1);
    const float v = (direction.x * q.x + direction.y * q.y +
                     direction.z * q.z) * inverse;
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    const float hit =
        (edge2.x * q.x + edge2.y * q.y + edge2.z * q.z) * inverse;
    if (hit < 0.0f || hit > maximum) {
        return false;
    }
    *distance = hit;
    return true;
}

static bool find_wall(PBWorldScene *scene, float move_x, float move_z,
                      PBWorldVec3 *normal) {
    const float distance = sqrtf(move_x * move_x + move_z * move_z);
    if (distance < 0.001f) {
        return false;
    }
    PBWorldVec3 direction = { move_x / distance, 0.0f, move_z / distance };
    PBWorldVec3 origin = scene->player_position;
    origin.y += PB_WORLD_PLAYER_HEIGHT;
    float nearest = distance + PB_WORLD_PLAYER_RADIUS;
    bool found = false;
    for (uint32_t index = 0U;
         index < scene->stats.collision_triangles; index++) {
        const PBWorldCollisionTriangle *triangle =
            &scene->collision_triangles[index];
        if (fabsf(triangle->normal.y) > 0.72f) {
            continue;
        }
        const PBWorldVec3 first =
            scene->collision_vertices[triangle->vertex[0]];
        const PBWorldVec3 second =
            scene->collision_vertices[triangle->vertex[1]];
        const PBWorldVec3 third =
            scene->collision_vertices[triangle->vertex[2]];
        float hit = 0.0f;
        if (segment_triangle_hit(origin, direction, nearest, &first, &second,
                                 &third, &hit) && hit < nearest) {
            nearest = hit;
            *normal = triangle->normal;
            found = true;
        }
    }
    return found;
}

static float distance_to_bounds_xz(const PBWorldCollider *collider,
                                   PBWorldVec3 position) {
    float dx = 0.0f;
    float dz = 0.0f;
    if (position.x < collider->minimum.x) {
        dx = collider->minimum.x - position.x;
    } else if (position.x > collider->maximum.x) {
        dx = position.x - collider->maximum.x;
    }
    if (position.z < collider->minimum.z) {
        dz = collider->minimum.z - position.z;
    } else if (position.z > collider->maximum.z) {
        dz = position.z - collider->maximum.z;
    }
    return sqrtf(dx * dx + dz * dz);
}

static void release_owned_scene(PBWorldScene *scene,
                                PBMemoryMonitor *memory) {
    if (scene == NULL || memory == NULL) {
        return;
    }
    for (uint16_t index = 0U; index < scene->texture_count; index++) {
        pb_decoded_texture_release(&scene->textures[index].decoded, memory);
    }
    pb_decoded_texture_release(&scene->background, memory);
    for (size_t index = 0U; index < PB_WORLD_PLAYER_FRAME_COUNT; index++) {
        pb_decoded_texture_release(&scene->player_frames[index], memory);
    }
    pb_decoded_texture_release(&scene->star_piece, memory);
    if (scene->triangles != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, scene->triangles,
                       scene->triangles_allocation);
        scene->triangles = NULL;
    }
    if (scene->collision_vertices != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, scene->collision_vertices,
                       scene->collision_vertices_allocation);
        scene->collision_vertices = NULL;
    }
    if (scene->collision_triangles != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, scene->collision_triangles,
                       scene->collision_triangles_allocation);
        scene->collision_triangles = NULL;
    }
    if (scene->colliders != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, scene->colliders,
                       scene->colliders_allocation);
        scene->colliders = NULL;
    }
}

void pb_world_scene_init(PBWorldScene *scene) {
    if (scene != NULL) {
        memset(scene, 0, sizeof(*scene));
        scene->result = PB_WORLD_SCENE_NOT_ATTEMPTED;
        scene->archive_result = PB_O2R_OK;
        scene->current_floor = -1;
        scene->sign_collider = -1;
        scene->exit_collider = -1;
    }
}

PBWorldSceneResult pb_world_scene_load(PBWorldScene *scene,
                                       PBArchive *archive,
                                       const char *map_id,
                                       uint8_t entry_id,
                                       PBMemoryMonitor *memory) {
    if (scene == NULL || archive == NULL || archive->file == NULL ||
        map_id == NULL || memory == NULL) {
        return PB_WORLD_SCENE_INVALID_ARGUMENT;
    }
    pb_world_scene_init(scene);
    const PBMapContract *contract = find_map_contract(map_id);
    const PBEntryContract *entry = find_entry_contract(map_id, entry_id);
    if (contract == NULL || entry == NULL) {
        scene->result = PB_WORLD_SCENE_UNSUPPORTED_MAP;
        return scene->result;
    }
    memcpy(scene->map_id, map_id, strlen(map_id) + 1U);
    scene->entry_id = entry_id;

    char shape_name[PB_O2R_NAME_CAPACITY];
    char collision_name[PB_O2R_NAME_CAPACITY];
    char vertex_name[PB_O2R_NAME_CAPACITY];
    char display_list_prefix[PB_O2R_NAME_CAPACITY];
    if (snprintf(shape_name, sizeof(shape_name), "shapes/%s_shape", map_id) <=
            0 ||
        snprintf(collision_name, sizeof(collision_name), "collisions/%s_hit",
                 map_id) <= 0 ||
        snprintf(vertex_name, sizeof(vertex_name), "shapes/%s_shape/vtx",
                 map_id) <= 0 ||
        snprintf(display_list_prefix, sizeof(display_list_prefix),
                 "shapes/%s_shape/dlist_", map_id) <= 0) {
        scene->result = PB_WORLD_SCENE_INVALID_ARGUMENT;
        return scene->result;
    }

    uint8_t *shape_data = NULL;
    size_t shape_size = 0U;
    uint8_t *collision_data = NULL;
    size_t collision_size = 0U;
    uint8_t *vertex_data = NULL;
    size_t vertex_size = 0U;
    size_t display_list_count = 0U;
    PBWorldSceneResult failure = PB_WORLD_SCENE_ARCHIVE_ERROR;
    PBWorldLoadScratch *scratch =
        pb_memory_alloc(memory, PB_MEMORY_TRANSIENT, sizeof(*scratch));
    if (scratch == NULL) {
        scene->result = PB_WORLD_SCENE_OUT_OF_MEMORY;
        return scene->result;
    }
    memset(scratch, 0, sizeof(*scratch));
    PBO2REntry *display_list_entries = scratch->display_list_entries;
    PBDisplayListResource *display_lists = scratch->display_lists;
    PBShapeLeaf *leaves = scratch->leaves;

    PBO2RRequest core_requests[3] = {
        { .name = shape_name },
        { .name = collision_name },
        { .name = vertex_name },
    };
    scene->archive_result = pb_o2r_find_entries(
        archive, core_requests, 3U, &scene->archive_stats);
    if (scene->archive_result != PB_O2R_OK) {
        failure = map_archive_failure(scene->archive_result);
        goto fail;
    }

    if (!extract_entry(scene, archive, &core_requests[0].entry,
                       PB_SHAPE_MAX_BYTES, memory, &shape_data,
                       &shape_size)) {
        failure = map_archive_failure(scene->archive_result);
        goto fail;
    }
    PBBlobView shape_blob;
    if (!parse_blob(shape_data, shape_size, &shape_blob)) {
        failure = PB_WORLD_SCENE_SHAPE_INVALID;
        goto fail;
    }
    size_t leaf_count = 0U;
    if (!collect_shape_leaves(&shape_blob, leaves, scratch->visited,
                              PB_WORLD_MAX_SHAPE_NODES, &leaf_count,
                              &scene->stats.shape_nodes)) {
        failure = PB_WORLD_SCENE_SHAPE_INVALID;
        goto fail;
    }
    scene->stats.leaf_models = (uint32_t)leaf_count;

    if (!extract_entry(scene, archive, &core_requests[2].entry,
                       PB_VERTEX_MAX_BYTES, memory, &vertex_data,
                       &vertex_size)) {
        failure = map_archive_failure(scene->archive_result);
        goto fail;
    }
    PBResourceView vertices;
    uint32_t vertex_count = 0U;
    if (!parse_vertex_resource(vertex_data, vertex_size, &vertices,
                               &vertex_count)) {
        failure = PB_WORLD_SCENE_SHAPE_INVALID;
        goto fail;
    }
    scene->stats.source_vertices = vertex_count;

    scene->archive_result = pb_o2r_find_entries_with_prefix(
        archive, display_list_prefix, display_list_entries,
        PB_WORLD_MAX_DISPLAY_LISTS, &display_list_count,
        &scene->archive_stats);
    if (scene->archive_result != PB_O2R_OK) {
        failure = map_archive_failure(scene->archive_result);
        goto fail;
    }
    for (size_t index = 0U; index < display_list_count; index++) {
        uint8_t *data = NULL;
        size_t size = 0U;
        if (!extract_entry(scene, archive, &display_list_entries[index],
                           PB_DISPLAY_LIST_MAX_BYTES, memory, &data, &size) ||
            !parse_display_list_resource(data, size,
                                         &display_lists[index])) {
            if (data != NULL) {
                pb_memory_free(memory, PB_MEMORY_SCENE, data, size);
            }
            failure = scene->archive_result == PB_O2R_OK
                          ? PB_WORLD_SCENE_DISPLAY_LIST_INVALID
                          : map_archive_failure(scene->archive_result);
            goto fail;
        }
    }
    scene->stats.display_lists = (uint32_t)display_list_count;

    if (!load_map_textures(
            scene, archive, contract, leaves, leaf_count,
            scratch->texture_entries, PB_WORLD_TEXTURE_INDEX_CAPACITY,
            memory)) {
        failure = scene->archive_result == PB_O2R_OUT_OF_MEMORY
                      ? PB_WORLD_SCENE_OUT_OF_MEMORY
                      : (scene->archive_result == PB_O2R_CAPACITY_EXCEEDED
                             ? PB_WORLD_SCENE_CAPACITY
                             : PB_WORLD_SCENE_TEXTURE_INVALID);
        goto fail;
    }

    scene->triangles_allocation =
        PB_WORLD_MAX_TRIANGLES * sizeof(*scene->triangles);
    scene->triangles = pb_memory_alloc(memory, PB_MEMORY_SCENE,
                                       scene->triangles_allocation);
    if (scene->triangles == NULL) {
        failure = PB_WORLD_SCENE_OUT_OF_MEMORY;
        goto fail;
    }
    memset(scene->triangles, 0, scene->triangles_allocation);

    for (size_t leaf_index = 0U; leaf_index < leaf_count; leaf_index++) {
        char name[PB_O2R_NAME_CAPACITY];
        const int length = snprintf(name, sizeof(name), "%s%lX",
                                    display_list_prefix,
                                    (unsigned long)
                                        leaves[leaf_index].display_list_offset);
        const PBDisplayListResource *root =
            length > 0 && (size_t)length < sizeof(name)
                ? find_display_list_name(display_lists,
                                         display_list_entries,
                                         display_list_count, name)
                : NULL;
        if (root == NULL) {
            failure = PB_WORLD_SCENE_DISPLAY_LIST_INVALID;
            goto fail;
        }
        PBDisplayListInterpreter interpreter = {
            .scene = scene,
            .vertices = &vertices,
            .vertex_count = vertex_count,
            .display_lists = display_lists,
            .display_list_count = display_list_count,
            .leaf = &leaves[leaf_index],
        };
        if (!interpret_display_list(&interpreter, root, 0U)) {
            failure = scene->stats.triangles >= PB_WORLD_MAX_TRIANGLES
                          ? PB_WORLD_SCENE_CAPACITY
                          : PB_WORLD_SCENE_DISPLAY_LIST_INVALID;
            goto fail;
        }
    }
    if (scene->stats.triangles == 0U ||
        scene->stats.unsupported_commands != 0U) {
        failure = PB_WORLD_SCENE_DISPLAY_LIST_INVALID;
        goto fail;
    }

    if (!extract_entry(scene, archive, &core_requests[1].entry,
                       PB_COLLISION_MAX_BYTES, memory, &collision_data,
                       &collision_size)) {
        failure = map_archive_failure(scene->archive_result);
        goto fail;
    }
    PBBlobView collision_blob;
    if (!parse_blob(collision_data, collision_size, &collision_blob) ||
        !load_collision(scene, &collision_blob, &shape_blob, memory)) {
        failure = scene->collision_vertices == NULL ||
                          scene->collision_triangles == NULL ||
                          scene->colliders == NULL
                      ? PB_WORLD_SCENE_OUT_OF_MEMORY
                      : PB_WORLD_SCENE_COLLISION_INVALID;
        goto fail;
    }

    PBO2RRequest actor_requests[7] = {
        { .name = "backgrounds/nok_bg" },
        { .name = "backgrounds/nok_bg_pal0" },
        { .name = "sprites/player_sprite_0_raster_0" },
        { .name = "sprites/player_sprite_0_raster_2" },
        { .name = "sprites/player_sprite_0_pal_0" },
        { .name = "icons/anim/star_piece_0" },
        { .name = "icons/anim/star_piece_0.pal" },
    };
    const size_t actor_request_count = contract->has_star_piece ? 7U : 5U;
    scene->archive_result = pb_o2r_find_entries(
        archive, actor_requests, actor_request_count, &scene->archive_stats);
    if (scene->archive_result != PB_O2R_OK ||
        !decode_texture_entry(scene, archive, &actor_requests[0].entry,
                              &actor_requests[1].entry, memory,
                              &scene->background) ||
        !decode_texture_entry(scene, archive, &actor_requests[2].entry,
                              &actor_requests[4].entry, memory,
                              &scene->player_frames[0]) ||
        !decode_texture_entry(scene, archive, &actor_requests[3].entry,
                              &actor_requests[4].entry, memory,
                              &scene->player_frames[1]) ||
        (contract->has_star_piece &&
         !decode_texture_entry(scene, archive, &actor_requests[5].entry,
                               &actor_requests[6].entry, memory,
                               &scene->star_piece))) {
        failure = scene->archive_result == PB_O2R_OUT_OF_MEMORY
                      ? PB_WORLD_SCENE_OUT_OF_MEMORY
                      : (scene->archive_result == PB_O2R_ENTRY_NOT_FOUND
                             ? PB_WORLD_SCENE_RESOURCE_MISSING
                             : PB_WORLD_SCENE_TEXTURE_INVALID);
        goto fail;
    }

    scene->player_position = entry->position;
    scene->camera_target = entry->position;
    scene->camera_target.y += 35.0f;
    scene->player_yaw = entry->yaw;
    scene->transition_cooldown = PB_WORLD_EXIT_COOLDOWN;
    scene->entry_walk_frames =
        (strcmp(map_id, "mac_00") == 0 && entry_id == 1U) ||
                (strcmp(map_id, "mac_01") == 0 && entry_id == 0U)
            ? PB_WORLD_ENTRY_WALK_FRAMES
            : 0U;
    scene->transition_state = PB_WORLD_TRANSITION_FADE_IN;
    scene->transition_frame = 0U;
    scene->star_piece_active = contract->has_star_piece;
    scene->exit_collider = find_collider(scene, contract->exit_collider);
    scene->sign_collider = find_collider(scene, contract->sign_collider);
    memcpy(scene->exit_map, contract->exit_map,
           strlen(contract->exit_map) + 1U);
    scene->exit_entry = contract->exit_entry;
    float floor_height = entry->position.y;
    int16_t floor = -1;
    if (sample_floor(scene, entry->position.x,
                     entry->position.y + PB_WORLD_MAX_STEP,
                     entry->position.z, &floor_height, &floor)) {
        scene->player_position.y = floor_height;
        scene->current_floor = floor;
    }
    if (scene->exit_collider < 0 ||
        (contract->sign_collider != NULL && scene->sign_collider < 0)) {
        failure = PB_WORLD_SCENE_COLLISION_INVALID;
        goto fail;
    }

    scene->result = PB_WORLD_SCENE_READY;
    release_display_lists(display_lists, display_list_count, memory);
    pb_memory_free(memory, PB_MEMORY_SCENE, collision_data, collision_size);
    pb_memory_free(memory, PB_MEMORY_SCENE, vertex_data, vertex_size);
    pb_memory_free(memory, PB_MEMORY_SCENE, shape_data, shape_size);
    pb_memory_free(memory, PB_MEMORY_TRANSIENT, scratch, sizeof(*scratch));
    return scene->result;

fail:
    release_display_lists(display_lists, display_list_count, memory);
    if (collision_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, collision_data,
                       collision_size);
    }
    if (vertex_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, vertex_data, vertex_size);
    }
    if (shape_data != NULL) {
        pb_memory_free(memory, PB_MEMORY_SCENE, shape_data, shape_size);
    }
    release_owned_scene(scene, memory);
    pb_memory_free(memory, PB_MEMORY_TRANSIENT, scratch, sizeof(*scratch));
    scene->result = failure;
    return scene->result;
}

static PBWorldSceneEvent update_transition(PBWorldScene *scene) {
    if (scene->transition_state == PB_WORLD_TRANSITION_FADE_IN) {
        if (scene->transition_frame < PB_WORLD_FADE_FRAMES) {
            scene->transition_frame++;
        }
        if (scene->transition_frame >= PB_WORLD_FADE_FRAMES) {
            scene->transition_state = PB_WORLD_TRANSITION_NONE;
            scene->transition_frame = 0U;
        }
        return PB_WORLD_SCENE_EVENT_NONE;
    }
    if (scene->transition_state == PB_WORLD_TRANSITION_FADE_OUT) {
        if (scene->transition_frame < PB_WORLD_FADE_FRAMES) {
            scene->transition_frame++;
        }
        if (scene->transition_frame >= PB_WORLD_FADE_FRAMES) {
            scene->transition_state = PB_WORLD_TRANSITION_WAITING;
            memcpy(scene->requested_map, scene->exit_map,
                   sizeof(scene->requested_map));
            scene->requested_entry = scene->exit_entry;
            return PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED;
        }
    }
    return PB_WORLD_SCENE_EVENT_NONE;
}

PBWorldSceneEvent pb_world_scene_update(PBWorldScene *scene,
                                        const PBInputState *input) {
    if (scene == NULL || input == NULL ||
        scene->result != PB_WORLD_SCENE_READY) {
        return PB_WORLD_SCENE_EVENT_NONE;
    }
    scene->frames++;
    if (scene->transition_cooldown > 0U) {
        scene->transition_cooldown--;
    }
    const PBWorldSceneEvent transition = update_transition(scene);
    if (transition != PB_WORLD_SCENE_EVENT_NONE ||
        scene->transition_state == PB_WORLD_TRANSITION_FADE_OUT ||
        scene->transition_state == PB_WORLD_TRANSITION_WAITING) {
        return transition;
    }

    if (scene->message_timer != 0U) {
        if ((input->n64_pressed & (PB_N64_A | PB_N64_B)) != 0U) {
            scene->message_timer = 0U;
            scene->script_events++;
            return PB_WORLD_SCENE_EVENT_SIGN_CLOSED;
        }
        return PB_WORLD_SCENE_EVENT_NONE;
    }

    if (scene->sign_collider >= 0 &&
        (input->n64_pressed & PB_N64_A) != 0U) {
        const PBWorldCollider *sign =
            &scene->colliders[scene->sign_collider];
        if (sign->has_bounds &&
            distance_to_bounds_xz(sign, scene->player_position) <= 48.0f) {
            scene->message_timer = 1U;
            scene->script_events++;
            return PB_WORLD_SCENE_EVENT_SIGN_OPENED;
        }
    }

    int16_t movement_x = input->stick_x;
    int16_t movement_y = input->stick_y;
    if (movement_x == 0) {
        const bool left = (input->native_held & KEY_DLEFT) != 0U;
        const bool right = (input->native_held & KEY_DRIGHT) != 0U;
        if (left != right) {
            movement_x = left ? -80 : 80;
        }
    }
    if (movement_y == 0) {
        const bool up = (input->native_held & KEY_DUP) != 0U;
        const bool down = (input->native_held & KEY_DDOWN) != 0U;
        if (up != down) {
            movement_y = up ? 80 : -80;
        }
    }
    float move_x = (float)movement_x / 80.0f * 4.0f;
    float move_z = -(float)movement_y / 80.0f * 4.0f;
    if (scene->entry_walk_frames > 0U) {
        const float yaw = scene->player_yaw * (PB_WORLD_PI / 180.0f);
        move_x = sinf(yaw) * 3.0f;
        move_z = cosf(yaw) * 3.0f;
        scene->entry_walk_frames--;
    }
    scene->player_speed = sqrtf(move_x * move_x + move_z * move_z);
    if (scene->player_speed > 0.05f) {
        PBWorldVec3 wall_normal;
        if (find_wall(scene, move_x, move_z, &wall_normal)) {
            const float horizontal_length = sqrtf(
                wall_normal.x * wall_normal.x + wall_normal.z * wall_normal.z);
            if (horizontal_length > 0.001f) {
                wall_normal.x /= horizontal_length;
                wall_normal.z /= horizontal_length;
                const float into =
                    move_x * wall_normal.x + move_z * wall_normal.z;
                move_x -= into * wall_normal.x;
                move_z -= into * wall_normal.z;
            }
            scene->collision_blocks++;
        }
        const float candidate_x = scene->player_position.x + move_x;
        const float candidate_z = scene->player_position.z + move_z;
        float floor_height = scene->player_position.y;
        int16_t floor = scene->current_floor;
        if (sample_floor(scene, candidate_x,
                         scene->player_position.y + PB_WORLD_MAX_STEP,
                         candidate_z, &floor_height, &floor) &&
            fabsf(floor_height - scene->player_position.y) <=
                PB_WORLD_MAX_STEP) {
            scene->player_position.x = candidate_x;
            scene->player_position.y = floor_height;
            scene->player_position.z = candidate_z;
            scene->current_floor = floor;
            scene->movement_frames++;
            scene->player_frame =
                (uint8_t)((scene->movement_frames / 8U) %
                          PB_WORLD_PLAYER_FRAME_COUNT);
            scene->player_facing_left = move_x < -0.05f;
            scene->player_yaw = atan2f(move_x, move_z) *
                                (180.0f / PB_WORLD_PI);
        } else {
            scene->collision_blocks++;
        }
    } else {
        scene->player_frame = 0U;
    }

    scene->camera_target.x +=
        (scene->player_position.x - scene->camera_target.x) * 0.14f;
    scene->camera_target.y +=
        (scene->player_position.y + 35.0f - scene->camera_target.y) * 0.14f;
    scene->camera_target.z +=
        (scene->player_position.z - scene->camera_target.z) * 0.14f;

    if (scene->star_piece_active) {
        const float dx = scene->player_position.x + 420.0f;
        const float dz = scene->player_position.z - 410.0f;
        if (dx * dx + dz * dz <= 32.0f * 32.0f &&
            fabsf(scene->player_position.y - 20.0f) <= 70.0f) {
            scene->star_piece_active = false;
            scene->star_piece_collected = true;
            scene->script_events++;
            return PB_WORLD_SCENE_EVENT_STAR_PIECE_COLLECTED;
        }
    }

    if (scene->transition_cooldown == 0U && scene->exit_collider >= 0 &&
        scene->current_floor == scene->exit_collider) {
        scene->transition_state = PB_WORLD_TRANSITION_FADE_OUT;
        scene->transition_frame = 0U;
        scene->transition_count++;
        scene->script_events++;
        return PB_WORLD_SCENE_EVENT_TRANSITION_STARTED;
    }
    return PB_WORLD_SCENE_EVENT_NONE;
}

float pb_world_scene_fade_alpha(const PBWorldScene *scene) {
    if (scene == NULL) {
        return 0.0f;
    }
    if (scene->transition_state == PB_WORLD_TRANSITION_FADE_IN) {
        return 1.0f - (float)scene->transition_frame /
                          (float)PB_WORLD_FADE_FRAMES;
    }
    if (scene->transition_state == PB_WORLD_TRANSITION_FADE_OUT) {
        return (float)scene->transition_frame /
               (float)PB_WORLD_FADE_FRAMES;
    }
    return scene->transition_state == PB_WORLD_TRANSITION_WAITING ? 1.0f
                                                                  : 0.0f;
}

bool pb_world_scene_message_visible(const PBWorldScene *scene) {
    return scene != NULL && scene->message_timer != 0U;
}

void pb_world_scene_release_pixels(PBWorldScene *scene,
                                   PBMemoryMonitor *memory) {
    if (scene == NULL || memory == NULL || scene->pixels_released) {
        return;
    }
    for (uint16_t index = 0U; index < scene->texture_count; index++) {
        pb_decoded_texture_release(&scene->textures[index].decoded, memory);
    }
    pb_decoded_texture_release(&scene->background, memory);
    for (size_t index = 0U; index < PB_WORLD_PLAYER_FRAME_COUNT; index++) {
        pb_decoded_texture_release(&scene->player_frames[index], memory);
    }
    pb_decoded_texture_release(&scene->star_piece, memory);
    scene->pixels_released = true;
}

void pb_world_scene_release(PBWorldScene *scene, PBMemoryMonitor *memory) {
    if (scene == NULL || memory == NULL) {
        return;
    }
    release_owned_scene(scene, memory);
    pb_world_scene_init(scene);
}

const char *pb_world_scene_result_name(PBWorldSceneResult result) {
    switch (result) {
        case PB_WORLD_SCENE_NOT_ATTEMPTED:
            return "not attempted";
        case PB_WORLD_SCENE_READY:
            return "overworld ready";
        case PB_WORLD_SCENE_INVALID_ARGUMENT:
            return "invalid argument";
        case PB_WORLD_SCENE_UNSUPPORTED_MAP:
            return "unsupported map";
        case PB_WORLD_SCENE_RESOURCE_MISSING:
            return "resource missing";
        case PB_WORLD_SCENE_ARCHIVE_ERROR:
            return "archive rejected";
        case PB_WORLD_SCENE_SHAPE_INVALID:
            return "shape invalid";
        case PB_WORLD_SCENE_DISPLAY_LIST_INVALID:
            return "display list invalid";
        case PB_WORLD_SCENE_TEXTURE_INVALID:
            return "texture invalid";
        case PB_WORLD_SCENE_COLLISION_INVALID:
            return "collision invalid";
        case PB_WORLD_SCENE_OUT_OF_MEMORY:
            return "memory budget";
        case PB_WORLD_SCENE_CAPACITY:
            return "scene capacity";
        default:
            return "unknown";
    }
}

const char *pb_world_scene_event_name(PBWorldSceneEvent event) {
    switch (event) {
        case PB_WORLD_SCENE_EVENT_NONE:
            return "idle";
        case PB_WORLD_SCENE_EVENT_SIGN_OPENED:
            return "sign opened";
        case PB_WORLD_SCENE_EVENT_SIGN_CLOSED:
            return "sign closed";
        case PB_WORLD_SCENE_EVENT_STAR_PIECE_COLLECTED:
            return "star piece collected";
        case PB_WORLD_SCENE_EVENT_TRANSITION_STARTED:
            return "transition started";
        case PB_WORLD_SCENE_EVENT_TRANSITION_REQUESTED:
            return "map requested";
        default:
            return "unknown";
    }
}
