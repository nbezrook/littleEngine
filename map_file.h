#ifndef LITTLEENGINE_MAP_FILE_H
#define LITTLEENGINE_MAP_FILE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "math/map_types.h"


#ifdef __cplusplus
extern "C" {
#endif
enum { MAP_FILE_HEADER_BYTES = 24 };
typedef struct {
    uint32_t vertex_count;
    uint32_t triangle_count;
    uint32_t surface_count;
    uint32_t flags;
} MapFileHeader;
/**
 * Decode version-two header from a borrowed byte buffer (at least 24 bytes).
 * Reject unsupported versions, flags, and counts beyond runtime capacity.
 * Does not validate the body or install a map. No allocation.
 * NULL arguments are rejected; *out is unchanged on failure.
 */
bool map_file_read_header(MapFileHeader *out, const uint8_t *bytes, size_t size);

/**
 * Decode an entire version-two buffer into self-contained map storage.
 * No allocation, file IO, texture loading, or World mutation.
 * Rejects truncated/trailing data and invalid geometry; *out unchanged on failure.
 * bytes is borrowed for the call; decoded storage retains no pointers into it.
 * Version 1 requires IEEE-754 binary32 floats on the host platform.
 */
bool map_file_decode(MapData *out, const uint8_t *bytes, size_t size);

#ifdef __cplusplus
}
#endif

#endif
