#ifndef LITTLEENGINE_SERVER_H
#define LITTLEENGINE_SERVER_H

#include <stdint.h>
#include "world.h"
#include "math/math.h"


typedef struct {
    World world;
    uint64_t tick_number;
    PlayerCommand pending_command; // command to request the Server to honor
} Server;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Add an entity through the trusted host API.
 * @param server Non-null, zero-initialized or valid server.
 * @param entity Initial state, copied by value.
 * @return False if the world is full.
 * @note Setup operation; not an untrusted client command.
 */
bool server_add_entity(Server *server, Entity entity);

/**
 * @brief Advance the world and increment the simulation tick.
 * @param server Non-null valid server.
 * @param dt Nonnegative simulation step in seconds.
 */
void server_tick(Server *server, double dt);

/**
 * @brief Copy public entity state for presentation.
 * @param server Non-null valid server.
 * @param index Append-only slot index.
 * @param out Non-null separate destination.
 * @return False for an invalid index, leaving out unchanged.
 * @note Read between ticks on the owning thread.
 */
bool server_get_entity_state(const Server *server, size_t index, EntityState *out);

/**
 * @brief Publish a detached copy of all public entity states.
 * @param server Non-null valid server, read between simulation ticks.
 * @param out Non-null separate destination; unchanged on failure.
 * @return False if capacity is insufficient or a state read fails.
 */
bool server_build_snapshot(const Server *server, ServerSnapshot *out);

/**
 * @brief Build the room in the server-owned World.
 * @param server Non-null initialized or zero-initialized server.
 * @return False on construction failure; previous map is unchanged.
 * @note Startup operation; does not reset entities or publish map data.
 */
bool server_load_test_map(Server *server);
/**
 * @brief Spawn player in Server
 */
void server_spawn_player(Server *server, Vector3 position);
/**
 * @brief Request Server to honor PlayerCommand
 */
bool server_submit_player_command(Server *server, const PlayerCommand *command);
/**
 * @brief Copies map from Server construction.
 * @param server Non-null initialized or zero-initialized server.
 * @param out MapData returned
 * @return False on inability to access or allocate.
 */
bool server_copy_map(const Server *server, MapData *out);

#ifdef __cplusplus
}
#endif

#endif