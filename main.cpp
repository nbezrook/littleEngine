#define SDL_MAIN_USE_CALLBACKS 0
#include "include/SDL3/SDL.h"
#include <stdint.h>
#include <stdbool.h>
//#include <stdlib.h>
#include "engine.h"
#include "littleEngineApplication.hpp"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960

/**
 * @brief Create the application owner, initialize the engine, and run until shutdown.
 * @return 0 after normal exit; 1 if initialization fails.
 * @note Application destruction releases resources after successful initialization.
 */
int main() {
    Application littleEngineApp;

    if (!littleEngineApp.initialize(SCREEN_WIDTH, SCREEN_HEIGHT))
        return 1;

    littleEngineApp.run();
    return 0;
}