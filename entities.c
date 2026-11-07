#include "entities.h"

void entity_tick(Entity *entity, double dt)
{
    float step = (float)dt;

    entity->rotation.x += entity->rotation_rate.x * step;
    entity->rotation.y += entity->rotation_rate.y * step;
    entity->rotation.z += entity->rotation_rate.z * step;
}

EntityState entity_get_state(const Entity *entity)
{
    return (EntityState){
        .position = entity->position,
        .rotation = entity->rotation
    };
}