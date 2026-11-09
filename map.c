#include "map.h"
#include "math/primitives.h"
#include <math.h>    /* isfinite */
#include <string.h>  /* memchr */

// development asserts; map data is bad if these pop
_Static_assert(MAP_VERTEX_CAPACITY >= 8, "Room needs eight vertices; check Map");
_Static_assert(MAP_TRIANGLE_CAPACITY >= 12, "Room needs twelve triangles; check Map");
_Static_assert(MAP_SURFACE_CAPACITY >= 6, "Room needs six surfaces; check Map");
_Static_assert(MAP_MATERIAL_NAME_BYTES >= sizeof("stone_wall"),"Texture name must fit; check Map");



bool map_make_test_room(MapData *out)
{ 
    // UV REPEATS ON WALLS
    // AI GENERATED
    const Point2 repeats[6] = {
        {6.0f, 2.5f},  /* Z-negative wall */
        {6.0f, 2.5f},  /* Z-positive wall */
        {8.0f, 2.5f},  /* X-negative wall */
        {8.0f, 2.5f},  /* X-positive wall */
        {6.0f, 8.0f},  /* Floor */
        {6.0f, 8.0f}   /* Ceiling */
    };

    if (room_mesh.vertex_count != 8 || room_mesh.triangle_count != 12) {
        return false;
    }

    MapData result = {
        .vertex_count = 8,
        .triangle_count = 12,
        .surface_count = 6
    };

    for (size_t i = 0; i < result.vertex_count; ++i)
        result.vertices[i] = room_mesh.vertices[i];

    // axis-alligned rectangular bounds generator
    result.has_room_bounds = true;
    result.room_bounds.min = result.vertices[0];
    result.room_bounds.max = result.vertices[0];
    for (size_t i = 1; i < result.vertex_count; ++i) {
        Vector3 p = result.vertices[i];
        if (p.x < result.room_bounds.min.x) result.room_bounds.min.x = p.x;
        if (p.y < result.room_bounds.min.y) result.room_bounds.min.y = p.y;
        if (p.z < result.room_bounds.min.z) result.room_bounds.min.z = p.z;
        if (p.x > result.room_bounds.max.x) result.room_bounds.max.x = p.x;
        if (p.y > result.room_bounds.max.y) result.room_bounds.max.y = p.y;
        if (p.z > result.room_bounds.max.z) result.room_bounds.max.z = p.z;
    }

    result.player_spawn = (Vector3){0.0f, result.room_bounds.min.y, -4.0f};

    for (size_t i = 0; i < result.triangle_count; ++i)
        result.triangles[i] = room_mesh.triangles[i];

    for (size_t i = 0; i < result.surface_count; ++i) {
        result.surfaces[i] = (MapSurface){
            .first_triangle = i * 2,
            .triangle_count = 2,
            .material_name = "rough_stone"
        };
        for (size_t j = 0; j < 2; ++j) {
            Triangle *triangle = &result.triangles[i * 2 + j];

            triangle->uv_a.x *= repeats[i].x;
            triangle->uv_a.y *= repeats[i].y;

            triangle->uv_b.x *= repeats[i].x;
            triangle->uv_b.y *= repeats[i].y;

            triangle->uv_c.x *= repeats[i].x;
            triangle->uv_c.y *= repeats[i].y;
        }
    }

    if (!map_validate(&result)) return false;

    *out = result;
    return true;
}

bool map_validate(const MapData *map)
{
    if (!map || map->vertex_count > MAP_VERTEX_CAPACITY 
        || map->triangle_count > MAP_TRIANGLE_CAPACITY 
        || map->surface_count > MAP_SURFACE_CAPACITY) return false;

    Vector3 spawn = map->player_spawn;
    if (!isfinite(spawn.x) || !isfinite(spawn.y) || !isfinite(spawn.z)) return false;
    if (map->has_room_bounds) {
        Vector3 lo = map->room_bounds.min;
        Vector3 hi = map->room_bounds.max;
        //identify bounds in map
        if (!isfinite(lo.x) || !isfinite(lo.y) || !isfinite(lo.z) 
        ||!isfinite(hi.x) || !isfinite(hi.y) || !isfinite(hi.z) 
        ||lo.x >= hi.x || lo.y >= hi.y || lo.z >= hi.z 
        ||spawn.x < lo.x || spawn.x > hi.x || spawn.y < lo.y 
        || spawn.y > hi.y || spawn.z < lo.z || spawn.z > hi.z)
            return false;
    }

    for (size_t i = 0; i < map->vertex_count; ++i) {
        Vector3 p = map->vertices[i];
        if (!isfinite(p.x) || !isfinite(p.y) || !isfinite(p.z))
            return false;
    }

    for (size_t i = 0; i < map->triangle_count; ++i) {
        const Triangle *t = &map->triangles[i];
        if (t->a < 0 || t->b < 0 || t->c < 0 || (size_t)t->a >= map->vertex_count || (size_t)t->b >= map->vertex_count || (size_t)t->c >= map->vertex_count)
            return false;
        Point2 uv[3] = {t->uv_a, t->uv_b, t->uv_c};
        for (int j = 0; j < 3; ++j)
            if (!isfinite(uv[j].x) || !isfinite(uv[j].y))
                return false;
    }

    for (size_t i = 0; i < map->surface_count; ++i) {
        const MapSurface *surface = &map->surfaces[i];
        if (surface->first_triangle > map->triangle_count || surface->triangle_count == 0 
            || surface->triangle_count > map->triangle_count - surface->first_triangle 
            || !memchr(surface->material_name, '\0', sizeof surface->material_name))
            return false;
    }
    return true;
}