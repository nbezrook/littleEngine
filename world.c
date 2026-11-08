#include "world.h"
#include <math.h>

bool world_add_entity(World *world, Entity entity)
{
    if (world->entity_count >= WORLD_ENTITY_CAPACITY)
        return false;

    world->entities[world->entity_count++] = entity;
    return true;
}

void world_tick(World *world, const PlayerCommand *command, double dt)
{
    PlayerState *player = &world->player;
    player->yaw = command->yaw;
    player->pitch = command->pitch;

    float forward = command->forward;
    float right = command->right;
    float length_squared = forward * forward + right * right;
    if (length_squared > 1.0f) {
        float inverse_length = 1.0f / sqrtf(length_squared);
        forward *= inverse_length;
        right *= inverse_length;
    }

    const float speed = 3.0f; // simulation rule
    float distance = speed * (float)dt;
    float sy = sinf(player->yaw);
    float cy = cosf(player->yaw);
    player->position.x += (forward * sy + right * cy) * distance;
    player->position.z += (forward * cy - right * sy) * distance;

    for (size_t i = 0; i < world->entity_count; ++i)
        entity_tick(&world->entities[i], dt);
}

bool world_get_entity_state(const World *world, size_t index, EntityState *out)
{
    if (index >= world->entity_count)
        return false;

    *out = entity_get_state(&world->entities[index]);
    return true;
}
