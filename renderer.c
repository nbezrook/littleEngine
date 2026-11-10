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
    // good candidate for delegating to job; renderer needs to have way to lock and unlock
    renderer->shadow_maps=shadow_maps_create(SCENE_LIGHT_CAPACITY,256,0.01f);
    if (!renderer->shadow_maps) {
        SDL_SetError("Shadow-map allocation failed.");
        goto fail;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) goto fail;
    renderer->video_initialized = true;

renderer->prepared_scene = raster_scene_create(width, height, 8192, 1024 * 1024);

    if (!renderer->prepared_scene) {
        SDL_SetError("Prepared scene allocation failed.");
        goto fail;
    }

    renderer->workers = render_workers_create(0);

    if (!renderer->workers) {
        SDL_SetError("Render worker startup failed.");
        goto fail;
}

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

    render_workers_destroy(renderer->workers);
    raster_scene_destroy(renderer->prepared_scene);

    if (renderer->screen_texture) SDL_DestroyTexture(renderer->screen_texture);
    if (renderer->sdl_renderer) SDL_DestroyRenderer(renderer->sdl_renderer);
    if (renderer->window) SDL_DestroyWindow(renderer->window);
    shadow_maps_destroy(renderer->shadow_maps);
    framebuffer_shutdown(&renderer->framebuffer);
    if (renderer->video_initialized) SDL_Quit();
    *renderer = (EngineRenderer){0};
}

typedef struct {
    RasterScene *scene;
    RasterTarget target;
} TileWork;

/* Copilot

The render_tile_job function is a static function that takes a pointer to data, a tile index, and a worker index as parameters. 
It retrieves a TileWork structure from data and calls the raster_scene_render_tile function to render a specific tile of a scene, 
returning a boolean indicating success or failure.*/
static bool render_tile_job(void *data, size_t tile, size_t worker)
{
    (void)worker;
    TileWork *work = data;
    return raster_scene_render_tile(work->scene, &work->target, tile, RASTER_SIMD);
}



void render_frame(EngineRenderer *renderer, const SceneObject *objects,
    size_t object_count, const Camera *camera, const SceneLighting *lighting)
{
    if (!shadow_maps_build(renderer->shadow_maps,&renderer->scratch,objects,object_count,lighting)) {
        SDL_Log("Shadow-map build failed: invalid input, light budget, or unsupported shadowed light type.");
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


    if (!raster_scene_prepare(renderer->prepared_scene,objects, object_count, camera, lighting,&renderer->lighting_lut, renderer->shadow_maps)) {
        SDL_Log("Scene preparation failed.");
        return;
    }

    TileWork work = {renderer->prepared_scene, target};

    if (!render_workers_run(renderer->workers,raster_scene_job_count(renderer->prepared_scene),render_tile_job, &work)) {
        SDL_Log("Tile rendering failed.");
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
