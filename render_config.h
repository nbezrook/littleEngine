#ifndef LITTLEENGINE_RENDER_CONFIG_H
#define LITTLEENGINE_RENDER_CONFIG_H

// configuration of renderer
typedef struct {
    int shadow_face_size;   // pixels per square face
    float shadow_near_clip; // meters from light (prevent certain glitches)
} RenderConfig;

#endif