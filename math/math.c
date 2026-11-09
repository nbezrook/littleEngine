#ifndef MATH_C
#define MATH_C
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // for abs()
#include "math.h" 
#include <math.h>  // std c lib

/**
 * Clamp passed float by low and high float parameters.
 */
float clamp_float(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}


/**
 * @brief Rotate a vector about Y using x' = x*cos(a) - z*sin(a).
 * @param v Input vector, returned by value without modifying its source.
 * @param angle_rad Rotation angle in radians.
 * @return Rotated vector; Y is unchanged. This sign convention differs from positive camera yaw.
 */
Vector3 vec3_rotate_y(Vector3 v, float angle_rad) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    return (Vector3){
        .x = v.x * cos_a - v.z * sin_a,
        .y = v.y,
        .z = v.x * sin_a + v.z * cos_a
    };
}

/**
 * @brief Rotate a vector about X using y' = y*cos(a) - z*sin(a).
 * @param v Input vector.
 * @param angle_rad Rotation angle in radians.
 * @return Rotated vector; X is unchanged.
 */
Vector3 vec3_rotate_x(Vector3 v, float angle_rad) {
    float cos_a = cosf(angle_rad);
    float sin_a = sinf(angle_rad);

    return (Vector3){
        .x = v.x,
        .y = v.y * cos_a - v.z * sin_a,
        .z = v.y * sin_a + v.z * cos_a
    };
}

/**
 * @brief Rotate a vector about Z using x' = x*cos(a) - y*sin(a).
 * @param v Input vector.
 * @param angle_rad Rotation angle in radians.
 * @return Rotated vector; Z is unchanged.
 */
Vector3 vec3_rotate_z(Vector3 v, float angle_rad) {
    float c = cosf(angle_rad);
    float s = sinf(angle_rad);

    return (Vector3){
        .x = v.x * c - v.y * s,
        .y = v.x * s + v.y * c,
        .z = v.z
    };
}

float cFabs(float f) {
    union {
        float f; uint32_t i; } u = {.f = f};
    u.i &= 0x7FFFFFFF; // Clear the sign bit
    return u.f;
}

float min_finite(float a, float b) {
    return a < b ? a : b;
}

float max_finite(float a, float b) {
    return a > b ? a : b;
}

int cAbs(int n) {
    int const mask = n >> (sizeof(int) * 8 - 1);
    return (n + mask) ^ mask;
}



Vector3 vec3_subtract(Vector3 a, Vector3 b)
{
    return (Vector3){a.x - b.x, a.y - b.y, a.z - b.z};
}

float vec3_dot(Vector3 a, Vector3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vector3 vec3_cross(Vector3 a, Vector3 b)
{
    return (Vector3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

bool vec3_normalize(Vector3 value, Vector3 *out)
{
    if (!out) return false;
    float length_squared = vec3_dot(value, value);
    if (!isfinite(length_squared) || length_squared <= 0.0f)
        return false;

    float inverse_length = 1.0f / sqrtf(length_squared);
    Vector3 result = {
        value.x * inverse_length,
        value.y * inverse_length,
        value.z * inverse_length
    };
    *out = result;
    return true;
}

bool triangle_unit_normal(Vector3 a, Vector3 b, Vector3 c, Vector3 *out)
{
    Vector3 ab = vec3_subtract(b, a);
    Vector3 ac = vec3_subtract(c, a);
    return vec3_normalize(vec3_cross(ab, ac), out);
}




#endif // MATH_C