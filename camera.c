#include "camera.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

bool camera_project(const Camera *camera, Vector3 world, int width, int height, ProjectedVertex *out)
{
    Vector3 view = {
        // project camera onto world by position
        world.x - camera->position.x,
        world.y - camera->position.y,
        world.z - camera->position.z
    };
    // setting up camera projection for depth testing
    if (view.z < camera->near_clip)
        return false;

    float q = 1.0f / view.z;

    out->screen.x = width * 0.5f
        + camera->focal_length_pixels * view.x * q;

    out->screen.y = height * 0.5f
        - camera->focal_length_pixels * view.y * q;

    out->inverse_depth = q;
    return true;
}

bool depth_buffer_init(DepthBuffer *buffer, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    size_t w = (size_t)width;
    size_t h = (size_t)height;

    if (w > SIZE_MAX / h) return false;

    size_t count = w * h;
    
    if (count > SIZE_MAX / sizeof(*buffer->values)) return false;

    float *values = malloc(count * sizeof(*values));

    if (!values) return false;

    buffer->width = width;
    buffer->height = height;
    buffer->values = values;

    return true;
}

void depth_buffer_clear(DepthBuffer *buffer) {
    size_t count =(size_t)buffer->width*(size_t)buffer->height;
    for (size_t i = 0; i < count; ++i)
        buffer->values[i] = 0.0f;
}
void depth_buffer_shutdown(DepthBuffer *buffer) {
    if (buffer != NULL) {
        free(buffer->values);
        *buffer = (DepthBuffer){0};
    }
}