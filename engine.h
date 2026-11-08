#ifndef LITTLEENGINE_ENGINE_H
#define LITTLEENGINE_ENGINE_H

#include "renderer.h"
#include "console.h"
#include "server.h"
#include "camera.h"
#include "inputThinker.h"
#include "client.h"
#include "game_config.h"

enum { ENGINE_PATH_CAPACITY = 1024 };

typedef struct {
    char game_directory[ENGINE_PATH_CAPACITY];

    GameConfig config;

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
 * @param game_directory Nonempty asset root, absolute or relative to the working directory.
 * Copied into engine-owned storage; keep the working directory unchanged for relative roots.
 * @return True when startup succeeds; false when initialization fails.
 * @note Call on the main thread. A missing PNG selects the allocated fallback texture.
 * Early renderer/console failure paths do not currently guarantee complete cleanup.
 */
bool engine_init(Engine *engine, int width, int height, const char *game_directory);

/**
 * @brief Join the selected game directory and a trusted, relative asset filename.
 * @param engine Engine with a nonempty game_directory.
 * @param relative_path Nonempty path within the game directory, using forward slashes.
 * @param out Non-null separate output buffer; empty on truncation.
 * @param capacity Size of out in bytes; must be positive.
 * @return False for invalid arguments or insufficient output space.
 * @note Does not access files, normalize paths, or enforce a filesystem sandbox.
 */
bool engine_asset_path(const Engine *engine, const char *relative_path,
                       char *out, size_t capacity);
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