#ifndef LITTLEENGINE_CONSOLE_H
#define LITTLEENGINE_CONSOLE_H

#include "include/SDL3/SDL.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    char last_message[256];
} DebugConsole;

bool debug_console_init(DebugConsole *console);
bool debug_console_event(DebugConsole *console, const SDL_Event *event);
void debug_console_draw(DebugConsole *console);
void debug_console_shutdown(DebugConsole *console);

#endif