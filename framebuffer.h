#ifndef LITTLEENGINE_FRAMEBUFFER_H
#define LITTLEENGINE_FRAMEBUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include "depth_buffer.h"

/** Owns tightly packed XRGB8888 color pixels and matching reciprocal-depth storage.
 * RasterTarget descriptors borrow these arrays; they must finish before shutdown.
 */
typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int pitch; /* Bytes per color-buffer row. */
    DepthBuffer depth;
} FrameBuffer;

#ifdef __cplusplus
extern "C" {
#endif

/* Destination has no live allocations. Failure leaves it unchanged.
 * On success arrays are uninitialized; rasterizer_draw_scene clears them.
 */
bool framebuffer_init(FrameBuffer *buffer, int width, int height);

/* Free owned arrays and reset the structure; accepts NULL or zero state. */
void framebuffer_shutdown(FrameBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
