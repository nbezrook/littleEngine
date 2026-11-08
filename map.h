#ifndef LITTLEENGINE_MAP_H
#define LITTLEENGINE_MAP_H

#include <stdbool.h>
#include "math/map_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copy the built-in room into self-contained map storage.
 * @param out Non-null destination; unchanged on failure.
 * @return False if the source room has unexpected geometry counts.
 * @note Does not allocate memory or load textures.
 */
bool map_make_test_room(MapData *out);

/**
 * @brief Check native map counts, geometry indices, finite coordinates/UVs, and surface ranges.
 * @param map Map to inspect; NULL is rejected.
 * @return True for valid bounded data, including an empty map; false otherwise.
 * @note Checks texture-name termination, not asset existence or collision geometry quality.
 */
bool map_validate(const MapData *map);

#ifdef __cplusplus
}
#endif

#endif