#include "map.h"
#include "math/primitives.h"
#include <math.h>    /* isfinite */
#include <string.h>  /* memchr */

// development asserts; map data is bad if these pop
_Static_assert(MAP_VERTEX_CAPACITY >= 8, "Room needs eight vertices; check Map");
_Static_assert(MAP_TRIANGLE_CAPACITY >= 12, "Room needs twelve triangles; check Map");
_Static_assert(MAP_SURFACE_CAPACITY >= 6, "Room needs six surfaces; check Map");
_Static_assert(MAP_TEXTURE_NAME_BYTES >= sizeof("stone_wall"),"Texture name must fit; check Map");

bool map_make_test_room(MapData *out)
{
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

    for (size_t i = 0; i < result.triangle_count; ++i)
        result.triangles[i] = room_mesh.triangles[i];

    for (size_t i = 0; i < result.surface_count; ++i) {
        result.surfaces[i] = (MapSurface){
            .first_triangle = i * 2,
            .triangle_count = 2,
            .texture_name = "stone_wall"
        };
    }

    if (!map_validate(&result)) return false;

    *out = result;
    return true;
}
bool map_validate(const MapData *map)
{
    if (!map || map->vertex_count > MAP_VERTEX_CAPACITY || map->triangle_count > MAP_TRIANGLE_CAPACITY || map->surface_count > MAP_SURFACE_CAPACITY) return false;

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
            || !memchr(surface->texture_name, '\0', sizeof surface->texture_name))
            return false;
    }
    return true;
}
