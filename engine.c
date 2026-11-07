#include "engine.h"
#include "entities.h"
#include "math/primitives.h"

static void engine_tick (Engine *engine, double dt) {
    ++engine->tic_number;
    scene_object_tick(&engine->object, dt);
}

bool engine_init(Engine *engine, int width, int height) {
    *engine = (Engine){0};

    engine->camera = (Camera){
    .position = {0.0f, 0.0f, -4.0f},
    .focal_length_pixels = 400.0f,
    .near_clip = 0.1f};

    if (!renderer_init(
            &engine->renderer, "my littleEngine", width, height)) {
        SDL_Log("Engine initialization failed: %s", SDL_GetError());
        return false;
    }

    if (!debug_console_init(&engine->console)) {
        SDL_Log("Console initialization failed: %s", SDL_GetError());
        renderer_shutdown(&engine->renderer);
        return false;
    }

    // tic init
    engine->previous_time = SDL_GetPerformanceCounter();

    // little unit cube object 
    engine->object = (SceneObject) {
        .mesh = &cube_mesh,
        .rotation_rate = {.x = 0.5f, .y = 1.0f},
        .color = 0x0000FF00
    };

    // REPLACE WITH SCENE SPACE

    return true;
}
bool engine_frame(Engine *engine) {
    const double tic_duration = 1.0 / 60.0;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            return false;
        }

        if (debug_console_event(&engine->console, &event)) {
            continue; // Event handled by console
        }

    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
        SDL_GetWindowFromEvent(&event) == engine->renderer.window)
        return false;

        // INPUT HERE

    }

    // TIC CODE

    Uint64 now = SDL_GetPerformanceCounter();
    double elapsed = (double)(now - engine->previous_time) / (double)SDL_GetPerformanceFrequency();

    engine->previous_time = now;
    if (elapsed > 0.25) {
        elapsed = 0.25; // Avoid spiral of death
    }

    engine->accumulator += elapsed;

    while (engine->accumulator >= tic_duration) {
        engine_tick(engine, tic_duration);
        engine->accumulator -= tic_duration;
    }

    render_frame(&engine->renderer, &engine->object, &engine->camera);

    if (!(SDL_GetWindowFlags(engine->console.window)
    & SDL_WINDOW_HIDDEN)) {
        debug_console_draw(&engine->console);
    }

    return true;
}

void engine_shutdown(Engine *engine) {
    debug_console_shutdown(&engine->console);
    renderer_shutdown(&engine->renderer);
}