#define SDL_MAIN_USE_CALLBACKS 0
#include "include/SDL3/SDL.h"
#include <stdint.h>
#include <stdbool.h>
//#include <stdlib.h>
#include "engine.h"
#include "littleEngineApplication.hpp"
#include <cstring>

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960

/**
 * @brief Create the application owner, initialize the engine, and run until shutdown.
 * @return 0 after normal exit; 1 if initialization fails.
 * @note Application destruction releases resources after successful initialization.
 */
int main(int argc, char **argv) {
    const char *game_directory = ".";
    if (argc != 1) {
        if (argc != 3 || std::strcmp(argv[1], "-game") != 0 || !argv[2][0]) {
            SDL_Log("Usage: engine.exe [-game <game-directory>]");
            return 2;
        }
        game_directory = argv[2];
    }
    Application littleEngineApp;

    if (!littleEngineApp.initialize(SCREEN_WIDTH, SCREEN_HEIGHT, game_directory))
        return 1;

    littleEngineApp.run();
    return 0;
}
