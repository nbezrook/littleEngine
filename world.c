#include "world.h"
#include <math.h>
#include "math/math.h"

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


    float sy = sinf(player->yaw);
    float cy = cosf(player->yaw);

    float speed = world->player_rules.walk_speed;

    world->player_motion.velocity.x = (forward * sy + right * cy) * speed;

    world->player_motion.velocity.z = (forward * cy - right * sy) * speed;

    // LEGAL JUMP?
    if (command->jump && world->player_motion.grounded) {
        world->player_motion.velocity.y =
            world->player_rules.jump_speed;

        world->player_motion.grounded = false;
    }

    // PLAYER PHYSICS
    physics_integrate(&player->position, &world->player_motion, world->gravity, dt);

    if (world->map.has_room_bounds) {
        physics_constrain_room(&player->position, &world->player_motion, &world->map.room_bounds, world->player_rules.half_width, world->player_rules.height);
    }


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