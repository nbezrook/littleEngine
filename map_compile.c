#include "map_source.h"
#include "map.h"
#include <math.h>
#include <string.h>

_Static_assert(MAP_VERTEX_CAPACITY >= 8, "Box room needs eight vertices");
_Static_assert(MAP_TRIANGLE_CAPACITY >= 12, "Box room needs twelve triangles");
_Static_assert(MAP_SURFACE_CAPACITY >= 6, "Box room needs six surfaces");

/* Box topology: dimensions and materials come from the authoring source. */
static const Triangle box_triangles[12] = {
    {0,1,2, {0,1},{1,1},{1,0}}, /* Z-negative */
    {0,2,3, {0,1},{1,0},{0,0}},
    {4,6,5, {0,1},{1,0},{1,1}}, /* Z-positive */
    {4,7,6, {0,1},{0,0},{1,0}},
    {0,7,4, {0,1},{1,0},{1,1}}, /* X-negative */
    {0,3,7, {0,1},{0,0},{1,0}},
    {1,6,2, {0,1},{1,0},{0,0}}, /* X-positive */
    {1,5,6, {0,1},{1,1},{1,0}},
    {0,5,1, {0,1},{1,0},{1,1}}, /* Floor */
    {0,4,5, {0,1},{0,0},{1,0}},
    {3,6,7, {0,1},{1,0},{0,0}}, /* Ceiling */
    {3,2,6, {0,1},{1,1},{1,0}}
};

static bool copy_material_name(char *out, const char *name)
{
    const char *end = memchr(name, '\0', MAP_MATERIAL_NAME_BYTES);
    if (!end || end == name) return false;
    memcpy(out, name, (size_t)(end - name) + 1);
    return true;
}

static Point2 map_uv(Point2 uv, Point2 repeats, Point2 offset,
                     float cosine, float sine)
{
    float u = uv.x * repeats.x;
    float v = uv.y * repeats.y;
    return (Point2){u * cosine - v * sine + offset.x, u * sine + v * cosine + offset.y};
}

bool map_compile_box_room(MapData *out, const BoxRoomDefinition *source)
{
    if (!out || !source) return false;

    Vector3 lo = source->bounds.min;
    Vector3 hi = source->bounds.max;
    if (!isfinite(lo.x) || !isfinite(lo.y) || !isfinite(lo.z) || !isfinite(hi.x) || !isfinite(hi.y) || !isfinite(hi.z))
        return false;

    float width = hi.x - lo.x;
    float height = hi.y - lo.y;
    float length = hi.z - lo.z;
    if (!isfinite(width) || !isfinite(height) || !isfinite(length) || width <= 0.0f || height <= 0.0f || length <= 0.0f)
        return false;

    MapData result = {0};
    result.vertex_count = 8;
    result.triangle_count = 12;
    result.surface_count = 6;
    result.has_room_bounds = true;
    result.room_bounds = source->bounds;
    result.player_spawn = source->player_spawn;

    result.vertices[0] = (Vector3){lo.x, lo.y, lo.z};
    result.vertices[1] = (Vector3){hi.x, lo.y, lo.z};
    result.vertices[2] = (Vector3){hi.x, hi.y, lo.z};
    result.vertices[3] = (Vector3){lo.x, hi.y, lo.z};
    result.vertices[4] = (Vector3){lo.x, lo.y, hi.z};
    result.vertices[5] = (Vector3){hi.x, lo.y, hi.z};
    result.vertices[6] = (Vector3){hi.x, hi.y, hi.z};
    result.vertices[7] = (Vector3){lo.x, hi.y, hi.z};

    const Point2 face_sizes[6] = {
        {width, height}, {width, height},
        {length, height}, {length, height},
        {width, length}, {width, length}
    };
    const SurfaceDefinition *definitions[6] = {
        &source->walls, &source->walls, &source->walls, &source->walls,
        &source->floor, &source->ceiling
    };

    for (size_t face = 0; face < 6; ++face) {
        MapSurface *surface = &result.surfaces[face];
        surface->first_triangle = face * 2;
        surface->triangle_count = 2;
        const SurfaceDefinition *definition = definitions[face];
        const SurfaceMapping *mapping = &definition->mapping;
        if (!copy_material_name(surface->material_name, definition->material_name) || !isfinite(mapping->metres_per_repeat.x) || !isfinite(mapping->metres_per_repeat.y)
            || mapping->metres_per_repeat.x <= 0.0f || mapping->metres_per_repeat.y <= 0.0f || !isfinite(mapping->offset.x) || !isfinite(mapping->offset.y)
            || !isfinite(mapping->rotation_radians))
            return false;
        Point2 repeats = {
            face_sizes[face].x / mapping->metres_per_repeat.x,
            face_sizes[face].y / mapping->metres_per_repeat.y
        };
        if (!isfinite(repeats.x) || !isfinite(repeats.y)
            || repeats.x <= 0.0f || repeats.y <= 0.0f)
            return false;
        float cosine = cosf(mapping->rotation_radians);
        float sine = sinf(mapping->rotation_radians);

        for (size_t corner_triangle = 0; corner_triangle < 2; ++corner_triangle) {
            size_t index = face * 2 + corner_triangle;
            Triangle triangle = box_triangles[index];
            triangle.uv_a = map_uv(triangle.uv_a, repeats, mapping->offset, cosine, sine);
            triangle.uv_b = map_uv(triangle.uv_b, repeats, mapping->offset, cosine, sine);
            triangle.uv_c = map_uv(triangle.uv_c, repeats, mapping->offset, cosine, sine);
            result.triangles[index] = triangle;
        }
    }

    if (!map_validate(&result)) return false;
    *out = result;
    return true;
}
