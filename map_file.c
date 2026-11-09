#include "map_file.h"
#include "math/map_types.h"
#include <string.h>

#include "map.h"
#include <float.h>
#include <limits.h>
#include <math.h>


static uint32_t read_u32_le(const uint8_t *p)
{
    // ugly byte buffer each byte is shifted into its specified position. casting before shifting makes the operations unsigned
    //reading individual bytes also avoids assuming that the input is aligned for a uint32_t
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* decoder
checks pointers and available header bytes
checks magic and version
decodes into a temporary MapFileHeader
rejects excessive counts and unknown flags
assigns *out only after success
*/
bool map_file_read_header(MapFileHeader *out, const uint8_t *bytes, size_t size)
{
    if (!out || !bytes || size < MAP_FILE_HEADER_BYTES)
        return false;

    if (memcmp(bytes, "LEMP", 4) != 0 || read_u32_le(bytes + 4) != 2)
        return false;

    MapFileHeader header = {
        .vertex_count   = read_u32_le(bytes + 8),
        .triangle_count = read_u32_le(bytes + 12),
        .surface_count  = read_u32_le(bytes + 16),
        .flags          = read_u32_le(bytes + 20)
    };
    //should be dynamic here
    if (header.vertex_count > MAP_VERTEX_CAPACITY || header.triangle_count > MAP_TRIANGLE_CAPACITY || header.surface_count > MAP_SURFACE_CAPACITY || (header.flags & ~UINT32_C(1)) != 0)
        return false;

    *out = header;
    return true;
}


//IEEE-754 binary32 float representation
_Static_assert(CHAR_BIT == 8 && sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && FLT_MIN_EXP == -125, "Map format requires 8-bit bytes and binary32 float");
_Static_assert(MAP_MATERIAL_NAME_BYTES == 64, "Version 2 names occupy 64 bytes");

typedef struct {
    const uint8_t *next;
    size_t remaining;
} MapReader;

static bool reader_u32(MapReader *reader, uint32_t *out)
{
    if (reader->remaining < 4) return false;
    *out = read_u32_le(reader->next);
    reader->next += 4;
    reader->remaining -= 4;
    return true;
}

static bool reader_float(MapReader *reader, float *out)
{
    uint32_t bits;
    if (!reader_u32(reader, &bits)) return false;
    float value;
    memcpy(&value, &bits, sizeof(value));
    if (!isfinite(value)) return false;
    *out = value;
    return true;
}

static bool reader_vec3(MapReader *reader, Vector3 *out)
{
    return reader_float(reader, &out->x) && reader_float(reader, &out->y) && reader_float(reader, &out->z);
}

static bool reader_uv(MapReader *reader, Point2 *out)
{
    return reader_float(reader, &out->x) && reader_float(reader, &out->y);
}

static bool add_record_bytes(size_t *total, uint32_t count, size_t stride)
{
    if (count > (SIZE_MAX - *total) / stride) return false;
    *total += (size_t)count * stride;
    return true;
}

bool map_file_decode(MapData *out, const uint8_t *bytes, size_t size)
{
    MapFileHeader header;
    if (!out || !map_file_read_header(&header, bytes, size)) return false;

    //header + spawn/min/max: 24 + 9 floats.
    size_t required = MAP_FILE_HEADER_BYTES + 36;
    if (!add_record_bytes(&required, header.vertex_count, 12) || !add_record_bytes(&required, header.triangle_count, 36) || !add_record_bytes(&required, header.surface_count, 72) || size != required)
        return false;

    MapData result = {0};
    result.vertex_count = header.vertex_count;
    result.triangle_count = header.triangle_count;
    result.surface_count = header.surface_count;
    result.has_room_bounds = (header.flags & UINT32_C(1)) != 0;

    MapReader reader = {bytes + MAP_FILE_HEADER_BYTES, size - MAP_FILE_HEADER_BYTES};

    if (!reader_vec3(&reader, &result.player_spawn) || !reader_vec3(&reader, &result.room_bounds.min) || !reader_vec3(&reader, &result.room_bounds.max))
        return false;

    for (size_t i = 0; i < result.vertex_count; ++i) {
        if (!reader_vec3(&reader, &result.vertices[i])) return false;
    }

    for (size_t i = 0; i < result.triangle_count; ++i) {
        Triangle *triangle = &result.triangles[i];
        uint32_t a, b, c;
        if (!reader_u32(&reader, &a) || !reader_u32(&reader, &b)|| !reader_u32(&reader, &c)|| a >= result.vertex_count 
        || b >= result.vertex_count || c >= result.vertex_count || a > INT_MAX || b > INT_MAX || c > INT_MAX)
            return false;

        triangle->a = (int)a;
        triangle->b = (int)b;
        triangle->c = (int)c;
        if (!reader_uv(&reader, &triangle->uv_a) || !reader_uv(&reader, &triangle->uv_b) || !reader_uv(&reader, &triangle->uv_c))
            return false;
    }

    for (size_t i = 0; i < result.surface_count; ++i) {
        MapSurface *surface = &result.surfaces[i];
        uint32_t first, count;
        if (!reader_u32(&reader, &first) || !reader_u32(&reader, &count) || reader.remaining < 64)
            return false;
        surface->first_triangle = first;
        surface->triangle_count = count;
        memcpy(surface->material_name, reader.next, 64);
        reader.next += 64;
        reader.remaining -= 64;
    }

    if (reader.remaining != 0 || !map_validate(&result)) return false;
    *out = result;
    return true;
}
