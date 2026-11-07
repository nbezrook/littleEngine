#include "engine.h"
#include "entities.h"
#include "math/primitives.h"
#include "assetsThinker.h"

/**
 * @brief Advance one fixed simulation step: sample movement, move the camera, and rotate the object.
 * @param engine Non-null initialized engine.
 * @param dt Step duration in seconds; currently 1/60. Does not poll events or render.
 */
static void engine_tick (Engine *engine, double dt) {
    InputMovement movement = input_get_movement(&engine->input);
    camera_move(&engine->camera,movement.forward,movement.right,3.0f,dt);
    server_tick(&engine->server, dt);
}

bool engine_init(Engine *engine, int width, int height) {
    *engine = (Engine){0};

    if (!debug_console_log_init(&engine->console)) {
        SDL_Log("Log capture initialization failed: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Starting littleEngine...");

    engine->camera = (Camera){
    .position = {0.0f, 0.0f, -4.0f},
    .focal_length_pixels = 400.0f,
    .near_clip = 0.1f};

    if (!server_add_entity(&engine->server, (Entity){
            .position = {-1.5f, 0.0f, 0.0f},
            .rotation_rate = {0.5f, 1.0f, 0.0f}
        }) ||
        !server_add_entity(&engine->server, (Entity){
            .position = {1.5f, 0.0f, 1.0f},
            .rotation_rate = {0.0f, -0.7f, 0.3f}
        })) {
        debug_console_shutdown(&engine->console);
        return false;
    }
    
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

    // texture init

    CPUTexture *checker = texture_book_create(&engine->textures, 64, 64);

    if (!checker) {
        SDL_Log("Checkerboard texture initialization failed.");
        engine_shutdown(engine);
        return false;
    }

    texture_make_checker(checker);
    CPUTexture *image = texture_book_load_png(&engine->textures, "IndexOfMiracles/tex/FACE.png");

    if (!image) {
        image = checker;
    }

    // little unit cube object 
    client_init(&engine->client, &cube_mesh, image);
    // REPLACE WITH SCENE SPACE


    // tic init
    engine->previous_time = SDL_GetPerformanceCounter();





    // input int

    if (!input_init(&engine->input, engine->renderer.window)) {
    engine_shutdown(engine);
    return false;
    }


    SDL_Log("littleEngine initialized.");
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

        if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && SDL_GetWindowFromEvent(&event) == engine->renderer.window) {
            return false;
        }

        input_event(&engine->input, &event);
    } 

    InputLook look = input_take_look(&engine->input);

    camera_apply_look(&engine->camera, look.yaw, look.pitch);

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

    ServerSnapshot snapshot;

    if (!server_build_snapshot(&engine->server, &snapshot) || !client_receive_snapshot(&engine->client, &snapshot)) {
        SDL_Log("Server snapshot exchange failed.");
        return false;
    }

    render_frame(
        &engine->renderer,
        engine->client.objects,
        engine->client.object_count,
        &engine->camera
    );

    if (!(SDL_GetWindowFlags(engine->console.window)
    & SDL_WINDOW_HIDDEN)) {
        debug_console_draw(&engine->console);
    }

    return true;
}

void engine_shutdown(Engine *engine) {
    input_shutdown(&engine->input);
    texture_book_shutdown(&engine->textures);
    debug_console_shutdown(&engine->console);
    renderer_shutdown(&engine->renderer);
}