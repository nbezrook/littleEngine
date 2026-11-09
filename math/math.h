#ifndef LITTLEENGINE_MATH_H
#define LITTLEENGINE_MATH_H
#include "math_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

float clamp_float(float value, float low, float high);

Vector3 vec3_subtract(Vector3 a, Vector3 b);
float vec3_dot(Vector3 a, Vector3 b);
Vector3 vec3_cross(Vector3 a, Vector3 b);
/** Returns false for NULL out or zero/nonfinite squared length, including
 * arithmetic underflow/overflow; leaves *out unchanged on failure.
 * Intended for ordinary engine-scale vectors. No allocation.
 */
bool vec3_normalize(Vector3 value, Vector3 *out);
/** Compute the winding-dependent unit normal; same failure contract. */
bool triangle_unit_normal(Vector3 a, Vector3 b, Vector3 c, Vector3 *out);


Vector3 vec3_rotate_x(Vector3 value, float radians);
Vector3 vec3_rotate_y(Vector3 value, float radians);
Vector3 vec3_rotate_z(Vector3 value, float radians);


/**
 * @brief Return the smaller of two finite floats using a simple comparison.
 * @param a Finite operand.
 * @param b Finite operand.
 * @return a when a < b, otherwise b; not a substitute for fminf's NaN/signed-zero contract.
 */
float min_finite(float a, float b);
/**
 * @brief Return the larger of two finite floats using a simple comparison.
 * @param a Finite operand.
 * @param b Finite operand.
 * @return a when a > b, otherwise b; not a substitute for fmaxf's NaN/signed-zero contract.
 */
float max_finite(float a, float b);


/**
 * @brief Compute integer magnitude using an arithmetic-right-shift bit trick.
 * @param n Input other than INT_MIN; intermediate signed arithmetic must remain representable.
 * @return Nonnegative magnitude on the assumed two's-complement, arithmetic-shift target.
 * @warning This implementation is not a fully portable replacement for abs.
 */
int cAbs(int n);

/**
 * @brief Clear the sign bit of a 32-bit IEEE-754 float representation.
 * @param f Input float.
 * @return Input representation with its sign bit cleared.
 * @warning Assumes the target's float layout matches uint32_t; this is not format-independent.
 */
float cFabs(float f);

#ifdef __cplusplus
}
#endif

#endif