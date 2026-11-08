#ifndef LITTLEENGINE_RENDERER_H
#define LITTLEENGINE_RENDERER_H

#include "include/SDL3/SDL.h"
#include "math/math_types.h"
#include "camera.h"
#include "texture.h"
#include "depth_buffer.h"

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

typedef struct {
    SDL_Window *window;
    SDL_Renderer *sdl_renderer;
    SDL_Texture *screen_texture;
    int width;
    int height;
    RenderScratch scratch;
    DepthBuffer depth;
} EngineRenderer;

    #ifdef __cplusplus
    extern "C" {
    #endif

    /**
     * @brief Initialize SDL video and create the window, renderer, streaming image, and depth buffer.
     * @param renderer Non-null destination with pointer fields initially zero and no live resources.
     * @param title Window title string.
     * @param width Positive framebuffer width in pixels.
     * @param height Positive framebuffer height in pixels.
     * @return True on success; false on SDL or depth-buffer failure.
     * @note Main thread only. Some early SDL failure paths currently leave resources allocated.
     */
    bool renderer_init(
        EngineRenderer *renderer,
        const char *title,
        int width,
        int height
    );



    /**
     * @brief Clear and rasterize one object, draw its texture preview, and present the image.
     * @param renderer Non-null initialized renderer, used on the main thread.
     * @param objects Objects retrieved from server snapshot.
     * @param object_count Self-explanatory
     * @param camera Non-null camera used for projection.
     * @note CPU rasterization uses pitch-aware rows and a depth buffer cleared once per frame.
     * There is no triangle near-plane clipping; an unprojectable vertex rejects the mesh.
     */
    void render_frame(EngineRenderer *renderer, const SceneObject *objects, size_t object_count, const Camera *camera);

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