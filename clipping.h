#ifndef LITTLEENGINE_CLIPPING_H
#define LITTLEENGINE_CLIPPING_H

#include "math/math_types.h"

typedef struct {
    Vector3 position; /* Camera space, before projection. */
    Point2 uv;        /* Original texture coordinates. */
} ClipVertex;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Clip a camera-space triangle against z >= near_z.
 * @param input Three finite vertices with original UV coordinates.
 * @param near_z Positive near-plane distance.
 * @param output Space for four vertices; must not overlap input.
 * @return Output polygon vertex count: 0, 3, or 4.
 * @note Preserves polygon order. Plane-touching cases can produce
 * degenerate triangles, which the rasterizer must reject.
 */
int clip_triangle_near(const ClipVertex input[3], float near_z, ClipVertex output[4]);

#ifdef __cplusplus
}
#endif

#endif