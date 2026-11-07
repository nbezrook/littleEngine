#ifndef LITTLEENGINE_ENGINE_H
#define LITTLEENGINE_ENGINE_H

#include "renderer.h"
#include "console.h"
#include "entities.h"
#include "camera.h"

typedef struct {
    EngineRenderer renderer;
    DebugConsole console;

    Camera camera;

    Uint64 previous_time;
    Uint64 tic_number;
    double accumulator;

    SceneObject object;

    TextureBook textures;
} Engine;

#ifdef __cplusplus
extern "C" {
#endif

bool engine_init(Engine *engine, int width, int height);
bool engine_frame(Engine *engine);
void engine_shutdown(Engine *engine);

#ifdef __cplusplus
}
#endif

#endif