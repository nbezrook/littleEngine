#ifndef LITTLEENGINE_PHYSICS_H
#define LITTLEENGINE_PHYSICS_H

#include <stdbool.h>
#include "math/math_types.h"
#include "math/map_types.h"

typedef struct {
    Vector3 velocity; // meters per sec
    bool grounded; // supported by floor surface
} PhysicsMotion;

#ifdef __cplusplus
extern "C" {
#endif

/* semi-implicit Euler integration <3 Finite state, finite nonnegative gravity and a positive fixed dt required. */
void physics_integrate(Vector3 *position, PhysicsMotion *motion,
    float gravity, double dt);

/* Feet-based box inside an enclosing room; caller validates dimensions fit. */
void physics_constrain_room(Vector3 *position, PhysicsMotion *motion,
    const RoomBounds *room, float half_width, float height);

#ifdef __cplusplus
}
#endif

#endif
