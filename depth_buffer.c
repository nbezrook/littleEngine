#include "depth_buffer.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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