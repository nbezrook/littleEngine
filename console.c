#include "console.h"
#include <stdio.h>

static void SDLCALL console_log_output(
    void *userdata,
    int category,
    SDL_LogPriority priority,
    const char *message)
    {
        DebugConsole *console = userdata;

        (void)category;
        (void)priority;
        SDL_strlcpy(
            console->last_message,
            message,
            sizeof(console->last_message)
        );
    }

bool debug_console_init(DebugConsole *console) {
    *console = (DebugConsole){0};
    SDL_SetLogOutputFunction(console_log_output, console);
    return SDL_CreateWindowAndRenderer(
        "littleEngine Console",
        800, 400,
        SDL_WINDOW_RESIZABLE,
        &console->window,
        &console->renderer
    );

}

void debug_console_shutdown(DebugConsole *console) {
    if (console->renderer)
        SDL_DestroyRenderer(console->renderer);

    if (console->window)
        SDL_DestroyWindow(console->window);

    *console = (DebugConsole){0};
}

void debug_console_draw(DebugConsole *console) {
    SDL_Renderer *r = console->renderer;
    // clear console and draw black
    SDL_SetRenderDrawColor(r, 20, 20, 25, 255);
    SDL_RenderClear(r);
    // draw text in right color
    SDL_SetRenderDrawColor(r, 230, 230, 230, 255);
    // greet 
    SDL_RenderDebugText(r, 10, 10, "littleEngine 0.1");
    SDL_RenderDebugText(r, 10, 22, "Ready to draw cubes in software :)");
    SDL_RenderDebugText(r, 10, 34, console->last_message);
    // push console to screen
    SDL_RenderPresent(r);
}

bool debug_console_event(
    DebugConsole *console,
    const SDL_Event *event)
{
    if (SDL_GetWindowFromEvent(event) != console->window)
        return false;

    if (event->type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
        SDL_HideWindow(console->window);

    return true;
}

void debug_console_wait_for_close(DebugConsole *console) {
    bool waiting = true;
    SDL_Event event;

    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                waiting = false;
            }
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                SDL_GetWindowFromEvent(&event) == console->window) {
                waiting = false;
            }
        }
        if (!waiting) break;

        debug_console_draw(console);
        SDL_Delay(16);
    }
}