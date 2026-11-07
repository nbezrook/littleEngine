#ifndef LITTLEENGINE_CONSOLE_H
#define LITTLEENGINE_CONSOLE_H

#include "include/SDL3/SDL.h"

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    char last_message[256];
} DebugConsole;

/**
 * @brief Create a separate SDL debug window and install the SDL log callback.
 * @param console Non-null destination with no live resources; overwritten with zero state first.
 * @return True when SDL creates the console window and renderer; false on failure.
 * @note Main thread only; SDL video must already be initialized.
 * The registered callback borrows console, which must remain at a stable address.
 */
bool debug_console_init(DebugConsole *console);
/**
 * @brief Consume events belonging to the console; hide it on a close request.
 * @param console Non-null console state.
 * @param event Non-null event borrowed from the application's event loop.
 * @return True for any console-window event; false for an event belonging elsewhere.
 */
bool debug_console_event(DebugConsole *console, const SDL_Event *event);
/**
 * @brief Draw the console heading and latest log message, then present its window.
 * @param console Non-null successfully initialized console, used on the main thread.
 */
void debug_console_draw(DebugConsole *console);
/**
 * @brief Destroy the console renderer/window and reset the structure.
 * @param console Non-null initialized or zero-initialized console, used on the main thread.
 * @warning Does not unregister the SDL log callback; it still points at console afterward.
 * The caller must preserve that state or replace the callback before its lifetime ends.
 */
void debug_console_shutdown(DebugConsole *console);
/**
 * @brief Pump events and draw the console until quit or a console close request.
 * @param console Non-null visible, initialized console, used on the main thread.
 * @note Blocking startup/error-display loop; consumes events and does not run game ticks.
 * It can display an error while the process is alive, not survive a process crash.
 */
void debug_console_wait_for_close(DebugConsole *console);

#endif