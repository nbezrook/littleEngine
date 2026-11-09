#include "framebuffer.h"
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

bool framebuffer_init(FrameBuffer *buffer, int width, int height)
{
    if (!buffer || width <= 0 || height <= 0 ||
        width > INT_MAX / (int)sizeof(uint32_t))
        return false;

    size_t w = (size_t)width;
    size_t h = (size_t)height;
    if (w > SIZE_MAX / h) return false;
    size_t count = w * h;
    if (count > SIZE_MAX / sizeof(uint32_t)) return false;

    FrameBuffer result = {0};
    result.pixels = malloc(count * sizeof(*result.pixels));
    if (!result.pixels) return false;
    if (!depth_buffer_init(&result.depth, width, height)) {
        free(result.pixels);
        return false;
    }
    result.width = width;
    result.height = height;
    result.pitch = width * (int)sizeof(*result.pixels);
    *buffer = result;
    return true;
}

void framebuffer_shutdown(FrameBuffer *buffer)
{
    if (!buffer) return;
    free(buffer->pixels);
    depth_buffer_shutdown(&buffer->depth);
    *buffer = (FrameBuffer){0};
}
