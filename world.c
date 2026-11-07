#include "world.h"

bool world_add_entity(World *world, Entity entity)
{
    if (world->entity_count >= WORLD_ENTITY_CAPACITY)
        return false;

    world->entities[world->entity_count++] = entity;
    return true;
}

void world_tick(World *world, double dt)
{
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