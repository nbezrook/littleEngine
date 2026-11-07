#ifndef LITTLEENGINE_ENGINE_H
#define LITTLEENGINE_ENGINE_H

#include "renderer.h"
#include "console.h"
#include "server.h"
#include "camera.h"
#include "inputThinker.h"
#include "client.h"

typedef struct {
    EngineRenderer renderer;
    DebugConsole console;

    Camera camera;

    InputThinker input;

    Uint64 previous_time;
    double accumulator;

    Server server;
    Client client;
    TextureBook textures;
} Engine;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the renderer, debug console, assets, camera, timing, and input.
 * @param engine Non-null destination with no live engine resources; overwritten during initialization.
 * @param width Positive rendering width in pixels.
 * @param height Positive rendering height in pixels.
 * @return True when startup succeeds; false when initialization fails.
 * @note Call on the main thread. A missing PNG selects the allocated fallback texture.
 * Early renderer/console failure paths do not currently guarantee complete cleanup.
 */
bool engine_init(Engine *engine, int width, int height);
/**
 * @brief Route SDL events, run accumulated 60 Hz ticks, and render one frame.
 * @param engine Non-null successfully initialized engine, used on the main thread.
 * @return False for an application quit or main-window close request; true otherwise.
 * @note Elapsed time is capped at 0.25 seconds. Input state persists between ticks.
 * The current loop applies movement but does not yet consume look deltas.
 */
bool engine_frame(Engine *engine);
/**
 * @brief Release input, textures, console, renderer resources, and SDL video state.
 * @param engine Non-null initialized engine, or zero-initialized state after a supported startup failure.
 * @note Call on the main thread once per engine lifetime. Do not use the engine afterward.
 */
void engine_shutdown(Engine *engine);

#ifdef __cplusplus
}
#endif

#endif