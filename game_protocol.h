#ifndef LITTLEENGINE_GAME_PROTOCOL_H
#define LITTLEENGINE_GAME_PROTOCOL_H

#include "math/math_types.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>


/**
 * @brief Structured player command as understood by the server
 */
typedef struct {
    float forward;
    float right;
    float yaw;
    float pitch;
    bool jump;
} PlayerCommand;
/**
 * @brief Description of player state.
 */
typedef struct {
    Vector3 position;
    float yaw;
    float pitch;
} PlayerState;

/**
 * @brief Public simulation state copied to presentation.
 * @note Runtime data, not a serialized file or network format.
 */
typedef struct {
    Vector3 position; /* World units. */
    Vector3 rotation; /* Euler angles in radians. */
} EntityState;

enum {SNAPSHOT_ENTITY_CAPACITY = 128};

/**
 * @brief Public world state at one server tick.
 * @note Native runtime data, not a serialized file/network format.
 */
typedef struct {
    uint64_t tick_number;
    size_t entity_count;
    EntityState entities[SNAPSHOT_ENTITY_CAPACITY];
    PlayerState player; // server sends description of player states, this can be expanded to more
} ServerSnapshot;

#endif
