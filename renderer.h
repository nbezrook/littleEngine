#ifndef LITTLEENGINE_RENDERER_H
#define LITTLEENGINE_RENDERER_H

#include "include/SDL3/SDL.h"
#include "math/math_types.h"
#include "camera.h"

enum {
    RENDER_VERTEX_CAPACITY = 4096
};

typedef struct {
    ProjectedVertex projected[RENDER_VERTEX_CAPACITY];
} RenderScratch;

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

void render_frame(EngineRenderer *renderer, const SceneObject *object, const Camera *camera);
void renderer_shutdown(EngineRenderer *renderer);

#endif