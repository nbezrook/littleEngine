#ifndef LITTLEENGINE_ENTITIES_H
#define LITTLEENGINE_ENTITIES_H

#include "math/math_types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

void scene_object_tick(SceneObject *object, double dt);

#ifdef __cplusplus
}
#endif

#endif