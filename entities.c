#ifndef ENTITIES_C
#define ENTITIES_C
#include "entities.h"
#include "math/primitives.h"

SceneObject object = {};

void scene_object_tick(SceneObject *object, double dt) {
    float step = (float)dt;

    object->rotation.x += object->rotation_rate.x * step;
    object->rotation.y += object->rotation_rate.y * step;
    object->rotation.z += object->rotation_rate.z * step;
}

#endif