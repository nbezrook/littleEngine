#include "map_source.h"
#include "map.h"
#include <math.h>
#include <string.h>
#include "lighting.h"

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

static Point2 map_uv(Point2 uv, Point2 repeats, Point2 offset, float cosine, float sine)
{
    float u = uv.x * repeats.x;
    float v = uv.y * repeats.y;
    return (Point2){u * cosine - v * sine + offset.x, u * sine + v * cosine + offset.y};
}
static bool append_box(
    MapData *map, const RoomBounds *bounds,
    const SurfaceDefinition *definitions[6], bool inward)
{
    if (map->vertex_count > MAP_VERTEX_CAPACITY - 8 ||
        map->triangle_count > MAP_TRIANGLE_CAPACITY - 12 ||
        map->surface_count > MAP_SURFACE_CAPACITY - 6)
        return false;

    Vector3 lo=bounds->min, hi=bounds->max;
    if (!isfinite(lo.x)||!isfinite(lo.y)||!isfinite(lo.z)||
        !isfinite(hi.x)||!isfinite(hi.y)||!isfinite(hi.z)) return false;
    float width=hi.x-lo.x, height=hi.y-lo.y, length=hi.z-lo.z;
    if (!isfinite(width)||!isfinite(height)||!isfinite(length)||
        width<=0||height<=0||length<=0) return false;

    size_t vertex_base=map->vertex_count;
    size_t triangle_base=map->triangle_count;
    size_t surface_base=map->surface_count;
    Vector3 vertices[8]={
        {lo.x,lo.y,lo.z},{hi.x,lo.y,lo.z},
        {hi.x,hi.y,lo.z},{lo.x,hi.y,lo.z},
        {lo.x,lo.y,hi.z},{hi.x,lo.y,hi.z},
        {hi.x,hi.y,hi.z},{lo.x,hi.y,hi.z}
    };
    for(size_t i=0;i<8;++i) map->vertices[vertex_base+i]=vertices[i];
    const Point2 face_sizes[6]={
        {width,height},{width,height},{length,height},
        {length,height},{width,length},{width,length}
    };
    for(size_t face=0;face<6;++face) {
        const SurfaceDefinition *definition=definitions[face];
        const SurfaceMapping *mapping=&definition->mapping;
        MapSurface *surface=&map->surfaces[surface_base+face];
        if (!copy_material_name(surface->material_name,definition->material_name)|| !isfinite(mapping->metres_per_repeat.x)||!isfinite(mapping->metres_per_repeat.y)|| mapping->metres_per_repeat.x<=0||mapping->metres_per_repeat.y<=0|| !isfinite(mapping->offset.x)||!isfinite(mapping->offset.y)|| !isfinite(mapping->rotation_radians)) return false;
        Point2 repeats={face_sizes[face].x/mapping->metres_per_repeat.x, face_sizes[face].y/mapping->metres_per_repeat.y};
        if(!isfinite(repeats.x)||!isfinite(repeats.y)||repeats.x<=0||repeats.y<=0)
            return false;
        float cosine=cosf(mapping->rotation_radians),sine=sinf(mapping->rotation_radians);
        surface->first_triangle=triangle_base+face*2;
        surface->triangle_count=2;
        for(size_t corner=0;corner<2;++corner) {
            Triangle triangle=box_triangles[face*2+corner];
            triangle.uv_a=map_uv(triangle.uv_a,repeats,mapping->offset,cosine,sine);
            triangle.uv_b=map_uv(triangle.uv_b,repeats,mapping->offset,cosine,sine);
            triangle.uv_c=map_uv(triangle.uv_c,repeats,mapping->offset,cosine,sine);
            if(!inward) {
                int index=triangle.b;triangle.b=triangle.c;triangle.c=index;
                Point2 uv=triangle.uv_b;triangle.uv_b=triangle.uv_c;triangle.uv_c=uv;
            }
            triangle.a+=(int)vertex_base;
            triangle.b+=(int)vertex_base;
            triangle.c+=(int)vertex_base;
            map->triangles[triangle_base+face*2+corner]=triangle;
        }
    }
    map->vertex_count+=8;
    map->triangle_count+=12;
    map->surface_count+=6;
    return true;
}

bool map_compile_box_room(MapData *out,const BoxRoomDefinition *source)
{
    if(!out||!source||source->solid_count>MAP_SOURCE_SOLID_CAPACITY) return false;
    MapData result={0};
    result.has_room_bounds=true;
    result.room_bounds=source->bounds;
    result.player_spawn=source->player_spawn;
    if(!lighting_prepare(&result.lighting,&source->lighting)) return false;
    const SurfaceDefinition *shell[6]={
        &source->walls,&source->walls,&source->walls,&source->walls,
        &source->floor,&source->ceiling
    };
    if(!append_box(&result,&source->bounds,shell,true)) return false;
    for(size_t i=0;i<source->solid_count;++i) {
        const SolidBoxDefinition *solid=&source->solids[i];
        Vector3 lo=solid->bounds.min,hi=solid->bounds.max;
        if(lo.x<source->bounds.min.x||lo.y<source->bounds.min.y||lo.z<source->bounds.min.z|| hi.x>source->bounds.max.x||hi.y>source->bounds.max.y||hi.z>source->bounds.max.z)
            return false;
        const SurfaceDefinition *faces[6]={
            &solid->surface,&solid->surface,&solid->surface,
            &solid->surface,&solid->surface,&solid->surface
        };
        if(!append_box(&result,&solid->bounds,faces,false)) return false;
    }
    if(!map_validate(&result)) return false;
    *out=result;
    return true;
}
