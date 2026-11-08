#ifndef PRIMITIVES_C
#define PRIMITIVES_C
#include "math_types.h"
#include "primitives.h"

// PRIMITIVES
// UNIT CUBE
// AI GENERATED, THANKS
Vector3 cube_vertices[8] = {
    {-1.0f, -1.0f, -1.0f}, // 0: Bottom-Left-Front
    { 1.0f, -1.0f, -1.0f}, // 1: Bottom-Right-Front
    { 1.0f,  1.0f, -1.0f}, // 2: Top-Right-Front
    {-1.0f,  1.0f, -1.0f}, // 3: Top-Left-Front
    {-1.0f, -1.0f,  1.0f}, // 4: Bottom-Left-Back
    { 1.0f, -1.0f,  1.0f}, // 5: Bottom-Right-Back
    { 1.0f,  1.0f,  1.0f}, // 6: Top-Right-Back
    {-1.0f,  1.0f,  1.0f}  // 7: Top-Left-Back
};

Vector3 cube_points[] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f}, { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
};

Edge cube_edges[] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Front face
    {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Back face
    {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting links
};

// PRIMITIVE TRIANGLE FACES FOR UNIT CUBE
static Triangle cube_triangles[] = {
    // Z-negative
    {0,2,1, {0,1},{1,0},{1,1}},
    {0,3,2, {0,1},{0,0},{1,0}},

    // Z-positive
    {4,5,6, {0,1},{1,1},{1,0}},
    {4,6,7, {0,1},{1,0},{0,0}},

    // X-negative
    {0,4,7, {0,1},{1,1},{1,0}},
    {0,7,3, {0,1},{1,0},{0,0}},

    // X-positive
    {1,2,6, {0,1},{0,0},{1,0}},
    {1,6,5, {0,1},{1,0},{1,1}},

    // Y-negative
    {0,1,5, {0,1},{1,1},{1,0}},
    {0,5,4, {0,1},{1,0},{0,0}},

    // Y-positive
    {3,7,6, {0,1},{0,0},{1,0}},
    {3,6,2, {0,1},{1,0},{1,1}}
};

WireframeMesh cube_mesh = {
    .vertices = cube_points,
    .vertex_count = 8,

    .edges = cube_edges,
    .edge_count = 12,

    .triangles = cube_triangles,
    .triangle_count = sizeof(cube_triangles) / sizeof(cube_triangles[0])
};

// ROOM VERTICES TO CONNECT WITH TRIANGLES
static Vector3 room_vertices[] = {
    {-6, -2, -8}, {6, -2, -8}, {6, 3, -8}, {-6, 3, -8},
    {-6, -2,  8}, {6, -2,  8}, {6, 3,  8}, {-6, 3,  8}
};

static Triangle room_triangles[] = {
    /* Z-negative wall. */
    {0,1,2, {0,1},{1,1},{1,0}},
    {0,2,3, {0,1},{1,0},{0,0}},
    /* Z-positive wall. */
    {4,6,5, {0,1},{1,0},{1,1}},
    {4,7,6, {0,1},{0,0},{1,0}},
    /* X-negative wall. */
    {0,7,4, {0,1},{1,0},{1,1}},
    {0,3,7, {0,1},{0,0},{1,0}},
    /* X-positive wall. */
    {1,6,2, {0,1},{1,0},{0,0}},
    {1,5,6, {0,1},{1,1},{1,0}},
    /* Floor. */
    {0,5,1, {0,1},{1,0},{1,1}},
    {0,4,5, {0,1},{0,0},{1,0}},
    /* Ceiling. */
    {3,6,7, {0,1},{1,0},{0,0}},
    {3,2,6, {0,1},{1,1},{1,0}}
};

WireframeMesh room_mesh = {
    .vertices = room_vertices,
    .vertex_count = 8,
    .edges = cube_edges,
    .edge_count = 12,
    .triangles = room_triangles,
    .triangle_count = sizeof(room_triangles) / sizeof(room_triangles[0])
};

#endif