#include "client.h"
#include "map.h"
#include <cstring>
#include "assetsThinker.h"

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
        SceneObject &object = client->objects[client->map_object_count + i];

        object = SceneObject{};
        object.mesh = client->demo_mesh;
        object.texture = client->demo_texture;
        object.color = 0x0000FF00;
        object.position = snapshot->entities[i].position;
        object.rotation = snapshot->entities[i].rotation;
    }

    client->object_count = client->map_object_count + snapshot->entity_count;
    client->server_tick_number = snapshot->tick_number;
    return true;
}

bool client_receive_map(Client *client, const MapData *map, TextureBook *textures)
{
    // if no textures, no fallback, failed validation, too many vertices on map, or mismatch of object count
    if (!textures || !textures->fallback || !map_validate(map) || map->vertex_count > RENDER_VERTEX_CAPACITY || client->object_count != client->map_object_count) {
        return false;
    }
    client->map = *map;

    for (size_t i = 0; i < client->map.surface_count; ++i) {
        const MapSurface &surface = client->map.surfaces[i];

        WireframeMesh &mesh = client->map_meshes[i];
        mesh = WireframeMesh{};

        mesh.vertices = client->map.vertices;
        mesh.vertex_count = static_cast<int>(client->map.vertex_count);

        mesh.triangles = client->map.triangles + surface.first_triangle;

        mesh.triangle_count = static_cast<int>(surface.triangle_count);

        SceneObject &object = client->objects[i];
        object = SceneObject{};

        object.mesh = &mesh;
        object.texture = texture_book_resolve(textures, surface.texture_name);
        object.color = 0x00808080;
    }

    client->map_object_count = client->map.surface_count;
    client->object_count = client->map_object_count;
    return true;
}