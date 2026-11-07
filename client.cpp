#include "client.h"

void client_init(Client *client, const WireframeMesh *mesh, const CPUTexture *texture)
{
    *client = Client{};
    client->demo_mesh = mesh;
    client->demo_texture = texture;
    SDL_Log("Client initialized.");
}

bool client_receive_snapshot(Client *client, const ServerSnapshot *snapshot)
{
    if (snapshot->entity_count > SNAPSHOT_ENTITY_CAPACITY)
        return false;

    for (size_t i = 0; i < snapshot->entity_count; ++i) {
        SceneObject &object = client->objects[i];

        object = SceneObject{};
        object.mesh = client->demo_mesh;
        object.texture = client->demo_texture;
        object.color = 0x0000FF00;
        object.position = snapshot->entities[i].position;
        object.rotation = snapshot->entities[i].rotation;
    }

    client->object_count = snapshot->entity_count;
    client->server_tick_number = snapshot->tick_number;
    return true;
}