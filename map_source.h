#ifndef LITTLEENGINE_MAP_SOURCE_H
#define LITTLEENGINE_MAP_SOURCE_H
#include <stdbool.h>
#include "math/map_types.h"

typedef struct {
    Point2 metres_per_repeat;
    Point2 offset;
    float rotation_radians;
} SurfaceMapping;

typedef struct {
    char material_name[MAP_MATERIAL_NAME_BYTES];
    SurfaceMapping mapping;
} SurfaceDefinition;

typedef struct {
    RoomBounds bounds;
    Vector3 player_spawn;
    SurfaceDefinition walls;
    SurfaceDefinition floor;
    SurfaceDefinition ceiling;
} BoxRoomDefinition;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Load version-two JSON containing exactly one box room.
 * Startup/loading operation: file IO and temporary JSON allocations occur here.
 * Copies all values; no pointers into the document survive this call.
 * Rejects bad dimensions, spawn, numbers, and material-name lengths and mapping settings.
 * NULL/empty arguments are rejected; failure leaves *out unchanged.
 * Does not resolve textures or validate the player's body against the room.
 * Call on the loading/owning thread before publishing the result.
 */
bool box_room_definition_load_json(BoxRoomDefinition *out, const char *path);

bool map_compile_box_room(MapData *out,const BoxRoomDefinition *source);

#ifdef __cplusplus
}
#endif
#endif
