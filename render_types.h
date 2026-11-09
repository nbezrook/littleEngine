#ifndef LITTLEENGINE_RENDER_TYPES_H
#define LITTLEENGINE_RENDER_TYPES_H
#include <stdint.h>
#include "math/math_types.h"
#include "texture.h"

enum {
    RENDER_VERTEX_CAPACITY = 4096,
};

typedef struct {
    ProjectedVertex projected[RENDER_VERTEX_CAPACITY];
    Vector3 view[RENDER_VERTEX_CAPACITY];
} RenderScratch;

typedef struct {
    const WireframeMesh *mesh;

    Vector3 position;
    Vector3 rotation;       // radians
    
    const CPUTexture *texture;

    uint32_t color;
} SceneObject;


#endif
