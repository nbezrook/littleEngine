#ifndef LITTLEENGINE_RAYCAST_H
#define LITTLEENGINE_RAYCAST_H
#include "math/math_types.h"
#include <stdbool.h>


typedef struct {
    Vector3 origin;
    Vector3 direction;        // unit length 
    float min_distance;
    float max_distance;
} RaySegment;


#ifdef __cplusplus
extern "C" {
#endif
/** Two-sided triangle intersection. Requires finite vertices and a finite,
 * unit-direction ray with 0<=min_distance<=max_distance. No allocation.
 * out_distance may be NULL; otherwise unchanged on a miss. */
bool raycast_triangle(const RaySegment *ray, Vector3 a, Vector3 b, Vector3 c, float *out_distance);
#ifdef __cplusplus
}
#endif
#endif
