#ifndef MATH_C
#define MATH_C
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h> // for abs()
#include <math.h>
#include "math_types.h"

/**
 * Clamp passed float by low and high float parameters.
 */
float clamp_float(float value, float low, float high)
{
    return value < low ? low : (value > high ? high : value);
}

/**
 * Test a triangle in view space, where the camera is at the origin.
 * Assumes consistently wound, one-sided geometry.
 */
static bool triangle_is_front_facing(Vector3 a, Vector3 b, Vector3 c)
{
    //produces a normal determined by the triangle vertex order
    Vector3 ab = {b.x - a.x, b.y - a.y, b.z - a.z};
    Vector3 ac = {c.x - a.x, c.y - a.y, c.z - a.z};
    // direction from vertex a toward the camera is -a camera is at zero in view space
    Vector3 normal = {ab.y * ac.z - ab.z * ac.y,ab.z * ac.x - ab.x * ac.z,ab.x * ac.y - ab.y * ac.x};
    //front-facing normal has normal * (-a) > 0, equivalently normal * a < 0
    float facing = normal.x * a.x
                 + normal.y * a.y
                 + normal.z * a.z;

    return facing < 0.0f;
}

/**
 * @brief Compute integer magnitude using an arithmetic-right-shift bit trick.
 * @param n Input other than INT_MIN; intermediate signed arithmetic must remain representable.
 * @return Nonnegative magnitude on the assumed two's-complement, arithmetic-shift target.
 * @warning This implementation is not a fully portable replacement for abs.
 */
int cAbs(int n) {
    int const mask = n >> (sizeof(int) * 8 - 1);
    return (n + mask) ^ mask;
}

/**
 * @brief Clear the sign bit of a 32-bit IEEE-754 float representation.
 * @param f Input float.
 * @return Input representation with its sign bit cleared.
 * @warning Assumes the target's float layout matches uint32_t; this is not format-independent.
 */
float cFabs(float f) {
    union {
        float f; uint32_t i; } u = {.f = f};
    u.i &= 0x7FFFFFFF; // Clear the sign bit
    return u.f;
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

/**
 * @brief Return the signed 2D cross product (b-a) x (p-a).
 * @param a Directed edge start.
 * @param b Directed edge end.
 * @param p Point to classify.
 * @return Signed doubled triangle area, used for coverage and barycentric weights.
 */
static float edge(Point2 a, Point2 b, Point2 p) {
    // signed 2d cross product
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}

/**
 * @brief Identify inclusive edges for the rasterizer's screen-space top-left fill rule.
 * @param a Directed edge start.
 * @param b Directed edge end.
 * @return True for upward edges or horizontal edges directed right.
 * @note Assumes screen Y increases downward and interior edge values are positive.
 */
static bool inclusive_edge(Point2 a, Point2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y; 
    return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
}

/**
 * @brief Return the smaller of two finite floats using a simple comparison.
 * @param a Finite operand.
 * @param b Finite operand.
 * @return a when a < b, otherwise b; not a substitute for fminf's NaN/signed-zero contract.
 */
static inline float min_finite(float a, float b) {
    return a < b ? a : b;
}

/**
 * @brief Return the larger of two finite floats using a simple comparison.
 * @param a Finite operand.
 * @param b Finite operand.
 * @return a when a > b, otherwise b; not a substitute for fmaxf's NaN/signed-zero contract.
 */
static inline float max_finite(float a, float b) {
    return a > b ? a : b;
}

#endif // MATH_C