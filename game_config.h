#ifndef LITTLEENGINE_GAME_CONFIG_H
#define LITTLEENGINE_GAME_CONFIG_H

enum {
    GAME_MAP_PATH_BYTES = 256 
};


typedef struct {
    float walk_speed;          // meters p sec
    float half_width;          // meters
    float height;
    float eye_height;
    float max_pitch_radians;
    float jump_speed;
} PlayerRules;

typedef struct {
    unsigned tick_hz;
    PlayerRules player;
    float vertical_fov_radians;
    float near_clip;           // meters
    float gravity;

    char start_map[GAME_MAP_PATH_BYTES];
} GameConfig;

#endif