#include "server.h"
#include "map.h"
#include <math.h>


void server_spawn_player(Server *server, Vector3 position)
{
    server->world.player = (PlayerState){.position = position};
    server->pending_command = (PlayerCommand){0};
    server->world.player_motion = (PhysicsMotion){0};
}

bool server_submit_player_command(Server *server, const PlayerCommand *command)
{
    if (!command || !isfinite(command->forward) || !isfinite(command->right) || !isfinite(command->yaw) || !isfinite(command->pitch))
        return false;
    PlayerCommand accepted = *command;
    accepted.forward = clamp_float(accepted.forward, -1.0f, 1.0f);
    accepted.right = clamp_float(accepted.right, -1.0f, 1.0f);
    accepted.yaw = remainderf(accepted.yaw, 6.28318530718f);
    //limit pitch by world rules
    float limit = server->world.player_rules.max_pitch_radians;
    accepted.pitch = clamp_float(accepted.pitch, -limit, limit);

    server->pending_command = accepted;
    return true;
}


bool server_add_entity(Server *server, Entity entity)
{
    return world_add_entity(&server->world, entity);
}

void server_tick(Server *server, double dt)
{
    world_tick(&server->world, &server->pending_command, dt);
    server->pending_command.jump = false;
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
        .player = server->world.player,
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

bool server_load_test_map(Server *server)
{
    MapData map;
    if (!map_make_test_room(&map) || !map.has_room_bounds) return false;

    const PlayerRules *rules = &server->world.player_rules;
    Vector3 lo = map.room_bounds.min;
    Vector3 hi = map.room_bounds.max;
    Vector3 spawn = map.player_spawn;
    float r = rules->half_width;
    float h = rules->height;
    if (!isfinite(r) || !isfinite(h) || r <= 0.0f || h <= 0.0f 
    || r > (hi.x - lo.x) * 0.5f || r > (hi.z - lo.z) * 0.5f 
    || h > hi.y - lo.y || spawn.x < lo.x + r || spawn.x > hi.x - r 
    || spawn.z < lo.z + r || spawn.z > hi.z - r || spawn.y < lo.y 
    || spawn.y > hi.y - h)
        return false;

    server->world.map = map;
    server_spawn_player(server, map.player_spawn);
    return true;
}

bool server_copy_map(const Server *server, MapData *out)
{
    if (!map_validate(&server->world.map))
        return false;

    *out = server->world.map;
    return true;
}

void server_init(Server *server, const PlayerRules *rules, float gravity)
{
    *server = (Server){0};
    server->world.player_rules = *rules;
    server->world.gravity = gravity;
}
