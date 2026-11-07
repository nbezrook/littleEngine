#include "server.h"

bool server_add_entity(Server *server, Entity entity)
{
    return world_add_entity(&server->world, entity);
}

void server_tick(Server *server, double dt)
{
    world_tick(&server->world, dt);
    ++server->tick_number;
}

bool server_get_entity_state(const Server *server, size_t index, EntityState *out)
{
    return world_get_entity_state(&server->world, index, out);
}

bool server_build_snapshot(const Server *server, ServerSnapshot *out)
{
    if (server->world.entity_count > SNAPSHOT_ENTITY_CAPACITY)
        return false;

    ServerSnapshot snapshot = {
        .tick_number = server->tick_number,
        .entity_count = server->world.entity_count
    };

    for (size_t i = 0; i < snapshot.entity_count; ++i) {
        if (!world_get_entity_state(
                &server->world, i, &snapshot.entities[i])) {
            return false;
        }
    }

    *out = snapshot;
    return true;
}