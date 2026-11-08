#ifndef LITTLEENGINE_CLIENT_H
#define LITTLEENGINE_CLIENT_H

#include "game_protocol.h"
#include "renderer.h"
#include "math/map_types.h"
#include "inputThinker.h"



typedef struct {
    size_t object_count;
    uint64_t server_tick_number;

    MapData map;
    WireframeMesh map_meshes[MAP_SURFACE_CAPACITY];

    SceneObject objects[MAP_SURFACE_CAPACITY + SNAPSHOT_ENTITY_CAPACITY];

    size_t map_object_count;

    float max_pitch_radians;
    float eye_height;

    PlayerCommand command; // PlayerCommand to send as request to Server
    PlayerState player; // PlayerState to query

    const WireframeMesh *demo_mesh;
    const CPUTexture *demo_texture;
} Client;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the demo client's render object.
 * @param client Non-null destination; existing contents are reset.
 * @param mesh Non-null borrowed mesh, which must outlive rendering.
 * @param texture Borrowed texture, or NULL for solid color.
 * @note No allocation or resource ownership is transferred.
 */
void client_init(Client *client, const WireframeMesh *mesh, const CPUTexture *texture, float max_pitch_radians, float eye_height);

/**
 * @brief Replace presentation state from a complete server snapshot.
 * @param client Non-null initialized client.
 * @param snapshot Non-null snapshot; copied values are not retained by pointer.
 * @return False for excessive entity count, leaving Client state unchanged.
 */
bool client_receive_snapshot(Client *client, const ServerSnapshot *snapshot);

/**
 * @brief Some pattern as for snapshot; replace map state from Server to convert the received map into render objects.
 * @param client Non-null initialized client.
 * @param map Non-null and initialized map.
 * @param textures TextureBook from which to retrieve and resolve textures
 * @return False for inability to receive map from Server
 */
bool client_receive_map(Client *client, const MapData *map, TextureBook *textures);


/** Sample held movement and consume mouse deltas once per display frame.
 * Keeps absolute requested angles independently of the last received snapshot.
 */
void client_sample_input(Client *client, InputThinker *input);

/** Return the latest intent by value; sampling is a separate operation. */
PlayerCommand client_build_command(const Client *client);

/** Derive view pose from the last snapshot, preserving projection settings. */
void client_update_camera(const Client *client, Camera *camera);

PlayerCommand client_take_command(Client *client);

#ifdef __cplusplus
}
#endif

#endif