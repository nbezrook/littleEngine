#include "engine.h"
#include "entities.h"
#include "math/primitives.h"
#include "assetsThinker.h"
#include <stdio.h>
#include <string.h>

// ugly f-ing thing for the sake of resolving engine asset paths, can probably be improved
bool engine_asset_path(const Engine *engine, const char *relative_path, char *out, size_t capacity)
{
    if (!out || capacity == 0) return false;
    out[0] = '\0';
    if (!engine || !relative_path || !relative_path[0]) return false;
    size_t length = strlen(engine->game_directory);
    if (length == 0) return false;
    char last = engine->game_directory[length - 1];
    const char *separator = (last == '/' || last == '\\') ? "" : "/";
    int written = snprintf(out, capacity, "%s%s%s", engine->game_directory,separator, relative_path);
    if (written < 0 || (size_t)written >= capacity) {
        out[0] = '\0';
        return false;
    }
    return true;
}

/**
 * @brief Advance one fixed simulation step: sample movement, move the camera, and rotate the object.
 * @param engine Non-null initialized engine.
 * @param dt Step duration in seconds; currently 1/60. Does not poll events or render.
 */
static bool engine_tick (Engine *engine, double dt) {
    PlayerCommand command = client_build_command(&engine->client);
    if (!server_submit_player_command(&engine->server, &command)) {
        SDL_Log("Player command rejected.");
        return false;
    }

    server_tick(&engine->server, dt);
    return true;
}

bool engine_init(Engine *engine, int width, int height, const char *game_directory) {
    *engine = (Engine){0};

    if (!game_directory || !game_directory[0] ||
        strlen(game_directory) >= sizeof(engine->game_directory)) {
        SDL_Log("Invalid or oversized game directory.");
        return false;
    }
    memcpy(engine->game_directory, game_directory, strlen(game_directory) + 1);

    if (!debug_console_log_init(&engine->console)) {
        SDL_Log("Log capture initialization failed: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Starting littleEngine...");
    SDL_Log("Game directory: %s", engine->game_directory);

    //shouldn't be hardcoded, change later (game config?)
    engine->camera = (Camera){
    .focal_length_pixels = 400.0f,
    .near_clip = 0.1f};

    if (!server_load_test_map(&engine->server)) {
        SDL_Log("Test map creation failed.");
        debug_console_shutdown(&engine->console);
        return false;
    }

    // spawn player
    server_spawn_player(&engine->server,(Vector3){0.0f, 0.0f, -4.0f});


    if (!server_add_entity(&engine->server, (Entity){
            .position = {-1.5f, 0.0f, 0.0f},
            .rotation_rate = {0.5f, 1.0f, 0.0f}
        }) || !server_add_entity(&engine->server, (Entity){
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

    engine->textures.fallback = checker;
    char catalog_path[ENGINE_PATH_CAPACITY];



    if (!engine_asset_path(engine, "textures.json",catalog_path, sizeof catalog_path) || !texture_book_read_catalog(&engine->textures, catalog_path)) {
        SDL_Log("Texture catalog initialization failed.");
        engine_shutdown(engine);
        return false;
    }

    const CPUTexture *image = texture_book_resolve(&engine->textures, "face");

    if (!image) {
        image = checker;
    }

    // little unit cube object 
    client_init(&engine->client, &cube_mesh, image);
    // REPLACE WITH SCENE SPACE


    // tic init
    engine->previous_time = SDL_GetPerformanceCounter();


    MapData map;

    if (!server_copy_map(&engine->server, &map) || !client_receive_map(&engine->client,&map,&engine->textures)) {
        SDL_Log("Map transfer to Client failed.");
        engine_shutdown(engine);
        return false;
    }


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

    client_sample_input(&engine->client, &engine->input);

    // TIC CODE

    Uint64 now = SDL_GetPerformanceCounter();
    double elapsed = (double)(now - engine->previous_time) / (double)SDL_GetPerformanceFrequency();

    engine->previous_time = now;
    if (elapsed > 0.25) {
        elapsed = 0.25; //spiral of death
    }

    engine->accumulator += elapsed;

    while (engine->accumulator >= tic_duration) {
        if (!engine_tick(engine, tic_duration)) return false;
        engine->accumulator -= tic_duration;
    }

    ServerSnapshot snapshot;

    if (!server_build_snapshot(&engine->server, &snapshot) || !client_receive_snapshot(&engine->client, &snapshot)) {
        SDL_Log("Server snapshot exchange failed.");
        return false;
    }

    client_update_camera(&engine->client, &engine->camera);

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
