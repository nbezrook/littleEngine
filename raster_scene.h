#ifndef LITTLEENGINE_RASTER_SCENE_H
#define LITTLEENGINE_RASTER_SCENE_H
#include "rasterizer.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct RasterScene RasterScene;
// split up work to delegate, and decide which path to use. SIMD should be available for most machines.
typedef enum { RASTER_SCALAR,RASTER_SIMD } RasterKernel;

// keep track of raster scene
typedef struct {
    size_t prepared_triangles,tile_references;
    uint64_t shaded_pixels;
} RasterSceneStats;


#ifdef __cplusplus
extern "C" {
#endif
/** Allocate a fixed-resolution prepared frame, visibility IDs, bins and scratch.
 * Dimensions are 1..16384; capacities are positive, triangles < UINT32_MAX.
 * Returns NULL on invalid settings/overflow/allocation failure. No allocation
 * occurs in preparation or tile jobs. Destroy only after all readers finish. */
RasterScene *raster_scene_create(int width,int height,size_t triangle_capacity,size_t reference_capacity);

void raster_scene_destroy(RasterScene *scene);
/** Owner-thread preparation. Validate/transform/clip once, prepare attribute
 * planes and tile bins. Rejects unrepresentable attribute coefficients.
 * Failure invalidates the frame; never publishes partial
 * bins. Geometry is borrowed during this call. Texture pixels, LUT and maps
 * remain immutable and alive until tile jobs finish. Maps must correspond to the
 * same scene/light snapshot. Lighting/camera are copied.
 * Opaque, camera-front-facing surfaces; no SDL, I/O or allocation. */
bool raster_scene_prepare(RasterScene *scene,const SceneObject *objects,size_t count, const Camera *camera,const SceneLighting *lighting,const LightingLUT *lut,const ShadowMaps *maps);

size_t raster_scene_job_count(const RasterScene *scene);

bool raster_scene_simd_available(void);
/** Execute exactly once for each tile index [0,job_count) before presentation.
 * Concurrent jobs require distinct indices and one compatible target. Each job
 * clears/owns its colour, depth and winner-ID region, resolves visibility, then
 * shades each winning pixel once. Exact equal depths keep the first prepared
 * primitive. SIMD falls back to scalar on builds without SSE2. No allocation.
 * Underflowing reciprocal depths are skipped. Unrepresentable texture samples
 * use the object's solid colour, avoiding invalid float-to-index conversions.
 * Target storage must not alias scene, textures, inputs or other frame resources. */
bool raster_scene_render_tile(RasterScene *scene,const RasterTarget *target,size_t tile,RasterKernel kernel);
/** Owner thread only after every tile finishes; no concurrent preparation/jobs. */
bool raster_scene_stats(const RasterScene *scene,RasterSceneStats *out);
#ifdef __cplusplus
}
#endif
#endif
