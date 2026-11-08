#include "client.h"
#include "map.h"
#include <cstring>
#include "assetsThinker.h"
#include <cmath>

void client_sample_input(Client *client, InputThinker *input)
{
    InputMovement movement = input_get_movement(input);
    InputLook look = input_take_look(input);
    client->command.forward = movement.forward;
    client->command.right = movement.right;
    // magic values; should not be hardcoded
    client->command.yaw = std::remainder(client->command.yaw + look.yaw, 6.28318530718f);
    client->command.pitch += look.pitch;
    if (client->command.pitch > 1.55f) client->command.pitch = 1.55f;
    if (client->command.pitch < -1.55f) client->command.pitch = -1.55f;
}

PlayerCommand client_build_command(const Client *client)
{
    return client->command;
}

void client_update_camera(const Client *client, Camera *camera)
{
    camera->position = client->player.position;
    camera->yaw = client->player.yaw;
    camera->pitch = client->player.pitch;
}

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

    client->player = snapshot->player;

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
        //hardcoded values to be abstracted away later
        object.mesh = &mesh;
        object.texture = texture_book_resolve(textures, surface.texture_name);
        object.color = 0x00808080;
    }

    client->map_object_count = client->map.surface_count;
    client->object_count = client->map_object_count;
    return true;
}