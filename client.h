#ifndef LITTLEENGINE_CLIENT_H
#define LITTLEENGINE_CLIENT_H

#include "game_protocol.h"
#include "renderer.h"


typedef struct {
    SceneObject objects[SNAPSHOT_ENTITY_CAPACITY];
    size_t object_count;
    uint64_t server_tick_number;

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
void client_init(Client *client, const WireframeMesh *mesh, const CPUTexture *texture);

/**
 * @brief Replace presentation state from a complete server snapshot.
 * @param client Non-null initialized client.
 * @param snapshot Non-null snapshot; copied values are not retained by pointer.
 * @return False for excessive entity count, leaving Client state unchanged.
 */
bool client_receive_snapshot(Client *client, const ServerSnapshot *snapshot);

#ifdef __cplusplus
}
#endif

#endif