#ifndef LITTLEENGINE_WORLD_H
#define LITTLEENGINE_WORLD_H

#include <stdbool.h>
#include <stddef.h>
#include "entities.h"

#include "math/map_types.h"

enum { WORLD_ENTITY_CAPACITY = 128 };

typedef struct {
    MapData map;
    Entity entities[WORLD_ENTITY_CAPACITY];
    size_t entity_count;
    PlayerState player; // authoritative player; can be expanded
} World;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy an entity into the world's append-only storage.
 * @param world Non-null, zero-initialized or valid world.
 * @param entity Initial entity state, copied by value.
 * @return False if full, leaving the world unchanged.
 */
bool world_add_entity(World *world, Entity entity);

/**
 * @brief Advance all entities by one simulation step.
 * @param world Non-null valid world.
 * @param world PlayerCommand that interfaces with the world
 * @param dt Nonnegative step in seconds.
 */
void world_tick(World *world, const PlayerCommand *command, double dt);

/**
 * @brief Copy one entity's public state to the caller.
 * @param world Non-null valid world.
 * @param index Slot index, not a persistent entity identifier.
 * @param out Non-null separate destination; unchanged on failure.
 * @return False if index is out of range.
 * @note Call between updates on the owning thread.
 */
bool world_get_entity_state(const World *world, size_t index, EntityState *out
);

#ifdef __cplusplus
}
#endif

#endif