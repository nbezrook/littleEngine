#ifndef LITTLEENGINE_LIGHTING_TYPES_H
#define LITTLEENGINE_LIGHTING_TYPES_H
#include "math/math_types.h"
#include <stdbool.h>
#include <stddef.h>


enum { 
    SCENE_LIGHT_CAPACITY = 16 
};

typedef enum { 
    LIGHT_POINT, LIGHT_DIRECTIONAL 
} LightType;


typedef struct {
    LightType type;
    Vector3 position;          // point lights, meters in space
    Vector3 direction_to_light;// unit direction
    Vector3 color;             // [0,1] rgb
    float intensity;           // relative strength
    float radius;              // speaks for itself
    bool casts_shadows;
} Light;


typedef struct {
    Vector3 ambient;           // [0,1] rgb
    size_t light_count;
    Light lights[SCENE_LIGHT_CAPACITY];
    float shadow_bias;         // positive normal offset 
} SceneLighting;

#endif
