#ifndef LITTLEENGINE_MAP_TYPES_H
#define LITTLEENGINE_MAP_TYPES_H

#include <stddef.h>
#include "math_types.h"

enum {
    MAP_VERTEX_CAPACITY = 128,
    MAP_TRIANGLE_CAPACITY = 256,
    MAP_SURFACE_CAPACITY = 16,
    MAP_TEXTURE_NAME_BYTES = 64
};

/**
 * @brief Consecutive triangles sharing one texture reference.
 */
typedef struct {
    size_t first_triangle;
    size_t triangle_count;
    char texture_name[MAP_TEXTURE_NAME_BYTES];
} MapSurface;

/**
 * @brief Self-contained map geometry and surface definitions.
 * @note Counts describe occupied array entries.
 * Native runtime data, not a serialized file/network format.
 */
typedef struct {
    Vector3 vertices[MAP_VERTEX_CAPACITY];
    Triangle triangles[MAP_TRIANGLE_CAPACITY];
    MapSurface surfaces[MAP_SURFACE_CAPACITY];

    size_t vertex_count;
    size_t triangle_count;
    size_t surface_count;
} MapData;

#endif