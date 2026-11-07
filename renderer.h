#ifndef LITTLEENGINE_RENDERER_H
#define LITTLEENGINE_RENDERER_H

#include "include/SDL3/SDL.h"
#include "math/math_types.h"
#include "camera.h"

enum {
    RENDER_VERTEX_CAPACITY = 4096,
    TEXTURE_CAPACITY = 64
};

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
} CPUTexture;

typedef struct {
    CPUTexture textures[TEXTURE_CAPACITY];
    size_t count;
} TextureBook;

typedef struct {
    ProjectedVertex projected[RENDER_VERTEX_CAPACITY];
} RenderScratch;

typedef struct {
    const WireframeMesh *mesh;

    Vector3 position;
    Vector3 rotation;       // radians
    Vector3 rotation_rate;  // rad per sec

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

bool renderer_init(
    EngineRenderer *renderer,
    const char *title,
    int width,
    int height
);

    #ifdef __cplusplus
    extern "C" {
    #endif

    void render_frame(EngineRenderer *renderer, const SceneObject *object, const Camera *camera);
    void renderer_shutdown(EngineRenderer *renderer);

    bool texture_init(CPUTexture *texture, int width, int height);
    void texture_shutdown(CPUTexture *texture);

    void texture_make_checker(CPUTexture *texture);

    CPUTexture *texture_book_create(TextureBook *book, int width, int height);
    void texture_book_shutdown(TextureBook *book);

    uint32_t texture_sample(const CPUTexture *texture, float u, float v);

    #ifdef __cplusplus
    }
    #endif

#endif