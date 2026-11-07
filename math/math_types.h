#ifndef LITTLEENGINE_MATHTYPES_H
#define LITTLEENGINE_MATHTYPES_H
#include <stdint.h>

typedef struct {
    float x;
    float y;
    float z;
} Vector3;

typedef struct {
    float x;
    float y;
} Point2;

typedef struct {
    int start_index;
    int end_index;
} Edge;

typedef struct {
    int a;
    int b;
    int c;

    Point2 uv_a;
    Point2 uv_b;
    Point2 uv_c;
} Triangle;

typedef struct {
    Vector3* vertices;
    int vertex_count;
    Edge* edges;
    int edge_count;
    Triangle *triangles;
    int triangle_count;
} WireframeMesh;

typedef struct {
    Point2 screen;
    float inverse_depth;
} ProjectedVertex;

#endif