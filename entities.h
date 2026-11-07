#ifndef LITTLEENGINE_ENTITIES_H
#define LITTLEENGINE_ENTITIES_H

#include "math/math_types.h"
#include "game_protocol.h"

typedef struct {
    Vector3 position;       /* World units. */
    Vector3 rotation;       /* Radians. */
    Vector3 rotation_rate;  /* Radians per second. */
} Entity;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Advance an entity's rotation without touching rendering resources.
 * @param entity Non-null simulation entity.
 * @param dt Nonnegative simulation step in seconds.
 */
void entity_tick(Entity *entity, double dt);

/**
 * @brief Copy the entity's public state for presentation.
 * @param entity Non-null simulation entity.
 * @return Position and rotation by value.
 * @note Read between simulation updates on the owning thread.
 */
EntityState entity_get_state(const Entity *entity);

#ifdef __cplusplus
}
#endif

#endif