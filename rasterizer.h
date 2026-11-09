#ifndef LITTLEENGINE_RASTERIZER_H
#define LITTLEENGINE_RASTERIZER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "render_types.h"
#include "camera.h"
#include "depth_buffer.h"
#include "lighting.h"
#include "shadow_scene.h"

/** Borrowed output view; pixels contain XRGB8888 and pitch is bytes per row.
 * Color/depth storage must be separate, valid, and exclusively writable during drawing.
 * Depth dimensions match width/height. The descriptor owns no allocation.
 */
typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    int pitch;
    DepthBuffer *depth;
} RasterTarget;

#ifdef __cplusplus
extern "C" {
#endif

/** Clear and render one complete scene synchronously, using exclusive scratch.
 * Scene/camera/mesh/texture inputs remain valid and unchanged until return.
 * Returns false for invalid target metadata or rejected mesh; output can be partial.
 * Mesh arrays/indices and finite geometry must already have been validated by the caller.
 * Zero objects is allowed. No SDL calls, file I/O, logging, or allocation occur here.
 */
bool rasterizer_draw_scene(RasterTarget *target, RenderScratch *scratch, const SceneObject *objects, size_t object_count, const Camera *camera, const SceneLighting *lighting, const LightingLUT *lut, const ShadowScene *shadows);

/** wireframe debug tightly packed pixels only; no depth testing. */
bool draw_wireframe_mesh(uint32_t *pixels, int width, int height, WireframeMesh mesh, Vector3 rotation, uint32_t color, Vector3 position, const Camera *camera, RenderScratch *scratch);

#ifdef __cplusplus
}
#endif

#endif
