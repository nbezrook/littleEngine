#include "renderer.h"
#include "rasterizer.h"

bool renderer_init(EngineRenderer *renderer, const char *title, int width, int height)
{
    *renderer = (EngineRenderer){0};
    if (!lighting_lut_init(&renderer->lighting_lut)) return false;
    if (!framebuffer_init(&renderer->framebuffer, width, height)) {
        SDL_SetError("CPU framebuffer initialization failed (%d x %d).", width, height);
        SDL_Log("%s", SDL_GetError());
        return false;
    }
    renderer->shadow_scene=shadow_scene_create(SHADOW_TRIANGLE_CAPACITY);
    if (!renderer->shadow_scene) {
        SDL_SetError("Shadow scene allocation failed.");
        goto fail;
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) goto fail;
    renderer->video_initialized = true;

    renderer->window = SDL_CreateWindow(title, width, height, 0);
    if (!renderer->window) goto fail;
    renderer->sdl_renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->sdl_renderer) goto fail;
    renderer->screen_texture = SDL_CreateTexture(renderer->sdl_renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    if (!renderer->screen_texture) goto fail;

    SDL_Log("littleEngine renderer initialized: %d by %d", width, height);
    return true;

fail:
    SDL_Log("Renderer initialization failed: %s", SDL_GetError());
    renderer_shutdown(renderer);
    return false;
}

void renderer_shutdown(EngineRenderer *renderer)
{
    if (!renderer) return;
    if (renderer->screen_texture) SDL_DestroyTexture(renderer->screen_texture);
    if (renderer->sdl_renderer) SDL_DestroyRenderer(renderer->sdl_renderer);
    if (renderer->window) SDL_DestroyWindow(renderer->window);
    shadow_scene_destroy(renderer->shadow_scene);
    framebuffer_shutdown(&renderer->framebuffer);
    if (renderer->video_initialized) SDL_Quit();
    *renderer = (EngineRenderer){0};
}

void render_frame(EngineRenderer *renderer, const SceneObject *objects,
    size_t object_count, const Camera *camera, const SceneLighting *lighting)
{
    if (!shadow_scene_build(renderer->shadow_scene,objects,object_count)) {
        SDL_Log("Shadow scene exceeds its budget or contains invalid geometry.");
        return;
    }
    FrameBuffer *frame = &renderer->framebuffer;
    RasterTarget target = {
        .pixels = frame->pixels,
        .width = frame->width,
        .height = frame->height,
        .pitch = frame->pitch,
        .depth = &frame->depth
    };
    if (!rasterizer_draw_scene(&target, &renderer->scratch, objects, object_count, camera, lighting, &renderer->lighting_lut, renderer->shadow_scene)) {
        SDL_Log("CPU rasterizer rejected the target or scene geometry.");
        return;
    }
    if (!SDL_UpdateTexture(renderer->screen_texture, NULL, frame->pixels, frame->pitch)) {
        SDL_Log("Framebuffer upload failed: %s", SDL_GetError());
        return;
    }
    SDL_SetRenderDrawColor(renderer->sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer->sdl_renderer);
    SDL_RenderTexture(renderer->sdl_renderer, renderer->screen_texture, NULL, NULL);
    SDL_RenderPresent(renderer->sdl_renderer);
}
