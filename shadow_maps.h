#ifndef LITTLEENGINE_SHADOW_MAPS_H
#define LITTLEENGINE_SHADOW_MAPS_H
#include "render_types.h"
#include "lighting_types.h"
#include <stddef.h>
#include <stdbool.h>
// alternative to ray casting lighting system; i think ultimately a better deal
typedef struct ShadowMaps ShadowMaps;


#ifdef __cplusplus
extern "C" {
#endif


/** Allocate renderer-owned storage for capacity*6 square float depth faces.
 * capacity is 1..SCENE_LIGHT_CAPACITY; face_size>=4; near_clip is positive metres.
 * Returns NULL on invalid settings, overflow or allocation failure. Destroy after
 * all readers finish. No allocation occurs during build/query. */
ShadowMaps *shadow_maps_create(size_t capacity,int face_size,float near_clip);
void shadow_maps_destroy(ShadowMaps *maps); /* NULL accepted. */
/** Rebuild all enabled point-light maps synchronously using exclusive scratch.
 * Borrows immutable validated geometry and lighting for the duration of this call.
 * Must finish before any queries. Failure invalidates all maps, never publishing
 * partial results. Shadow-casting directional lights are unsupported and fail.
 * Opaque, two-sided casters; near clipping only; geometry inside the near plane
 * may be missed. No SDL, file I/O, logging or allocation. */
bool shadow_maps_build(ShadowMaps *maps,RenderScratch *scratch,
    const SceneObject *objects,size_t object_count,const SceneLighting *lighting);
/** Read-only hard-shadow query by the same light index supplied to build.
 * Requires finite point and unit surface normal; successful build must precede
 * concurrent queries. Returns false for disabled/absent maps, empty texels,
 * outside-radius points or points inside the near plane. The metre-valued bias
 * offsets the receiver plane and comparison. No allocation or mutation. */
bool shadow_maps_occluded(const ShadowMaps *maps,size_t light_index,
    Vector3 point,Vector3 normal);


    
#ifdef __cplusplus
}
#endif
#endif
