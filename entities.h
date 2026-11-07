#ifndef LITTLEENGINE_ENTITIES_H
#define LITTLEENGINE_ENTITIES_H

#include "math/math_types.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Integrate an object's Euler rotation using its angular velocity.
 * @param object Non-null object; mesh and texture ownership are unaffected.
 * @param dt Simulation step in seconds. rotation_rate is radians per second.
 * @note Updates rotation only; angles accumulate without wrapping.
 */
void scene_object_tick(SceneObject *object, double dt);

#ifdef __cplusplus
}
#endif

#endif