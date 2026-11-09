#ifndef LITTLEENGINE_SHADOW_SCENE_H
#define LITTLEENGINE_SHADOW_SCENE_H
#include "render_types.h"
#include "raycast.h"
#include <stddef.h>

typedef struct ShadowScene ShadowScene;
enum { SHADOW_TRIANGLE_CAPACITY = 4096 }; // start up budget, this might explode out of control in more complicated scene, use more dynamic management
#ifdef __cplusplus
extern "C" {
#endif

/** Allocates a cache with fixed capacity. No allocation occurs in build/query. */
ShadowScene *shadow_scene_create(size_t capacity);
void shadow_scene_destroy(ShadowScene *scene);
/** Transform every opaque object triangle, including off-screen/back faces.
 * Build before rendering, with exclusive access. Failure empties the cache.
 * Mesh pointers/indices and finite geometry are validated. */
bool shadow_scene_build(ShadowScene *scene, const SceneObject *objects, size_t count);
size_t shadow_scene_triangle_count(const ShadowScene *scene);
/** Read-only finite segment query. Ignore the receiving primitive, identified
 * by its original object index and triangle index. Every other face is two-sided.
 * NULL scene is empty. Inputs must satisfy RaySegment's contract. */
bool shadow_scene_occluded(const ShadowScene *scene, const RaySegment *ray, size_t skip_object, size_t skip_triangle);
#ifdef __cplusplus
}
#endif
#endif
