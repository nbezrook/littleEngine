#define SDL_MAIN_USE_CALLBACKS 0
#include "include/SDL3/SDL.h"
#include <stdint.h>
#include <stdbool.h>
//#include <stdlib.h>
#include "engine.h"
#include "littleEngineApplication.hpp"

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 960

int main() {
    Application littleEngineApp;

    if (!littleEngineApp.initialize(SCREEN_WIDTH, SCREEN_HEIGHT))
        return 1;

    littleEngineApp.run();
    return 0;
}