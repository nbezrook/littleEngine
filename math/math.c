#ifndef MATH_C
#define MATH_C
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // for abs()
#include <math.h>
#include "math_types.h"

int cAbs(int n) {
    int const mask = n >> (sizeof(int) * 8 - 1);
    return (n + mask) ^ mask;
}

float cFabs(float f) {
    union {
        float f; uint32_t i; } u = {.f = f};
    u.i &= 0x7FFFFFFF; // Clear the sign bit
    return u.f;
}

Vector3 vec3_rotate_y(Vector3 v, float angle_rad) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    return (Vector3){
        .x = v.x * cos_a - v.z * sin_a,
        .y = v.y,
        .z = v.x * sin_a + v.z * cos_a
    };
}

Vector3 vec3_rotate_x(Vector3 v, float angle_rad) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    return (Vector3){
        .x = v.x,
        .y = v.y * cos_a - v.z * sin_a,
        .z = v.y * sin_a + v.z * cos_a
    };
}

Vector3 vec3_rotate_z(Vector3 v, float angle_rad) {
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);

    return (Vector3){
        .x = v.x * c - v.y * s,
        .y = v.x * s + v.y * c,
        .z = v.z
    };
}

static float edge(Point2 a, Point2 b, Point2 p) {
    // signed 2d cross product
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}

static bool inclusive_edge(Point2 a, Point2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y; 
    return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
}

static inline float min_finite(float a, float b) {
    return a < b ? a : b;
}

static inline float max_finite(float a, float b) {
    return a > b ? a : b;
}

#endif // MATH_C