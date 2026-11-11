#ifndef LITTLEENGINE_RENDERER_H
#define LITTLEENGINE_RENDERER_H

#include "include/SDL3/SDL.h"
#include "math/math_types.h"
#include "camera.h"
#include "texture.h"
#include "depth_buffer.h"
#include "render_types.h"
#include "framebuffer.h"
#include "lighting.h"
#include "shadow_maps.h"
// threaded SIMD code
#include "render_workers.h"
#include "raster_scene.h"

#include "render_config.h"


// class for profiling rendering 
typedef struct {
    double shadow_ms;
    double prepare_ms;
    double tiles_ms;
    double present_ms;

    Uint64 window_start;
    Uint64 frequency;
    unsigned frames;
} RenderProfile;


typedef struct {
    SDL_Window *window;
    SDL_Renderer *sdl_renderer;
    SDL_Texture *screen_texture;
    bool video_initialized;

    RenderProfile profile;

    RenderScratch scratch;
    LightingLUT lighting_lut;
    ShadowMaps *shadow_maps; // renderer handles persistent depth cubemaps
    FrameBuffer framebuffer;
    //render workers do render jobs
    RenderWorkers *workers;
    RasterScene *prepared_scene;

    bool debug;
} EngineRenderer;

    #ifdef __cplusplus
    extern "C" {
    #endif

    /**
     * @brief Create presentation resources, framebuffer, and persistent point-light shadow maps.
     * @param renderer Non-null destination with pointer fields initially zero and no live resources.
     * @param title Window title string.
     * @param width Positive framebuffer width in pixels.
     * @param height Positive framebuffer height in pixels.
     * @return True on success; false on allocation, framebuffer, or SDL failure.
     * @note Main thread only; partial initialization is cleaned up on failure.
     */
    bool renderer_init(EngineRenderer *renderer, const char *title, int width, int height, const RenderConfig *config);


    /**
     * @brief Rebuild enabled point-light shadow maps, shade the scene, and present it.
     * @param renderer Non-null initialized renderer, used on the main thread.
     * @param objects Objects retrieved from server snapshot.
     * @param object_count Self-explanatory
     * @param camera Non-null camera used for projection.
     * @note Inputs remain immutable during the synchronous call. Maps and scratch
     * have exclusive renderer access. No per-frame allocation. Unsupported
     * shadow-casting light types fail map generation and are logged here.
     */
    void render_frame(EngineRenderer *renderer, const SceneObject *objects, size_t object_count, const Camera *camera, const SceneLighting *lighting);

    /**
     * @brief Free depth storage, destroy SDL rendering resources, and call SDL_Quit.
     * @param renderer Non-null initialized or appropriately zero-initialized renderer.
     * @note Main thread only. SDL handles are not reset; do not call twice on the same state.
     */
    void renderer_shutdown(EngineRenderer *renderer);

    #ifdef __cplusplus
    }
    #endif

#endif
