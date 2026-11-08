#include "physics.h"

void physics_integrate(Vector3 *position, PhysicsMotion *motion,
    float gravity, double dt)
{
    float seconds = (float)dt;
    motion->grounded = false;
    motion->velocity.y -= gravity * seconds;
    position->x += motion->velocity.x * seconds;
    position->y += motion->velocity.y * seconds;
    position->z += motion->velocity.z * seconds;
}

static void constrain_axis(float *position, float *velocity, float low, float high)
{
    if (*position <= low) {
        *position = low;
        if (*velocity < 0.0f) *velocity = 0.0f;
    }
    if (*position >= high) {
        *position = high;
        if (*velocity > 0.0f) *velocity = 0.0f;
    }
}

void physics_constrain_room(Vector3 *position, PhysicsMotion *motion,
    const RoomBounds *room, float half_width, float height)
{
    constrain_axis(&position->x, &motion->velocity.x,
        room->min.x + half_width, room->max.x - half_width);
    constrain_axis(&position->z, &motion->velocity.z,
        room->min.z + half_width, room->max.z - half_width);
    if (position->y <= room->min.y) {
        position->y = room->min.y;
        if (motion->velocity.y <= 0.0f) {
            motion->velocity.y = 0.0f;
            motion->grounded = true;
        }
    }
    float ceiling = room->max.y - height;
    if (position->y >= ceiling) {
        position->y = ceiling;
        if (motion->velocity.y > 0.0f) motion->velocity.y = 0.0f;
    }
}
