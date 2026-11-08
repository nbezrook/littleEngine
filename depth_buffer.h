#ifndef LITTLEENGINE_DEPTH_BUFFER_H
#define LITTLEENGINE_DEPTH_BUFFER_H

#include <stdbool.h>

typedef struct {
    float *values;
    int width;
    int height;
} DepthBuffer;

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate reciprocal-depth storage. Destination must have no live allocation.
 * Invalid sizes/allocation failure leave it unchanged. Clear before drawing.
 */
bool depth_buffer_init(DepthBuffer *buffer, int width, int height);

/* Reset to zero. Larger positive reciprocal depth represents nearer pixels. */
void depth_buffer_clear(DepthBuffer *buffer);

/* Free values and reset the struct; accepts NULL. */
void depth_buffer_shutdown(DepthBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
