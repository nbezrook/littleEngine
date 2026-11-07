#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include "math/math_types.h"

typedef struct {
    Vector3 position;
    float focal_length_pixels;
    float near_clip;
} Camera;

typedef struct {
    float *values;
    int width;
    int height;
} DepthBuffer;

// dancing with a little C 
#ifdef __cplusplus
extern "C" {
#endif

bool camera_project(const Camera *camera, Vector3 world, int width, int height, ProjectedVertex *out);
bool depth_buffer_init(DepthBuffer *buffer, int width, int height);
void depth_buffer_clear(DepthBuffer *buffer);
void depth_buffer_shutdown(DepthBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
